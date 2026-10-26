// Billing engine (prices, upgrade / downgrade, promo codes, expiry), payment providers
// (secrets, Stripe and PayPal adapters against a fake transport), support tickets,
// error journal and console permissions.
#include <ctime>
#include <mutex>

#include "harness.hpp"
#include "nhill/money.hpp"
#include "services/providers.hpp"
#include "util/crypto.hpp"

using namespace nht;

namespace {

struct Plan {
    std::string id, monthly, annual;
};

Plan makePlan(int tier, const std::string& monthly, const std::string& annual, int maxUsers = 10) {
    auto& h = Harness::get();
    std::string code = uniq("tier") ;
    for (auto& c : code) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto r = h.su("INSERT INTO plans (code, name_fr, name_en, billing_period, price_minor, currency, max_users, tier) VALUES ('" +
                  code + "', 'Formule " + std::to_string(tier) + "', 'Plan', 'monthly', " +
                  std::to_string(*nhill::parseMoney(monthly, 2)) + ", 'USD', " + std::to_string(maxUsers) + ", " +
                  std::to_string(tier) + ") RETURNING id");
    Plan p;
    p.id = r[0][0].c_str();
    p.monthly = h.su("SELECT id FROM plan_prices WHERE plan_id = '" + p.id + "'")[0][0].c_str();
    p.annual = h.su("INSERT INTO plan_prices (plan_id, duration_months, currency, price_minor) VALUES ('" + p.id + "', 12, 'USD', " +
                    std::to_string(*nhill::parseMoney(annual, 2)) + ") RETURNING id")[0][0].c_str();
    return p;
}

std::string adminToken() {
    static std::string t = adminLogin("superadmin");
    return t;
}

Response admin(const std::string& method, const std::string& path, const json& body = nullptr) {
    return Harness::get().request(method, path, body, adminToken());
}

// Marks an order paid through the configured generic webhook (harness secret).
Response payWebhook(const std::string& orderId, const std::string& amount) {
    auto& h = Harness::get();
    json body = {{"event_id", uniq("evt_")},
                 {"type", "payment.succeeded"},
                 {"data", {{"order_id", orderId}, {"amount", amount}, {"currency", "USD"}, {"provider_reference", uniq("TX")}}}};
    std::string raw = body.dump();
    std::string t = std::to_string(static_cast<long long>(std::time(nullptr)));
    httplib::Client cli("127.0.0.1", h.port);
    httplib::Headers hd{{"X-Nhill-Timestamp", t},
                        {"X-Nhill-Signature", "v1=" + nh::crypto::hmacSha256Hex("test-webhook-secret-0123456789abcdef",
                                                                                t + "." + raw)}};
    auto res = cli.Post("/api/v1/webhooks/payments/generic", hd, raw, "application/json");
    Response r;
    r.status = res->status;
    r.body = json::parse(res->body);
    return r;
}

json order(const Tenant& t, const json& body) {
    auto r = Harness::get().post("/api/v1/subscription/orders", body, t.token);
    if (r.status != 201) throw std::runtime_error("order failed: " + r.raw);
    return r.body["order"];
}

json sub(const Tenant& t) { return Harness::get().get("/api/v1/subscription", t.token).body["subscription"]; }

// Ends the trial and starts an active paid period of `days` remaining out of 30.
void activeOn(const Tenant& t, const Plan& p, int daysLeft) {
    auto o = order(t, {{"price_id", p.monthly}});
    ASSERT_EQ(payWebhook(o["id"], o["amount"]).body["outcome"], "applied");
    Harness::get().su("UPDATE subscriptions SET is_trial = false, status = 'active', period_start = now() - interval '" +
                      std::to_string(30 - daysLeft) + " days', period_end = now() + interval '" + std::to_string(daysLeft) +
                      " days' WHERE establishment_id = '" + t.id + "'");
}

}  // namespace

