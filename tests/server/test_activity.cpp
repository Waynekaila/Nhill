// (Harness tenants are created in the DRC profile: time zone Africa/Kinshasa.)
// Activity journal per employee, per-employee report, and the creators'
// aggregated statistics per establishment.
#include <gtest/gtest.h>

#include "harness.hpp"

using namespace nht;

namespace {

struct Shop {
    Tenant t;
    std::string sellerToken, sellerEmail, customer, product;
};

// A shop where a seller (not the owner) issues an invoice of 3 x 10 000 CDF and records a payment.
Shop shopWithSeller() {
    auto& h = Harness::get();
    Shop s;
    s.t = createTenant("company");
    s.sellerToken = addMember(s.t, "sales", &s.sellerEmail);
    s.customer = createCustomer(s.t, "Client Activité");
    s.product = createProduct(s.t, "ACT-" + uniq().substr(0, 6), "10000.00");
    adjustStock(s.t, s.product, "20");
    auto draft = h.post("/api/v1/sales-documents", {{"doc_type", "invoice"}, {"party_id", s.customer},
                         {"lines", json::array({{{"product_id", s.product}, {"quantity", "3"}}})}}, s.sellerToken, newKey());
    EXPECT_EQ(draft.status, 201) << draft.raw;
    auto issued = h.post("/api/v1/sales-documents/" + draft.body["id"].get<std::string>() + "/issue", json::object(),
                         s.sellerToken, newKey());
    EXPECT_EQ(issued.status, 200) << issued.raw;
    auto pay = h.post("/api/v1/payments", {{"document_id", draft.body["id"]}, {"amount", "12000.00"}, {"method", "cash"}},
                      s.sellerToken, newKey());
    EXPECT_EQ(pay.status, 201) << pay.raw;
    return s;
}

std::string today() {
    return Harness::get().su("SELECT (now() AT TIME ZONE 'Africa/Kinshasa')::date::text")[0][0].c_str();
}

}  // namespace

TEST(Activity, OwnerSeesWhoDidWhatAndSellerOnlySeesOwnActions) {
    auto& h = Harness::get();
    Shop s = shopWithSeller();
    auto all = h.get("/api/v1/activity?limit=200", s.t.token);
    ASSERT_EQ(all.status, 200) << all.raw;
    EXPECT_EQ(all.body["scope"], "establishment");
    bool sellerIssued = false, ownerCreatedProduct = false;
    std::string sellerId;
    for (const auto& i : all.body["items"]) {
        if (i["action"] == "sales_document.issued" && i["actor"] == "Membre sales") {
            sellerIssued = true;
            sellerId = i["actor_id"];
            EXPECT_EQ(i["action_label"], "Document émis");
            EXPECT_EQ(i["reference"].get<std::string>().rfind("FA-", 0), 0u) << i.dump();  // document number resolved
        }
        if (i["action"] == "product.created" && i["actor"] == "Propriétaire Test") ownerCreatedProduct = true;
    }
    EXPECT_TRUE(sellerIssued);
    EXPECT_TRUE(ownerCreatedProduct);
    EXPECT_GE(all.body["members"].size(), 2u);

    // Filter by employee
    auto mine = h.get("/api/v1/activity?user_id=" + sellerId, s.t.token);
    for (const auto& i : mine.body["items"]) EXPECT_EQ(i["actor_id"], sellerId);
    EXPECT_GE(mine.body["total"].get<int>(), 3);  // created, issued, payment (+ sign-in)
    // Filter by action prefix
    auto pays = h.get("/api/v1/activity?action=payment.", s.t.token);
    ASSERT_EQ(pays.body["items"].size(), 1u);

    // The seller asking for the owner's actions still only gets his own.
    std::string ownerId = h.get("/api/v1/me", s.t.token).body["user"]["id"];
    auto spy = h.get("/api/v1/activity?user_id=" + ownerId, s.sellerToken);
    ASSERT_EQ(spy.status, 200);
    EXPECT_EQ(spy.body["scope"], "self");
    EXPECT_TRUE(spy.body["members"].empty());
    for (const auto& i : spy.body["items"]) EXPECT_EQ(i["actor_id"], sellerId);
    EXPECT_GE(spy.body["items"].size(), 3u);
    // ... and cannot read the per-employee report.
    EXPECT_EQ(h.get("/api/v1/reports/staff?from=" + today() + "&to=" + today(), s.sellerToken).status, 403);
}

