// Sales documents shared by Nhill Business and Nhill POS (3.0.0): an invoice issued at
// the till is the same document as an invoice issued in Business (model, template,
// numbering sequence, rounding, wording); every document prints in A4 and on a roll.
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>

#include "db.hpp"
#include "harness.hpp"
#include "nhill/money.hpp"
#include "services/documents.hpp"

using namespace nht;

namespace {

Response post(const std::string& path, const json& body, const std::string& token) {
    return Harness::get().post(path, body, token, newKey());
}

std::string run(const std::string& cmd) {
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    pclose(p);
    return out;
}

std::string pdfText(const std::string& pdf) {
    std::string path = "/tmp/" + uniq("parity") + ".pdf";
    std::ofstream(path, std::ios::binary) << pdf;
    std::string txt = run("pdftotext -raw " + path + " - 2>/dev/null");
    std::remove(path.c_str());
    return txt;
}

// Page size of the first page ("595 842" for A4 portrait).
std::string mediaBox(const std::string& pdf) {
    std::smatch m;
    std::regex re("/MediaBox\\s*\\[\\s*0\\s+0\\s+([0-9.]+)\\s+([0-9.]+)");
    if (!std::regex_search(pdf, m, re)) return "";
    return std::to_string(static_cast<int>(std::stod(m[1]) + 0.5)) + " " + std::to_string(static_cast<int>(std::stod(m[2]) + 0.5));
}

json model(const Tenant& t, const std::string& id) {
    auto& env = Harness::get().env();
    return nh::db::run(env.pool, t.id, [&](nh::db::Tx& tx) { return nh::documents::toJson(nh::documents::build(env, tx, id)); });
}

// Everything but what identifies one issue (number, date and time, till, fingerprint).
json comparable(json m) {
    json meta = json::array();
    for (const auto& kv : m["meta"]) {
        std::string k = kv[0];
        if (k == "Date d'émission" || k == "Échéance" || k == "Caisse" || k == "Caissier") continue;
        meta.push_back(kv);
    }
    m["meta"] = meta;
    for (const char* k : {"number", "pos", "generated"}) m.erase(k);
    json pays = json::array();
    for (const auto& kv : m["payments"]) {
        std::string label = kv[0];
        pays.push_back({label.substr(0, label.find(" · ")), kv[1]});  // method, without the date
    }
    m["payments"] = pays;
    return m;
}

}  // namespace

