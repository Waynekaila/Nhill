// Commerce scenarios: stock, invoicing and numbering, payments, tenant
// isolation, roles, idempotency (network loss), PDF/CSV exports, logos.
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <set>

#include "nhill/money.hpp"
#include "harness.hpp"
#include "util/crypto.hpp"

using namespace nht;

namespace {

std::string stockOf(const Tenant& t, const std::string& product) {
    auto r = Harness::get().get("/api/v1/products/" + product, t.token);
    return r.body["stock_quantity"];
}

std::string runCmd(const std::string& cmd) {
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    pclose(p);
    return out;
}

std::string pdfText(const std::string& pdf, bool raw = false) {
    std::string path = "/tmp/" + uniq("doc") + ".pdf";
    std::ofstream(path, std::ios::binary) << pdf;
    std::string txt = runCmd(std::string("pdftotext ") + (raw ? "-raw " : "-layout ") + path + " - 2>/dev/null");
    std::remove(path.c_str());
    return txt;
}

// Minimal valid 32x32 RGB PNG (built with zlib-free stored deflate blocks is
// overkill: generate with Python's zlib is unavailable in C++, so we read
// the project's own icon).
std::string projectPng() {
    std::ifstream in(std::string(NHILL_SOURCE_DIR) + "/assets/icons/nhill-business-256.png", std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

}  // namespace

TEST(Stock, MovementsOnlyAndNegativeStockRefused) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string p = createProduct(t, "RIZ-25", "45000.00");
    adjustStock(t, p, "10");
    EXPECT_EQ(stockOf(t, p), "10");
    std::string c = createCustomer(t);
    std::string inv = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "3"}}}));
    auto r = issue(t, inv);
    ASSERT_EQ(r.status, 200) << r.raw;
    EXPECT_EQ(stockOf(t, p), "7");
    // Selling more than available: refused, and no invoice number consumed.
    std::string big = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "8"}}}));
    auto fail = issue(t, big);
    EXPECT_EQ(fail.status, 409);
    EXPECT_EQ(fail.body["error"]["code"], "insufficient_stock");
    EXPECT_EQ(stockOf(t, p), "7");
    std::string small = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "1"}}}));
    auto ok2 = issue(t, small);
    EXPECT_EQ(ok2.body["number"].get<std::string>().substr(8), "000002") << "no gap after the failed attempt";
    // The application role cannot touch stock levels directly.
    EXPECT_THROW(h.asApp(t.id, "UPDATE stock_levels SET quantity_milli = 999999"), pqxx::sql_error);
    auto mv = h.get("/api/v1/stock/movements?product_id=" + p, t.token);
    EXPECT_EQ(mv.body["items"].size(), 3u);  // opening, sale, sale
}

TEST(Stock, ServicesDoNotTrackStockAndAdjustmentNeedsReason) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string s = createProduct(t, "SERV-1", "100.00", false);
    auto r = h.post("/api/v1/stock/adjustments", {{"product_id", s}, {"quantity", "1"}, {"reason", "x"}}, t.token, newKey());
    EXPECT_EQ(r.status, 422);
    EXPECT_EQ(r.body["error"]["code"], "product_not_stocked");
    std::string p = createProduct(t, "GOOD-1", "1.00");
    auto noReason = h.post("/api/v1/stock/adjustments", {{"product_id", p}, {"quantity", "1"}}, t.token, newKey());
    EXPECT_EQ(noReason.status, 422);
}