TEST(Billing, PlansExposePricesPerDurationAndCurrency) {
    auto p = makePlan(1, "10.00", "100.00");
    auto r = Harness::get().get("/api/v1/plans");
    bool found = false;
    for (const auto& plan : r.body["items"])
        if (plan["id"] == p.id) {
            found = true;
            EXPECT_EQ(plan["prices"].size(), 2u);
            EXPECT_EQ(plan["tier"], 1);
        }
    EXPECT_TRUE(found);
}

TEST(Billing, UpgradeIsImmediateWithProRataCredit) {
    auto& h = Harness::get();
    auto basic = makePlan(10, "30.00", "300.00");
    auto pro = makePlan(20, "60.00", "600.00");
    Tenant t = createTenant("company");
    activeOn(t, basic, 15);  // half of a 30-day period left, 30.00 paid
    auto q = h.post("/api/v1/subscription/quote", {{"price_id", pro.monthly}}, t.token);
    ASSERT_EQ(q.status, 200) << q.raw;
    EXPECT_EQ(q.body["kind"], "upgrade");
    EXPECT_EQ(q.body["credit"], "15.00");
    EXPECT_EQ(q.body["total"], "45.00");
    auto o = order(t, {{"price_id", pro.monthly}});
    EXPECT_EQ(o["kind"], "upgrade");
    EXPECT_EQ(payWebhook(o["id"], o["amount"]).body["outcome"], "applied");
    auto s = sub(t);
    EXPECT_EQ(s["plan"]["id"], pro.id);
    EXPECT_EQ(s["status"], "active");
    // New period starts now for one month.
    auto days = h.su("SELECT round(extract(epoch FROM period_end - now()) / 86400) FROM subscriptions WHERE establishment_id = '" +
                     t.id + "'")[0][0].as<int>();
    EXPECT_GE(days, 28);
    EXPECT_LE(days, 31);
}

TEST(Billing, DowngradeIsPaidNowAndEffectiveAtPeriodEnd) {
    auto& h = Harness::get();
    auto basic = makePlan(30, "20.00", "200.00");
    auto pro = makePlan(40, "50.00", "500.00");
    Tenant t = createTenant("company");
    activeOn(t, pro, 10);
    auto o = order(t, {{"price_id", basic.monthly}});
    EXPECT_EQ(o["kind"], "downgrade");
    EXPECT_EQ(o["credit"], "0.00");
    ASSERT_EQ(payWebhook(o["id"], o["amount"]).body["outcome"], "applied");
    auto s = sub(t);
    EXPECT_EQ(s["plan"]["id"], pro.id);                // still on the higher plan
    EXPECT_FALSE(s["pending_change"].is_null());
    // A second change cannot be stacked on a scheduled one.
    EXPECT_EQ(h.post("/api/v1/subscription/quote", {{"price_id", pro.monthly}}, t.token).body["error"]["code"],
              "pending_plan_change");
    h.su("UPDATE subscriptions SET pending_from = now() - interval '1 minute' WHERE establishment_id = '" + t.id + "'");
    auto st = nh::jobs::subscriptionLifecycle(h.env());
    EXPECT_GE(st.planChanges, 1);
    s = sub(t);
    EXPECT_EQ(s["plan"]["id"], basic.id);
    EXPECT_TRUE(s["pending_change"].is_null());
}

