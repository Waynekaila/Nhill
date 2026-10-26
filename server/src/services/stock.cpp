// Stock: levels (read-only, maintained by the database), movements, adjustments, transfers.
#include "nhill/money.hpp"
#include "services/services.hpp"

namespace nh {

namespace {

bool multiWarehouseAllowed(db::Tx& tx, const std::string& est) {
    auto r = tx.one(
        "SELECT s.is_trial, COALESCE((p.features ->> 'multi_warehouse')::boolean, false) FROM subscriptions s "
        "LEFT JOIN plans p ON p.id = s.plan_id WHERE s.establishment_id = $1",
        est);
    return db::boolean(r[0]) || db::boolean(r[1]);
}

std::string defaultWarehouse(db::Tx& tx) {
    return db::str(tx.one("SELECT id FROM warehouses WHERE is_default")[0]);
}

}  // namespace

void registerStockRoutes(App& app, Env& env) {
    app.get("/api/v1/warehouses", {Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT id, name, is_default, active FROM warehouses ORDER BY is_default DESC, name"))
                items.push_back({{"id", db::str(r[0])}, {"name", db::str(r[1])}, {"is_default", db::boolean(r[2])},
                                 {"active", db::boolean(r[3])}});
            return {{"items", items}, {"multi_warehouse", multiWarehouseAllowed(tx, ctx.actor.establishmentId)}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/warehouses", {Access::User, "stock.adjust", true}, [&env](Ctx& ctx) {
        std::string name = in::reqStr(ctx.body(), "name", 80);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            if (!multiWarehouseAllowed(tx, ctx.actor.establishmentId)) throw conflict("feature_not_in_plan");
            auto r = tx.one("INSERT INTO warehouses (establishment_id, name) VALUES ($1, $2) RETURNING id",
                            ctx.actor.establishmentId, name);
            audit(tx, ctx.actor, "warehouse.created", "warehouse", db::str(r[0]), {{"name", name}});
            return {{"id", db::str(r[0])}, {"name", name}, {"is_default", false}, {"active", true}};
        });
        ctx.json(201, out);
    });