TEST(Invoice, TotalsTaxesAndImmutability) {
    auto& h = Harness::get();
    Tenant t = createTenant("company", {{"nif", uniq("NIF")}}, true);  // VAT-registered: TVA16 default
    std::string c = createCustomer(t, "SARL Kin Distribution");
    std::string p = createProduct(t, "HUILE-5L", "1999.99");
    adjustStock(t, p, "100");
    std::string id = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "3"}, {"discount_bp", 1000}},
                                                     {{"description", "Livraison Gombe"}, {"quantity", "1"}, {"unit_price", "5000.00"}}}));
    auto d = h.get("/api/v1/sales-documents/" + id, t.token).body;
    EXPECT_EQ(d["status"], "draft");
    EXPECT_TRUE(d["number"].is_null());
    // line 1: 5 999.97 - 600.00 = 5 399.97 ; VAT 16 % = 864.00 ; line 2: 5 000.00 + 800.00
    EXPECT_EQ(d["net"], "10399.97");
    EXPECT_EQ(d["tax"], "1664.00");
    EXPECT_EQ(d["total"], "12063.97");
    auto is = issue(t, id);
    ASSERT_EQ(is.status, 200) << is.raw;
    std::string number = is.body["number"];
    EXPECT_EQ(number.substr(0, 3), "FA-");
    EXPECT_FALSE(is.body["pdf_sha256"].is_null());
    EXPECT_EQ(is.body["profile"], "CD");
    // Issued document: no edit, no delete, even directly in SQL.
    auto put = h.request("PUT", "/api/v1/sales-documents/" + id, {{"lines", json::array({{{"description", "x"}, {"quantity", "1"}, {"unit_price", "1.00"}}})}}, t.token);
    EXPECT_EQ(put.status, 409);
    EXPECT_EQ(h.request("DELETE", "/api/v1/sales-documents/" + id, nullptr, t.token).status, 409);
    EXPECT_THROW(h.asApp(t.id, "UPDATE sales_documents SET total_minor = 1 WHERE id = '" + id + "'"), pqxx::sql_error);
    EXPECT_THROW(h.asApp(t.id, "DELETE FROM sales_documents WHERE id = '" + id + "'"), pqxx::sql_error);
    EXPECT_THROW(h.asApp(t.id, "UPDATE sales_document_lines SET unit_price_minor = 1 WHERE document_id = '" + id + "'"), pqxx::sql_error);
    // Drafts can be deleted.
    std::string draft = draftInvoice(t, c, json::array({{{"description", "Brouillon"}, {"quantity", "1"}, {"unit_price", "1.00"}}}));
    EXPECT_EQ(h.request("DELETE", "/api/v1/sales-documents/" + draft, nullptr, t.token).status, 204);
}

TEST(Invoice, AmountsMustBeDecimalStrings) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string c = createCustomer(t);
    auto r = h.post("/api/v1/sales-documents", {{"doc_type", "invoice"}, {"party_id", c},
                                                {"lines", json::array({{{"description", "x"}, {"quantity", "1"}, {"unit_price", 0.1}}})}},
                    t.token, newKey());
    EXPECT_EQ(r.status, 422);
    EXPECT_EQ(r.body["error"]["details"]["reason"], "amount_must_be_decimal_string");
    auto r2 = h.post("/api/v1/sales-documents", {{"doc_type", "invoice"}, {"party_id", c},
                                                 {"lines", json::array({{{"description", "x"}, {"quantity", "1"}, {"unit_price", "0.123"}}})}},
                     t.token, newKey());
    EXPECT_EQ(r2.body["error"]["details"]["reason"], "invalid_amount");
    // 0.10 + 0.20 is exactly 0.30
    std::string id = draftInvoice(t, c, json::array({{{"description", "a"}, {"quantity", "1"}, {"unit_price", "0.10"}},
                                                     {{"description", "b"}, {"quantity", "1"}, {"unit_price", "0.20"}}}));
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + id, t.token).body["net"], "0.30");
}

TEST(Invoice, ConcurrentIssuingGivesUniqueConsecutiveNumbers) {
    Tenant t = createTenant("company");
    std::string c = createCustomer(t);
    const int n = 24;
    std::vector<std::string> drafts;
    for (int i = 0; i < n; ++i)
        drafts.push_back(draftInvoice(t, c, json::array({{{"description", "Prestation " + std::to_string(i)}, {"quantity", "1"}, {"unit_price", "10.00"}}})));
    std::vector<std::future<Response>> fs;
    for (const auto& d : drafts) fs.push_back(std::async(std::launch::async, [&t, d] { return issue(t, d); }));
    std::set<std::string> numbers;
    for (auto& f : fs) {
        auto r = f.get();
        ASSERT_EQ(r.status, 200) << r.raw;
        numbers.insert(r.body["number"].get<std::string>());
    }
    EXPECT_EQ(numbers.size(), static_cast<size_t>(n));
    std::set<int> seqs;
    for (const auto& num : numbers) seqs.insert(std::stoi(num.substr(num.rfind('-') + 1)));
    EXPECT_EQ(*seqs.begin(), 1);
    EXPECT_EQ(*seqs.rbegin(), n);
}