TEST(Billing, PromoCodesAreValidatedReservedAndConsumed) {
    auto& h = Harness::get();
    auto p = makePlan(50, "40.00", "400.00");
    std::string code = uniq("WELCOME");
    for (auto& c : code) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    code.erase(std::remove(code.begin(), code.end(), '.'), code.end());
    code = code.substr(0, 30);
    auto created = admin("POST", "/api/v1/admin/promo-codes",
                         {{"code", code}, {"discount_kind", "percent"}, {"percent_bp", 2500}, {"max_redemptions", 1},
                          {"plan_ids", {p.id}}, {"min_duration_months", 1}});
    ASSERT_EQ(created.status, 201) << created.raw;

    Tenant a = createTenant("company"), b = createTenant("company");
    auto q = h.post("/api/v1/subscription/quote", {{"price_id", p.monthly}, {"promo_code", code}}, a.token);
    ASSERT_EQ(q.status, 200) << q.raw;
    EXPECT_EQ(q.body["discount"], "10.00");
    EXPECT_EQ(q.body["total"], "30.00");
    // Unknown code, other plan.
    EXPECT_EQ(h.post("/api/v1/subscription/quote", {{"price_id", p.monthly}, {"promo_code", "NOPE-XYZ"}}, a.token)
                  .body["error"]["code"], "promo_invalid");
    auto other = makePlan(51, "40.00", "400.00");
    EXPECT_EQ(h.post("/api/v1/subscription/quote", {{"price_id", other.monthly}, {"promo_code", code}}, a.token)
                  .body["error"]["code"], "promo_not_applicable");

    auto o = order(a, {{"price_id", p.monthly}, {"promo_code", code}});
    EXPECT_EQ(o["amount"], "30.00");
    EXPECT_EQ(o["promo_code"], code);
    // Reserved by A: the single redemption is not available to B.
    EXPECT_EQ(h.post("/api/v1/subscription/orders", {{"price_id", p.monthly}, {"promo_code", code}}, b.token)
                  .body["error"]["code"], "promo_exhausted");
    // A replaces its own order: its reservation is released and reused, not counted twice.
    auto o2 = order(a, {{"price_id", p.monthly}, {"promo_code", code}});
    EXPECT_EQ(o2["amount"], "30.00");
    ASSERT_EQ(payWebhook(o2["id"], "30.00").body["outcome"], "applied");
    auto red = admin("GET", "/api/v1/admin/promo-codes/" + created.body["id"].get<std::string>() + "/redemptions");
    int confirmed = 0;
    for (const auto& r : red.body["items"]) confirmed += r["status"] == "confirmed";
    EXPECT_EQ(confirmed, 1);
    // Promo codes are immutable in their substance.
    EXPECT_EQ(admin("PATCH", "/api/v1/admin/promo-codes/" + created.body["id"].get<std::string>(), {{"percent_bp", 9000}}).status,
              422);
}

TEST(Billing, FullyDiscountedOrderIsAppliedWithoutPayment) {
    auto p = makePlan(60, "15.00", "150.00");
    std::string code = "FREE" + nh::crypto::toHex(nh::crypto::randomBytes(4));
    for (auto& c : code) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    ASSERT_EQ(admin("POST", "/api/v1/admin/promo-codes", {{"code", code}, {"discount_kind", "percent"}, {"percent_bp", 10000}})
                  .status, 201);
    Tenant t = createTenant("company");
    auto o = order(t, {{"price_id", p.monthly}, {"promo_code", code}});
    EXPECT_EQ(o["status"], "paid");
    EXPECT_EQ(o["amount"], "0.00");
    EXPECT_EQ(sub(t)["plan"]["id"], p.id);
}

TEST(Billing, UnpaidOrdersExpireAndReleaseTheirPromo) {
    auto& h = Harness::get();
    auto p = makePlan(70, "15.00", "150.00");
    std::string code = "EXP" + nh::crypto::toHex(nh::crypto::randomBytes(4));
    for (auto& c : code) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    ASSERT_EQ(admin("POST", "/api/v1/admin/promo-codes",
                    {{"code", code}, {"discount_kind", "fixed"}, {"currency", "USD"}, {"amount", "5.00"}, {"max_redemptions", 1}})
                  .status, 201);
    Tenant a = createTenant("company"), b = createTenant("company");
    auto o = order(a, {{"price_id", p.monthly}, {"promo_code", code}});
    EXPECT_EQ(o["amount"], "10.00");
    h.su("UPDATE subscription_orders SET expires_at = now() - interval '1 minute' WHERE id = '" + o["id"].get<std::string>() + "'");
    auto st = nh::jobs::subscriptionLifecycle(h.env());
    EXPECT_GE(st.ordersExpired, 1);
    // The released code can now be used by another establishment.
    EXPECT_EQ(h.post("/api/v1/subscription/quote", {{"price_id", p.monthly}, {"promo_code", code}}, b.token).status, 200);
    // A payment confirmed late is still honoured (money was taken).
    EXPECT_EQ(payWebhook(o["id"], "10.00").body["outcome"], "applied");
}

