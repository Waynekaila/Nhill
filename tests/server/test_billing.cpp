// Subscriptions: trial, reminders, grace period, suspension, annual plans,
// provider webhooks (signature, duplicates, mismatches), manual reconciliation.
#include <ctime>
#include <future>

#include "nhill/money.hpp"
#include "harness.hpp"
#include "util/crypto.hpp"

using namespace nht;

namespace {

std::string planId(const std::string& period, const std::string& price = "25.00") {
    auto& h = Harness::get();
    std::string code = uniq("plan-");
    for (auto& c : code) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto r = h.su("INSERT INTO plans (code, name_fr, name_en, billing_period, price_minor, currency, max_users, features) "
                  "VALUES ('" + code + "', 'Formule', 'Plan', '" + period + "', " +
                  std::to_string(*nhill::parseMoney(price, 2)) + ", 'USD', 10, '{\"multi_warehouse\": true}') RETURNING id");
    return r[0][0].c_str();
}

Response webhook(const json& body, const std::string& secret = "test-webhook-secret-0123456789abcdef",
                 long long ts = 0) {
    auto& h = Harness::get();
    std::string raw = body.dump();
    std::string t = std::to_string(ts ? ts : static_cast<long long>(std::time(nullptr)));
    httplib::Client cli("127.0.0.1", h.port);
    httplib::Headers hd{{"X-Nhill-Timestamp", t}, {"X-Nhill-Signature", "v1=" + nh::crypto::hmacSha256Hex(secret, t + "." + raw)}};
    auto res = cli.Post("/api/v1/webhooks/payments/generic", hd, raw, "application/json");
    Response r;
    r.status = res->status;
    r.raw = res->body;
    if (!res->body.empty()) r.body = json::parse(res->body);
    return r;
}

json paidEvent(const std::string& orderId, const std::string& amount, const std::string& eventId = "") {
    return {{"event_id", eventId.empty() ? uniq("evt_") : eventId},
            {"type", "payment.succeeded"},
            {"data", {{"order_id", orderId}, {"amount", amount}, {"currency", "USD"}, {"provider_reference", uniq("TX")}}}};
}

std::string order(const Tenant& t, const std::string& plan) {
    auto r = Harness::get().post("/api/v1/subscription/orders", {{"plan_id", plan}}, t.token);
    if (r.status != 201) throw std::runtime_error("order failed: " + r.raw);
    return r.body["order"]["id"];
}

json sub(const Tenant& t) { return Harness::get().get("/api/v1/subscription", t.token).body["subscription"]; }

}  // namespace

TEST(Subscription, TrialReminderGraceAndSuspension) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    // J-7: first reminder, sent once.
    h.su("UPDATE subscriptions SET period_end = now() + interval '6 days' WHERE establishment_id = '" + t.id + "'");
    auto s1 = nh::jobs::subscriptionLifecycle(h.env());
    EXPECT_GE(s1.remindersSent, 1);
    nh::jobs::subscriptionLifecycle(h.env());
    auto rem = h.su("SELECT count(*) FROM subscription_events WHERE establishment_id = '" + t.id + "' AND kind = 'reminder_sent'");
    EXPECT_EQ(rem[0][0].as<int>(), 1);
    // Due date passed, no payment: grace period of 7 days, service maintained.
    h.su("UPDATE subscriptions SET period_start = now() - interval '30 days', period_end = now() - interval '1 hour' "
         "WHERE establishment_id = '" + t.id + "'");
    nh::jobs::subscriptionLifecycle(h.env());
    json s = sub(t);
    EXPECT_EQ(s["status"], "past_due");
    EXPECT_TRUE(s["write_allowed"].get<bool>());
    auto grace = h.su("SELECT extract(epoch FROM grace_until - period_end) / 86400 FROM subscriptions WHERE establishment_id = '" + t.id + "'");
    EXPECT_NEAR(grace[0][0].as<double>(), 7.0, 0.001);
    EXPECT_EQ(h.post("/api/v1/parties", {{"is_customer", true}, {"name", "Pendant la grâce"}}, t.token).status, 201);
    // End of grace: read-only suspension.
    h.su("UPDATE subscriptions SET period_end = now() - interval '8 days', grace_until = now() - interval '1 hour' "
         "WHERE establishment_id = '" + t.id + "'");
    nh::jobs::subscriptionLifecycle(h.env());
    EXPECT_EQ(sub(t)["status"], "suspended");
    auto w = h.post("/api/v1/parties", {{"is_customer", true}, {"name", "Après la grâce"}}, t.token);
    EXPECT_EQ(w.status, 402);
    EXPECT_EQ(h.get("/api/v1/customers", t.token).status, 200);                      // still readable
    EXPECT_EQ(h.get("/api/v1/products/export.csv", t.token).status, 200);            // still exportable
    auto n = h.su("SELECT kind FROM notifications WHERE establishment_id = '" + t.id + "' ORDER BY created_at");
    std::vector<std::string> kinds;
    for (const auto& row : n) kinds.push_back(row[0].c_str());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), "subscription_reminder"), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), "subscription_grace"), kinds.end());
    EXPECT_NE(std::find(kinds.begin(), kinds.end(), "subscription_suspended"), kinds.end());
    // Paying after suspension restores write access, starting now.
    std::string plan = planId("monthly");
    std::string o = order(t, plan);
    EXPECT_EQ(webhook(paidEvent(o, "25.00")).body["outcome"], "applied");
    EXPECT_EQ(sub(t)["status"], "active");
    EXPECT_EQ(h.post("/api/v1/parties", {{"is_customer", true}, {"name", "Après paiement"}}, t.token).status, 201);
}