TEST(Invoice, NetworkLossDuringSaleIsSafeToRetry) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string p = createProduct(t, "SAC-CIMENT", "18.50");
    adjustStock(t, p, "20");
    std::string c = createCustomer(t);
    // Creation retried with the same key after a "lost" response: one draft only.
    std::string key = newKey();
    json body = {{"doc_type", "invoice"}, {"party_id", c}, {"lines", json::array({{{"product_id", p}, {"quantity", "2"}}})}};
    auto first = h.post("/api/v1/sales-documents", body, t.token, key);
    auto retry = h.post("/api/v1/sales-documents", body, t.token, key);
    EXPECT_EQ(first.status, 201);
    EXPECT_EQ(retry.status, 201);
    EXPECT_EQ(retry.header("Idempotent-Replayed"), "true");
    EXPECT_EQ(first.body["id"], retry.body["id"]);
    std::string id = first.body["id"];
    // Issue retried in parallel with the same key: one number, one stock movement.
    std::string ik = newKey();
    auto a = std::async(std::launch::async, [&] { return issue(t, id, ik); });
    auto b = std::async(std::launch::async, [&] { return issue(t, id, ik); });
    auto ra = a.get(), rb = b.get();
    EXPECT_EQ(ra.status, 200) << ra.raw;
    EXPECT_EQ(rb.status, 200) << rb.raw;
    EXPECT_EQ(ra.body["number"], rb.body["number"]);
    EXPECT_EQ(stockOf(t, p), "18");
    // Same key, different content: refused.
    auto reuse = h.post("/api/v1/sales-documents", {{"doc_type", "quote"}, {"party_id", c}, {"lines", body["lines"]}}, t.token, key);
    EXPECT_EQ(reuse.status, 409);
    EXPECT_EQ(reuse.body["error"]["code"], "idempotency_key_reused");
    // No key: refused for sensitive writes.
    EXPECT_EQ(h.post("/api/v1/sales-documents", body, t.token).body["error"]["code"], "idempotency_key_required");
}

TEST(Payments, ConcurrentPaymentsCannotOverpay) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string cashier = addMember(t, "sales");
    std::string c = createCustomer(t);
    std::string id = draftInvoice(t, c, json::array({{{"description", "Commande"}, {"quantity", "1"}, {"unit_price", "100.00"}}}));
    ASSERT_EQ(issue(t, id).status, 200);
    auto pay = [&](const std::string& tok) {
        return h.post("/api/v1/payments", {{"document_id", id}, {"amount", "70.00"}, {"method", "mobile_money"}, {"reference", "MPESA-1"}},
                      tok, newKey());
    };
    auto f1 = std::async(std::launch::async, [&] { return pay(t.token); });
    auto f2 = std::async(std::launch::async, [&] { return pay(cashier); });
    auto r1 = f1.get(), r2 = f2.get();
    int ok = (r1.status == 201) + (r2.status == 201);
    EXPECT_EQ(ok, 1) << r1.raw << " / " << r2.raw;
    auto d = h.get("/api/v1/sales-documents/" + id, t.token).body;
    EXPECT_EQ(d["paid"], "70.00");
    EXPECT_EQ(d["status"], "partially_paid");
    auto last = h.post("/api/v1/payments", {{"document_id", id}, {"amount", "30.00"}, {"method", "cash"}}, t.token, newKey());
    EXPECT_EQ(last.status, 201);
    d = h.get("/api/v1/sales-documents/" + id, t.token).body;
    EXPECT_EQ(d["status"], "paid");
    EXPECT_EQ(d["balance"], "0.00");
    auto wrongCur = h.post("/api/v1/payments", {{"document_id", id}, {"amount", "1.00"}, {"currency", "USD"}, {"method", "cash"}}, t.token, newKey());
    EXPECT_EQ(wrongCur.status, 409);  // paid invoice: not payable any more
}