TEST(Billing, FailedPaymentsAreCountedAndNotified) {
    auto& h = Harness::get();
    auto p = makePlan(80, "15.00", "150.00");
    Tenant t = createTenant("company");
    auto o = order(t, {{"price_id", p.monthly}});
    json body = {{"event_id", uniq("evt_")},
                 {"type", "payment.failed"},
                 {"data", {{"order_id", o["id"]}, {"amount", "15.00"}, {"currency", "USD"}}}};
    std::string raw = body.dump();
    std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
    httplib::Client cli("127.0.0.1", h.port);
    auto res = cli.Post("/api/v1/webhooks/payments/generic",
                        {{"X-Nhill-Timestamp", ts},
                         {"X-Nhill-Signature", "v1=" + nh::crypto::hmacSha256Hex("test-webhook-secret-0123456789abcdef", ts + "." + raw)}},
                        raw, "application/json");
    EXPECT_EQ(json::parse(res->body)["outcome"], "marked_failed");
    EXPECT_EQ(sub(t)["failed_payments"], 1);
    auto notes = admin("GET", "/api/v1/admin/notifications");
    bool seen = false;
    for (const auto& n : notes.body["items"]) seen = seen || (n["kind"] == "payment_failed" && n["params"]["order_id"] == o["id"]);
    EXPECT_TRUE(seen);
}

// ------------------------------------------------------------------ providers
namespace {
struct FakeCall {
    std::string method, url, body;
    httplib::Headers headers;
};
std::mutex gCallsMutex;
std::vector<FakeCall> gCalls;
}  // namespace

TEST(Providers, SecretsAreWriteOnlyAndEncrypted) {
    auto& h = Harness::get();
    std::string code = "stripe" + nh::crypto::toHex(nh::crypto::randomBytes(3));
    auto r = admin("POST", "/api/v1/admin/payment-providers",
                   {{"code", code}, {"kind", "stripe"}, {"display_name", "Cartes"}, {"currencies", {"USD"}},
                    {"settings", {{"success_url", "https://example.org/ok?o={order_id}"}}},
                    {"secrets", {{"secret_key", "sk_test_secretvalue_123"}, {"webhook_secret", "whsec_testsecret"}}}});
    ASSERT_EQ(r.status, 201) << r.raw;
    EXPECT_EQ(r.raw.find("sk_test_secretvalue_123"), std::string::npos);
    EXPECT_EQ(r.body["secret_fields_set"].size(), 2u);
    auto row = h.su("SELECT encode(secret_cipher,'escape') LIKE '%sk_test%', settings::text FROM payment_providers WHERE code = '" +
                    code + "'");
    EXPECT_FALSE(row[0][0].as<bool>());
    auto list = admin("GET", "/api/v1/admin/payment-providers");
    EXPECT_EQ(list.raw.find("sk_test_secretvalue_123"), std::string::npos);
    EXPECT_EQ(list.raw.find("whsec_testsecret"), std::string::npos);
    auto audit = h.su("SELECT count(*) FROM audit_log WHERE details::text LIKE '%sk_test_secretvalue%'")[0][0].as<int>();
    EXPECT_EQ(audit, 0);
    // Unknown field, plain-HTTP URL, billing role (only superadmin configures providers).
    EXPECT_EQ(admin("PATCH", "/api/v1/admin/payment-providers/" + r.body["id"].get<std::string>(),
                    {{"settings", {{"evil", "x"}}}}).status, 422);
    EXPECT_EQ(admin("PATCH", "/api/v1/admin/payment-providers/" + r.body["id"].get<std::string>(),
                    {{"settings", {{"cancel_url", "http://example.org"}}}}).status, 422);
    std::string billing = adminLogin("billing");
    EXPECT_EQ(h.request("PATCH", "/api/v1/admin/payment-providers/" + r.body["id"].get<std::string>(), {{"enabled", true}}, billing)
                  .status, 403);
    // Payoneer has no adapter: it cannot be enabled.
    auto py = admin("POST", "/api/v1/admin/payment-providers",
                    {{"code", "payoneer" + nh::crypto::toHex(nh::crypto::randomBytes(3))}, {"kind", "payoneer"},
                     {"display_name", "Payoneer"}});
    ASSERT_EQ(py.status, 201);
    EXPECT_FALSE(py.body["implemented"].get<bool>());
    EXPECT_EQ(admin("PATCH", "/api/v1/admin/payment-providers/" + py.body["id"].get<std::string>(), {{"enabled", true}})
                  .body["error"]["code"], "provider_not_implemented");
}