TEST(Subscription, AnnualPlanStaysAnnualDespiteMonthlyCodeRotation) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    // Trial ended yesterday; pay an annual plan.
    h.su("UPDATE subscriptions SET period_start = now() - interval '31 days', period_end = now() - interval '1 day' "
         "WHERE establishment_id = '" + t.id + "'");
    std::string plan = planId("annual", "240.00");
    EXPECT_EQ(webhook(paidEvent(order(t, plan), "240.00")).body["outcome"], "applied");
    auto len = h.su("SELECT period_start + interval '1 year' = period_end FROM subscriptions WHERE establishment_id = '" + t.id + "'");
    EXPECT_TRUE(len[0][0].as<bool>()) << "exactly one calendar year";
    std::string endBefore = sub(t)["period_end"];
    // 12 code rotations later, the subscription is untouched and nothing was billed.
    for (int i = 0; i < 12; ++i) {
        h.su("UPDATE establishment_codes SET created_at = now() - interval '31 days', expires_at = now() - interval '1 second' "
             "WHERE status = 'active' AND establishment_id = '" + t.id + "'");
        nh::jobs::rotateCodes(h.env());
    }
    EXPECT_EQ(sub(t)["period_end"], endBefore);
    auto orders = h.su("SELECT count(*) FROM subscription_orders WHERE establishment_id = '" + t.id + "'");
    EXPECT_EQ(orders[0][0].as<int>(), 1);
    auto codes = h.su("SELECT count(*) FROM establishment_codes WHERE establishment_id = '" + t.id + "'");
    EXPECT_EQ(codes[0][0].as<int>(), 13);
}

TEST(Subscription, AnnualExpiryAndPaymentDuringGraceKeepsContractDates) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string plan = planId("annual", "240.00");
    h.su("UPDATE subscriptions SET plan_id = '" + plan + "', status = 'active', is_trial = false, "
         "period_start = now() - interval '1 year' - interval '2 days', period_end = now() - interval '2 days' "
         "WHERE establishment_id = '" + t.id + "'");
    nh::jobs::subscriptionLifecycle(h.env());
    EXPECT_EQ(sub(t)["status"], "past_due");
    auto before = h.su("SELECT period_end FROM subscriptions WHERE establishment_id = '" + t.id + "'")[0][0].as<std::string>();
    EXPECT_EQ(webhook(paidEvent(order(t, plan), "240.00")).body["outcome"], "applied");
    auto after = h.su("SELECT period_start, period_start + interval '1 year' = period_end FROM subscriptions WHERE establishment_id = '" + t.id + "'");
    EXPECT_EQ(after[0][0].as<std::string>(), before) << "new period starts at the contractual end, not at payment date";
    EXPECT_TRUE(after[0][1].as<bool>());
    EXPECT_EQ(sub(t)["status"], "active");
}

TEST(Subscription, PayingDuringTrialKeepsRemainingTrialDays) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    auto trialEnd = h.su("SELECT period_end FROM subscriptions WHERE establishment_id = '" + t.id + "'")[0][0].as<std::string>();
    std::string plan = planId("monthly");
    webhook(paidEvent(order(t, plan), "25.00"));
    auto s = h.su("SELECT period_start, status, is_trial FROM subscriptions WHERE establishment_id = '" + t.id + "'");
    EXPECT_EQ(s[0][0].as<std::string>(), trialEnd);
    EXPECT_EQ(std::string(s[0][1].c_str()), "active");
    EXPECT_FALSE(s[0][2].as<bool>());
}