TEST(Invoice, CreditNoteRestocksAndClosesInvoice) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string p = createProduct(t, "TOLE-3M", "12.00");
    adjustStock(t, p, "10");
    std::string c = createCustomer(t);
    std::string inv = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "4"}}}));
    ASSERT_EQ(issue(t, inv).status, 200);
    EXPECT_EQ(stockOf(t, p), "6");
    auto cn = h.post("/api/v1/sales-documents/" + inv + "/credit-note", {{"restock", true}, {"reason", "Marchandise retournée"}}, t.token, newKey());
    ASSERT_EQ(cn.status, 201) << cn.raw;
    auto cni = issue(t, cn.body["id"]);
    ASSERT_EQ(cni.status, 200) << cni.raw;
    EXPECT_EQ(cni.body["number"].get<std::string>().substr(0, 3), "AV-");
    EXPECT_EQ(cni.body["related_number"], h.get("/api/v1/sales-documents/" + inv, t.token).body["number"]);
    auto d = h.get("/api/v1/sales-documents/" + inv, t.token).body;
    EXPECT_EQ(d["status"], "credited");
    EXPECT_EQ(d["balance"], "0.00");
    EXPECT_EQ(stockOf(t, p), "10");
    // Cannot credit more than the invoice.
    auto again = h.post("/api/v1/sales-documents/" + inv + "/credit-note", json::object(), t.token, newKey());
    EXPECT_EQ(again.status, 409);
}

TEST(Invoice, QuoteToInvoiceConversion) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string c = createCustomer(t);
    std::string q = draftInvoice(t, c, json::array({{{"description", "Étude"}, {"quantity", "2"}, {"unit_price", "150.00"}}}), "quote");
    auto sent = issue(t, q);
    EXPECT_EQ(sent.body["status"], "sent");
    EXPECT_EQ(sent.body["number"].get<std::string>().substr(0, 3), "DV-");
    EXPECT_EQ(h.post("/api/v1/sales-documents/" + q + "/status", {{"status", "accepted"}}, t.token).status, 200);
    auto conv = h.post("/api/v1/sales-documents/" + q + "/convert", {{"to", "invoice"}}, t.token, newKey());
    ASSERT_EQ(conv.status, 201) << conv.raw;
    EXPECT_EQ(conv.body["doc_type"], "invoice");
    EXPECT_EQ(conv.body["total"], sent.body["total"]);
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + q, t.token).body["status"], "converted");
    // Invalid transition is refused and history is kept
    EXPECT_EQ(h.post("/api/v1/sales-documents/" + q + "/status", {{"status", "declined"}}, t.token).status, 409);
    EXPECT_GE(h.get("/api/v1/sales-documents/" + q, t.token).body["history"].size(), 3u);
}

TEST(Invoice, ForeignCurrencyRequiresAndFreezesExchangeRate) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");  // main currency CDF
    std::string c = createCustomer(t);
    std::string id = draftInvoice(t, c, json::array({{{"description", "Conseil"}, {"quantity", "1"}, {"unit_price", "100.00"}}}), "invoice", "USD");
    auto noRate = issue(t, id);
    EXPECT_EQ(noRate.status, 422);
    EXPECT_EQ(noRate.body["error"]["details"]["reason"], "fx_rate_required");
    auto rate = h.post("/api/v1/exchange-rates", {{"from", "USD"}, {"to", "CDF"}, {"rate", "2850.50"}, {"rate_date", "2026-01-01"}, {"source", "BCC"}}, t.token);
    ASSERT_EQ(rate.status, 201) << rate.raw;
    auto ok = issue(t, id);
    ASSERT_EQ(ok.status, 200) << ok.raw;
    EXPECT_EQ(ok.body["fx_rate"], "2850.5000000000");
    EXPECT_EQ(ok.body["fx_rate_source"], "BCC");
    // A later rate does not change the issued document.
    h.post("/api/v1/exchange-rates", {{"from", "USD"}, {"to", "CDF"}, {"rate", "3000"}, {"rate_date", "2026-01-02"}}, t.token);
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + id, t.token).body["fx_rate"], "2850.5000000000");
}