TEST(Providers, StripeCheckoutAndVerifiedWebhook) {
    auto& h = Harness::get();
    std::string code = "stripe" + nh::crypto::toHex(nh::crypto::randomBytes(3));
    auto created = admin("POST", "/api/v1/admin/payment-providers",
                         {{"code", code}, {"kind", "stripe"}, {"display_name", "Carte bancaire"}, {"currencies", {"USD"}},
                          {"settings", {{"success_url", "https://example.org/ok?o={order_id}"},
                                        {"cancel_url", "https://example.org/ko?o={order_id}"}}},
                          {"secrets", {{"secret_key", "sk_test_abc"}, {"webhook_secret", "whsec_fake_endpoint_secret"}}}});
    ASSERT_EQ(created.status, 201);
    ASSERT_EQ(admin("PATCH", "/api/v1/admin/payment-providers/" + created.body["id"].get<std::string>(), {{"enabled", true}}).status,
              200);
    auto p = makePlan(90, "25.00", "250.00");
    Tenant t = createTenant("company");
    std::string sessionAmount = "2500";
    std::string sessionOrder;
    std::string paymentStatus = "paid";
    nh::providers::setTransportForTests([&](const std::string& m, const std::string& url, const httplib::Headers& hd,
                                            const std::string& body, const std::string&) {
        std::lock_guard<std::mutex> l(gCallsMutex);
        gCalls.push_back({m, url, body, hd});
        if (m == "POST" && url == "https://api.stripe.com/v1/checkout/sessions") {
            auto pos = body.find("client_reference_id=");
            sessionOrder = body.substr(pos + 20, 36);
            return nh::providers::HttpResponse{200, json{{"id", "cs_test_1"}, {"url", "https://checkout.stripe.com/c/pay/cs_test_1"}}.dump()};
        }
        if (m == "GET" && url == "https://api.stripe.com/v1/checkout/sessions/cs_test_1")
            return nh::providers::HttpResponse{
                200, json{{"id", "cs_test_1"}, {"payment_status", paymentStatus}, {"amount_total", std::stoll(sessionAmount)},
                          {"currency", "usd"}, {"metadata", {{"order_id", sessionOrder}}}, {"payment_intent", "pi_123"}}
                         .dump()};
        return nh::providers::HttpResponse{404, "{}"};
    });
    auto r = h.post("/api/v1/subscription/orders", {{"price_id", p.monthly}, {"provider", code}}, t.token);
    ASSERT_EQ(r.status, 201) << r.raw;
    EXPECT_EQ(r.body["checkout"]["checkout_url"], "https://checkout.stripe.com/c/pay/cs_test_1");
    std::string orderId = r.body["order"]["id"];
    {
        std::lock_guard<std::mutex> l(gCallsMutex);
        const auto& c = gCalls.front();
        EXPECT_NE(c.body.find("unit_amount%5D=2500"), std::string::npos);
        EXPECT_NE(c.body.find("currency%5D=usd"), std::string::npos);
        EXPECT_EQ(c.headers.find("Authorization")->second, "Bearer sk_test_abc");
    }
    auto stripeEvent = [&](const std::string& secret) {
        json ev = {{"id", uniq("evt_")}, {"type", "checkout.session.completed"},
                   {"data", {{"object", {{"id", "cs_test_1"}}}}}};
        std::string raw = ev.dump();
        std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
        httplib::Client cli("127.0.0.1", h.port);
        auto res = cli.Post("/api/v1/webhooks/payments/" + code,
                            {{"Stripe-Signature", "t=" + ts + ",v1=" + nh::crypto::hmacSha256Hex(secret, ts + "." + raw)}}, raw,
                            "application/json");
        return std::make_pair(res->status, json::parse(res->body));
    };
    EXPECT_EQ(stripeEvent("whsec_wrong").first, 401);
    // Session re-read from the API reports another amount: refused.
    sessionAmount = "100";
    EXPECT_EQ(stripeEvent("whsec_fake_endpoint_secret").second["outcome"], "amount_mismatch");
    sessionAmount = "2500";
    paymentStatus = "unpaid";
    EXPECT_EQ(stripeEvent("whsec_fake_endpoint_secret").second["outcome"], "ignored:payment_status=unpaid");
    paymentStatus = "paid";
    auto ok = stripeEvent("whsec_fake_endpoint_secret");
    EXPECT_EQ(ok.first, 200);
    EXPECT_EQ(ok.second["outcome"], "applied");
    EXPECT_EQ(sub(t)["plan"]["id"], p.id);
    nh::providers::setTransportForTests(nullptr);
}