TEST(Webhook, DuplicateDeliveryIsAppliedOnce) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string plan = planId("monthly");
    std::string o = order(t, plan);
    json ev = paidEvent(o, "25.00", uniq("evt_dup_"));
    // Delivered twice in parallel, then once more.
    auto f1 = std::async(std::launch::async, [&] { return webhook(ev); });
    auto f2 = std::async(std::launch::async, [&] { return webhook(ev); });
    auto r1 = f1.get(), r2 = f2.get();
    auto r3 = webhook(ev);
    EXPECT_EQ(r1.status, 200);
    EXPECT_EQ(r2.status, 200);
    int applied = (r1.body["outcome"] == "applied") + (r2.body["outcome"] == "applied");
    EXPECT_EQ(applied, 1);
    EXPECT_EQ(r3.body["status"], "duplicate");
    auto ev2 = h.su("SELECT count(*) FROM subscription_events WHERE establishment_id = '" + t.id + "' AND kind IN ('activated','renewed')");
    EXPECT_EQ(ev2[0][0].as<int>(), 1);
    // A different event for the same, already paid order changes nothing.
    EXPECT_EQ(webhook(paidEvent(o, "25.00")).body["outcome"], "already_paid");
}

TEST(Webhook, SignatureTimestampAndAmountAreVerified) {
    Tenant t = createTenant("company");
    std::string plan = planId("monthly");
    std::string o = order(t, plan);
    EXPECT_EQ(webhook(paidEvent(o, "25.00"), "wrong-secret-wrong-secret-wrong-secret").status, 401);
    EXPECT_EQ(webhook(paidEvent(o, "25.00"), "test-webhook-secret-0123456789abcdef", std::time(nullptr) - 3600).status, 401);
    EXPECT_EQ(webhook(paidEvent(o, "1.00")).body["outcome"], "amount_mismatch");
    EXPECT_EQ(sub(t)["status"], "trialing");
    // A client claiming to have paid has no route to change its subscription.
    auto& h = Harness::get();
    EXPECT_EQ(h.post("/api/v1/webhooks/payments/generic", paidEvent(o, "25.00")).status, 401);
    EXPECT_EQ(h.post("/api/v1/webhooks/payments/unknownprovider", paidEvent(o, "25.00")).status, 404);
}

TEST(Webhook, FailedPaymentIsRecordedWithoutRights) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string o = order(t, planId("monthly"));
    json ev = {{"event_id", uniq("evt_fail_")}, {"type", "payment.failed"},
               {"data", {{"order_id", o}, {"amount", "25.00"}, {"currency", "USD"}}}};
    EXPECT_EQ(webhook(ev).body["outcome"], "marked_failed");
    auto st = h.su("SELECT status FROM subscription_orders WHERE id = '" + o + "'");
    EXPECT_EQ(std::string(st[0][0].c_str()), "failed");
    EXPECT_EQ(sub(t)["is_trial"], true);
}

TEST(Subscription, PlanAudienceAndSeatLimits) {
    auto& h = Harness::get();
    Tenant individual = createTenant("individual");
    auto r = h.su("INSERT INTO plans (code, name_fr, name_en, billing_period, price_minor, currency, audience) VALUES ('" +
                  uniq("pme-") + "', 'PME', 'SME', 'monthly', 5000, 'USD', 'company') RETURNING id");
    auto o = h.post("/api/v1/subscription/orders", {{"plan_id", r[0][0].c_str()}}, individual.token);
    EXPECT_EQ(o.status, 409);
    EXPECT_EQ(o.body["error"]["code"], "plan_audience_mismatch");
    std::string viewer = addMember(createTenant("company"), "viewer");
    EXPECT_EQ(h.post("/api/v1/subscription/orders", {{"plan_id", r[0][0].c_str()}}, viewer).status, 403);
}

// Only owners and administrators see the plan, prices, payments and related options.
TEST(Subscription, PlanDetailsAreReservedToOwnersAndAdministrators) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string admin = addMember(t, "admin");
    EXPECT_EQ(h.get("/api/v1/subscription", admin).status, 200);
    for (const char* role : {"manager", "sales", "accountant", "viewer", "cashier", "cash_manager"}) {
        Tenant other = createTenant("company");  // trial user limit: one establishment per role
        std::string token = addMember(other, role);
        EXPECT_EQ(h.get("/api/v1/subscription", token).status, 403) << role;
        auto me = h.get("/api/v1/me", token);
        ASSERT_EQ(me.status, 200);
        EXPECT_FALSE(me.body["subscription"].contains("plan")) << role;
        EXPECT_FALSE(me.body["subscription"].contains("period_end")) << role;
        EXPECT_TRUE(me.body["subscription"]["write_allowed"].get<bool>()) << role;
        EXPECT_EQ(h.post("/api/v1/subscription/quote", {{"plan_id", "00000000-0000-0000-0000-000000000000"}}, token).status, 403) << role;
    }
    EXPECT_TRUE(h.get("/api/v1/me", t.token).body["subscription"].contains("period_end"));
}
