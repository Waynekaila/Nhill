// Dashboard and reports (JSON, CSV, PDF). Calculation rules: docs/technique.md §Rapports.
#include "nhill/money.hpp"
#include "pdf/pdf.hpp"
#include "services/entitlements.hpp"
#include "services/sales.hpp"
#include "services/report.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"
#include "util/csv.hpp"

namespace nh {



void registerReportRoutes(App& app, Env& env) {
    // Key figures. Revenue = net (tax-exclusive) amount of invoices issued in the
    // period minus credit notes issued in the period, per currency.
    app.get("/api/v1/reports/dashboard", {Access::User}, [&env](Ctx& ctx) {
        Period p = period(ctx);
        // Users without reports.read (sales, viewer) get their own figures only:
        // their sales and receivables, no expenses, payables or stock value.
        const bool full = App::roleHas(ctx.actor.role, "reports.read");
        const std::optional<std::string> mine = full ? std::nullopt : std::optional<std::string>(ctx.actor.userId);
        // Figures of the plan's features only: invoices and/or till receipts, expenses and
        // purchases when included, main warehouse only without several warehouses.
        const std::string types = entitlements::docTypesArray(ctx.actor);
        const auto& f = ctx.actor.features;
        const bool hasExpenses = f.count(entitlements::key::kExpenses) > 0, hasPurchases = f.count(entitlements::key::kPurchases) > 0,
                   hasInvoices = f.count(entitlements::key::kInvoices) > 0, multi = f.count(entitlements::key::kMultiWarehouse) > 0;
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json revenue = json::object();
            for (const auto& r : tx.q(
                     "SELECT d.currency, sum(CASE WHEN d.doc_type = 'credit_note' THEN -d.net_minor ELSE d.net_minor END), "
                     "sum(CASE WHEN d.doc_type = 'credit_note' THEN -d.total_minor ELSE d.total_minor END), "
                     "count(*) FILTER (WHERE d.doc_type = 'invoice'), count(*) FILTER (WHERE d.doc_type = 'receipt') "
                     "FROM sales_documents d WHERE d.doc_type IN ('invoice','receipt','credit_note') "
                     "AND d.status <> 'draft' AND d.issue_date BETWEEN $1::date AND $2::date AND ($3::uuid IS NULL OR d.issued_by = $3) "
                     "AND " + entitlements::docVisibleSql("d", 4) + " GROUP BY d.currency",
                     p.from, p.to, mine, types)) {
                std::string c = db::str(r[0]);
                revenue[c] = {{"net", money(r[1].as<int64_t>(), c)}, {"total", money(r[2].as<int64_t>(), c)},
                              {"invoices", r[3].as<int>()}, {"receipts", r[4].as<int>()}};
            }
            json expenses = json::object();
            if (full && hasExpenses)
            for (const auto& r : tx.q("SELECT currency, sum(amount_minor) FROM expenses WHERE expense_date BETWEEN $1::date "
                                      "AND $2::date GROUP BY currency",
                                      p.from, p.to))
                expenses[db::str(r[0])] = money(r[1].as<int64_t>(), db::str(r[0]));
            json receivables = json::object();
            if (hasInvoices)
            for (const auto& r : tx.q(
                     "SELECT currency, sum(total_minor - paid_minor - credited_minor), sum(CASE WHEN due_date < nh_local_today() "
                     "THEN total_minor - paid_minor - credited_minor ELSE 0 END), count(*), count(*) FILTER (WHERE due_date < "
                     "nh_local_today()) FROM sales_documents WHERE doc_type = 'invoice' AND status IN ('issued','partially_paid') "
                     "AND ($1::uuid IS NULL OR issued_by = $1) GROUP BY currency",
                     mine)) {
                std::string c = db::str(r[0]);
                receivables[c] = {{"open", money(r[1].as<int64_t>(), c)}, {"overdue", money(r[2].as<int64_t>(), c)},
                                  {"open_count", r[3].as<int>()}, {"overdue_count", r[4].as<int>()}};
            }
            json payables = json::object();
            if (full && (hasPurchases || hasExpenses))
            for (const auto& r : tx.q(
                     "SELECT currency, sum(due) FROM (SELECT currency, total_minor - paid_minor AS due FROM purchases WHERE "
                     "status = 'received' AND $1::boolean UNION ALL SELECT currency, amount_minor - paid_minor FROM expenses "
                     "WHERE $2::boolean) x WHERE due > 0 GROUP BY currency",
                     hasPurchases, hasExpenses))
                payables[db::str(r[0])] = money(r[1].as<int64_t>(), db::str(r[0]));
            json lowStock = json::array();
            for (const auto& r : tx.q(
                     "SELECT p.id, p.sku, p.name, p.unit, COALESCE(sum(l.quantity_milli), 0) q, p.reorder_level_milli "
                     "FROM products p LEFT JOIN stock_levels l ON l.product_id = p.id AND ($1::boolean OR l.warehouse_id = "
                     "(SELECT id FROM warehouses WHERE is_default)) WHERE p.track_stock AND p.active AND "
                     "p.reorder_level_milli > 0 GROUP BY p.id HAVING COALESCE(sum(l.quantity_milli), 0) <= p.reorder_level_milli "
                     "ORDER BY q LIMIT 10",
                     multi))
                lowStock.push_back({{"product_id", db::str(r[0])}, {"sku", db::str(r[1])}, {"name", db::str(r[2])},
                                    {"unit", db::str(r[3])}, {"quantity", qty(r[4].as<int64_t>())},
                                    {"reorder_level", qty(r[5].as<int64_t>())}});
            auto stock = tx.one("SELECT count(*) FROM products WHERE track_stock AND active");
            json stockValue = json::object();
            if (full)
            for (const auto& r : tx.q("SELECT p.currency, sum(l.quantity_milli * p.purchase_price_minor / 1000) FROM "
                                      "stock_levels l JOIN products p ON p.id = l.product_id WHERE l.quantity_milli > 0 "
                                      "AND ($1::boolean OR l.warehouse_id = (SELECT id FROM warehouses WHERE is_default)) "
                                      "GROUP BY p.currency",
                                      multi))
                stockValue[db::str(r[0])] = money(r[1].as<int64_t>(), db::str(r[0]));
            json top = json::array();
            for (const auto& r : tx.q(
                     "SELECT COALESCE(p.name, l.description), d.currency, sum(l.quantity_milli), sum(l.net_minor) FROM "
                     "sales_document_lines l JOIN sales_documents d ON d.id = l.document_id LEFT JOIN products p ON "
                     "p.id = l.product_id WHERE d.doc_type IN ('invoice','receipt') AND d.status <> 'draft' AND d.issue_date BETWEEN "
                     "$1::date AND $2::date AND ($3::uuid IS NULL OR d.issued_by = $3) AND " + entitlements::docVisibleSql("d", 4) +
                         " GROUP BY 1, 2 ORDER BY 4 DESC LIMIT 5",
                     p.from, p.to, mine, types))
                top.push_back({{"name", db::str(r[0])}, {"currency", db::str(r[1])}, {"quantity", qty(r[2].as<int64_t>())},
                               {"net", money(r[3].as<int64_t>(), db::str(r[1]))}});
            json recent = json::array();
            for (const auto& r : tx.q(
                     "SELECT d.id, d.number, p.name, d.issue_date::text, d.currency, d.total_minor, d.status FROM "
                     "sales_documents d JOIN parties p ON p.id = d.party_id WHERE d.doc_type IN ('invoice','receipt') AND d.status <> "
                     "'draft' AND ($1::uuid IS NULL OR d.issued_by = $1) AND " + entitlements::docVisibleSql("d", 2) +
                         " ORDER BY d.issued_at DESC LIMIT 5",
                     mine, types))
                recent.push_back({{"id", db::str(r[0])}, {"number", db::str(r[1])}, {"party_name", db::str(r[2])},
                                  {"issue_date", db::str(r[3])}, {"currency", db::str(r[4])},
                                  {"total", money(r[5].as<int64_t>(), db::str(r[4]))}, {"status", db::str(r[6])}});
            return {{"period", {{"from", p.from}, {"to", p.to}}},
                    {"scope", full ? "establishment" : "self"},
                    {"revenue", revenue},
                    {"expenses", expenses},
                    {"receivables", receivables},
                    {"payables", payables},
                    {"stock", {{"tracked_products", stock[0].as<int>()}, {"low_stock", lowStock},
                               {"estimated_value", stockValue}}},
                    {"top_products", top},
                    {"recent_sales", recent},
                    {"subscription", subscriptions::view(tx, ctx.actor.establishmentId)}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/reports/sales", {Access::User, "reports.read", false, {}, "sales.invoices|pos"}, [&env](Ctx& ctx) {
        Period p = period(ctx);
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            Report r{"report.sales",
                     {"col.date", "col.number", "col.type", "col.customer", "col.currency", "col.net", "col.tax", "col.total",
                      "col.paid", "col.balance", "col.issued_by"},
                     {Col::Date, Col::Text, Col::Text, Col::Text, Col::Text, Col::Money, Col::Money, Col::Money, Col::Money,
                      Col::Money, Col::Text},
                     {-1, -1, -1, -1, -1, 4, 4, 4, 4, 4, -1},
                     {},
                     true};
            for (const auto& row : tx.q(
                     "SELECT d.issue_date::text, d.number, d.doc_type, p.name, d.currency, d.net_minor, d.tax_minor, "
                     "d.total_minor, d.paid_minor, d.total_minor - d.paid_minor - d.credited_minor, d.doc_type = 'credit_note', "
                     "COALESCE(u.full_name, '') FROM sales_documents d JOIN parties p ON p.id = d.party_id "
                     "LEFT JOIN users u ON u.id = d.issued_by WHERE d.doc_type IN ('invoice','receipt','credit_note') "
                     "AND d.status <> 'draft' AND d.issue_date BETWEEN $1::date AND $2::date AND " +
                         entitlements::docVisibleSql("d", 3) + " ORDER BY d.issue_date, d.number",
                     p.from, p.to, entitlements::docTypesArray(ctx.actor))) {
                std::string c = db::str(row[4]);
                bool cn = db::boolean(row[10]);
                auto m = [&](int i) { return money(cn ? -row[i].as<int64_t>() : row[i].as<int64_t>(), c); };
                r.rows.push_back({db::str(row[0]), db::str(row[1]), env.i18n.t(ctx.actor.lang, "doctype." + db::str(row[2])),
                                  db::str(row[3]), c, m(5), m(6), m(7), money(row[8].as<int64_t>(), c), m(9),
                                  db::str(row[11])});
            }
            sendReport(env, ctx, tx, r, "ventes_" + p.from + "_" + p.to, &p);
        });
    });

