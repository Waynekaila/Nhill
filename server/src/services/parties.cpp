// Customers and suppliers (one "party" may be both).
#include "services/entitlements.hpp"
#include "services/services.hpp"

namespace nh {

namespace {

constexpr const char* kPartySelect =
    "SELECT p.id, p.is_customer, p.is_supplier, p.kind, p.name, p.email, p.phone, p.address, p.city, p.country_code, "
    "p.tax_ids, p.payment_terms_days, p.notes, p.active, nh_iso(p.created_at) FROM parties p ";

json partyJson(const pqxx::row& r) {
    auto nullable = [](const pqxx::field& f) { return f.is_null() ? json(nullptr) : json(db::str(f)); };
    return {{"id", db::str(r[0])},
            {"is_customer", db::boolean(r[1])},
            {"is_supplier", db::boolean(r[2])},
            {"kind", db::str(r[3])},
            {"name", db::str(r[4])},
            {"email", nullable(r[5])},
            {"phone", nullable(r[6])},
            {"address", db::str(r[7])},
            {"city", db::str(r[8])},
            {"country_code", nullable(r[9])},
            {"tax_ids", json::parse(db::str(r[10]))},
            {"payment_terms_days", r[11].as<int>()},
            {"notes", db::str(r[12])},
            {"active", db::boolean(r[13])},
            {"created_at", db::str(r[14])}};
}

json taxIds(const json& b) {
    json out = json::object();
    if (!b.contains("tax_ids") || b["tax_ids"].is_null()) return out;
    if (!b["tax_ids"].is_object()) throw invalid("tax_ids", "must_be_object");
    for (auto it = b["tax_ids"].begin(); it != b["tax_ids"].end(); ++it) {
        if (it.key().size() > 24) throw invalid("tax_ids", "invalid_key");
        auto v = in::optStr(b["tax_ids"], it.key(), 64);
        if (v) out[it.key()] = *v;
    }
    return out;
}

bool has(const Actor& a, const char* feature) { return a.features.count(feature) > 0; }

// Suppliers exist for purchases and expenses; without either they are hidden.
bool suppliersVisible(const Actor& a) {
    return has(a, entitlements::key::kPurchases) || has(a, entitlements::key::kExpenses);
}

// Balances per currency: open receivables (customer, invoices) and payables (supplier,
// purchases), each only when the plan includes the feature it comes from.
json balances(db::Tx& tx, const Actor& a, const std::string& partyId) {
    json out = json::object();
    if (has(a, entitlements::key::kInvoices))
    for (const auto& r : tx.q(
             "SELECT currency, sum(total_minor - paid_minor - credited_minor) FROM sales_documents "
             "WHERE party_id = $1 AND doc_type = 'invoice' AND status IN ('issued','partially_paid','paid','credited') "
             "GROUP BY currency",
             partyId))
        out["receivable"][db::str(r[0])] = money(r[1].as<int64_t>(), db::str(r[0]));
    if (has(a, entitlements::key::kPurchases))
    for (const auto& r : tx.q(
             "SELECT currency, sum(total_minor - paid_minor) FROM purchases WHERE supplier_id = $1 AND status = 'received' "
             "GROUP BY currency",
             partyId))
        out["payable"][db::str(r[0])] = money(r[1].as<int64_t>(), db::str(r[0]));
    return out;
}

}  // namespace

void registerPartyRoutes(App& app, Env& env) {
    auto list = [&env](Ctx& ctx, const std::string& role) {
        auto pg = page(ctx);
        auto search = ctx.query("search");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            std::string where = std::string("WHERE p.") + (role == "customer" ? "is_customer" : "is_supplier") +
                                " AND ($1::text IS NULL OR p.name ILIKE nh_like_contains($1) OR p.email ILIKE nh_like_contains($1) "
                                "OR p.phone LIKE nh_like_contains($1)) AND ($2 = 'all' OR p.active) ";
            std::string active = ctx.query("active").value_or("1") == "all" ? "all" : "1";
            int total = tx.one("SELECT count(*) FROM parties p " + where, search, active)[0].as<int>();
            json items = json::array();
            bool balanceVisible = role == "customer" ? has(ctx.actor, entitlements::key::kInvoices)
                                                     : has(ctx.actor, entitlements::key::kPurchases);
            std::string balanceCol =
                !balanceVisible ? "'{}'::json"
                : role == "customer"
                    ? "(SELECT COALESCE(json_object_agg(currency, s), '{}') FROM (SELECT currency, "
                      "sum(total_minor - paid_minor - credited_minor) s FROM sales_documents d WHERE d.party_id = p.id AND "
                      "d.doc_type = 'invoice' AND d.status <> 'draft' GROUP BY currency) x)"
                    : "(SELECT COALESCE(json_object_agg(currency, s), '{}') FROM (SELECT currency, sum(total_minor - "
                      "paid_minor) s FROM purchases d WHERE d.supplier_id = p.id AND d.status = 'received' GROUP BY "
                      "currency) x)";
            std::string sql = std::string(kPartySelect);
            sql.replace(sql.find(" FROM parties p "), 16, ", " + balanceCol + " FROM parties p ");
            for (const auto& r : tx.q(sql + where + "ORDER BY lower(p.name) LIMIT $3 OFFSET $4", search, active, pg.limit,
                                      pg.offset)) {
                json p = partyJson(r);
                json bal = json::object();
                json raw = json::parse(db::str(r[15]));
                for (auto it = raw.begin(); it != raw.end(); ++it)
                    bal[it.key()] = money(it.value().get<int64_t>(), it.key());
                p["balance"] = bal;
                items.push_back(p);
            }
            return {{"items", items}, {"total", total}};
        });
        ctx.json(200, out);
    };
    app.get("/api/v1/customers", {Access::User}, [list](Ctx& ctx) { list(ctx, "customer"); });
    app.get("/api/v1/suppliers", {Access::User, "", false, {}, "purchases|expenses"},
            [list](Ctx& ctx) { list(ctx, "supplier"); });

    app.get("/api/v1/parties/:id", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "party");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            json p = partyJson(tx.one(std::string(kPartySelect) + "WHERE p.id = $1", id));
            // A supplier-only party does not exist for a plan without purchases or expenses.
            if (!p["is_customer"].get<bool>() && !suppliersVisible(ctx.actor)) throw notFound("party");
            p["balances"] = balances(tx, ctx.actor, id);
            json docs = json::array();
            for (const auto& r : tx.q(
                     "SELECT d.id, d.doc_type, d.status, d.number, nh_iso(d.created_at), d.issue_date::text, d.currency, "
                     "d.total_minor, d.paid_minor, d.credited_minor FROM sales_documents d WHERE d.party_id = $1 AND " +
                         entitlements::docVisibleSql("d", 2) + " ORDER BY d.created_at DESC LIMIT 50",
                     id, entitlements::docTypesArray(ctx.actor)))
                docs.push_back({{"id", db::str(r[0])}, {"doc_type", db::str(r[1])}, {"status", db::str(r[2])},
                                {"number", r[3].is_null() ? json(nullptr) : json(db::str(r[3]))},
                                {"issue_date", db::str(r[5])}, {"currency", db::str(r[6])},
                                {"total", money(r[7].as<int64_t>(), db::str(r[6]))},
                                {"balance", money(r[7].as<int64_t>() - r[8].as<int64_t>() - r[9].as<int64_t>(),
                                                  db::str(r[6]))}});
            p["documents"] = docs;
            json purchases = json::array();
            if (has(ctx.actor, entitlements::key::kPurchases))
            for (const auto& r : tx.q("SELECT id, status, supplier_reference, purchase_date::text, currency, total_minor, "
                                      "paid_minor FROM purchases WHERE supplier_id = $1 ORDER BY purchase_date DESC LIMIT 50",
                                      id))
                purchases.push_back({{"id", db::str(r[0])}, {"status", db::str(r[1])},
                                     {"supplier_reference", db::str(r[2])}, {"purchase_date", db::str(r[3])},
                                     {"currency", db::str(r[4])}, {"total", money(r[5].as<int64_t>(), db::str(r[4]))},
                                     {"balance", money(r[5].as<int64_t>() - r[6].as<int64_t>(), db::str(r[4]))}});
            p["purchases"] = purchases;
            return p;
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/parties", {Access::User, "parties.write", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        bool isCustomer = in::optBool(b, "is_customer", false);
        bool isSupplier = in::optBool(b, "is_supplier", false);
        if (!isCustomer && !isSupplier) throw invalid("is_customer", "customer_or_supplier_required");
        // Suppliers are a purchasing and expenses concern: the 'sales' role may only create
        // customers. The plan must include purchases or expenses.
        if (isSupplier && !suppliersVisible(ctx.actor)) entitlements::requireFeature(ctx.actor, entitlements::key::kPurchases);
        if (isSupplier && !App::allowed(ctx.actor, "purchases.write") && !App::allowed(ctx.actor, "expenses.write"))
            throw forbidden("permission_denied");
        std::string kind = in::optEnum(b, "kind", {"individual", "company"}).value_or("company");
        std::string name = in::reqStr(b, "name", 200);
        auto email = in::optEmail(b, "email");
        auto phone = in::optPhone(b, "phone");
        std::string address = in::optStr(b, "address", 300).value_or("");
        std::string city = in::optStr(b, "city", 100).value_or("");
        auto country = in::optStr(b, "country_code", 2);
        int terms = in::optInt(b, "payment_terms_days", 0, 0, 365);
        std::string notes = in::optStr(b, "notes", 2000).value_or("");
        json ids = taxIds(b);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto r = tx.one(
                "INSERT INTO parties (establishment_id, is_customer, is_supplier, kind, name, email, phone, address, city, "
                "country_code, tax_ids, payment_terms_days, notes) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11::jsonb,$12,$13) "
                "RETURNING id",
                ctx.actor.establishmentId, isCustomer, isSupplier, kind, name, email, phone, address, city, country,
                ids.dump(), terms, notes);
            std::string id = db::str(r[0]);
            audit(tx, ctx.actor, "party.created", "party", id, {{"name", name}});
            return partyJson(tx.one(std::string(kPartySelect) + "WHERE p.id = $1", id));
        });
        ctx.json(201, out);
    });

    app.patch("/api/v1/parties/:id", {Access::User, "parties.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "party");
        const auto& b = ctx.body();
        std::optional<bool> isCustomer, isSupplier, active;
        if (b.contains("is_customer")) isCustomer = in::optBool(b, "is_customer", true);
        if (b.contains("is_supplier")) isSupplier = in::optBool(b, "is_supplier", false);
        if (b.contains("active")) active = in::optBool(b, "active", true);
        auto kind = in::optEnum(b, "kind", {"individual", "company"});
        auto name = in::optStr(b, "name", 200);
        auto email = in::optEmail(b, "email");
        auto phone = in::optPhone(b, "phone");
        auto address = in::optStr(b, "address", 300);
        auto city = in::optStr(b, "city", 100);
        auto country = in::optStr(b, "country_code", 2);
        std::optional<int> terms;
        if (b.contains("payment_terms_days")) terms = in::optInt(b, "payment_terms_days", 0, 0, 365);
        auto notes = in::optStr(b, "notes", 2000);
        std::optional<std::string> ids;
        if (b.contains("tax_ids")) ids = taxIds(b).dump();
        if (isSupplier && *isSupplier && !suppliersVisible(ctx.actor))
            entitlements::requireFeature(ctx.actor, entitlements::key::kPurchases);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto cur = tx.opt("SELECT is_customer FROM parties WHERE id = $1", id);
            if (!cur) throw notFound("party");
            if (!db::boolean((*cur)[0]) && !suppliersVisible(ctx.actor)) throw notFound("party");
            tx.one("UPDATE parties SET is_customer = COALESCE($2, is_customer), is_supplier = COALESCE($3, is_supplier), "
                   "kind = COALESCE($4, kind), name = COALESCE($5, name), email = COALESCE($6, email), "
                   "phone = COALESCE($7, phone), address = COALESCE($8, address), city = COALESCE($9, city), "
                   "country_code = COALESCE($10, country_code), tax_ids = COALESCE($11::jsonb, tax_ids), "
                   "payment_terms_days = COALESCE($12, payment_terms_days), notes = COALESCE($13, notes), "
                   "active = COALESCE($14, active) WHERE id = $1 RETURNING id",
                   id, isCustomer, isSupplier, kind, name, email, phone, address, city, country, ids, terms, notes, active);
            audit(tx, ctx.actor, "party.updated", "party", id, b);
            return partyJson(tx.one(std::string(kPartySelect) + "WHERE p.id = $1", id));
        });
        ctx.json(200, out);
    });
}

}  // namespace nh
