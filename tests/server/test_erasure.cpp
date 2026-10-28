// Permanent erasure of an establishment (terms of service: 90 days of export after the
// end of access, then deletion) and the retention clean-ups of migration 0014.
#include <cctype>

#include "harness.hpp"

using namespace nht;

namespace {

std::string superToken() {
    static std::string t = adminLogin("superadmin");
    return t;
}

Response adminReq(const std::string& method, const std::string& path, const json& body = nullptr,
                  const std::string& token = "") {
    return Harness::get().request(method, path, body, token.empty() ? superToken() : token);
}

// End of full access N days ago (period and grace period in the past).
void endAccess(const Tenant& t, int daysAgo) {
    auto& h = Harness::get();
    const std::string d = std::to_string(daysAgo);
    h.su("UPDATE subscriptions SET status = 'past_due', period_start = now() - interval '" + std::to_string(daysAgo + 60) +
         " days', period_end = now() - interval '" + std::to_string(daysAgo + 20) + " days', grace_until = now() - interval '" +
         d + " days' WHERE establishment_id = '" + t.id + "'");
}

std::string tradeName(const Tenant& t) {
    return Harness::get().su("SELECT trade_name FROM establishments WHERE id = '" + t.id + "'")[0][0].c_str();
}

// An establishment with an issued invoice and a paid subscription order.
Tenant busyTenant() {
    Tenant t = createTenant();
    std::string c = createCustomer(t);
    std::string p = createProduct(t, uniq("SKU"), "10.00", false);
    std::string doc = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "2"}}}));
    EXPECT_EQ(issue(t, doc).status, 200);
    auto& h = Harness::get();
    std::string code = uniq("erase");
    for (auto& ch : code) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    std::string plan = h.su("INSERT INTO plans (code, name_fr, name_en, billing_period, price_minor, currency, max_users, tier) "
                            "VALUES ('" + code + "', 'Formule', 'Plan', 'monthly', 2500, 'USD', 10, 1) RETURNING id")[0][0].c_str();
    h.su("INSERT INTO subscription_orders (establishment_id, plan_id, amount_minor, currency, status, provider, "
         "provider_reference, paid_at) VALUES ('" + t.id + "', '" + plan + "', 2500, 'USD', 'paid', 'manual', '" + uniq("REF") +
         "', now() - interval '150 days')");
    return t;
}

int count(const std::string& sql) { return Harness::get().su(sql)[0][0].as<int>(); }

}  // namespace

TEST(Erasure, NotPossibleWhileTheEstablishmentHasFullAccess) {
    Tenant t = createTenant();
    auto d = adminReq("GET", "/api/v1/admin/establishments/" + t.id);
    ASSERT_EQ(d.status, 200) << d.raw;
    EXPECT_FALSE(d.body["erasure"]["eligible"].get<bool>());
    EXPECT_EQ(d.body["erasure"]["reason"], "access_active");
    // Not even at the customer's request: the account must end first.
    auto r = adminReq("POST", "/api/v1/admin/establishments/" + t.id + "/erase",
                      {{"reason", "demande du client"}, {"confirm_trade_name", tradeName(t)}, {"customer_request", true}});
    EXPECT_EQ(r.status, 409) << r.raw;
    EXPECT_EQ(r.body["error"]["code"], "not_erasable");
    EXPECT_EQ(count("SELECT count(*) FROM establishments WHERE id = '" + t.id + "'"), 1);
}

TEST(Erasure, WaitsNinetyDaysAfterTheEndOfAccessUnlessTheCustomerAsks) {
    Tenant t = createTenant();
    endAccess(t, 10);
    auto d = adminReq("GET", "/api/v1/admin/establishments/" + t.id);
    ASSERT_EQ(d.status, 200);
    EXPECT_FALSE(d.body["erasure"]["eligible"].get<bool>());
    EXPECT_EQ(d.body["erasure"]["reason"], "expired");
    EXPECT_FALSE(d.body["erasure"]["erasable_from"].is_null());
    json body = {{"reason", "fin du contrat"}, {"confirm_trade_name", tradeName(t)}};
    EXPECT_EQ(adminReq("POST", "/api/v1/admin/establishments/" + t.id + "/erase", body).status, 409);
    // At the customer's request, before the 90 days: allowed once access has ended.
    body["customer_request"] = true;
    auto r = adminReq("POST", "/api/v1/admin/establishments/" + t.id + "/erase", body);
    ASSERT_EQ(r.status, 200) << r.raw;
    EXPECT_TRUE(r.body["early"].get<bool>());
    EXPECT_EQ(count("SELECT count(*) FROM establishments WHERE id = '" + t.id + "'"), 0);
}