TEST(Isolation, OtherEstablishmentsDataIsUnreachable) {
    auto& h = Harness::get();
    Tenant a = createTenant("company");
    Tenant b = createTenant("company");
    std::string ca = createCustomer(a, "Client secret de A");
    std::string pa = createProduct(a, "SECRET-A", "1.00");
    std::string inv = draftInvoice(a, ca, json::array({{{"description", "x"}, {"quantity", "1"}, {"unit_price", "5.00"}}}));
    ASSERT_EQ(issue(a, inv).status, 200);
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + inv, b.token).status, 404);
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + inv + "/pdf", b.token).status, 404);
    EXPECT_EQ(h.get("/api/v1/parties/" + ca, b.token).status, 404);
    EXPECT_EQ(h.get("/api/v1/products/" + pa, b.token).status, 404);
    EXPECT_EQ(h.request("PATCH", "/api/v1/products/" + pa, {{"name", "piraté"}}, b.token).status, 404);
    auto pay = h.post("/api/v1/payments", {{"document_id", inv}, {"amount", "1.00"}, {"method", "cash"}}, b.token, newKey());
    EXPECT_EQ(pay.status, 404);
    // Using A's customer on B's invoice is impossible (composite foreign keys + RLS).
    auto cross = h.post("/api/v1/sales-documents", {{"doc_type", "invoice"}, {"party_id", ca},
                                                    {"lines", json::array({{{"description", "x"}, {"quantity", "1"}, {"unit_price", "1.00"}}})}},
                        b.token, newKey());
    EXPECT_EQ(cross.status, 422);
    for (auto& item : h.get("/api/v1/customers?active=all", b.token).body["items"]) EXPECT_NE(item["id"], ca);
    // Even raw SQL under B's tenant context sees nothing of A.
    EXPECT_EQ(h.asApp(b.id, "SELECT count(*) FROM parties WHERE id = '" + ca + "'")[0][0].as<int>(), 0);
    EXPECT_THROW(h.asApp(b.id, "INSERT INTO parties (establishment_id, is_customer, name) VALUES ('" + a.id + "', true, 'x')"),
                 pqxx::sql_error);
    EXPECT_EQ(h.asApp(b.id, "UPDATE products SET name = 'x' WHERE id = '" + pa + "'").affected_rows(), 0);
}

TEST(Roles, ServerEnforcesPermissions) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string viewer = addMember(t, "viewer");
    std::string sales = addMember(t, "sales");
    std::string accountant = addMember(t, "accountant");
    EXPECT_EQ(h.post("/api/v1/products", {{"sku", "V1"}, {"name", "x"}, {"sale_price", "1.00"}}, viewer).status, 403);
    EXPECT_EQ(h.post("/api/v1/products", {{"sku", "S1"}, {"name", "x"}, {"sale_price", "1.00"}}, sales).status, 403);
    EXPECT_EQ(h.post("/api/v1/parties", {{"is_supplier", true}, {"name", "Fournisseur"}}, sales).status, 403);
    EXPECT_EQ(h.post("/api/v1/parties", {{"is_customer", true}, {"name", "Client"}}, sales).status, 201);
    EXPECT_EQ(h.get("/api/v1/members", sales).status, 403);
    EXPECT_EQ(h.get("/api/v1/reports/sales?from=2026-01-01&to=2026-12-31", viewer).status, 403);
    EXPECT_EQ(h.get("/api/v1/reports/sales?from=2026-01-01&to=2026-12-31", accountant).status, 200);
    EXPECT_EQ(h.post("/api/v1/invitations", {{"email", "x@example.cd"}, {"role", "admin"}}, accountant).status, 403);
    EXPECT_EQ(h.get("/api/v1/products", viewer).status, 200);
}