TEST(Providers, PaypalOrderCaptureAfterVerifiedApproval) {
    auto& h = Harness::get();
    std::string code = "paypal" + nh::crypto::toHex(nh::crypto::randomBytes(3));
    auto created = admin("POST", "/api/v1/admin/payment-providers",
                         {{"code", code}, {"kind", "paypal"}, {"display_name", "PayPal"}, {"environment", "test"},
                          {"settings", {{"client_id", "client-1"}, {"webhook_id", "WH-1"},
                                        {"return_url", "https://example.org/ok?o={order_id}"},
                                        {"cancel_url", "https://example.org/ko?o={order_id}"}}},
                          {"secrets", {{"client_secret", "pp-secret"}}}});
    ASSERT_EQ(created.status, 201) << created.raw;
    ASSERT_EQ(admin("PATCH", "/api/v1/admin/payment-providers/" + created.body["id"].get<std::string>(), {{"enabled", true}}).status,
              200);
    auto p = makePlan(95, "19.99", "199.00");
    Tenant t = createTenant("company");
    std::string orderId;
    bool verified = true;
    nh::providers::setTransportForTests([&](const std::string& m, const std::string& url, const httplib::Headers&,
                                            const std::string& body, const std::string&) {
        const std::string base = "https://api-m.sandbox.paypal.com";
        if (url == base + "/v1/oauth2/token") return nh::providers::HttpResponse{200, R"({"access_token":"A21"})"};
        if (url == base + "/v2/checkout/orders") {
            auto j = json::parse(body);
            orderId = j["purchase_units"][0]["custom_id"];
            EXPECT_EQ(j["purchase_units"][0]["amount"]["value"], "19.99");
            return nh::providers::HttpResponse{201, R"({"id":"PP-ORDER-1","links":[{"rel":"payer-action","href":"https://www.sandbox.paypal.com/checkoutnow?token=PP-ORDER-1"}]})"};
        }
        if (url == base + "/v1/notifications/verify-webhook-signature")
            return nh::providers::HttpResponse{200, json{{"verification_status", verified ? "SUCCESS" : "FAILURE"}}.dump()};
        if (url == base + "/v2/checkout/orders/PP-ORDER-1/capture")
            return nh::providers::HttpResponse{
                201, json{{"status", "COMPLETED"},
                          {"purchase_units", {{{"payments", {{"captures", {{{"id", "CAP-1"}, {"status", "COMPLETED"}, {"custom_id", orderId},
                                                                          {"amount", {{"currency_code", "USD"}, {"value", "19.99"}}}}}}}}}}}}
                         .dump()};
        (void)m;
        return nh::providers::HttpResponse{404, "{}"};
    });
    auto r = h.post("/api/v1/subscription/orders", {{"price_id", p.monthly}, {"provider", code}}, t.token);
    ASSERT_EQ(r.status, 201) << r.raw;
    EXPECT_EQ(r.body["checkout"]["checkout_url"], "https://www.sandbox.paypal.com/checkoutnow?token=PP-ORDER-1");
    auto approved = [&]() {
        json ev = {{"id", uniq("WH-EVT-")}, {"event_type", "CHECKOUT.ORDER.APPROVED"},
                   {"resource", {{"id", "PP-ORDER-1"}, {"purchase_units", {{{"custom_id", orderId}}}}}}};
        httplib::Client cli("127.0.0.1", h.port);
        auto res = cli.Post("/api/v1/webhooks/payments/" + code,
                            {{"PAYPAL-TRANSMISSION-ID", "t1"}, {"PAYPAL-TRANSMISSION-SIG", "sig"},
                             {"PAYPAL-AUTH-ALGO", "SHA256withRSA"}, {"PAYPAL-CERT-URL", "https://api.sandbox.paypal.com/cert"},
                             {"PAYPAL-TRANSMISSION-TIME", "2026-10-26T00:00:00Z"}},
                            ev.dump(), "application/json");
        return std::make_pair(res->status, json::parse(res->body));
    };
    verified = false;
    EXPECT_EQ(approved().first, 401);
    verified = true;
    auto ok = approved();
    EXPECT_EQ(ok.second["outcome"], "applied") << ok.second.dump();
    EXPECT_EQ(sub(t)["plan"]["id"], p.id);
    nh::providers::setTransportForTests(nullptr);
}

// ------------------------------------------------------------------ console actions
TEST(Console, SubscriptionActionsAreAuditedAndRoleRestricted) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    EXPECT_EQ(admin("POST", "/api/v1/admin/establishments/" + t.id + "/subscription", {{"action", "suspend"}}).status, 422);
    auto s = admin("POST", "/api/v1/admin/establishments/" + t.id + "/subscription",
                   {{"action", "suspend"}, {"reason", "Fraude suspectée sur le moyen de paiement"}});
    ASSERT_EQ(s.status, 200) << s.raw;
    EXPECT_EQ(s.body["subscription"]["status"], "suspended");
    EXPECT_EQ(sub(t)["write_allowed"], false);
    auto e = admin("POST", "/api/v1/admin/establishments/" + t.id + "/subscription",
                   {{"action", "extend"}, {"days", 15}, {"reason", "Geste commercial après incident"}});
    EXPECT_EQ(e.body["subscription"]["status"], "active");
    int audited = h.su("SELECT count(*) FROM audit_log WHERE establishment_id = '" + t.id + "' AND action LIKE 'admin.subscription_%'")[0][0]
                      .as<int>();
    EXPECT_EQ(audited, 2);
    std::string support = adminLogin("support");
    EXPECT_EQ(h.request("POST", "/api/v1/admin/establishments/" + t.id + "/subscription",
                        {{"action", "extend"}, {"days", 30}, {"reason", "Pas autorisé pour le support"}}, support)
                  .status, 403);
    // A tenant token never reaches the console API.
    EXPECT_EQ(h.request("GET", "/api/v1/admin/transactions", nullptr, t.token).status, 401);
}