TEST(Activity, ExportsAndPerEmployeeReport) {
    auto& h = Harness::get();
    Shop s = shopWithSeller();
    auto csv = h.get("/api/v1/activity?format=csv", s.t.token);
    ASSERT_EQ(csv.status, 200) << csv.raw;
    EXPECT_NE(csv.raw.find("Membre sales"), std::string::npos);
    EXPECT_NE(csv.raw.find("Paiement enregistré"), std::string::npos);
    EXPECT_NE(csv.raw.find("Utilisateur"), std::string::npos);  // header
    auto pdf = h.get("/api/v1/activity?format=pdf&from=" + today() + "&to=" + today(), s.t.token);
    ASSERT_EQ(pdf.status, 200);
    EXPECT_EQ(pdf.raw.rfind("%PDF-", 0), 0u);

    std::string q = "?from=" + today() + "&to=" + today();
    auto staff = h.get("/api/v1/reports/staff" + q, s.t.token);
    ASSERT_EQ(staff.status, 200) << staff.raw;
    bool found = false;
    for (const auto& row : staff.body["rows"]) {
        if (row[0] == "Membre sales" && row[3] == "CDF") {
            found = true;
            EXPECT_EQ(row[4], "1");          // invoices issued
            EXPECT_EQ(row[5], "30000.00");   // invoiced incl. tax
            EXPECT_EQ(row[8], "1");          // payments received
            EXPECT_EQ(row[9], "12000.00");
        }
    }
    EXPECT_TRUE(found) << staff.raw;
    EXPECT_EQ(h.get("/api/v1/reports/staff" + q + "&format=pdf", s.t.token).raw.rfind("%PDF-", 0), 0u);
    // The sales journal says who issued each invoice.
    auto journal = h.get("/api/v1/reports/sales" + q + "&format=csv", s.t.token);
    EXPECT_NE(journal.raw.find("Émis par"), std::string::npos);
    EXPECT_NE(journal.raw.find("Membre sales"), std::string::npos);
}

TEST(PlatformStats, AggregatesPerEstablishmentWithoutRecordsAndAudited) {
    auto& h = Harness::get();
    Shop s = shopWithSeller();
    std::string billing = adminLogin("billing");
    std::string q = "?from=" + today() + "&to=" + today();
    auto st = h.get("/api/v1/admin/establishments/" + s.t.id + "/stats" + q, billing);
    ASSERT_EQ(st.status, 200) << st.raw;
    ASSERT_EQ(st.body["volumes"].size(), 1u);
    EXPECT_EQ(st.body["volumes"][0]["currency"], "CDF");
    EXPECT_EQ(st.body["volumes"][0]["invoices_issued"], 1);
    EXPECT_EQ(st.body["volumes"][0]["invoiced_total"], "30000.00");
    EXPECT_EQ(st.body["volumes"][0]["payments_in"], "12000.00");
    EXPECT_EQ(st.body["counts"]["customers"], 1);
    EXPECT_EQ(st.body["counts"]["products"], 1);
    EXPECT_EQ(st.body["users"]["active_by_role"]["sales"], 1);
    EXPECT_GE(st.body["users"]["actions"].get<int>(), 5);
    // Aggregates only: no customer name, no product, no document number.
    EXPECT_EQ(st.raw.find("Client Activité"), std::string::npos);
    EXPECT_EQ(st.raw.find("FA-"), std::string::npos);
    EXPECT_EQ(st.raw.find("ACT-"), std::string::npos);

    // The consultation is visible to the establishment in its own activity log.
    auto log = h.get("/api/v1/activity?action=admin.", s.t.token);
    ASSERT_GE(log.body["items"].size(), 1u);
    EXPECT_EQ(log.body["items"][0]["actor"], "Support Nhill");
    EXPECT_EQ(log.body["items"][0]["action_label"], "Statistiques agrégées consultées par Nhill");

    // Reports (platform-wide and per establishment)
    auto rep = h.get("/api/v1/admin/reports/establishments" + q + "&format=csv", billing);
    ASSERT_EQ(rep.status, 200) << rep.raw;
    EXPECT_NE(rep.raw.find(h.su("SELECT trade_name FROM establishments WHERE id = '" + s.t.id + "'")[0][0].c_str()),
              std::string::npos);
    EXPECT_NE(rep.raw.find("30000"), std::string::npos);
    auto one = h.get("/api/v1/admin/reports/establishments/" + s.t.id + q + "&format=pdf", billing);
    ASSERT_EQ(one.status, 200);
    EXPECT_EQ(one.raw.rfind("%PDF-", 0), 0u);

    // Least privilege: support agents do not see business volumes.
    std::string support = adminLogin("support");
    EXPECT_EQ(h.get("/api/v1/admin/establishments/" + s.t.id + "/stats" + q, support).status, 403);
    EXPECT_EQ(h.get("/api/v1/admin/reports/establishments" + q, support).status, 403);
}