TEST(Documents, TillInvoiceIsTheSameDocumentAsABusinessInvoice) {
    auto& h = Harness::get();
    Tenant t = createTenant("company", {{"rccm", "CD/KNG/RCCM/" + uniq("26-B-")}});
    ASSERT_EQ(h.request("PATCH", "/api/v1/establishment/settings",
                        {{"document_mentions", "Marchandises vendues ni reprises ni échangées après 7 jours."},
                         {"payment_instructions", "Mobile Money 0810000000"}},
                        t.token)
                  .status,
              200);
    std::string c = createCustomer(t, "Pharmacie du Fleuve");
    std::string p = createProduct(t, uniq("SAV"), "1500.00");
    adjustStock(t, p, "20");
    auto reg = post("/api/v1/pos/registers", {{"code", "c1"}, {"name", "Caisse 1"}}, t.token);
    ASSERT_EQ(reg.status, 201) << reg.raw;
    auto sess = post("/api/v1/pos/sessions", {{"register_id", reg.body["id"]}}, t.token);
    ASSERT_EQ(sess.status, 201) << sess.raw;
    std::string sid = sess.body["id"];

    // Business: invoice for 2 items, paid in cash.
    std::string inv = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "2"}}}));
    auto issued = issue(t, inv);
    ASSERT_EQ(issued.status, 200) << issued.raw;
    ASSERT_EQ(post("/api/v1/payments", {{"document_id", inv}, {"amount", issued.body["total"]}, {"method", "cash"}}, t.token).status,
              201);
    // Till: the same sale as an invoice for the same customer.
    auto noCustomer = post("/api/v1/pos/sales",
                           {{"session_id", sid}, {"document", "invoice"}, {"lines", {{{"product_id", p}, {"quantity", "2"}}}},
                            {"payments", {{{"method", "cash"}, {"amount", issued.body["total"]}}}}},
                           t.token);
    EXPECT_EQ(noCustomer.status, 422);
    EXPECT_EQ(noCustomer.body["error"]["details"]["reason"], "required_for_invoice");
    auto sale = post("/api/v1/pos/sales",
                     {{"session_id", sid}, {"document", "invoice"}, {"customer_id", c},
                      {"lines", {{{"product_id", p}, {"quantity", "2"}}}},
                      {"payments", {{{"method", "cash"}, {"amount", issued.body["total"]}}}}},
                     t.token);
    ASSERT_EQ(sale.status, 201) << sale.raw;
    const json& till = sale.body["receipt"];
    EXPECT_EQ(till["doc_type"], "invoice");
    EXPECT_EQ(till["status"], "paid");

    // Same numbering sequence and rules: FA-<year>-000001 then FA-<year>-000002.
    std::string n1 = issued.body["number"], n2 = till["number"];
    EXPECT_EQ(n1.substr(0, 3), "FA-");
    EXPECT_EQ(n2.substr(0, n2.size() - 6), n1.substr(0, n1.size() - 6));
    EXPECT_EQ(std::stoi(n2.substr(n2.size() - 6)), std::stoi(n1.substr(n1.size() - 6)) + 1);
    // Same amounts (same rounding of the same lines).
    for (const char* k : {"gross", "discount", "net", "tax", "total"}) EXPECT_EQ(till[k], issued.body[k]) << k;

    // Same model: identity, customer, lines, totals, payments, mentions, footer.
    json a = model(t, inv), b = model(t, till["id"]);
    EXPECT_FALSE(a["pos"].get<bool>());
    EXPECT_TRUE(b["pos"].get<bool>());
    EXPECT_EQ(comparable(a), comparable(b)) << a.dump(1) << "\n---\n" << b.dump(1);
    bool tillShown = false;
    for (const auto& kv : b["meta"]) tillShown |= kv[0] == "Caisse";
    EXPECT_TRUE(tillShown) << "the till document says where it was issued";
    EXPECT_NE(a["notices"].dump().find("ni reprises"), std::string::npos);

    // Same template: the A4 PDF of the till invoice (from Nhill POS) and of the Business
    // invoice contain the same text once the issue details are removed.
    auto pdfA = h.get("/api/v1/sales-documents/" + inv + "/pdf", t.token);
    auto pdfB = h.get("/api/v1/pos/sales/" + till["id"].get<std::string>() + "/receipt.pdf", t.token);  // A4 by default
    ASSERT_EQ(pdfA.status, 200);
    ASSERT_EQ(pdfB.status, 200);
    EXPECT_EQ(mediaBox(pdfA.raw), "595 842");
    EXPECT_EQ(mediaBox(pdfB.raw), "595 842");
    auto clean = [&](const std::string& txt) {
        std::vector<std::string> keep;
        std::istringstream in(txt);
        for (std::string line; std::getline(in, line);) {
            if (line.empty() || line.find(n1) != std::string::npos || line.find(n2) != std::string::npos ||
                line.find("Empreinte") != std::string::npos || line.find("Caisse") != std::string::npos ||
                line.find("Caissier") != std::string::npos || line.find("Date d'émission") != std::string::npos ||
                line.find("Échéance") != std::string::npos || line.find("Page") != std::string::npos ||
                line.find(" · Session ") != std::string::npos || line == "Propriétaire Test")
                continue;
            keep.push_back(line);
        }
        return keep;
    };
    std::string ta = pdfText(pdfA.raw), tb = pdfText(pdfB.raw);
    EXPECT_EQ(clean(ta), clean(tb)) << ta << "\n---\n" << tb;
    for (const std::string& txt : {ta, tb}) {
        EXPECT_NE(txt.find("FACTURE"), std::string::npos);
        EXPECT_NE(txt.find("Pharmacie du Fleuve"), std::string::npos);
        EXPECT_NE(txt.find("RCCM"), std::string::npos);
        EXPECT_NE(txt.find("Paiements reçus"), std::string::npos);
        EXPECT_NE(txt.find("Espèces"), std::string::npos);
        EXPECT_NE(txt.find("Payée"), std::string::npos);
        EXPECT_NE(txt.find("ni reprises"), std::string::npos);
        EXPECT_NE(txt.find("Mobile Money 0810000000"), std::string::npos);
    }

    // Both layouts for every document: the Business invoice on a roll, the till invoice too.
    auto rollA = h.get("/api/v1/sales-documents/" + inv + "/pdf?format=ticket&width=58", t.token);
    ASSERT_EQ(rollA.status, 200);
    EXPECT_EQ(mediaBox(rollA.raw).substr(0, 4), "164 ") << mediaBox(rollA.raw);  // 58 mm
    auto rollB = h.get("/api/v1/pos/sales/" + till["id"].get<std::string>() + "/receipt.pdf?format=ticket", t.token);
    ASSERT_EQ(rollB.status, 200);
    EXPECT_EQ(mediaBox(rollB.raw).substr(0, 4), "227 ") << mediaBox(rollB.raw);  // 80 mm (register setting)
    std::string ra = pdfText(rollA.raw), rb = pdfText(rollB.raw);
    for (const std::string& txt : {ra, rb}) {
        EXPECT_NE(txt.find("FACTURE"), std::string::npos) << txt;
        EXPECT_NE(txt.find("Pharmacie du Fleuve"), std::string::npos);
        EXPECT_NE(txt.find("Total TTC"), std::string::npos);
    }
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + inv + "/pdf?format=letter", t.token).status, 422);
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + inv + "/pdf?format=ticket&width=72", t.token).status, 422);

    // A till receipt: roll by default, A4 on demand, with the same identity block.
    auto rc = post("/api/v1/pos/sales",
                   {{"session_id", sid}, {"lines", {{{"product_id", p}, {"quantity", "1"}}}},
                    {"payments", {{{"method", "cash"}, {"amount", "2000.00"}, {"tendered", "2000.00"}}}}},
                   t.token);
    if (rc.status != 201) {  // the total depends on the tax profile: pay the exact amount
        auto price = h.get("/api/v1/products/" + p, t.token).body["sale_price"];
        rc = post("/api/v1/pos/sales",
                  {{"session_id", sid}, {"lines", {{{"product_id", p}, {"quantity", "1"}}}},
                   {"payments", {{{"method", "cash"}, {"amount", price}}}}},
                  t.token);
    }
    ASSERT_EQ(rc.status, 201) << rc.raw;
    std::string rid = rc.body["receipt"]["id"];
    auto roll = h.get("/api/v1/pos/sales/" + rid + "/receipt.pdf", t.token);
    EXPECT_EQ(mediaBox(roll.raw).substr(0, 4), "227 ");
    std::string rtxt = pdfText(roll.raw);
    EXPECT_NE(rtxt.find("TICKET DE CAISSE"), std::string::npos) << rtxt;
    EXPECT_NE(rtxt.find("RCCM"), std::string::npos) << "same identity block as invoices";
    EXPECT_NE(rtxt.find("document de gestion, non fiscal"), std::string::npos);
    EXPECT_NE(rtxt.find("Merci de votre visite."), std::string::npos);
    auto a4 = h.get("/api/v1/pos/sales/" + rid + "/receipt.pdf?format=a4", t.token);
    EXPECT_EQ(mediaBox(a4.raw), "595 842");
    EXPECT_EQ(pdfText(a4.raw).find("Facturé à"), std::string::npos) << "walk-in customer";

    // The session counts the till invoice as a sale.
    auto s = h.get("/api/v1/pos/sessions/" + sid, t.token);
    EXPECT_EQ(s.body["sales_count"], 2);
    // A till invoice is found by its number and can be returned at the till.
    auto found = h.get("/api/v1/pos/receipts?number=" + n2, t.token);
    ASSERT_EQ(found.status, 200) << found.raw;
    EXPECT_EQ(found.body["id"], till["id"]);
    auto ret = post("/api/v1/pos/sales/" + till["id"].get<std::string>() + "/return",
                    {{"session_id", sid}, {"reason", "Erreur de quantité"}, {"lines", {{{"line_no", 1}, {"quantity", "1"}}}}},
                    t.token);
    EXPECT_EQ(ret.status, 201) << ret.raw;
}