TEST(Documents, PdfInvoiceContainsLegalDataAndNotice) {
    auto& h = Harness::get();
    Tenant t = createTenant("company", {{"rccm", "CD/KNG/RCCM/" + uniq("26-B-")}, {"nif", "A" + uniq()}}, true);
    std::string c = createCustomer(t, "Établissements Lumumba & Fils");
    std::string id = draftInvoice(t, c, json::array({{{"description", "Sac de ciment 50 kg — livraison à Limete"}, {"quantity", "12"}, {"unit_price", "18500.00"}}}));
    auto draftPdf = h.get("/api/v1/sales-documents/" + id + "/pdf", t.token);
    ASSERT_EQ(draftPdf.status, 200);
    EXPECT_EQ(draftPdf.raw.substr(0, 5), "%PDF-");
    EXPECT_NE(pdfText(draftPdf.raw, true).find("BROUILLON"), std::string::npos);  // rotated watermark
    auto is = issue(t, id);
    ASSERT_EQ(is.status, 200);
    auto pdf = h.get("/api/v1/sales-documents/" + id + "/pdf", t.token);
    std::string txt = pdfText(pdf.raw);
    EXPECT_NE(txt.find("FACTURE"), std::string::npos) << txt;
    EXPECT_NE(txt.find(is.body["number"].get<std::string>()), std::string::npos);
    EXPECT_NE(txt.find("Lumumba & Fils"), std::string::npos);
    EXPECT_NE(txt.find("RCCM"), std::string::npos);
    EXPECT_NE(txt.find("NIF"), std::string::npos);
    EXPECT_NE(txt.find("ne remplace pas la facture normalisée"), std::string::npos) << "VAT-registered CD notice";
    EXPECT_EQ(pdfText(pdf.raw, true).find("BROUILLON"), std::string::npos);
    EXPECT_NE(txt.find("12 u."), std::string::npos) << "unit translated";
    // Deterministic rendering: the stored fingerprint matches a re-render.
    EXPECT_EQ(nh::crypto::sha256Hex(pdf.raw), is.body["pdf_sha256"]);
}

TEST(Documents, LogoValidationAndInvalidStoredLogoFallback) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string png = projectPng();
    ASSERT_GT(png.size(), 100u) << "brand icon missing";
    auto bad = h.request("PUT", "/api/v1/establishment/logo", nullptr, t.token, "", "image/png", "\x89PNG\r\n\x1a\nnot really a png at all, sorry");
    EXPECT_EQ(bad.status, 422);
    std::string truncated = png.substr(0, png.size() / 2);
    auto trunc = h.request("PUT", "/api/v1/establishment/logo", nullptr, t.token, "", "image/png", truncated);
    EXPECT_EQ(trunc.status, 422);
    auto mismatch = h.request("PUT", "/api/v1/establishment/logo", nullptr, t.token, "", "image/jpeg", png);
    EXPECT_EQ(mismatch.status, 422);
    auto ok = h.request("PUT", "/api/v1/establishment/logo", nullptr, t.token, "", "image/png", png);
    ASSERT_EQ(ok.status, 200) << ok.raw;
    std::string c = createCustomer(t);
    std::string id = draftInvoice(t, c, json::array({{{"description", "x"}, {"quantity", "1"}, {"unit_price", "1.00"}}}));
    auto pdf = h.get("/api/v1/sales-documents/" + id + "/pdf", t.token);
    std::string path = "/tmp/" + uniq("logo") + ".pdf";
    std::ofstream(path, std::ios::binary) << pdf.raw;
    std::string imgs = runCmd("pdfimages -list " + path);
    EXPECT_NE(imgs.find("256"), std::string::npos) << imgs;  // embedded at native resolution
    // The stored logo gets corrupted (e.g. a bad restore): documents still render, with the text identity.
    h.su("UPDATE objects SET data = decode('00ff00ff', 'hex') WHERE establishment_id = '" + t.id + "' AND purpose = 'logo'");
    auto pdf2 = h.get("/api/v1/sales-documents/" + id + "/pdf", t.token);
    EXPECT_EQ(pdf2.status, 200);
    EXPECT_NE(pdfText(pdf2.raw).find("Boutique"), std::string::npos);
    std::remove(path.c_str());
}

