// Plans and rights (3.0.0). Capabilities are set per plan through Nhill Console (no code);
// for every plan and every role, a feature outside the plan is absent from the rights the
// applications display (/me, /entitlements) and refused by the API, its data included
// (lists, details, reports, dashboard, payments, stock movements, activity, notifications).
// Also: limits, plan changes, expiry, grace and suspension, fallback plans, assignment
// rules, retried requests after a connection loss and concurrent requests on a limit.
#include <cstdlib>
#include <fstream>
#include <future>
#include <map>
#include <set>

#include "harness.hpp"
#include "util/crypto.hpp"
#include "util/zip.hpp"

using namespace nht;

namespace {

const std::vector<std::string> kFeatures = {"sales.invoices", "sales.quotes",   "pos",
                                            "purchases",      "expenses",       "stock.multi_warehouse",
                                            "multi_currency", "catalog.import", "reports.advanced"};

std::string billingToken() {
    static std::string t = adminLogin("billing");
    return t;
}

Response admin(const std::string& method, const std::string& path, const json& body = nullptr) {
    return Harness::get().request(method, path, body, billingToken());
}

Response call(const std::string& method, const std::string& path, const json& body, const std::string& token,
              const std::string& contentType = "application/json", const std::string& raw = "") {
    return Harness::get().request(method, path, body, token, newKey(), contentType, raw);
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// A plan whose capabilities are set through Nhill Console: every feature on except `off`,
// plus the given limits.
std::string makePlan(const std::set<std::string>& off, const json& limits = json::object(),
                     const std::string& kind = "paid") {
    auto& h = Harness::get();
    std::string id = h.su("INSERT INTO plans (code, name_fr, name_en, billing_period, price_minor, currency, kind) VALUES ('" +
                          lower(uniq("ent-")) + "', 'Formule test', 'Test plan', 'monthly', 1000, 'USD', '" + kind +
                          "') RETURNING id")[0][0].c_str();
    json values = json::object();
    for (const auto& f : kFeatures) values[f] = off.count(f) == 0;
    for (auto it = limits.begin(); it != limits.end(); ++it) values[it.key()] = it.value();
    auto r = admin("PUT", "/api/v1/admin/plans/" + id + "/entitlements", {{"values", values}});
    if (r.status != 200) throw std::runtime_error("entitlements: " + r.raw);
    return id;
}

// Assigns the plan from Nhill Console, as an active period of 30 days.
void putOn(const Tenant& t, const std::string& plan) {
    auto r = admin("POST", "/api/v1/admin/establishments/" + t.id + "/subscription",
                   {{"action", "assign_plan"}, {"plan_id", plan}, {"mode", "active"}, {"days", 30}, {"reason", "Formule de test"}});
    if (r.status != 200) throw std::runtime_error("assign_plan: " + r.raw);
}

std::set<std::string> permissions(const std::string& token) {
    auto me = Harness::get().get("/api/v1/me", token);
    if (me.status != 200) throw std::runtime_error("me: " + me.raw);
    std::set<std::string> out;
    for (const auto& p : me.body["permissions"]) out.insert(p.get<std::string>());
    return out;
}

// Permissions whose feature is `feature` (same mapping as the server).
bool permissionOf(const std::string& perm, const std::string& feature) {
    if (feature == "pos") return perm.rfind("pos.", 0) == 0;
    if (feature == "purchases") return perm.rfind("purchases.", 0) == 0;
    if (feature == "expenses") return perm.rfind("expenses.", 0) == 0;
    return false;
}

struct Probe {
    std::string method, path;
    json body;
    std::string contentType = "application/json";
    std::string raw;
};

// Requests that reach the feature directly. Every one must be refused outside the plan.
std::vector<Probe> probesFor(const std::string& f, const std::string& customer, const std::string& product) {
    json line = json::array({{{"product_id", product}, {"quantity", "1"}}});
    if (f == "sales.invoices")
        return {{"POST", "/api/v1/sales-documents", {{"doc_type", "invoice"}, {"party_id", customer}, {"lines", line}}},
                {"GET", "/api/v1/reports/receivables", nullptr}};
    if (f == "sales.quotes")
        return {{"POST", "/api/v1/sales-documents", {{"doc_type", "quote"}, {"party_id", customer}, {"lines", line}}},
                {"POST", "/api/v1/sales-documents", {{"doc_type", "order"}, {"party_id", customer}, {"lines", line}}}};
    if (f == "pos")
        return {{"GET", "/api/v1/pos/config", nullptr},
                {"GET", "/api/v1/pos/catalog", nullptr},
                {"GET", "/api/v1/pos/registers", nullptr},
                {"POST", "/api/v1/pos/registers", {{"code", "x1"}, {"name", "Caisse"}}},
                {"GET", "/api/v1/pos/sessions", nullptr},
                {"GET", "/api/v1/pos/receipts", nullptr},
                {"PATCH", "/api/v1/pos/settings", json::object()}};
    if (f == "purchases")
        return {{"GET", "/api/v1/purchases", nullptr}, {"POST", "/api/v1/purchases", {{"lines", json::array()}}}};
    if (f == "expenses")
        return {{"GET", "/api/v1/expenses", nullptr},
                {"GET", "/api/v1/expense-categories", nullptr},
                {"POST", "/api/v1/expenses", {{"description", "Loyer"}, {"amount", "10.00"}, {"currency", "CDF"}}},
                {"GET", "/api/v1/reports/expenses", nullptr}};
    if (f == "stock.multi_warehouse")
        return {{"POST", "/api/v1/warehouses", {{"name", "Dépôt 2"}}},
                {"POST", "/api/v1/stock/transfers",
                 {{"product_id", product}, {"from_warehouse_id", product}, {"to_warehouse_id", customer}, {"quantity", "1"}}}};
    if (f == "multi_currency")
        return {{"GET", "/api/v1/exchange-rates", nullptr},
                {"POST", "/api/v1/exchange-rates", {{"from", "USD"}, {"to", "CDF"}, {"rate", "2800"}}},
                {"POST", "/api/v1/sales-documents",
                 {{"doc_type", "quote"}, {"party_id", customer}, {"lines", line}, {"currency", "USD"}}}};
    if (f == "catalog.import") return {{"POST", "/api/v1/products/import", nullptr, "text/csv", "sku;name\nIMP-1;Importé\n"}};
    if (f == "reports.advanced")
        return {{"GET", "/api/v1/reports/products", nullptr}, {"GET", "/api/v1/reports/staff", nullptr}};
    return {};
}

// Copy of the "items" of a list (never iterate over a member of a temporary response).
json items(const std::string& path, const std::string& token) {
    auto r = Harness::get().get(path, token);
    if (r.status != 200) throw std::runtime_error(path + ": " + r.raw);
    return r.body["items"];
}

std::string today() {
    return Harness::get().su("SELECT (now() AT TIME ZONE 'Africa/Kinshasa')::date::text")[0][0].c_str();
}

}  // namespace

// ------------------------------------------------------------------ Nhill Console

TEST(Plans, ConsoleSetsTheCapabilitiesOfAPlanWithoutCode) {
    auto& h = Harness::get();
    auto cat = admin("GET", "/api/v1/admin/capabilities");
    ASSERT_EQ(cat.status, 200) << cat.raw;
    std::set<std::string> keys;
    for (const auto& c : cat.body["items"]) keys.insert(c["key"].get<std::string>());
    for (const auto& f : kFeatures) EXPECT_TRUE(keys.count(f)) << f;
    for (const char* l : {"users.max", "pos.registers.max", "stock.warehouses.max", "catalog.products.max",
                          "documents.month.max", "storage.mb.max"})
        EXPECT_TRUE(keys.count(l)) << l;

    std::string plan = makePlan({"pos", "purchases"}, {{"users.max", 3}, {"catalog.products.max", nullptr}});
    auto got = admin("GET", "/api/v1/admin/plans/" + plan + "/entitlements");
    ASSERT_EQ(got.status, 200);
    EXPECT_EQ(got.body["values"]["pos"], false);
    EXPECT_EQ(got.body["values"]["purchases"], false);
    EXPECT_EQ(got.body["values"]["sales.invoices"], true);
    EXPECT_EQ(got.body["values"]["users.max"], 3);
    EXPECT_TRUE(got.body["values"]["catalog.products.max"].is_null());  // unlimited
    // The 2.x columns follow (2.x clients read them).
    EXPECT_EQ(h.su("SELECT max_users FROM plans WHERE id = '" + plan + "'")[0][0].as<int>(), 3);

    // Values are validated: unknown key, wrong kind, out of range.
    EXPECT_EQ(admin("PUT", "/api/v1/admin/plans/" + plan + "/entitlements", {{"values", {{"teleport", true}}}}).status, 422);
    EXPECT_EQ(admin("PUT", "/api/v1/admin/plans/" + plan + "/entitlements", {{"values", {{"pos", 5}}}}).status, 422);
    EXPECT_EQ(admin("PUT", "/api/v1/admin/plans/" + plan + "/entitlements", {{"values", {{"users.max", true}}}}).status, 422);
    EXPECT_EQ(admin("PUT", "/api/v1/admin/plans/" + plan + "/entitlements", {{"values", {{"users.max", 0}}}}).status, 422);
    // Only billing administrators (and superadmins) change plans.
    std::string support = adminLogin("support");
    EXPECT_EQ(h.request("PUT", "/api/v1/admin/plans/" + plan + "/entitlements", {{"values", {{"pos", true}}}}, support).status,
              403);
    // Every change is audited with the values before and after.
    auto audit = h.su("SELECT details FROM audit_log WHERE action = 'admin.plan_entitlements_updated' AND entity_id = '" +
                      plan + "' ORDER BY id DESC LIMIT 1");
    ASSERT_EQ(audit.size(), 1u);
    json d = json::parse(audit[0][0].c_str());
    EXPECT_TRUE(d.contains("before"));
    EXPECT_EQ(d["after"]["pos"], false);
}

// ------------------------------------------------------------------ plan × role × route

TEST(Plans, EveryFeatureOutsideThePlanIsAbsentAndRefusedForEveryRole) {
    auto& h = Harness::get();
    for (const auto& feature : kFeatures) {
        SCOPED_TRACE("feature " + feature);
        Tenant t = createTenant("company");
        std::string customer = createCustomer(t);
        std::string product = createProduct(t, uniq("P"), "1500.00");
        putOn(t, makePlan({feature}));

        std::vector<std::pair<std::string, std::string>> roles = {{"owner", t.token}};
        for (const char* r : {"admin", "manager", "sales", "accountant", "viewer"}) roles.push_back({r, addMember(t, r)});
        if (feature == "pos") {
            // Till roles do not exist without the till.
            for (const char* r : {"cashier", "cash_manager"}) {
                auto inv = call("POST", "/api/v1/invitations", {{"email", uniq(r) + "@example.cd"}, {"role", r}}, t.token);
                EXPECT_EQ(inv.status, 403) << r;
                EXPECT_EQ(inv.body["error"]["code"], "feature_not_included");
            }
        } else {
            for (const char* r : {"cashier", "cash_manager"}) roles.push_back({r, addMember(t, r)});
        }

        for (const auto& [role, token] : roles) {
            SCOPED_TRACE("role " + role);
            // Rights shown by the applications.
            auto perms = permissions(token);
            EXPECT_FALSE(perms.count("feature." + feature));
            for (const auto& p : perms) EXPECT_FALSE(permissionOf(p, feature)) << p;
            auto ent = h.get("/api/v1/entitlements", token);
            ASSERT_EQ(ent.status, 200) << ent.raw;
            for (const auto& f : ent.body["features"]) EXPECT_NE(f.get<std::string>(), feature);
            EXPECT_FALSE(ent.body["labels"].contains(feature));
            // API: refused, whatever the role.
            for (const auto& p : probesFor(feature, customer, product)) {
                auto r = call(p.method, p.path, p.body, token, p.contentType, p.raw);
                EXPECT_EQ(r.status, 403) << p.method << " " << p.path << " " << r.raw;
                std::string code = r.body.is_object() ? r.body["error"].value("code", "") : "";
                if (role == "owner" || role == "admin") {
                    EXPECT_EQ(code, "feature_not_included") << p.method << " " << p.path;
                } else {
                    EXPECT_TRUE(code == "feature_not_included" || code == "permission_denied") << p.path << " " << code;
                }
            }
        }
    }

    // Control: the same requests pass the feature check when the plan includes everything.
    Tenant t = createTenant("company");
    std::string customer = createCustomer(t);
    std::string product = createProduct(t, uniq("P"), "1500.00");
    putOn(t, makePlan({}));
    auto perms = permissions(t.token);
    for (const auto& f : kFeatures) EXPECT_TRUE(perms.count("feature." + f)) << f;
    for (const auto& f : kFeatures)
        for (const auto& p : probesFor(f, customer, product)) {
            auto r = call(p.method, p.path, p.body, t.token, p.contentType, p.raw);
            std::string code = r.body.is_object() && r.body.contains("error") ? r.body["error"].value("code", "") : "";
            EXPECT_NE(code, "feature_not_included") << p.method << " " << p.path << " " << r.raw;
        }
}

TEST(Plans, SuppliersExistOnlyWithPurchasesOrExpenses) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    auto sup = h.post("/api/v1/parties", {{"is_supplier", true}, {"name", "Grossiste"}}, t.token);
    ASSERT_EQ(sup.status, 201);
    std::string id = sup.body["id"];
    putOn(t, makePlan({"purchases"}));  // expenses still there: suppliers stay
    EXPECT_EQ(h.get("/api/v1/suppliers", t.token).status, 200);
    EXPECT_EQ(h.get("/api/v1/parties/" + id, t.token).status, 200);
    putOn(t, makePlan({"purchases", "expenses"}));
    EXPECT_EQ(h.get("/api/v1/suppliers", t.token).status, 403);
    EXPECT_EQ(h.get("/api/v1/parties/" + id, t.token).status, 404);
    EXPECT_EQ(h.request("PATCH", "/api/v1/parties/" + id, {{"name", "Autre"}}, t.token).status, 404);
    auto created = h.post("/api/v1/parties", {{"is_supplier", true}, {"name", "Nouveau"}}, t.token);
    EXPECT_EQ(created.status, 403);
    EXPECT_EQ(created.body["error"]["code"], "feature_not_included");
    // Customers are not affected.
    EXPECT_EQ(h.post("/api/v1/parties", {{"is_customer", true}, {"name", "Client"}}, t.token).status, 201);
}

