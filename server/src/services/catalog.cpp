// Catalogue: tax rates, categories, products and services (with CSV import/export).
#include "nhill/money.hpp"
#include "services/entitlements.hpp"
#include "services/services.hpp"
#include "util/csv.hpp"

namespace nh {

namespace {

constexpr const char* kProductSelect =
    "SELECT p.id, p.kind, p.sku, p.barcode, p.name, p.description, p.category_id, c.name, p.unit, p.currency, "
    "p.purchase_price_minor, p.sale_price_minor, p.tax_rate_id, t.rate_bp, t.code, p.track_stock, "
    "p.reorder_level_milli, p.active, COALESCE((SELECT sum(quantity_milli) FROM stock_levels l "
    "WHERE l.product_id = p.id), 0), nh_iso(p.created_at), nh_iso(p.updated_at) "
    "FROM products p LEFT JOIN product_categories c ON c.id = p.category_id "
    "LEFT JOIN tax_rates t ON t.id = p.tax_rate_id ";

json productJson(const pqxx::row& r) {
    auto nullable = [](const pqxx::field& f) { return f.is_null() ? json(nullptr) : json(db::str(f)); };
    std::string cur = db::str(r[9]);
    int64_t stock = r[18].as<int64_t>();
    int64_t reorder = r[16].as<int64_t>();
    bool track = db::boolean(r[15]);
    return {{"id", db::str(r[0])},
            {"kind", db::str(r[1])},
            {"sku", db::str(r[2])},
            {"barcode", nullable(r[3])},
            {"name", db::str(r[4])},
            {"description", db::str(r[5])},
            {"category_id", nullable(r[6])},
            {"category_name", nullable(r[7])},
            {"unit", db::str(r[8])},
            {"currency", cur},
            {"purchase_price", money(r[10].as<int64_t>(), cur)},
            {"sale_price", money(r[11].as<int64_t>(), cur)},
            {"tax_rate_id", nullable(r[12])},
            {"tax_rate_bp", r[13].is_null() ? json(nullptr) : json(r[13].as<int>())},
            {"tax_code", nullable(r[14])},
            {"track_stock", track},
            {"reorder_level", qty(reorder)},
            {"active", db::boolean(r[17])},
            {"stock_quantity", track ? json(qty(stock)) : json(nullptr)},
            {"low_stock", track && stock <= reorder && reorder > 0},
            {"created_at", db::str(r[19])},
            {"updated_at", db::str(r[20])}};
}

json taxJson(const pqxx::row& r) {
    return {{"id", db::str(r[0])},       {"code", db::str(r[1])},       {"label", db::str(r[2])},
            {"rate_bp", r[3].as<int>()}, {"kind", db::str(r[4])},       {"is_default", db::boolean(r[5])},
            {"active", db::boolean(r[6])}};
}

struct ProductInput {
    std::optional<std::string> kind, sku, barcode, name, description, categoryId, unit, currency, taxRateId;
    std::optional<int64_t> purchase, sale, reorder;
    std::optional<bool> track, active;
};

ProductInput parseProduct(const json& b, const std::string& currencyForPrices) {
    ProductInput p;
    p.kind = in::optEnum(b, "kind", {"good", "service"});
    p.sku = in::optStr(b, "sku", 40);
    p.name = in::optStr(b, "name", 200);
    p.description = in::optStr(b, "description", 2000);
    p.categoryId = in::optUuid(b, "category_id");
    p.unit = in::optStr(b, "unit", 16);
    p.taxRateId = in::optUuid(b, "tax_rate_id");
    if (auto bc = in::optStr(b, "barcode", 32)) p.barcode = bc;
    p.currency = currencyForPrices;
    p.purchase = in::optMoney(b, "purchase_price", currencyForPrices);
    p.sale = in::optMoney(b, "sale_price", currencyForPrices);
    if (b.contains("reorder_level") && !b["reorder_level"].is_null()) {
        p.reorder = in::reqQty(b, "reorder_level");
        if (*p.reorder < 0) throw invalid("reorder_level", "negative");
    }
    if (b.contains("track_stock")) p.track = in::optBool(b, "track_stock", true);
    if (b.contains("active")) p.active = in::optBool(b, "active", true);
    return p;
}

std::string defaultTaxRate(db::Tx& tx) {
    auto r = tx.opt("SELECT id FROM tax_rates WHERE is_default AND active LIMIT 1");
    return r ? db::str((*r)[0]) : std::string();
}

std::string insertProduct(db::Tx& tx, const ProductInput& p, const std::string& est) {
    std::string kind = p.kind.value_or("good");
    bool track = p.track.value_or(kind == "good");
    if (kind == "service") track = false;
    std::optional<std::string> tax = p.taxRateId;
    if (!tax) {
        auto d = defaultTaxRate(tx);
        if (!d.empty()) tax = d;
    }
    return db::str(tx.one(
        "INSERT INTO products (establishment_id, kind, sku, barcode, name, description, category_id, unit, currency, "
        "purchase_price_minor, sale_price_minor, tax_rate_id, track_stock, reorder_level_milli) "
        "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14) RETURNING id",
        est, kind, *p.sku, p.barcode, *p.name, p.description.value_or(""), p.categoryId, p.unit.value_or("unit"),
        *p.currency, p.purchase.value_or(0), p.sale.value_or(0), tax, track, p.reorder.value_or(0))[0]);
}

}  // namespace

void registerCatalogRoutes(App& app, Env& env) {
    // ---------------------------------------------------------------- tax rates
    app.get("/api/v1/tax-rates", {Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT id, code, label, rate_bp, kind, is_default, active FROM tax_rates "
                                      "ORDER BY is_default DESC, rate_bp DESC, code"))
                items.push_back(taxJson(r));
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/tax-rates", {Access::User, "settings.manage", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string code = in::reqStr(b, "code", 16);
        for (auto& ch : code) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        std::string label = in::reqStr(b, "label", 80);
        int rate = in::optInt(b, "rate_bp", -1, 0, 10000);
        if (rate < 0) throw invalid("rate_bp", "required");
        std::string kind = in::optEnum(b, "kind", {"standard", "reduced", "zero", "exempt"}).value_or("standard");
        bool isDefault = in::optBool(b, "is_default", false);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            if (isDefault) tx.q("UPDATE tax_rates SET is_default = false WHERE is_default");
            auto r = tx.one("INSERT INTO tax_rates (establishment_id, code, label, rate_bp, kind, is_default) "
                            "VALUES ($1,$2,$3,$4,$5,$6) RETURNING id, code, label, rate_bp, kind, is_default, active",
                            ctx.actor.establishmentId, code, label, rate, kind, isDefault);
            audit(tx, ctx.actor, "tax_rate.created", "tax_rate", db::str(r[0]), b);
            return taxJson(r);
        });
        ctx.json(201, out);
    });

    // The rate of an existing code is immutable: documents keep their own
    // snapshot, and a new rate is a new code.
    app.patch("/api/v1/tax-rates/:id", {Access::User, "settings.manage", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"));
        const auto& b = ctx.body();
        if (b.contains("rate_bp")) throw invalid("rate_bp", "immutable_create_new_code");
        auto label = in::optStr(b, "label", 80);
        std::optional<bool> active, isDefault;
        if (b.contains("active")) active = in::optBool(b, "active", true);
        if (b.contains("is_default")) isDefault = in::optBool(b, "is_default", false);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            if (isDefault && *isDefault) tx.q("UPDATE tax_rates SET is_default = false WHERE is_default AND id <> $1", id);
            auto r = tx.one("UPDATE tax_rates SET label = COALESCE($2, label), active = COALESCE($3, active), "
                            "is_default = COALESCE($4, is_default) WHERE id = $1 "
                            "RETURNING id, code, label, rate_bp, kind, is_default, active",
                            id, label, active, isDefault);
            audit(tx, ctx.actor, "tax_rate.updated", "tax_rate", id, b);
            return taxJson(r);
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- categories
    app.get("/api/v1/product-categories", {Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT c.id, c.name, (SELECT count(*) FROM products p WHERE p.category_id = c.id) "
                                      "FROM product_categories c ORDER BY lower(c.name)"))
                items.push_back({{"id", db::str(r[0])}, {"name", db::str(r[1])}, {"product_count", r[2].as<int>()}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/product-categories", {Access::User, "catalog.write", true}, [&env](Ctx& ctx) {
        std::string name = in::reqStr(ctx.body(), "name", 80);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto r = tx.one("INSERT INTO product_categories (establishment_id, name) VALUES ($1, $2) RETURNING id",
                            ctx.actor.establishmentId, name);
            return {{"id", db::str(r[0])}, {"name", name}, {"product_count", 0}};
        });
        ctx.json(201, out);
    });

    app.del("/api/v1/product-categories/:id", {Access::User, "catalog.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"));
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            if (tx.opt("SELECT 1 FROM products WHERE category_id = $1 LIMIT 1", id)) throw conflict("category_in_use");
            if (tx.q("DELETE FROM product_categories WHERE id = $1 RETURNING id", id).empty()) throw notFound("category");
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- products
    app.get("/api/v1/products", {Access::User}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        std::optional<std::string> search = ctx.query("search");
        std::optional<std::string> category = ctx.query("category_id");
        if (category && !in::isUuid(*category)) category.reset();
        bool lowOnly = ctx.query("low_stock").value_or("") == "1";
        std::string activeFilter = ctx.query("active").value_or("1");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            std::string where =
                "WHERE ($1::text IS NULL OR p.name ILIKE '%' || $1 || '%' OR p.sku ILIKE '%' || $1 || '%' "
                "OR p.barcode = $1) AND ($2::uuid IS NULL OR p.category_id = $2) "
                "AND ($3 = 'all' OR p.active = ($3 = '1')) ";
            if (lowOnly)
                where += "AND p.track_stock AND p.reorder_level_milli > 0 AND COALESCE((SELECT sum(quantity_milli) FROM "
                         "stock_levels l WHERE l.product_id = p.id), 0) <= p.reorder_level_milli ";
            auto total = tx.one("SELECT count(*) FROM products p " + where, search, category, activeFilter)[0].as<int>();
            json items = json::array();
            for (const auto& r : tx.q(std::string(kProductSelect) + where + "ORDER BY lower(p.name) LIMIT $4 OFFSET $5",
                                      search, category, activeFilter, pg.limit, pg.offset))
                items.push_back(productJson(r));
            return {{"items", items}, {"total", total}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/products/export.csv", {Access::User}, [&env](Ctx& ctx) {
        std::string body = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            csv::Writer w;
            w.row({"sku", "name", "kind", "unit", "currency", "purchase_price", "sale_price", "tax_code", "barcode",
                   "category", "reorder_level", "track_stock", "stock_quantity", "active"});
            for (const auto& r : tx.q(std::string(kProductSelect) + "ORDER BY p.sku")) {
                json p = productJson(r);
                w.row({p["sku"], p["name"], p["kind"], p["unit"], p["currency"], p["purchase_price"], p["sale_price"],
                       p["tax_code"].is_null() ? "" : p["tax_code"].get<std::string>(),
                       p["barcode"].is_null() ? "" : p["barcode"].get<std::string>(),
                       p["category_name"].is_null() ? "" : p["category_name"].get<std::string>(), p["reorder_level"],
                       p["track_stock"].get<bool>() ? "1" : "0",
                       p["stock_quantity"].is_null() ? "" : p["stock_quantity"].get<std::string>(),
                       p["active"].get<bool>() ? "1" : "0"});
            }
            return w.str();
        });
        ctx.file(body, "text/csv; charset=utf-8", "products.csv");
    });

    app.get("/api/v1/products/:id", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "product");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return productJson(tx.one(std::string(kProductSelect) + "WHERE p.id = $1", id));
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/products", {Access::User, "catalog.write", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            std::string currency = b.contains("currency") ? in::reqCurrency(b, "currency")
                                                          : db::str(tx.one("SELECT main_currency FROM establishments "
                                                                           "WHERE id = $1", ctx.actor.establishmentId)[0]);
            ProductInput p = parseProduct(b, currency);
            if (!p.sku) throw invalid("sku", "required");
            if (!p.name) throw invalid("name", "required");
            entitlements::requireCurrency(tx, ctx.actor, currency);
            if (p.active.value_or(true)) entitlements::requireCapacity(tx, ctx.actor, entitlements::key::kProducts);
            std::string id = insertProduct(tx, p, ctx.actor.establishmentId);
            audit(tx, ctx.actor, "product.created", "product", id, {{"sku", *p.sku}});
            return productJson(tx.one(std::string(kProductSelect) + "WHERE p.id = $1", id));
        });
        ctx.json(201, out);
    });

    app.patch("/api/v1/products/:id", {Access::User, "catalog.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "product");
        const auto& b = ctx.body();
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto cur = tx.one("SELECT currency, kind, active FROM products WHERE id = $1 FOR UPDATE", id);
            if (b.contains("currency") && in::reqCurrency(b, "currency") != db::str(cur[0]))
                throw invalid("currency", "immutable");
            ProductInput p = parseProduct(b, db::str(cur[0]));
            if (p.kind && *p.kind != db::str(cur[1])) throw invalid("kind", "immutable");
            // Reactivating a product counts against the plan limit like a new one.
            if (p.active && *p.active && !db::boolean(cur[2]))
                entitlements::requireCapacity(tx, ctx.actor, entitlements::key::kProducts);
            tx.q("UPDATE products SET sku = COALESCE($2, sku), barcode = COALESCE($3, barcode), name = COALESCE($4, name), "
                 "description = COALESCE($5, description), category_id = COALESCE($6, category_id), "
                 "unit = COALESCE($7, unit), purchase_price_minor = COALESCE($8, purchase_price_minor), "
                 "sale_price_minor = COALESCE($9, sale_price_minor), tax_rate_id = COALESCE($10, tax_rate_id), "
                 "track_stock = COALESCE($11, track_stock), reorder_level_milli = COALESCE($12, reorder_level_milli), "
                 "active = COALESCE($13, active) WHERE id = $1",
                 id, p.sku, p.barcode, p.name, p.description, p.categoryId, p.unit, p.purchase, p.sale, p.taxRateId,
                 p.track, p.reorder, p.active);
            audit(tx, ctx.actor, "product.updated", "product", id, b);
            return productJson(tx.one(std::string(kProductSelect) + "WHERE p.id = $1", id));
        });
        ctx.json(200, out);
    });

    // All-or-nothing import, matching existing products on SKU.
    app.post("/api/v1/products/import", {Access::User, "catalog.write", true, {}, entitlements::key::kCatalogImport},
             [&env](Ctx& ctx) {
        auto rows = csv::parse(ctx.req.body);
        if (rows.size() < 2) throw invalid("file", "empty_or_header_only");
        if (rows.size() > 5001) throw invalid("file", "too_many_rows");
        std::map<std::string, size_t> col;
        for (size_t i = 0; i < rows[0].size(); ++i) col[rows[0][i]] = i;
        for (const char* req : {"sku", "name"})
            if (!col.count(req)) throw invalid("file", std::string("missing_column_") + req);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            std::string mainCur = db::str(tx.one("SELECT main_currency FROM establishments WHERE id = $1",
                                                 ctx.actor.establishmentId)[0]);
            std::map<std::string, std::string> taxByCode, catByName;
            for (const auto& r : tx.q("SELECT code, id FROM tax_rates")) taxByCode[db::str(r[0])] = db::str(r[1]);
            for (const auto& r : tx.q("SELECT lower(name), id FROM product_categories")) catByName[db::str(r[0])] = db::str(r[1]);
            json errors = json::array();
            int created = 0, updated = 0;
            for (size_t i = 1; i < rows.size(); ++i) {
                auto get = [&](const char* name) -> std::string {
                    auto it = col.find(name);
                    return it == col.end() || it->second >= rows[i].size() ? "" : rows[i][it->second];
                };
                try {
                    json b = json::object();
                    for (const char* k : {"sku", "name", "kind", "unit", "barcode", "description"})
                        if (!get(k).empty()) b[k] = get(k);
                    std::string currency = get("currency").empty() ? mainCur : get("currency");
                    for (const char* k : {"purchase_price", "sale_price", "reorder_level"})
                        if (!get(k).empty()) b[k] = get(k);
                    if (!get("track_stock").empty()) b["track_stock"] = get("track_stock") == "1";
                    if (!get("tax_code").empty()) {
                        auto it = taxByCode.find(get("tax_code"));
                        if (it == taxByCode.end()) throw invalid("tax_code", "unknown_tax_code");
                        b["tax_rate_id"] = it->second;
                    }
                    if (!get("category").empty()) {
                        std::string lc = get("category");
                        for (auto& ch : lc) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                        auto it = catByName.find(lc);
                        if (it == catByName.end()) {
                            auto r = tx.one("INSERT INTO product_categories (establishment_id, name) VALUES ($1,$2) "
                                            "RETURNING id",
                                            ctx.actor.establishmentId, get("category"));
                            it = catByName.emplace(lc, db::str(r[0])).first;
                        }
                        b["category_id"] = it->second;
                    }
                    ProductInput p = parseProduct(b, currency);
                    if (!p.sku) throw invalid("sku", "required");
                    if (!p.name) throw invalid("name", "required");
                    entitlements::requireCurrency(tx, ctx.actor, currency);
                    auto existing = tx.opt("SELECT id, currency FROM products WHERE sku = $1", *p.sku);
                    if (existing) {
                        if (db::str((*existing)[1]) != currency) throw invalid("currency", "immutable");
                        tx.q("UPDATE products SET name = $2, purchase_price_minor = COALESCE($3, purchase_price_minor), "
                             "sale_price_minor = COALESCE($4, sale_price_minor), tax_rate_id = COALESCE($5, tax_rate_id), "
                             "barcode = COALESCE($6, barcode), category_id = COALESCE($7, category_id), "
                             "unit = COALESCE($8, unit), reorder_level_milli = COALESCE($9, reorder_level_milli) WHERE id = $1",
                             db::str((*existing)[0]), *p.name, p.purchase, p.sale, p.taxRateId, p.barcode, p.categoryId,
                             p.unit, p.reorder);
                        ++updated;
                    } else {
                        insertProduct(tx, p, ctx.actor.establishmentId);
                        ++created;
                    }
                } catch (const ApiError& e) {
                    errors.push_back({{"line", i + 1}, {"error", e.code}, {"details", e.details}});
                    if (errors.size() >= 50) break;
                }
            }
            if (!errors.empty()) throw ApiError(422, "import_failed", "no product was imported", {{"errors", errors}});
            // The whole file counts against the product limit of the plan (nothing is kept on refusal).
            if (created > 0) {
                try {
                    entitlements::requireCapacity(tx, ctx.actor, entitlements::key::kProducts, 0);
                } catch (ApiError& e) {
                    if (e.details.is_object() && e.details.contains("used"))
                        e.details["used"] = e.details["used"].get<int64_t>() - created;
                    e.details["adding"] = created;
                    throw;
                }
            }
            audit(tx, ctx.actor, "product.imported", "product", "", {{"created", created}, {"updated", updated}});
            return {{"created", created}, {"updated", updated}};
        });
        ctx.json(200, out);
    });
}

}  // namespace nh