TEST(PlatformStats, AggregateFunctionsKeepTenantIsolation) {
    auto& h = Harness::get();
    Shop a = shopWithSeller();
    Tenant b = createTenant("company");
    // Called from tenant B's context, the function returns A's aggregates but the
    // tenant setting is restored afterwards: B still cannot see A's rows.
    auto r = h.asApp(b.id, "SELECT (SELECT count(*) FROM nh_platform_establishment_volumes('" + a.t.id +
                               "', (now() AT TIME ZONE 'Africa/Kinshasa')::date, (now() AT TIME ZONE 'Africa/Kinshasa')::date)), current_setting('app.establishment_id'), "
                               "(SELECT count(*) FROM parties)");
    EXPECT_EQ(r[0][0].as<int>(), 1);
    EXPECT_EQ(std::string(r[0][1].c_str()), b.id);
    EXPECT_EQ(r[0][2].as<int>(), 0);
    // Row data stays unreachable without a tenant context.
    EXPECT_EQ(h.asApp(std::nullopt, "SELECT count(*) FROM sales_documents")[0][0].as<int>(), 0);
}

TEST(Activity, SellerDashboardShowsOwnFiguresOnly) {
    auto& h = Harness::get();
    Shop s = shopWithSeller();
    // The owner also issues an invoice of 1 x 10 000.
    std::string d = draftInvoice(s.t, s.customer, json::array({{{"product_id", s.product}, {"quantity", "1"}}}));
    ASSERT_EQ(issue(s.t, d).status, 200);
    std::string q = "?from=" + today() + "&to=" + today();
    auto owner = h.get("/api/v1/reports/dashboard" + q, s.t.token);
    ASSERT_EQ(owner.status, 200) << owner.raw;
    EXPECT_EQ(owner.body["scope"], "establishment");
    EXPECT_EQ(owner.body["revenue"]["CDF"]["invoices"], 2);
    auto seller = h.get("/api/v1/reports/dashboard" + q, s.sellerToken);
    ASSERT_EQ(seller.status, 200) << seller.raw;
    EXPECT_EQ(seller.body["scope"], "self");
    EXPECT_EQ(seller.body["revenue"]["CDF"]["invoices"], 1);
    EXPECT_EQ(seller.body["revenue"]["CDF"]["total"], "30000.00");
    EXPECT_TRUE(seller.body["expenses"].empty());
    EXPECT_TRUE(seller.body["payables"].empty());
    EXPECT_TRUE(seller.body["stock"]["estimated_value"].empty());
    EXPECT_EQ(seller.body["recent_sales"].size(), 1u);
}

TEST(LocalTime, DocumentsAreDatedInTheEstablishmentTimeZone) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    EXPECT_EQ(h.get("/api/v1/establishment", t.token).body["timezone"], "Africa/Kinshasa");
    std::string c = createCustomer(t);
    std::string d = draftInvoice(t, c, json::array({{{"description", "x"}, {"quantity", "1"}, {"unit_price", "1.00"}}}));
    auto doc = h.get("/api/v1/sales-documents/" + d, t.token);
    EXPECT_EQ(doc.body["issue_date"], today());  // Kinshasa date, even when UTC is still the previous day
    // A time zone in another region changes "today" accordingly.
    auto bad = h.request("PATCH", "/api/v1/establishment", {{"timezone", "Mars/Olympus"}}, t.token);
    EXPECT_EQ(bad.status, 422);
    ASSERT_EQ(h.request("PATCH", "/api/v1/establishment", {{"timezone", "Pacific/Kiritimati"}}, t.token).status, 200);
    std::string d2 = draftInvoice(t, c, json::array({{{"description", "y"}, {"quantity", "1"}, {"unit_price", "1.00"}}}));
    std::string kiri = h.su("SELECT (now() AT TIME ZONE 'Pacific/Kiritimati')::date::text")[0][0].c_str();
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + d2, t.token).body["issue_date"], kiri);
}