// ------------------------------------------------------------------ indirect access

TEST(Plans, DataOfARemovedFeatureIsNotReachableIndirectlyAndComesBack) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    std::string full = makePlan({});
    putOn(t, full);
    std::string c = createCustomer(t);
    std::string p = createProduct(t, uniq("P"), "1500.00");
    adjustStock(t, p, "50");
    // Quote (kept by the reduced plan), invoice with payment.
    std::string q = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "1"}}}), "quote");
    ASSERT_EQ(issue(t, q).status, 200);
    std::string inv = draftInvoice(t, c, json::array({{{"product_id", p}, {"quantity", "2"}}}));
    ASSERT_EQ(issue(t, inv).status, 200);
    ASSERT_EQ(call("POST", "/api/v1/payments", {{"document_id", inv}, {"amount", "3000.00"}, {"method", "cash"}}, t.token).status,
              201);
    // Purchase received and paid, paid expense.
    auto sup = h.post("/api/v1/parties", {{"is_supplier", true}, {"name", "Grossiste"}}, t.token);
    std::string supplier = sup.body["id"];
    auto po = call("POST", "/api/v1/purchases",
                   {{"supplier_id", supplier}, {"lines", json::array({{{"product_id", p}, {"quantity", "5"}, {"unit_cost", "900.00"}}})}},
                   t.token);
    ASSERT_EQ(po.status, 201) << po.raw;
    std::string poId = po.body["id"];
    ASSERT_EQ(call("POST", "/api/v1/purchases/" + poId + "/receive", json::object(), t.token).status, 200);
    ASSERT_EQ(call("POST", "/api/v1/payments", {{"purchase_id", poId}, {"amount", "4500.00"}, {"method", "cash"}}, t.token).status,
              201);
    ASSERT_EQ(call("POST", "/api/v1/expenses",
                   {{"description", "Loyer"}, {"amount", "200.00"}, {"currency", "CDF"}, {"paid", {{"method", "cash"}}}}, t.token)
                  .status,
              201);
    // Second warehouse and transfer.
    auto w2 = call("POST", "/api/v1/warehouses", {{"name", "Dépôt Limete"}}, t.token);
    ASSERT_EQ(w2.status, 201) << w2.raw;
    std::string mainW;
    for (const auto& w : items("/api/v1/warehouses", t.token))
        if (w["is_default"].get<bool>()) mainW = w["id"].get<std::string>();
    auto tr = call("POST", "/api/v1/stock/transfers",
                   {{"product_id", p}, {"from_warehouse_id", mainW}, {"to_warehouse_id", w2.body["id"]}, {"quantity", "1"}}, t.token);
    ASSERT_EQ(tr.status, 201) << tr.raw;
    // Till: register, session, sale, closing with a variance (manager notification).
    auto reg = call("POST", "/api/v1/pos/registers", {{"code", "c1"}, {"name", "Caisse 1"}}, t.token);
    ASSERT_EQ(reg.status, 201) << reg.raw;
    auto sess = call("POST", "/api/v1/pos/sessions", {{"register_id", reg.body["id"]}}, t.token);
    ASSERT_EQ(sess.status, 201) << sess.raw;
    std::string sid = sess.body["id"];
    auto sale = call("POST", "/api/v1/pos/sales",
                     {{"session_id", sid}, {"lines", {{{"product_id", p}, {"quantity", "1"}}}},
                      {"payments", {{{"method", "cash"}, {"amount", "1500.00"}}}}},
                     t.token);
    ASSERT_EQ(sale.status, 201) << sale.raw;
    std::string receipt = sale.body["receipt"]["id"];
    ASSERT_EQ(call("POST", "/api/v1/pos/sessions/" + sid + "/close", {{"counted", {{"cash", "1400.00"}}}}, t.token).status, 200);

    auto before = h.get("/api/v1/me", t.token);
    std::string rightsBefore = before.header("X-Nhill-Rights");
    ASSERT_FALSE(rightsBefore.empty());
    EXPECT_EQ(before.body["rights_version"], rightsBefore);

    // The plan changes: only quotes, the catalogue and the main warehouse remain.
    putOn(t, makePlan({"sales.invoices", "pos", "purchases", "expenses", "stock.multi_warehouse"}));
    auto after = h.get("/api/v1/me", t.token);
    EXPECT_NE(after.header("X-Nhill-Rights"), rightsBefore) << "clients reload their rights";
    EXPECT_EQ(after.body["rights_version"], after.header("X-Nhill-Rights"));

    // Documents: only the quote exists for the user.
    auto docs = h.get("/api/v1/sales-documents", t.token);
    ASSERT_EQ(docs.status, 200);
    EXPECT_EQ(docs.body["total"], 1);
    for (const auto& d : docs.body["items"]) EXPECT_EQ(d["doc_type"], "quote");
    EXPECT_EQ(h.get("/api/v1/sales-documents?type=invoice", t.token).body["total"], 0);
    for (const std::string& id : {inv, receipt}) {
        EXPECT_EQ(h.get("/api/v1/sales-documents/" + id, t.token).status, 404);
        EXPECT_EQ(h.get("/api/v1/sales-documents/" + id + "/pdf", t.token).status, 404);
    }
    EXPECT_EQ(call("POST", "/api/v1/sales-documents/" + inv + "/credit-note", {{"reason", "Retour client"}}, t.token).status, 404);
    EXPECT_EQ(call("POST", "/api/v1/sales-documents/" + q + "/convert", {{"to", "invoice"}}, t.token).status, 403);
    // Payments of invoices, receipts, purchases and expenses.
    EXPECT_EQ(h.get("/api/v1/payments", t.token).body["items"].size(), 0u);
    // Dashboard and reports.
    auto dash = h.get("/api/v1/reports/dashboard?from=2026-01-01&to=" + today(), t.token);
    ASSERT_EQ(dash.status, 200) << dash.raw;
    EXPECT_TRUE(dash.body["revenue"].empty()) << dash.body["revenue"].dump();
    EXPECT_TRUE(dash.body["expenses"].empty());
    EXPECT_TRUE(dash.body["receivables"].empty());
    EXPECT_TRUE(dash.body["payables"].empty() || dash.body["payables"].is_null()) << dash.body["payables"].dump();
    EXPECT_EQ(h.get("/api/v1/reports/sales", t.token).status, 403);
    EXPECT_EQ(h.get("/api/v1/reports/payables", t.token).status, 403);
    auto payRep = h.get("/api/v1/reports/payments?from=2026-01-01&to=" + today(), t.token);
    ASSERT_EQ(payRep.status, 200) << payRep.raw;
    EXPECT_EQ(payRep.body["rows"].size(), 0u) << payRep.body["rows"].dump();
    // Stock: main warehouse only, manual movements only.
    auto ws = h.get("/api/v1/warehouses", t.token).body["items"];
    ASSERT_EQ(ws.size(), 1u);
    EXPECT_TRUE(ws[0]["is_default"].get<bool>());
    EXPECT_EQ(h.get("/api/v1/stock?warehouse_id=" + w2.body["id"].get<std::string>(), t.token).status, 404);
    auto moves = items("/api/v1/stock/movements", t.token);
    EXPECT_FALSE(moves.empty()) << "the opening stock stays visible";
    for (const auto& m : moves) EXPECT_EQ(m["source_type"], "manual") << m.dump();
    // Activity log, notifications, parties.
    auto acts = items("/api/v1/activity?limit=200", t.token);
    EXPECT_FALSE(acts.empty());
    for (const auto& a : acts) {
        std::string action = a["action"];
        EXPECT_NE(action.rfind("purchase.", 0), 0u) << action;
        EXPECT_NE(action.rfind("expense", 0), 0u) << action;
        EXPECT_NE(action.rfind("pos.", 0), 0u) << action;
        EXPECT_NE(action, "stock.transferred");
        EXPECT_NE(action, "warehouse.created");
        if (a["entity"] == "sales_document") {
            EXPECT_NE(a["entity_id"], inv) << action;
        }
        if (a["entity"] == "party") {
            EXPECT_NE(a["entity_id"], supplier) << action;
        }
    }
    for (const auto& n : items("/api/v1/notifications", t.token)) EXPECT_NE(n["kind"], "pos_cash_variance");
    EXPECT_EQ(h.get("/api/v1/parties/" + supplier, t.token).status, 404);
    auto cust = h.get("/api/v1/parties/" + c, t.token);
    ASSERT_EQ(cust.status, 200);
    EXPECT_FALSE(cust.body["balances"].contains("receivable"));
    EXPECT_EQ(cust.body["purchases"].size(), 0u);
    for (const auto& d : cust.body["documents"]) EXPECT_EQ(d["doc_type"], "quote");
    auto custs = items("/api/v1/customers", t.token);
    ASSERT_FALSE(custs.empty());  // the customer and the till's walk-in customer
    for (const auto& x : custs) EXPECT_TRUE(x["balance"].empty()) << x.dump();

    // Nothing was deleted: back on the full plan, everything is there again.
    putOn(t, full);
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + inv, t.token).status, 200);
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + receipt, t.token).status, 200);
    EXPECT_EQ(h.get("/api/v1/purchases/" + poId, t.token).status, 200);
    // Invoice, purchase, expense and till payments.
    EXPECT_EQ(h.get("/api/v1/payments", t.token).body["items"].size(), 4u);
    bool variance = false;
    for (const auto& n : items("/api/v1/notifications", t.token)) variance |= n["kind"] == "pos_cash_variance";
    EXPECT_TRUE(variance);
}