TEST(Reports, CsvAndPdfExportsAndDashboard) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string c = createCustomer(t, "=HYPERLINK(\"http://evil\")");
    std::string p = createProduct(t, "EAU-1.5L", "1500.00");
    adjustStock(t, p, "50");
    std::string id = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "10"}}}));
    ASSERT_EQ(issue(t, id).status, 200);
    h.post("/api/v1/expenses", {{"description", "Loyer octobre"}, {"amount", "250000.00"}, {"currency", "CDF"}, {"expense_date", "2026-10-01"},
                                {"paid", {{"method", "cash"}}}}, t.token, newKey());
    std::string today = h.su("SELECT (now() AT TIME ZONE 'Africa/Kinshasa')::date::text")[0][0].c_str();
    auto dash = h.get("/api/v1/reports/dashboard?from=2026-01-01&to=" + today, t.token);
    ASSERT_EQ(dash.status, 200) << dash.raw;
    EXPECT_EQ(dash.body["revenue"]["CDF"]["net"], "15000.00");
    EXPECT_EQ(dash.body["receivables"]["CDF"]["open"], "15000.00");
    EXPECT_EQ(dash.body["expenses"]["CDF"], "250000.00");
    auto csv = h.get("/api/v1/reports/sales?format=csv&from=2026-01-01&to=" + today, t.token);
    EXPECT_EQ(csv.status, 200);
    EXPECT_EQ(csv.raw.substr(0, 3), "\xEF\xBB\xBF");
    EXPECT_NE(csv.raw.find("15000.00"), std::string::npos);
    EXPECT_NE(csv.raw.find("'=HYPERLINK"), std::string::npos) << "formula injection neutralised";
    auto pdf = h.get("/api/v1/reports/sales?format=pdf&from=2026-01-01&to=" + today, t.token);
    EXPECT_EQ(pdf.raw.substr(0, 5), "%PDF-");
    EXPECT_NE(pdfText(pdf.raw).find("15 000,00"), std::string::npos);
    auto prod = h.get("/api/v1/products/export.csv", t.token);
    EXPECT_NE(prod.raw.find("EAU-1.5L"), std::string::npos);
    for (const char* r : {"expenses", "stock-movements", "payments", "products"})
        EXPECT_EQ(h.get(std::string("/api/v1/reports/") + r + "?format=csv&from=2026-01-01&to=" + today, t.token).status, 200) << r;
    EXPECT_EQ(h.get("/api/v1/reports/receivables?format=pdf", t.token).status, 200);
    EXPECT_EQ(h.get("/api/v1/reports/payables?format=csv", t.token).status, 200);
}

TEST(Purchases, ReceiveStocksAndPay) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    auto sup = h.post("/api/v1/parties", {{"is_supplier", true}, {"name", "Grossiste Matadi"}}, t.token);
    std::string p = createProduct(t, "FARINE-25", "30000.00");
    auto po = h.post("/api/v1/purchases", {{"supplier_id", sup.body["id"]}, {"supplier_reference", "BL-77"},
                                           {"lines", json::array({{{"product_id", p}, {"quantity", "40"}, {"unit_cost", "22000.00"}}})}},
                     t.token, newKey());
    ASSERT_EQ(po.status, 201) << po.raw;
    auto rec = h.post("/api/v1/purchases/" + po.body["id"].get<std::string>() + "/receive", json::object(), t.token, newKey());
    ASSERT_EQ(rec.status, 200) << rec.raw;
    EXPECT_EQ(stockOf(t, p), "40");
    EXPECT_EQ(h.get("/api/v1/products/" + p, t.token).body["purchase_price"], "22000.00");
    auto pay = h.post("/api/v1/payments", {{"purchase_id", po.body["id"]}, {"amount", "880000.00"}, {"method", "bank_transfer"}}, t.token, newKey());
    EXPECT_EQ(pay.status, 201) << pay.raw;
    EXPECT_EQ(h.get("/api/v1/purchases/" + po.body["id"].get<std::string>(), t.token).body["balance"], "0.00");
    // A received purchase is immutable.
    EXPECT_THROW(h.asApp(t.id, "UPDATE purchases SET total_minor = 0 WHERE id = '" + po.body["id"].get<std::string>() + "'"), pqxx::sql_error);
}