TEST(Documents, TillInvoiceNeedsTheInvoiceFeature) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string plan = h.su("INSERT INTO plans (code, name_fr, name_en, billing_period, price_minor, currency) VALUES ('" +
                            uniq("doc-") + "', 'Caisse seule', 'Till only', 'monthly', 1000, 'USD') RETURNING id")[0][0].c_str();
    h.su("INSERT INTO plan_entitlements (plan_id, key, enabled) VALUES ('" + plan + "', 'sales.invoices', false)");
    h.su("UPDATE subscriptions SET plan_id = '" + plan + "', is_trial = false, status = 'active' WHERE establishment_id = '" +
         t.id + "'");
    std::string c = createCustomer(t);
    std::string p = createProduct(t, uniq("P"), "10.00", false);
    auto reg = post("/api/v1/pos/registers", {{"code", "c1"}, {"name", "Caisse 1"}}, t.token);
    ASSERT_EQ(reg.status, 201) << reg.raw;
    auto sess = post("/api/v1/pos/sessions", {{"register_id", reg.body["id"]}}, t.token);
    ASSERT_EQ(sess.status, 201);
    auto r = post("/api/v1/pos/sales",
                  {{"session_id", sess.body["id"]}, {"document", "invoice"}, {"customer_id", c},
                   {"lines", {{{"product_id", p}, {"quantity", "1"}}}}, {"payments", {{{"method", "cash"}, {"amount", "10.00"}}}}},
                  t.token);
    EXPECT_EQ(r.status, 403);
    EXPECT_EQ(r.body["error"]["code"], "feature_not_included");
}

// Amounts are formatted by the shared library (server and applications): every currency
// offered by the database must be known there with the same number of decimals.
TEST(Documents, EveryCurrencyHasTheSameMinorUnitsEverywhere) {
    auto rows = Harness::get().su("SELECT code, minor_units FROM currencies WHERE active ORDER BY code");
    EXPECT_GE(rows.size(), 30u);
    for (const auto& r : rows)
        EXPECT_EQ(nhill::currencyMinorUnits(r[0].c_str()), r[1].as<int>()) << r[0].c_str();
}