// ------------------------------------------------------------------ limits

TEST(Plans, LimitsAreEnforcedByTheServer) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    putOn(t, makePlan({}, {{"catalog.products.max", 2}, {"documents.month.max", 1}, {"stock.warehouses.max", 2},
                           {"users.max", 2}, {"storage.mb.max", 1}, {"pos.registers.max", 1}}));
    // Products (active ones).
    std::string p1 = createProduct(t, uniq("A"), "10.00", false);
    std::string p2 = createProduct(t, uniq("B"), "10.00", false);
    auto third = h.post("/api/v1/products", {{"sku", uniq("C")}, {"name", "Trop"}, {"sale_price", "1.00"}}, t.token);
    EXPECT_EQ(third.status, 409);
    EXPECT_EQ(third.body["error"]["code"], "plan_limit_reached");
    EXPECT_EQ(third.body["error"]["details"]["limit_key"], "catalog.products.max");
    EXPECT_EQ(third.body["error"]["details"]["limit"], 2);
    EXPECT_EQ(third.body["error"]["details"]["used"], 2);
    ASSERT_EQ(h.request("PATCH", "/api/v1/products/" + p2, {{"active", false}}, t.token).status, 200);
    createProduct(t, uniq("D"), "10.00", false);
    EXPECT_EQ(h.request("PATCH", "/api/v1/products/" + p2, {{"active", true}}, t.token).status, 409);  // reactivation counts
    auto imp = h.request("POST", "/api/v1/products/import", nullptr, t.token, "", "text/csv", "sku;name\nNEW-1;Nouveau\n");
    EXPECT_EQ(imp.status, 409);
    EXPECT_EQ(imp.body["error"]["details"]["adding"], 1);
    EXPECT_EQ(h.get("/api/v1/products?search=NEW-1", t.token).body["total"], 0) << "all or nothing";
    std::string sku1 = h.get("/api/v1/products/" + p1, t.token).body["sku"];
    EXPECT_EQ(h.request("POST", "/api/v1/products/import", nullptr, t.token, "", "text/csv", "sku;name\n" + sku1 + ";Renommé\n")
                  .status,
              200);  // updates only

    // Documents issued in the month; a retried issue (lost response) is not counted twice.
    std::string c = createCustomer(t);
    json line = json::array({{{"description", "Service"}, {"quantity", "1"}, {"unit_price", "100.00"}}});
    std::string inv1 = draftInvoice(t, c, line);
    std::string key = newKey();
    auto i1 = issue(t, inv1, key);
    ASSERT_EQ(i1.status, 200) << i1.raw;
    auto replay = issue(t, inv1, key);
    EXPECT_EQ(replay.status, 200) << replay.raw;
    EXPECT_EQ(replay.body["number"], i1.body["number"]);
    std::string inv2 = draftInvoice(t, c, line);  // drafts are free
    auto i2 = issue(t, inv2);
    EXPECT_EQ(i2.status, 409);
    EXPECT_EQ(i2.body["error"]["details"]["limit_key"], "documents.month.max");
    auto cn = call("POST", "/api/v1/sales-documents/" + inv1 + "/credit-note", {{"reason", "Erreur de prix"}}, t.token);
    ASSERT_EQ(cn.status, 201) << cn.raw;
    EXPECT_EQ(issue(t, cn.body["id"]).status, 200) << "credit notes are never blocked";

    // Warehouses (main one included), registers, users.
    EXPECT_EQ(call("POST", "/api/v1/warehouses", {{"name", "Dépôt 2"}}, t.token).status, 201);
    EXPECT_EQ(call("POST", "/api/v1/warehouses", {{"name", "Dépôt 3"}}, t.token).status, 409);
    EXPECT_EQ(call("POST", "/api/v1/pos/registers", {{"code", "c1"}, {"name", "Caisse 1"}}, t.token).status, 201);
    auto reg2 = call("POST", "/api/v1/pos/registers", {{"code", "c2"}, {"name", "Caisse 2"}}, t.token);
    EXPECT_EQ(reg2.status, 409);
    EXPECT_EQ(reg2.body["error"]["code"], "plan_register_limit");
    EXPECT_EQ(call("POST", "/api/v1/invitations", {{"email", uniq("u") + "@example.cd"}, {"role", "sales"}}, t.token).status, 201);
    auto inv3 = call("POST", "/api/v1/invitations", {{"email", uniq("u") + "@example.cd"}, {"role", "sales"}}, t.token);
    EXPECT_EQ(inv3.status, 409);
    EXPECT_EQ(inv3.body["error"]["code"], "user_limit_reached");

    // Stored files.
    auto exp = call("POST", "/api/v1/expenses", {{"description", "Achat"}, {"amount", "5.00"}, {"currency", "CDF"}}, t.token);
    ASSERT_EQ(exp.status, 201);
    std::string pdf = "%PDF-1.4\n" + std::string(1200 * 1024, 'x');
    auto up = h.request("PUT", "/api/v1/expenses/" + exp.body["id"].get<std::string>() + "/receipt", nullptr, t.token, "",
                        "application/pdf", pdf);
    EXPECT_EQ(up.status, 409);
    EXPECT_EQ(up.body["error"]["details"]["limit_key"], "storage.mb.max");

    // Usage shown to the applications.
    auto ent = h.get("/api/v1/entitlements", t.token).body;
    EXPECT_EQ(ent["limits"]["catalog.products.max"]["max"], 2);
    EXPECT_EQ(ent["limits"]["catalog.products.max"]["used"], 2);
    EXPECT_EQ(ent["limits"]["documents.month.max"]["used"], 1);
    EXPECT_EQ(ent["limits"]["pos.registers.max"]["used"], 1);
}