TEST(Support, TicketsAreScopedAndInternalNotesHidden) {
    auto& h = Harness::get();
    Tenant a = createTenant("company"), b = createTenant("company");
    auto r = h.post("/api/v1/support/tickets", {{"category", "bug"}, {"severity", "high"}, {"subject", "Impression du reçu"},
                                               {"body", "Le reçu ne sort pas.\nTicket de test."}, {"source", "pos"}},
                    a.token);
    ASSERT_EQ(r.status, 201) << r.raw;
    std::string id = r.body["id"];
    // Another establishment cannot see it (404, not 403).
    EXPECT_EQ(h.get("/api/v1/support/tickets/" + id, b.token).status, 404);
    EXPECT_EQ(h.post("/api/v1/support/tickets/" + id + "/messages", {{"body", "intrusion"}}, b.token).status, 404);
    // A cashier-like member only sees their own tickets.
    std::string viewer = addMember(a, "viewer");
    EXPECT_EQ(h.get("/api/v1/support/tickets/" + id, viewer).status, 404);

    ASSERT_EQ(admin("POST", "/api/v1/admin/tickets/" + id + "/messages", {{"body", "note interne : pilote d'imprimante"},
                                                                          {"internal", true}}).status, 201);
    ASSERT_EQ(admin("POST", "/api/v1/admin/tickets/" + id + "/messages", {{"body", "Pouvez-vous redémarrer la caisse ?"}}).status,
              201);
    ASSERT_EQ(admin("PATCH", "/api/v1/admin/tickets/" + id, {{"status", "waiting_customer"}}).status, 200);
    auto view = h.get("/api/v1/support/tickets/" + id, a.token);
    EXPECT_EQ(view.raw.find("note interne"), std::string::npos);
    EXPECT_NE(view.raw.find("redémarrer"), std::string::npos);
    EXPECT_EQ(view.body["status"], "waiting_customer");
    // The customer's reply re-opens the ticket.
    ASSERT_EQ(h.post("/api/v1/support/tickets/" + id + "/messages", {{"body", "Toujours en panne."}}, a.token).status, 201);
    EXPECT_EQ(admin("GET", "/api/v1/admin/tickets/" + id).body["status"], "open");
    EXPECT_EQ(admin("GET", "/api/v1/admin/tickets/" + id).body["messages"].size(), 5u);  // 3 + status + reply
}

