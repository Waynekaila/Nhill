// Purchases from suppliers: draft -> received (stock in) ; payments via /payments.
#include "nhill/money.hpp"
#include "services/services.hpp"

namespace nh {

namespace {

json loadPurchase(db::Tx& tx, const std::string& id) {
    auto p = tx.one(
        "SELECT p.id, p.status, p.supplier_id, s.name, p.supplier_reference, p.purchase_date::text, p.due_date::text, "
        "p.currency, p.warehouse_id, w.name, p.net_minor, p.tax_minor, p.total_minor, p.paid_minor, p.notes, "
        "nh_iso(p.received_at), nh_iso(p.created_at) FROM purchases p JOIN parties s ON s.id = p.supplier_id "
        "JOIN warehouses w ON w.id = p.warehouse_id WHERE p.id = $1",
        id);
    std::string cur = db::str(p[7]);
    json lines = json::array();
    for (const auto& l : tx.q("SELECT line_no, product_id, description, quantity_milli, unit_cost_minor, tax_rate_bp, net_minor, "
                              "tax_minor, total_minor FROM purchase_lines WHERE purchase_id = $1 ORDER BY line_no",
                              id))
        lines.push_back({{"line_no", l[0].as<int>()},
                         {"product_id", l[1].is_null() ? json(nullptr) : json(db::str(l[1]))},
                         {"description", db::str(l[2])},
                         {"quantity", qty(l[3].as<int64_t>())},
                         {"unit_cost", money(l[4].as<int64_t>(), cur)},
                         {"tax_rate_bp", l[5].as<int>()},
                         {"net", money(l[6].as<int64_t>(), cur)},
                         {"tax", money(l[7].as<int64_t>(), cur)},
                         {"total", money(l[8].as<int64_t>(), cur)}});
    return {{"id", db::str(p[0])},
            {"status", db::str(p[1])},
            {"supplier_id", db::str(p[2])},
            {"supplier_name", db::str(p[3])},
            {"supplier_reference", db::str(p[4])},
            {"purchase_date", db::str(p[5])},
            {"due_date", p[6].is_null() ? json(nullptr) : json(db::str(p[6]))},
            {"currency", cur},
            {"warehouse_id", db::str(p[8])},
            {"warehouse", db::str(p[9])},
            {"net", money(p[10].as<int64_t>(), cur)},
            {"tax", money(p[11].as<int64_t>(), cur)},
            {"total", money(p[12].as<int64_t>(), cur)},
            {"paid", money(p[13].as<int64_t>(), cur)},
            {"balance", money(p[12].as<int64_t>() - p[13].as<int64_t>(), cur)},
            {"notes", db::str(p[14])},
            {"received_at", p[15].is_null() ? json(nullptr) : json(db::str(p[15]))},
            {"created_at", db::str(p[16])},
            {"lines", lines}};
}

// Writes header totals + lines of a draft purchase.
void savePurchaseLines(db::Tx& tx, const std::string& est, const std::string& id, const json& b, const std::string& currency) {
    if (!b.contains("lines") || !b["lines"].is_array() || b["lines"].empty()) throw invalid("lines", "at_least_one_line");
    if (b["lines"].size() > 500) throw invalid("lines", "too_many_lines");
    std::vector<nhill::LineInput> inputs;
    struct L {
        std::optional<std::string> product;
        std::string description;
        int64_t qty, cost;
        int tax;
    };
    std::vector<L> ls;
    for (size_t i = 0; i < b["lines"].size(); ++i) {
        const json& l = b["lines"][i];
        const std::string f = "lines[" + std::to_string(i) + "].";
        L x;
        x.product = in::optUuid(l, "product_id");
        std::string defDesc;
        if (x.product) {
            auto p = tx.opt("SELECT name FROM products WHERE id = $1", *x.product);
            if (!p) throw invalid(f + "product_id", "unknown_product");
            defDesc = db::str((*p)[0]);
        }
        x.description = in::optStr(l, "description", 500).value_or(defDesc);
        if (x.description.empty()) throw invalid(f + "description", "required");
        x.qty = in::reqQty(l, "quantity");
        if (x.qty <= 0) throw invalid(f + "quantity", "must_be_positive");
        x.cost = in::reqMoney(l, "unit_cost", currency);
        x.tax = in::optInt(l, "tax_rate_bp", 0, 0, 10000);
        inputs.push_back({x.qty, x.cost, 0, x.tax});
        ls.push_back(x);
    }
    auto t = nhill::computeDocument(inputs, nhill::TaxRounding::PerLine);
    if (!t) throw invalid("lines", "amount_overflow");
    tx.q("DELETE FROM purchase_lines WHERE purchase_id = $1", id);
    for (size_t i = 0; i < ls.size(); ++i)
        tx.q("INSERT INTO purchase_lines (establishment_id, purchase_id, line_no, product_id, description, quantity_milli, "
             "unit_cost_minor, tax_rate_bp, net_minor, tax_minor, total_minor) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11)",
             est, id, static_cast<int>(i + 1), ls[i].product, ls[i].description, ls[i].qty, ls[i].cost, ls[i].tax,
             t->lines[i].netMinor, t->lines[i].taxMinor, t->lines[i].totalMinor);
    tx.q("UPDATE purchases SET net_minor = $2, tax_minor = $3, total_minor = $4 WHERE id = $1", id, t->netMinor, t->taxMinor,
         t->totalMinor);
}

}  // namespace

void registerPurchaseRoutes(App& app, Env& env) {
    app.get("/api/v1/purchases", {Access::User}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto status = ctx.query("status");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT p.id, p.status, s.name, p.supplier_reference, p.purchase_date::text, p.currency, p.total_minor, "
                     "p.paid_minor FROM purchases p JOIN parties s ON s.id = p.supplier_id WHERE ($1::text IS NULL OR "
                     "p.status = $1) ORDER BY p.purchase_date DESC, p.created_at DESC LIMIT $2 OFFSET $3",
                     status, pg.limit, pg.offset)) {
                std::string cur = db::str(r[5]);
                items.push_back({{"id", db::str(r[0])}, {"status", db::str(r[1])}, {"supplier_name", db::str(r[2])},
                                 {"supplier_reference", db::str(r[3])}, {"purchase_date", db::str(r[4])}, {"currency", cur},
                                 {"total", money(r[6].as<int64_t>(), cur)},
                                 {"balance", money(r[6].as<int64_t>() - r[7].as<int64_t>(), cur)}});
            }
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/purchases/:id", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "purchase");
        ctx.json(200, db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) { return loadPurchase(tx, id); }));
    });

    app.post("/api/v1/purchases", {Access::User, "purchases.write", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string supplier = in::reqUuid(b, "supplier_id");
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                auto s = tx.opt("SELECT is_supplier, active FROM parties WHERE id = $1", supplier);
                if (!s || !db::boolean((*s)[0]) || !db::boolean((*s)[1])) throw invalid("supplier_id", "not_an_active_supplier");
                std::string currency = b.contains("currency") ? in::reqCurrency(b, "currency")
                                                              : db::str(tx.one("SELECT main_currency FROM establishments WHERE id = $1",
                                                                               ctx.actor.establishmentId)[0]);
                auto wh = in::optUuid(b, "warehouse_id");
                std::string warehouse = wh ? *wh : db::str(tx.one("SELECT id FROM warehouses WHERE is_default")[0]);
                std::string id = db::str(tx.one(
                    "INSERT INTO purchases (establishment_id, supplier_id, supplier_reference, purchase_date, due_date, currency, "
                    "warehouse_id, notes, created_by) VALUES ($1,$2,$3,COALESCE($4::date, nh_local_today()),$5::date,$6,$7,$8,$9) "
                    "RETURNING id",
                    ctx.actor.establishmentId, supplier, in::optStr(b, "supplier_reference", 100).value_or(""),
                    in::optDate(b, "purchase_date"), in::optDate(b, "due_date"), currency, warehouse,
                    in::optStr(b, "notes", 2000).value_or(""), ctx.actor.userId)[0]);
                savePurchaseLines(tx, ctx.actor.establishmentId, id, b, currency);
                audit(tx, ctx.actor, "purchase.created", "purchase", id);
                return {201, loadPurchase(tx, id)};
            });
        });
        ctx.json(status, body);
    });

    app.put("/api/v1/purchases/:id", {Access::User, "purchases.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "purchase");
        const auto& b = ctx.body();
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto p = tx.one("SELECT status, currency FROM purchases WHERE id = $1 FOR UPDATE", id);
            if (db::str(p[0]) != "draft") throw conflict("purchase_immutable");
            tx.q("UPDATE purchases SET supplier_reference = COALESCE($2, supplier_reference), "
                 "purchase_date = COALESCE($3::date, purchase_date), due_date = COALESCE($4::date, due_date), "
                 "notes = COALESCE($5, notes) WHERE id = $1",
                 id, in::optStr(b, "supplier_reference", 100), in::optDate(b, "purchase_date"), in::optDate(b, "due_date"),
                 in::optStr(b, "notes", 2000));
            savePurchaseLines(tx, ctx.actor.establishmentId, id, b, db::str(p[1]));
            audit(tx, ctx.actor, "purchase.updated", "purchase", id);
            return loadPurchase(tx, id);
        });
        ctx.json(200, out);
    });

    // Goods received: stock movements for tracked products, optional update of
    // the product purchase price (last cost).
    app.post("/api/v1/purchases/:id/receive", {Access::User, "purchases.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "purchase");
        bool updateCost = in::optBool(ctx.body(), "update_purchase_prices", true);
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                auto p = tx.one("SELECT status, warehouse_id, currency FROM purchases WHERE id = $1 FOR UPDATE", id);
                if (db::str(p[0]) != "draft") throw conflict("purchase_not_draft");
                for (const auto& l : tx.q("SELECT l.product_id, l.quantity_milli, l.unit_cost_minor, pr.track_stock, pr.currency "
                                          "FROM purchase_lines l JOIN products pr ON pr.id = l.product_id "
                                          "WHERE l.purchase_id = $1",
                                          id)) {
                    if (db::boolean(l[3]))
                        tx.q("INSERT INTO stock_movements (establishment_id, product_id, warehouse_id, quantity_milli, kind, "
                             "source_type, source_id, created_by) VALUES ($1,$2,$3,$4,'purchase','purchase',$5,$6)",
                             ctx.actor.establishmentId, db::str(l[0]), db::str(p[1]), l[1].as<int64_t>(), id, ctx.actor.userId);
                    if (updateCost && db::str(l[4]) == db::str(p[2]))
                        tx.q("UPDATE products SET purchase_price_minor = $2 WHERE id = $1", db::str(l[0]), l[2].as<int64_t>());
                }
                tx.q("UPDATE purchases SET status = 'received', received_at = now(), received_by = $2 WHERE id = $1", id,
                     ctx.actor.userId);
                audit(tx, ctx.actor, "purchase.received", "purchase", id);
                return {200, loadPurchase(tx, id)};
            });
        });
        ctx.json(status, body);
    });

    app.post("/api/v1/purchases/:id/cancel", {Access::User, "purchases.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "purchase");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto p = tx.one("SELECT status FROM purchases WHERE id = $1 FOR UPDATE", id);
            if (db::str(p[0]) != "draft") throw conflict("purchase_not_draft");
            tx.q("UPDATE purchases SET status = 'canceled' WHERE id = $1", id);
            audit(tx, ctx.actor, "purchase.canceled", "purchase", id);
            return loadPurchase(tx, id);
        });
        ctx.json(200, out);
    });
}

}  // namespace nh