TEST(Plans, ConcurrentRequestsCannotExceedALimit) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    putOn(t, makePlan({}, {{"catalog.products.max", 1}}));
    std::vector<std::future<int>> calls;
    for (int i = 0; i < 6; ++i)
        calls.push_back(std::async(std::launch::async, [&, i] {
            return h.post("/api/v1/products", {{"sku", "CONC-" + std::to_string(i)}, {"name", "Concurrent"}, {"sale_price", "1.00"}},
                          t.token)
                .status;
        }));
    int created = 0, refused = 0;
    for (auto& f : calls) {
        int s = f.get();
        created += s == 201;
        refused += s == 409;
    }
    EXPECT_EQ(created, 1);
    EXPECT_EQ(refused, 5);
    EXPECT_EQ(h.get("/api/v1/products", t.token).body["total"], 1);
}

// ------------------------------------------------------------------ access over time

TEST(Plans, ExpiryAndSuspensionLimitWritesNotReads) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    putOn(t, makePlan({}));
    std::string viewer = addMember(t, "viewer");
    auto newProduct = [&] {
        return h.post("/api/v1/products", {{"sku", uniq("X")}, {"name", "Produit"}, {"sale_price", "1.00"}}, t.token);
    };
    // Grace period: the due date passed, service maintained.
    h.su("UPDATE subscriptions SET status = 'past_due', period_start = now() - interval '31 days', "
         "period_end = now() - interval '1 day', grace_until = now() + interval '2 days' WHERE establishment_id = '" + t.id + "'");
    EXPECT_EQ(newProduct().status, 201);
    EXPECT_EQ(h.get("/api/v1/entitlements", t.token).body["access"]["reason"], "grace");
    // End of grace (computed from the dates, even before the job runs): read and export only.
    h.su("UPDATE subscriptions SET grace_until = now() - interval '1 minute' WHERE establishment_id = '" + t.id + "'");
    auto w = newProduct();
    EXPECT_EQ(w.status, 402);
    EXPECT_EQ(w.body["error"]["code"], "subscription_suspended");
    EXPECT_EQ(w.body["error"]["details"]["reason"], "expired");
    EXPECT_EQ(h.get("/api/v1/products", t.token).status, 200);
    EXPECT_EQ(h.get("/api/v1/products/export.csv", t.token).status, 200);
    auto me = h.get("/api/v1/me", viewer).body;
    EXPECT_EQ(me["subscription"]["access"]["state"], "read_only");
    EXPECT_FALSE(me["subscription"]["write_allowed"].get<bool>());
    // Extension from the console restores writes at once.
    ASSERT_EQ(admin("POST", "/api/v1/admin/establishments/" + t.id + "/subscription",
                    {{"action", "extend"}, {"days", 30}, {"reason", "Paiement reçu en agence"}})
                  .status,
              200);
    EXPECT_EQ(newProduct().status, 201);
    // Administrative suspension, then reactivation.
    ASSERT_EQ(admin("POST", "/api/v1/admin/establishments/" + t.id + "/subscription",
                    {{"action", "suspend"}, {"reason", "Vérification du compte"}})
                  .status,
              200);
    auto s = newProduct();
    EXPECT_EQ(s.status, 402);
    EXPECT_EQ(s.body["error"]["details"]["reason"], "admin");
    ASSERT_EQ(admin("POST", "/api/v1/admin/establishments/" + t.id + "/subscription",
                    {{"action", "reactivate"}, {"reason", "Vérification terminée"}})
                  .status,
              200);
    EXPECT_EQ(newProduct().status, 201);
    // History of the administrative actions.
    auto ev = h.su("SELECT kind FROM subscription_events WHERE establishment_id = '" + t.id + "' ORDER BY created_at");
    std::set<std::string> kinds;
    for (const auto& r : ev) kinds.insert(r[0].c_str());
    for (const char* k : {"admin_assign_plan", "admin_extend", "admin_suspend", "admin_reactivate"}) EXPECT_TRUE(kinds.count(k)) << k;
}