TEST(Support, ErrorReportsAreGroupedByFingerprint) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string code = uniq("qml_");
    for (int i = 0; i < 3; ++i)
        ASSERT_EQ(h.post("/api/v1/client-errors", {{"source", "pos"}, {"code", code}, {"message", "TypeError: x is undefined"},
                                                  {"app_version", "0.2.0"}, {"platform", "linux"}},
                         t.token)
                      .status, 202);
    auto list = admin("GET", "/api/v1/admin/errors?source=pos");
    int occ = 0;
    int64_t eid = 0;
    for (const auto& e : list.body["items"])
        if (e["code"] == code) {
            occ = e["occurrences"];
            eid = e["id"];
        }
    EXPECT_EQ(occ, 3);
    EXPECT_EQ(admin("POST", "/api/v1/admin/errors/" + std::to_string(eid) + "/resolve").status, 204);
    // After resolution a new occurrence opens a new entry.
    h.post("/api/v1/client-errors", {{"source", "pos"}, {"code", code}, {"message", "TypeError: x is undefined"}}, t.token);
    EXPECT_EQ(h.su("SELECT count(*) FROM error_events WHERE code = '" + code + "'")[0][0].as<int>(), 2);
}

TEST(Support, ErrorReportsMaskPersonalData) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string code = uniq("net_");
    ASSERT_EQ(h.post("/api/v1/client-errors",
                     {{"source", "business"}, {"code", code},
                      {"message", "login failed for jean.mukendi@example.cd card 4111 1111 1111 1111 token "
                                  "AbCdEfGhIjKlMnOpQrStUvWxYz0123456789"},
                      {"context", {{"phone", "+243 81 234 5678"}, {"page", "PaymentsPage.qml:42"}}}},
                     t.token)
                  .status,
              202);
    auto r = h.su("SELECT message, context::text FROM error_events WHERE code = '" + code + "'");
    ASSERT_EQ(r.size(), 1u);
    std::string msg = r[0][0].c_str(), ctxs = r[0][1].c_str();
    EXPECT_EQ(msg.find("jean.mukendi"), std::string::npos) << msg;
    EXPECT_EQ(msg.find("4111"), std::string::npos) << msg;
    EXPECT_EQ(msg.find("AbCdEfGh"), std::string::npos) << msg;
    EXPECT_NE(msg.find("<email>"), std::string::npos) << msg;
    EXPECT_EQ(ctxs.find("234 5678"), std::string::npos) << ctxs;
    EXPECT_NE(ctxs.find("PaymentsPage.qml:42"), std::string::npos) << ctxs;  // diagnosis kept
}