TEST(Catalog, CsvImportIsAllOrNothing) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string good = "sku;name;sale_price;category\nA1;Savon;1500;Hygiène\nA2;Dentifrice;2500,50;Hygiène\n";
    auto ok = h.request("POST", "/api/v1/products/import", nullptr, t.token, "", "text/csv", good);
    ASSERT_EQ(ok.status, 200) << ok.raw;
    EXPECT_EQ(ok.body["created"], 2);
    std::string bad = "sku;name;sale_price\nB1;Ok;10\nB2;Prix faux;abc\n";
    auto ko = h.request("POST", "/api/v1/products/import", nullptr, t.token, "", "text/csv", bad);
    EXPECT_EQ(ko.status, 422);
    EXPECT_EQ(ko.body["error"]["details"]["errors"][0]["line"], 3);
    EXPECT_EQ(h.get("/api/v1/products?search=B1", t.token).body["total"], 0);
}

TEST(Documents, NoticeOnlyForVatRegisteredAndDefReferenceIsRecorded) {
    auto& h = Harness::get();
    // Not VAT-registered: commercial invoice without the "management document" notice.
    Tenant small = createTenant("company", json::object(), false);
    std::string c1 = createCustomer(small);
    std::string d1 = draftInvoice(small, c1, json::array({{{"description", "Service"}, {"quantity", "1"}, {"unit_price", "100.00"}}}));
    ASSERT_EQ(issue(small, d1).status, 200);
    std::string txt = pdfText(h.get("/api/v1/sales-documents/" + d1 + "/pdf", small.token).raw);
    EXPECT_EQ(txt.find("ne remplace pas la facture normalisée"), std::string::npos) << txt;

    // VAT-registered: the DEF reference can be recorded on the issued invoice (not on a draft).
    Tenant vat = createTenant("company", {{"rccm", "CD/KNG/RCCM/" + uniq("26-B-")}, {"nif", "A" + uniq()}}, true);
    std::string c2 = createCustomer(vat);
    std::string d2 = draftInvoice(vat, c2, json::array({{{"description", "Vente"}, {"quantity", "1"}, {"unit_price", "100.00"}}}));
    json ref = {{"def_nid", "DEF-TEST-0001"}, {"def_auth_code", "AUTH-TEST"}, {"normalized_invoice_number", "FN-TEST-1"}};
    EXPECT_EQ(h.request("PUT", "/api/v1/sales-documents/" + d2 + "/fiscal-reference", ref, vat.token).status, 409);  // draft
    ASSERT_EQ(issue(vat, d2).status, 200);
    auto r = h.request("PUT", "/api/v1/sales-documents/" + d2 + "/fiscal-reference", ref, vat.token);
    ASSERT_EQ(r.status, 200) << r.raw;
    EXPECT_EQ(r.body["fiscal_reference"]["def_nid"], "DEF-TEST-0001");
    auto log = h.get("/api/v1/activity?action=sales_document.fiscal_reference", vat.token);
    ASSERT_EQ(log.body["items"].size(), 1u);
    EXPECT_NE(log.body["items"][0]["summary"].get<std::string>().find("DEF-TEST-0001"), std::string::npos);
    // Amounts of the issued invoice are still immutable.
    EXPECT_THROW(h.asApp(vat.id, "UPDATE sales_documents SET total_minor = 1 WHERE id = '" + d2 + "'"), pqxx::sql_error);
}