TEST(Plans, EndOfGraceMovesToTheFallbackPlanWithItsRights) {
    auto& h = Harness::get();
    std::string fallback = makePlan({"pos", "purchases"}, json::object(), "free");
    std::string paid = makePlan({});
    h.su("UPDATE plans SET expiry_fallback_plan_id = '" + fallback + "' WHERE id = '" + paid + "'");
    Tenant t = createTenant("company");
    putOn(t, paid);
    EXPECT_EQ(h.get("/api/v1/pos/registers", t.token).status, 200);
    h.su("UPDATE subscriptions SET status = 'past_due', period_start = now() - interval '40 days', "
         "period_end = now() - interval '10 days', grace_until = now() - interval '1 hour' WHERE establishment_id = '" + t.id + "'");
    nh::jobs::subscriptionLifecycle(h.env());
    auto sub = h.su("SELECT plan_id, status, open_ended FROM subscriptions WHERE establishment_id = '" + t.id + "'");
    EXPECT_EQ(std::string(sub[0][0].c_str()), fallback);
    EXPECT_EQ(std::string(sub[0][1].c_str()), "active");
    EXPECT_TRUE(sub[0][2].as<bool>());
    auto r = h.get("/api/v1/pos/registers", t.token);
    EXPECT_EQ(r.status, 403);
    EXPECT_EQ(r.body["error"]["code"], "feature_not_included");
    EXPECT_EQ(h.post("/api/v1/parties", {{"is_customer", true}, {"name", "Toujours actif"}}, t.token).status, 201);
    bool changed = false;
    auto notes = h.get("/api/v1/notifications", t.token);
    for (const auto& n : notes.body["items"]) changed |= n["kind"] == "plan_changed";
    EXPECT_TRUE(changed) << notes.raw;
}

