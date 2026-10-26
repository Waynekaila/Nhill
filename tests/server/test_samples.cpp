// Opt-in generator of sample documents for the documentation:
//   NH_SAMPLES_DIR=docs/exemples ./test_server --gtest_filter=Samples.*
// Uses fictitious demo data clearly labelled as such (never shipped as real data).
#include <cstdlib>
#include <fstream>

#include "harness.hpp"

using namespace nht;

TEST(Samples, GenerateDocumentSamples) {
    const char* dir = std::getenv("NH_SAMPLES_DIR");
    if (!dir) GTEST_SKIP() << "set NH_SAMPLES_DIR to generate samples";
    auto& h = Harness::get();
    auto save = [&](const std::string& name, const std::string& bytes) {
        std::ofstream(std::string(dir) + "/" + name, std::ios::binary) << bytes;
    };
    Tenant t = createTenant("company", {{"rccm", "CD/KNG/RCCM/00-X-00000"}, {"nif", "A0000000X"}}, true);
    h.request("PATCH", "/api/v1/establishment", {{"trade_name", "EXEMPLE — Quincaillerie Démo"}, {"phone", "+243 81 000 0000"},
                                                 {"email", "contact@exemple.invalid"}}, t.token);
    h.request("PATCH", "/api/v1/establishment/settings", {{"payment_instructions", "Mobile Money : +243 81 000 0000 (EXEMPLE) · Espèces au comptoir"}}, t.token);
    std::ifstream pin(std::string(NHILL_SOURCE_DIR) + "/assets/brand/nhill-icon-256.png", std::ios::binary);
    std::string png((std::istreambuf_iterator<char>(pin)), std::istreambuf_iterator<char>());
    h.request("PUT", "/api/v1/establishment/logo", nullptr, t.token, "", "image/png", png);
    auto c = h.post("/api/v1/parties", {{"is_customer", true}, {"name", "CLIENT EXEMPLE SARL"}, {"address", "Boulevard du 30 Juin"},
                                        {"city", "Kinshasa"}, {"tax_ids", {{"nif", "B0000000Y"}}}}, t.token);
    std::string cust = c.body["id"];
    std::string p = createProduct(t, "CIM-50", "18500.00");
    adjustStock(t, p, "100");
    std::string draft = draftInvoice(t, cust, json::array({{{"product_id", p}, {"quantity", "12"}, {"discount_bp", 500}},
        {{"description", "Transport camion benne Kinshasa – Kasangulu, aller-retour, chargement et déchargement inclus"},
         {"quantity", "1"}, {"unit_price", "250000.00"}}}));
    save("exemple-facture-brouillon.pdf", h.get("/api/v1/sales-documents/" + draft + "/pdf", t.token).raw);
    issue(t, draft);
    h.post("/api/v1/payments", {{"document_id", draft}, {"amount", "100000.00"}, {"method", "mobile_money"}}, t.token, newKey());
    save("exemple-facture.pdf", h.get("/api/v1/sales-documents/" + draft + "/pdf", t.token).raw);
    std::string today = h.su("SELECT (now() AT TIME ZONE 'Africa/Kinshasa')::date::text")[0][0].c_str();
    save("exemple-journal-ventes.pdf", h.get("/api/v1/reports/sales?format=pdf&from=2026-01-01&to=" + today, t.token).raw);
    save("exemple-journal-ventes.csv", h.get("/api/v1/reports/sales?format=csv&from=2026-01-01&to=" + today, t.token).raw);
}