    app.get("/api/v1/reports/expenses", {Access::User, "reports.read", false, {}, entitlements::key::kExpenses},
            [&env](Ctx& ctx) {
        Period p = period(ctx);
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            Report r{"report.expenses",
                     {"col.date", "col.category", "col.description", "col.supplier", "col.currency", "col.amount", "col.tax",
                      "col.paid"},
                     {Col::Date, Col::Text, Col::Text, Col::Text, Col::Text, Col::Money, Col::Money, Col::Money},
                     {-1, -1, -1, -1, -1, 4, 4, 4},
                     {}};
            for (const auto& row : tx.q(
                     "SELECT e.expense_date::text, COALESCE(c.name,''), e.description, COALESCE(s.name,''), e.currency, "
                     "e.amount_minor, e.tax_minor, e.paid_minor FROM expenses e LEFT JOIN expense_categories c ON "
                     "c.id = e.category_id LEFT JOIN parties s ON s.id = e.supplier_id WHERE e.expense_date BETWEEN $1::date "
                     "AND $2::date ORDER BY e.expense_date",
                     p.from, p.to)) {
                std::string c = db::str(row[4]);
                r.rows.push_back({db::str(row[0]), db::str(row[1]), db::str(row[2]), db::str(row[3]), c,
                                  money(row[5].as<int64_t>(), c), money(row[6].as<int64_t>(), c),
                                  money(row[7].as<int64_t>(), c)});
            }
            sendReport(env, ctx, tx, r, "depenses_" + p.from + "_" + p.to, &p);
        });
    });

    app.get("/api/v1/reports/stock-movements", {Access::User, "reports.read"}, [&env](Ctx& ctx) {
        Period p = period(ctx);
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            Report r{"report.stock_movements",
                     {"col.date", "col.sku", "col.product", "col.warehouse", "col.kind", "col.quantity", "col.note"},
                     {Col::Date, Col::Text, Col::Text, Col::Text, Col::Text, Col::Qty, Col::Text},
                     {-1, -1, -1, -1, -1, -1, -1},
                     {}};
            for (const auto& row : tx.q(
                     "SELECT to_char(m.created_at, 'YYYY-MM-DD HH24:MI'), p.sku, p.name, w.name, m.kind, m.quantity_milli, "
                     "m.note FROM stock_movements m JOIN products p ON p.id = m.product_id JOIN warehouses w ON "
                     "w.id = m.warehouse_id WHERE m.created_at >= nh_local_start($1::date) AND m.created_at < nh_local_start($2::date + 1) "
                     "AND ($3::boolean OR w.is_default) AND " + entitlements::movementFilterSql("m", 3, 4, 5) + " ORDER BY m.id",
                     p.from, p.to, ctx.actor.features.count(entitlements::key::kMultiWarehouse) > 0,
                     ctx.actor.features.count(entitlements::key::kPurchases) > 0, entitlements::docTypesArray(ctx.actor)))
                r.rows.push_back({db::str(row[0]), db::str(row[1]), db::str(row[2]), db::str(row[3]),
                                  env.i18n.t(ctx.actor.lang, "move." + db::str(row[4])), qty(row[5].as<int64_t>()),
                                  db::str(row[6])});
            sendReport(env, ctx, tx, r, "mouvements_stock_" + p.from + "_" + p.to, &p);
        });
    });

    app.get("/api/v1/reports/receivables", {Access::User, "reports.read", false, {}, entitlements::key::kInvoices},
            [&env](Ctx& ctx) {
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            Report r{"report.receivables",
                     {"col.number", "col.customer", "col.date", "col.due_date", "col.days_overdue", "col.currency", "col.total",
                      "col.balance"},
                     {Col::Text, Col::Text, Col::Date, Col::Date, Col::Int, Col::Text, Col::Money, Col::Money},
                     {-1, -1, -1, -1, -1, -1, 5, 5},
                     {}};
            for (const auto& row : tx.q(
                     "SELECT d.number, p.name, d.issue_date::text, d.due_date::text, GREATEST(0, nh_local_today() - d.due_date), "
                     "d.currency, d.total_minor, d.total_minor - d.paid_minor - d.credited_minor FROM sales_documents d "
                     "JOIN parties p ON p.id = d.party_id WHERE d.doc_type = 'invoice' AND d.status IN ('issued','partially_paid') "
                     "ORDER BY d.due_date")) {
                std::string c = db::str(row[5]);
                r.rows.push_back({db::str(row[0]), db::str(row[1]), db::str(row[2]), db::str(row[3]), db::str(row[4]), c,
                                  money(row[6].as<int64_t>(), c), money(row[7].as<int64_t>(), c)});
            }
            sendReport(env, ctx, tx, r, "creances", nullptr);
        });
    });

    app.get("/api/v1/reports/payables", {Access::User, "reports.read", false, {}, "purchases|expenses"}, [&env](Ctx& ctx) {
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            Report r{"report.payables",
                     {"col.type", "col.supplier", "col.reference", "col.date", "col.due_date", "col.currency", "col.total",
                      "col.balance"},
                     {Col::Text, Col::Text, Col::Text, Col::Date, Col::Date, Col::Text, Col::Money, Col::Money},
                     {-1, -1, -1, -1, -1, -1, 5, 5},
                     {}};
            for (const auto& row : tx.q(
                     "SELECT 'purchase', s.name, p.supplier_reference, p.purchase_date::text, COALESCE(p.due_date::text,''), "
                     "p.currency, p.total_minor, p.total_minor - p.paid_minor FROM purchases p JOIN parties s ON "
                     "s.id = p.supplier_id WHERE p.status = 'received' AND p.paid_minor < p.total_minor AND $1::boolean UNION ALL "
                     "SELECT 'expense', COALESCE(s.name,''), e.description, e.expense_date::text, COALESCE(e.due_date::text,''), "
                     "e.currency, e.amount_minor, e.amount_minor - e.paid_minor FROM expenses e LEFT JOIN parties s ON "
                     "s.id = e.supplier_id WHERE e.paid_minor < e.amount_minor AND $2::boolean ORDER BY 5, 4",
                     ctx.actor.features.count(entitlements::key::kPurchases) > 0,
                     ctx.actor.features.count(entitlements::key::kExpenses) > 0)) {
                std::string c = db::str(row[5]);
                r.rows.push_back({env.i18n.t(ctx.actor.lang, "type." + db::str(row[0])), db::str(row[1]), db::str(row[2]),
                                  db::str(row[3]), db::str(row[4]), c, money(row[6].as<int64_t>(), c),
                                  money(row[7].as<int64_t>(), c)});
            }
            sendReport(env, ctx, tx, r, "dettes", nullptr);
        });
    });

    app.get("/api/v1/reports/payments", {Access::User, "reports.read"}, [&env](Ctx& ctx) {
        Period p = period(ctx);
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            Report r{"report.payments",
                     {"col.date", "col.direction", "col.method", "col.party", "col.reference", "col.currency", "col.amount",
                      "col.recorded_by"},
                     {Col::Date, Col::Text, Col::Text, Col::Text, Col::Text, Col::Text, Col::Money, Col::Text},
                     {-1, -1, -1, -1, -1, -1, 5, -1},
                     {}};
            for (const auto& row : tx.q(
                     "SELECT to_char(y.paid_at, 'YYYY-MM-DD'), y.direction, y.method, COALESCE(pt.name,''), "
                     "COALESCE(d.number, y.reference), y.currency, y.amount_minor, COALESCE(u.full_name, '') FROM payments y "
                     "LEFT JOIN parties pt ON pt.id = y.party_id LEFT JOIN sales_documents d ON d.id = y.document_id "
                     "LEFT JOIN users u ON u.id = y.created_by WHERE y.paid_at >= nh_local_start($1::date) "
                     "AND y.paid_at < nh_local_start($2::date + 1) AND " + entitlements::paymentFilterSql("y", 3, 4, 5) +
                         " ORDER BY y.paid_at",
                     p.from, p.to, ctx.actor.features.count(entitlements::key::kPurchases) > 0,
                     ctx.actor.features.count(entitlements::key::kExpenses) > 0, entitlements::docTypesArray(ctx.actor))) {
                std::string c = db::str(row[5]);
                r.rows.push_back({db::str(row[0]), env.i18n.t(ctx.actor.lang, "dir." + db::str(row[1])),
                                  env.i18n.t(ctx.actor.lang, "method." + db::str(row[2])), db::str(row[3]), db::str(row[4]), c,
                                  money(row[6].as<int64_t>(), c), db::str(row[7])});
            }
            sendReport(env, ctx, tx, r, "paiements_" + p.from + "_" + p.to, &p);
        });
    });

    // Estimated margin = net sales - quantity x CURRENT purchase price (not a
    // historical cost: labelled "estimated" everywhere).
    app.get("/api/v1/reports/products", {Access::User, "reports.read", false, {}, entitlements::key::kAdvancedReports},
            [&env](Ctx& ctx) {
        Period p = period(ctx);
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            Report r{"report.products",
                     {"col.sku", "col.product", "col.currency", "col.quantity", "col.net", "col.estimated_cost",
                      "col.estimated_margin"},
                     {Col::Text, Col::Text, Col::Text, Col::Qty, Col::Money, Col::Money, Col::Money},
                     {-1, -1, -1, -1, 2, 2, 2},
                     {}};
            for (const auto& row : tx.q(
                     "SELECT pr.sku, pr.name, d.currency, sum(l.quantity_milli), sum(l.net_minor), "
                     "sum(l.quantity_milli * pr.purchase_price_minor / 1000) FILTER (WHERE pr.currency = d.currency) "
                     "FROM sales_document_lines l JOIN sales_documents d ON d.id = l.document_id JOIN products pr ON "
                     "pr.id = l.product_id WHERE d.doc_type IN ('invoice','receipt') AND d.status <> 'draft' AND d.issue_date BETWEEN "
                     "$1::date AND $2::date AND " + entitlements::docVisibleSql("d", 3) +
                         " GROUP BY pr.sku, pr.name, d.currency ORDER BY 5 DESC",
                     p.from, p.to, entitlements::docTypesArray(ctx.actor))) {
                std::string c = db::str(row[2]);
                bool hasCost = !row[5].is_null();
                int64_t net = row[4].as<int64_t>(), cost = hasCost ? row[5].as<int64_t>() : 0;
                r.rows.push_back({db::str(row[0]), db::str(row[1]), c, qty(row[3].as<int64_t>()), money(net, c),
                                  hasCost ? money(cost, c) : "", hasCost ? money(net - cost, c) : ""});
            }
            sendReport(env, ctx, tx, r, "produits_" + p.from + "_" + p.to, &p);
        });
    });
}

}  // namespace nh