TEST(Plans, AssignmentRuleGivesItsPlanToANewEstablishment) {
    auto& h = Harness::get();
    std::string plan = makePlan({"pos"}, {{"users.max", 1}}, "free");
    auto rule = admin("POST", "/api/v1/admin/plan-rules",
                      {{"priority", 1}, {"audience", "individual"}, {"country_code", "CD"}, {"plan_id", plan},
                       {"mode", "grant"}, {"description", "Entrepreneurs individuels : formule gratuite"}});
    ASSERT_EQ(rule.status, 201) << rule.raw;
    std::string ruleId = rule.body["id"];
    struct Cleanup {
        std::string id;
        ~Cleanup() { admin("DELETE", "/api/v1/admin/plan-rules/" + id); }
    } cleanup{ruleId};
    Tenant t = createTenant("individual");
    auto sub = h.su("SELECT plan_id, status, open_ended, assignment_rule_id FROM subscriptions WHERE establishment_id = '" +
                    t.id + "'");
    EXPECT_EQ(std::string(sub[0][0].c_str()), plan);
    EXPECT_EQ(std::string(sub[0][1].c_str()), "active");
    EXPECT_TRUE(sub[0][2].as<bool>());
    EXPECT_EQ(std::string(sub[0][3].c_str()), ruleId);
    EXPECT_FALSE(permissions(t.token).count("feature.pos"));
    EXPECT_EQ(h.get("/api/v1/pos/config", t.token).status, 403);
    // A company is not concerned by the rule.
    Tenant other = createTenant("company");
    auto osub = h.su("SELECT plan_id IS NULL FROM subscriptions WHERE establishment_id = '" + other.id + "'");
    EXPECT_TRUE(osub[0][0].as<bool>());
}