TEST(Erasure, DeletesEverythingKeepsTheBillingArchiveAndLogsTheErasure) {
    auto& h = Harness::get();
    Tenant t = busyTenant();
    std::string extra;
    addMember(t, "sales", &extra);
    endAccess(t, 100);
    ASSERT_EQ(count("SELECT count(*) FROM subscription_orders WHERE establishment_id = '" + t.id + "' AND status = 'paid'"), 1);
    auto d = adminReq("GET", "/api/v1/admin/establishments/" + t.id);
    ASSERT_TRUE(d.body["erasure"]["eligible"].get<bool>()) << d.raw;
    // The list of establishments to erase.
    auto list = adminReq("GET", "/api/v1/admin/establishments?erasable=true&search=" + t.id);
    ASSERT_EQ(list.status, 200);
    EXPECT_EQ(list.body["total"], 1);

    // The trade name must be typed exactly; only a superadministrator may erase.
    json body = {{"reason", "fin du contrat + 90 jours"}, {"confirm_trade_name", "autre nom"}};
    EXPECT_EQ(adminReq("POST", "/api/v1/admin/establishments/" + t.id + "/erase", body).status, 422);
    body["confirm_trade_name"] = tradeName(t);
    std::string support = adminLogin("support");
    EXPECT_EQ(adminReq("POST", "/api/v1/admin/establishments/" + t.id + "/erase", body, support).status, 403);

    auto r = adminReq("POST", "/api/v1/admin/establishments/" + t.id + "/erase", body);
    ASSERT_EQ(r.status, 200) << r.raw;
    EXPECT_GE(r.body["documents"].get<int>(), 1);
    EXPECT_EQ(r.body["users_deleted"], 2);
    EXPECT_EQ(r.body["billing_orders_archived"], 1);
    EXPECT_FALSE(r.body["early"].get<bool>());

    // Nothing left of the establishment, its documents, its trail or its accounts.
    EXPECT_EQ(count("SELECT count(*) FROM establishments WHERE id = '" + t.id + "'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM sales_documents WHERE establishment_id = '" + t.id + "'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM parties WHERE establishment_id = '" + t.id + "'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM audit_log WHERE establishment_id = '" + t.id + "'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM subscription_events WHERE establishment_id = '" + t.id + "'"), 0);
    EXPECT_EQ(count("SELECT count(*) FROM users WHERE email IN ('" + t.ownerEmail + "', '" + extra + "')"), 0);
    EXPECT_EQ(h.post("/api/v1/auth/login", {{"email", t.ownerEmail}, {"password", t.password}}).status, 401);
    // Kept: the paid order for Nhill Solution's accounts, and the platform-level entry.
    EXPECT_EQ(count("SELECT count(*) FROM billing_archive WHERE establishment_id = '" + t.id +
                    "' AND keep_until > current_date + 3000"),
              1);
    EXPECT_EQ(count("SELECT count(*) FROM audit_log WHERE action = 'admin.establishment_erased' AND entity_id = '" + t.id +
                    "' AND establishment_id IS NULL"),
              1);
    EXPECT_EQ(adminReq("GET", "/api/v1/admin/establishments/" + t.id).status, 404);
}

TEST(Erasure, KeepsTheAccountOfAPersonWhoBelongsToAnotherEstablishment) {
    auto& h = Harness::get();
    Tenant a = createTenant();
    Tenant b = createTenant();
    h.su("INSERT INTO memberships (establishment_id, user_id, role) SELECT '" + b.id + "', id, 'viewer' FROM users WHERE email = '" +
         a.ownerEmail + "'");
    endAccess(a, 95);
    auto r = adminReq("POST", "/api/v1/admin/establishments/" + a.id + "/erase",
                      {{"reason", "fin du contrat"}, {"confirm_trade_name", tradeName(a)}});
    ASSERT_EQ(r.status, 200) << r.raw;
    EXPECT_EQ(r.body["users_deleted"], 0);
    EXPECT_EQ(count("SELECT count(*) FROM users WHERE email = '" + a.ownerEmail + "'"), 1);
    EXPECT_EQ(count("SELECT count(*) FROM establishments WHERE id = '" + b.id + "'"), 1);
}

TEST(Erasure, TheApplicationRoleCannotUseTheErasureFlag) {
    Tenant t = busyTenant();
    auto& h = Harness::get();
    // The setting alone is not enough: the guard also requires the owner role.
    EXPECT_ANY_THROW(h.asApp(t.id, "SELECT set_config('nhill.erasing_establishment', '" + t.id +
                                       "', true); DELETE FROM sales_documents WHERE status <> 'draft'"));
    EXPECT_GE(count("SELECT count(*) FROM sales_documents WHERE establishment_id = '" + t.id + "' AND status <> 'draft'"), 1);
    // Nor can it call the erasure on an establishment that still has access.
    EXPECT_ANY_THROW(h.asApp(t.id, "SELECT nh_erase_establishment('" + t.id + "', true)"));
    EXPECT_EQ(count("SELECT count(*) FROM establishments WHERE id = '" + t.id + "'"), 1);
}

TEST(Retention, OldSentEmailsAndEndedSessionsAreDeletedByTheDailyJob) {
    auto& h = Harness::get();
    Tenant t = createTenant();
    std::string addr = uniq("old") + "@nhill.test";
    h.su("INSERT INTO email_outbox (to_address, template, status, created_at, sent_at) VALUES ('" + addr +
         "', 'welcome', 'sent', now() - interval '40 days', now() - interval '40 days')");
    h.su("INSERT INTO email_outbox (to_address, template, status) VALUES ('" + addr + "', 'welcome', 'pending')");
    h.su("UPDATE sessions SET revoked_at = now() - interval '100 days' WHERE establishment_id = '" + t.id + "'");
    nh::jobs::runAll(h.env());
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE to_address = '" + addr + "' AND created_at < now() - interval '30 days'"), 0);
    // The recent message stays, whatever its sending outcome.
    EXPECT_EQ(count("SELECT count(*) FROM email_outbox WHERE to_address = '" + addr + "' AND created_at > now() - interval '1 day'"), 1);
    EXPECT_EQ(count("SELECT count(*) FROM sessions WHERE establishment_id = '" + t.id + "'"), 0);
}