    app.get("/api/v1/stock", {Access::User}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto search = ctx.query("search");
        auto warehouse = ctx.query("warehouse_id");
        if (warehouse && !in::isUuid(*warehouse)) warehouse.reset();
        bool lowOnly = ctx.query("low_stock").value_or("") == "1";
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            std::string sql =
                "SELECT p.id, p.sku, p.name, p.unit, p.reorder_level_milli, "
                "COALESCE((SELECT sum(l.quantity_milli) FROM stock_levels l WHERE l.product_id = p.id AND "
                "($2::uuid IS NULL OR l.warehouse_id = $2)), 0) AS q, p.purchase_price_minor, p.currency "
                "FROM products p WHERE p.track_stock AND p.active AND ($1::text IS NULL OR p.name ILIKE '%' || $1 || '%' "
                "OR p.sku ILIKE '%' || $1 || '%')";
            std::string outer = "SELECT * FROM (" + sql + ") s ";
            if (lowOnly) outer += "WHERE s.reorder_level_milli > 0 AND s.q <= s.reorder_level_milli ";
            int total = tx.one("SELECT count(*) FROM (" + outer + ") c", search, warehouse)[0].as<int>();
            json items = json::array();
            for (const auto& r : tx.q(outer + "ORDER BY lower(s.name) LIMIT $3 OFFSET $4", search, warehouse, pg.limit,
                                      pg.offset)) {
                int64_t q = r[5].as<int64_t>(), reorder = r[4].as<int64_t>();
                std::string cur = db::str(r[7]);
                items.push_back({{"product_id", db::str(r[0])},
                                 {"sku", db::str(r[1])},
                                 {"name", db::str(r[2])},
                                 {"unit", db::str(r[3])},
                                 {"quantity", qty(q)},
                                 {"reorder_level", qty(reorder)},
                                 {"low_stock", reorder > 0 && q <= reorder},
                                 // Valued at the current purchase price (estimate, see docs).
                                 {"estimated_value", money(*nhill::mulDiv(q, r[6].as<int64_t>(), 1000), cur)},
                                 {"currency", cur}});
            }
            return {{"items", items}, {"total", total}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/stock/movements", {Access::User}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto product = ctx.query("product_id");
        if (product && !in::isUuid(*product)) product.reset();
        auto from = ctx.query("from");
        auto to = ctx.query("to");
        if (from && !in::isDate(*from)) from.reset();
        if (to && !in::isDate(*to)) to.reset();
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT m.id, m.product_id, p.sku, p.name, w.name, m.quantity_milli, m.kind, m.source_type, "
                     "m.source_id, m.note, COALESCE(u.full_name, ''), nh_iso(m.created_at) FROM stock_movements m "
                     "JOIN products p ON p.id = m.product_id JOIN warehouses w ON w.id = m.warehouse_id "
                     "LEFT JOIN users u ON u.id = m.created_by WHERE ($1::uuid IS NULL OR m.product_id = $1) "
                     "AND ($2::date IS NULL OR m.created_at >= nh_local_start($2::date)) AND ($3::date IS NULL OR m.created_at < nh_local_start($3::date + 1)) "
                     "ORDER BY m.id DESC LIMIT $4 OFFSET $5",
                     product, from, to, pg.limit, pg.offset))
                items.push_back({{"id", r[0].as<int64_t>()},
                                 {"product_id", db::str(r[1])},
                                 {"sku", db::str(r[2])},
                                 {"product_name", db::str(r[3])},
                                 {"warehouse", db::str(r[4])},
                                 {"quantity", qty(r[5].as<int64_t>())},
                                 {"kind", db::str(r[6])},
                                 {"source_type", r[7].is_null() ? json(nullptr) : json(db::str(r[7]))},
                                 {"source_id", r[8].is_null() ? json(nullptr) : json(db::str(r[8]))},
                                 {"note", db::str(r[9])},
                                 {"created_by", db::str(r[10])},
                                 {"created_at", db::str(r[11])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    // Manual corrections (inventory count, breakage, opening balance). A reason is mandatory.
    app.post("/api/v1/stock/adjustments", {Access::User, "stock.adjust", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string product = in::reqUuid(b, "product_id");
        auto warehouse = in::optUuid(b, "warehouse_id");
        std::string kind = in::optEnum(b, "kind", {"adjustment", "opening"}).value_or("adjustment");
        std::string note = in::reqStr(b, "reason", 300);
        if (!b.contains("quantity") || !b["quantity"].is_string()) throw invalid("quantity", "required");
        auto q = nhill::parseQuantity(b["quantity"].get<std::string>());
        if (!q || *q == 0) throw invalid("quantity", "invalid_quantity");
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                std::string w = warehouse ? *warehouse : defaultWarehouse(tx);
                auto r = tx.one("INSERT INTO stock_movements (establishment_id, product_id, warehouse_id, quantity_milli, "
                                "kind, source_type, note, created_by) VALUES ($1,$2,$3,$4,$5,'manual',$6,$7) RETURNING id",
                                ctx.actor.establishmentId, product, w, *q, kind, note, ctx.actor.userId);
                auto level = tx.one("SELECT quantity_milli FROM stock_levels WHERE product_id = $1 AND warehouse_id = $2",
                                    product, w);
                audit(tx, ctx.actor, "stock.adjusted", "product", product, {{"quantity", qty(*q)}, {"reason", note}});
                return {201, {{"movement_id", r[0].as<int64_t>()}, {"quantity_after", qty(level[0].as<int64_t>())}}};
            });
        });
        ctx.json(status, body);
    });

    app.post("/api/v1/stock/transfers", {Access::User, "stock.adjust", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string product = in::reqUuid(b, "product_id");
        std::string from = in::reqUuid(b, "from_warehouse_id");
        std::string to = in::reqUuid(b, "to_warehouse_id");
        if (from == to) throw invalid("to_warehouse_id", "same_warehouse");
        int64_t q = in::reqQty(b, "quantity");
        if (q <= 0) throw invalid("quantity", "must_be_positive");
        std::string note = in::optStr(b, "note", 300).value_or("");
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                if (!multiWarehouseAllowed(tx, ctx.actor.establishmentId)) throw conflict("feature_not_in_plan");
                tx.q("INSERT INTO stock_movements (establishment_id, product_id, warehouse_id, quantity_milli, kind, "
                     "source_type, note, created_by) VALUES ($1,$2,$3,$4,'transfer_out','transfer',$5,$6)",
                     ctx.actor.establishmentId, product, from, -q, note, ctx.actor.userId);
                tx.q("INSERT INTO stock_movements (establishment_id, product_id, warehouse_id, quantity_milli, kind, "
                     "source_type, note, created_by) VALUES ($1,$2,$3,$4,'transfer_in','transfer',$5,$6)",
                     ctx.actor.establishmentId, product, to, q, note, ctx.actor.userId);
                audit(tx, ctx.actor, "stock.transferred", "product", product,
                      {{"from", from}, {"to", to}, {"quantity", qty(q)}});
                return {201, {{"status", "transferred"}}};
            });
        });
        ctx.json(status, body);
    });
}

}  // namespace nh