// ------------------------------------------------------------------ full export (option 1)

TEST(Plans, OwnerExportsAllDataWhateverThePlanAndTheSubscription) {
    auto& h = Harness::get();
    Tenant t = createTenant("company");
    putOn(t, makePlan({}));
    std::string c = createCustomer(t, "Client Export");
    std::string inv = draftInvoice(t, c, json::array({{{"description", "Prestation"}, {"quantity", "1"}, {"unit_price", "250.00"}}}));
    auto issued = issue(t, inv);
    ASSERT_EQ(issued.status, 200);
    std::string number = issued.body["number"];
    ASSERT_EQ(call("POST", "/api/v1/expenses", {{"description", "Carburant groupe"}, {"amount", "40.00"}, {"currency", "CDF"}}, t.token)
                  .status,
              201);
    std::string sales = addMember(t, "sales");
    // Plan without invoices nor expenses, subscription expired: the owner still exports everything.
    putOn(t, makePlan({"sales.invoices", "expenses"}));
    h.su("UPDATE subscriptions SET status = 'past_due', period_start = now() - interval '50 days', "
         "period_end = now() - interval '20 days', grace_until = now() - interval '1 day' WHERE establishment_id = '" + t.id + "'");
    EXPECT_EQ(h.get("/api/v1/sales-documents/" + inv, t.token).status, 404);  // hidden in the applications
    auto r = h.get("/api/v1/establishment/export", t.token);
    ASSERT_EQ(r.status, 200) << r.raw.substr(0, 300);
    EXPECT_NE(r.header("Content-Type").find("application/zip"), std::string::npos);
    EXPECT_NE(r.header("Content-Disposition").find(".zip"), std::string::npos);
    if (const char* dir = std::getenv("NH_SAMPLES_DIR")) {  // checked with standard tools (unzip -t)
        std::ofstream(std::string(dir) + "/export-sample.zip", std::ios::binary) << r.raw;
    }
    auto entries = nh::zip::read(r.raw);
    std::map<std::string, std::string> files(entries.begin(), entries.end());
    ASSERT_TRUE(files.count("README.txt"));
    ASSERT_TRUE(files.count("manifest.json"));
    json manifest = json::parse(files["manifest.json"]);
    EXPECT_EQ(manifest["format"], "nhill-export");
    EXPECT_EQ(manifest["establishment"]["id"], t.id);
    // Every file listed with its SHA-256.
    for (const auto& f : manifest["files"]) {
        ASSERT_TRUE(files.count(f["path"].get<std::string>())) << f["path"];
        EXPECT_EQ(nh::crypto::sha256Hex(files[f["path"].get<std::string>()]), f["sha256"]);
    }
    json docs = json::parse(files["json/sales_documents.json"]);
    bool found = false;
    for (const auto& d : docs) found |= d.value("number", json(nullptr)) == number;
    EXPECT_TRUE(found) << "invoice of a feature outside the plan is in the export";
    EXPECT_NE(files["csv/expenses.csv"].find("Carburant groupe"), std::string::npos);
    EXPECT_NE(files["csv/parties.csv"].find("Client Export"), std::string::npos);
    EXPECT_EQ(files["csv/sales_documents.csv"].rfind("\xEF\xBB\xBF", 0), 0u);
    // No secret, no hash, no other establishment.
    Tenant other = createTenant("company");
    createCustomer(other, "Client Voisin");
    for (const auto& [name, content] : files) {
        EXPECT_EQ(content.find("$argon2"), std::string::npos) << name;
        EXPECT_EQ(content.find("password_hash"), std::string::npos) << name;
        EXPECT_EQ(content.find("token_hash"), std::string::npos) << name;
        EXPECT_EQ(content.find(other.id), std::string::npos) << name;
    }
    auto again = nh::zip::read(h.get("/api/v1/establishment/export", other.token).raw);
    for (const auto& [name, content] : again) EXPECT_EQ(content.find("Client Export"), std::string::npos) << name;
    // Owner only; audited.
    auto refused = h.get("/api/v1/establishment/export", sales);
    EXPECT_EQ(refused.status, 403);
    EXPECT_EQ(refused.body["error"]["code"], "owner_only");
    EXPECT_GE(h.su("SELECT count(*) FROM audit_log WHERE establishment_id = '" + t.id + "' AND action = 'establishment.exported'")[0][0]
                  .as<int>(),
              1);
}
