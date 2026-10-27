// Payments received (invoices) and made (purchases, expenses, refunds), cash accounts, exchange rates.
#include "nhill/money.hpp"
#include "services/entitlements.hpp"
#include "services/sales.hpp"
#include "services/services.hpp"

namespace nh {

namespace {

json paymentJson(const pqxx::row& r) {
    std::string cur = db::str(r[6]);
    auto nullable = [](const pqxx::field& f) { return f.is_null() ? json(nullptr) : json(db::str(f)); };
    return {{"id", db::str(r[0])},
            {"direction", db::str(r[1])},
            {"party_name", nullable(r[2])},
            {"document_id", nullable(r[3])},
            {"document_number", nullable(r[4])},
            {"method", db::str(r[5])},
            {"currency", cur},
            {"amount", money(r[7].as<int64_t>(), cur)},
            {"paid_at", db::str(r[8])},
            {"reference", db::str(r[9])},
            {"account", nullable(r[10])},
            {"purchase_id", nullable(r[11])},
            {"expense_id", nullable(r[12])},
            {"created_by", db::str(r[13])}};
}

constexpr const char* kPaymentSelect =
    "SELECT y.id, y.direction, p.name, y.document_id, d.number, y.method, y.currency, y.amount_minor, nh_iso(y.paid_at), "
    "y.reference, a.name, y.purchase_id, y.expense_id, COALESCE(u.full_name,'') FROM payments y "
    "LEFT JOIN parties p ON p.id = y.party_id LEFT JOIN sales_documents d ON d.id = y.document_id "
    "LEFT JOIN cash_accounts a ON a.id = y.account_id LEFT JOIN users u ON u.id = y.created_by ";

}  // namespace

void registerPaymentRoutes(App& app, Env& env) {
    app.get("/api/v1/payments", {Access::User}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto dir = ctx.query("direction");
        if (dir && *dir != "in" && *dir != "out") dir.reset();
        auto from = ctx.query("from"), to = ctx.query("to");
        if (from && !in::isDate(*from)) from.reset();
        if (to && !in::isDate(*to)) to.reset();
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(std::string(kPaymentSelect) +
                                          "WHERE ($1::text IS NULL OR y.direction = $1) AND ($2::date IS NULL OR "
                                          "y.paid_at >= nh_local_start($2::date)) AND ($3::date IS NULL OR y.paid_at < nh_local_start($3::date + 1)) "
                                          "AND " + entitlements::paymentFilterSql("y", 6, 7, 8) +
                                          " ORDER BY y.paid_at DESC LIMIT $4 OFFSET $5",
                                      dir, from, to, pg.limit, pg.offset, ctx.actor.features.count(entitlements::key::kPurchases) > 0,
                                      ctx.actor.features.count(entitlements::key::kExpenses) > 0,
                                      entitlements::docTypesArray(ctx.actor)))
                items.push_back(paymentJson(r));
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    // Records a payment. The amount can never exceed what is still due, and the
    // currency must match the document (no implicit conversion).
    app.post("/api/v1/payments", {Access::User, "payments.write", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        auto docId = in::optUuid(b, "document_id");
        auto purchaseId = in::optUuid(b, "purchase_id");
        auto expenseId = in::optUuid(b, "expense_id");
        int targets = (docId ? 1 : 0) + (purchaseId ? 1 : 0) + (expenseId ? 1 : 0);
        if (targets != 1) throw invalid("document_id", "exactly_one_target_required");
        if (purchaseId) entitlements::requireFeature(ctx.actor, entitlements::key::kPurchases);
        if (expenseId) entitlements::requireFeature(ctx.actor, entitlements::key::kExpenses);
        std::string method = in::reqEnum(b, "method", {"cash", "mobile_money", "card", "bank_transfer", "cheque", "other"});
        auto account = in::optUuid(b, "account_id");
        std::string reference = in::optStr(b, "reference", 100).value_or("");
        std::string note = in::optStr(b, "note", 500).value_or("");
        auto paidAt = in::optDate(b, "paid_at");
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                std::string direction, currency, partyId;
                int64_t due = 0;
                if (docId) {
                    auto d = tx.one("SELECT doc_type, status, currency, total_minor, paid_minor, credited_minor, party_id, "
                                    "pos_session_id IS NOT NULL FROM sales_documents WHERE id = $1 FOR UPDATE",
                                    *docId);
                    std::string type = db::str(d[0]), st = db::str(d[1]);
                    if (!entitlements::docVisible(ctx.actor, type, db::boolean(d[7]))) throw notFound("document");
                    currency = db::str(d[2]);
                    partyId = db::str(d[6]);
                    if (type == "invoice") {
                        if (st != "issued" && st != "partially_paid") throw conflict("document_not_payable");
                        direction = "in";
                        due = d[3].as<int64_t>() - d[4].as<int64_t>() - d[5].as<int64_t>();
                    } else if (type == "credit_note") {
                        if (st != "issued") throw conflict("document_not_payable");
                        direction = "out";  // refund to the customer
                        due = d[3].as<int64_t>() - d[4].as<int64_t>();
                    } else {
                        throw conflict("document_not_payable");
                    }
                } else if (purchaseId) {
                    auto p = tx.one("SELECT status, currency, total_minor, paid_minor, supplier_id FROM purchases "
                                    "WHERE id = $1 FOR UPDATE",
                                    *purchaseId);
                    if (db::str(p[0]) != "received") throw conflict("purchase_not_payable");
                    direction = "out";
                    currency = db::str(p[1]);
                    partyId = db::str(p[4]);
                    due = p[2].as<int64_t>() - p[3].as<int64_t>();
                } else {
                    auto e = tx.one("SELECT currency, amount_minor, paid_minor, supplier_id FROM expenses WHERE id = $1 "
                                    "FOR UPDATE",
                                    *expenseId);
                    direction = "out";
                    currency = db::str(e[0]);
                    if (!e[3].is_null()) partyId = db::str(e[3]);
                    due = e[1].as<int64_t>() - e[2].as<int64_t>();
                }
                if (b.contains("currency") && in::reqCurrency(b, "currency") != currency)
                    throw invalid("currency", "must_match_document");
                int64_t amount = in::reqMoney(b, "amount", currency, false);
                if (amount > due) throw invalid("amount", "exceeds_amount_due");
                if (account) {
                    auto a = tx.opt("SELECT currency, active FROM cash_accounts WHERE id = $1", *account);
                    if (!a || !db::boolean((*a)[1])) throw invalid("account_id", "unknown_account");
                    if (db::str((*a)[0]) != currency) throw invalid("account_id", "account_currency_mismatch");
                }
                std::string pid = db::str(tx.one(
                    "INSERT INTO payments (establishment_id, direction, party_id, document_id, purchase_id, expense_id, method, "
                    "account_id, amount_minor, currency, paid_at, reference, note, created_by) VALUES ($1,$2,NULLIF($3,'')::uuid,"
                    "$4,$5,$6,$7,$8,$9,$10,COALESCE($11::date::timestamptz, now()),$12,$13,$14) RETURNING id",
                    ctx.actor.establishmentId, direction, partyId, docId, purchaseId, expenseId, method, account, amount,
                    currency, paidAt, reference, note, ctx.actor.userId)[0]);
                if (docId) {
                    auto d = tx.one("UPDATE sales_documents SET paid_minor = paid_minor + $2 WHERE id = $1 "
                                    "RETURNING doc_type, status, total_minor, paid_minor, credited_minor",
                                    *docId, amount);
                    if (db::str(d[0]) == "invoice") {
                        std::string st = invoiceStatusFor(d[2].as<int64_t>(), d[3].as<int64_t>(), d[4].as<int64_t>());
                        if (st != db::str(d[1])) {
                            tx.q("UPDATE sales_documents SET status = $2 WHERE id = $1", *docId, st);
                            tx.q("INSERT INTO document_status_history (establishment_id, document_id, from_status, to_status, "
                                 "actor_id, note) VALUES ($1,$2,$3,$4,$5,'payment')",
                                 ctx.actor.establishmentId, *docId, db::str(d[1]), st, ctx.actor.userId);
                        }
                    }
                } else if (purchaseId) {
                    tx.q("UPDATE purchases SET paid_minor = paid_minor + $2 WHERE id = $1", *purchaseId, amount);
                } else {
                    tx.q("UPDATE expenses SET paid_minor = paid_minor + $2 WHERE id = $1", *expenseId, amount);
                }
                audit(tx, ctx.actor, "payment.recorded", "payment", pid,
                      {{"amount", money(amount, currency)}, {"currency", currency}, {"method", method}});
                return {201, paymentJson(tx.one(std::string(kPaymentSelect) + "WHERE y.id = $1", pid))};
            });
        });
        ctx.json(status, body);
    });

    // ---------------------------------------------------------------- cash accounts
    // Balances count only the payments of the plan's features; accounts in another currency
    // than the main one need multi_currency.
    app.get("/api/v1/cash-accounts", {Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT a.id, a.name, a.kind, a.currency, a.active, COALESCE((SELECT sum(CASE WHEN y.direction = 'in' "
                     "THEN y.amount_minor ELSE -y.amount_minor END) FROM payments y WHERE y.account_id = a.id AND " +
                         entitlements::paymentFilterSql("y", 1, 2, 3) +
                         "), 0) FROM cash_accounts a WHERE ($4::boolean OR a.currency = "
                         "(SELECT main_currency FROM establishments WHERE id = a.establishment_id)) ORDER BY a.name",
                     ctx.actor.features.count(entitlements::key::kPurchases) > 0,
                     ctx.actor.features.count(entitlements::key::kExpenses) > 0, entitlements::docTypesArray(ctx.actor),
                     ctx.actor.features.count(entitlements::key::kMultiCurrency) > 0))
                items.push_back({{"id", db::str(r[0])}, {"name", db::str(r[1])}, {"kind", db::str(r[2])},
                                 {"currency", db::str(r[3])}, {"active", db::boolean(r[4])},
                                 {"recorded_balance", money(r[5].as<int64_t>(), db::str(r[3]))}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/cash-accounts", {Access::User, "settings.manage", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string name = in::reqStr(b, "name", 80);
        std::string kind = in::reqEnum(b, "kind", {"cash", "bank", "mobile_money", "other"});
        std::string currency = in::reqCurrency(b, "currency");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            entitlements::requireCurrency(tx, ctx.actor, currency);
            auto r = tx.one("INSERT INTO cash_accounts (establishment_id, name, kind, currency) VALUES ($1,$2,$3,$4) RETURNING id",
                            ctx.actor.establishmentId, name, kind, currency);
            audit(tx, ctx.actor, "cash_account.created", "cash_account", db::str(r[0]), b);
            return {{"id", db::str(r[0])}, {"name", name}, {"kind", kind}, {"currency", currency}, {"active", true}};
        });
        ctx.json(201, out);
    });

    // ---------------------------------------------------------------- exchange rates
    app.get("/api/v1/exchange-rates", {Access::User, "", false, {}, entitlements::key::kMultiCurrency}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT id, from_currency, to_currency, rate::text, rate_date::text, source "
                                      "FROM exchange_rates ORDER BY rate_date DESC, from_currency LIMIT 200"))
                items.push_back({{"id", db::str(r[0])}, {"from", db::str(r[1])}, {"to", db::str(r[2])},
                                 {"rate", nhill::ExchangeRate::parse(db::str(r[3]))->toString()},
                                 {"rate_date", db::str(r[4])}, {"source", db::str(r[5])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/exchange-rates", {Access::User, "settings.manage", true, {}, entitlements::key::kMultiCurrency},
             [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string from = in::reqCurrency(b, "from");
        std::string to = in::reqCurrency(b, "to");
        if (from == to) throw invalid("to", "same_currency");
        auto rate = nhill::ExchangeRate::parse(in::reqStr(b, "rate", 30));
        if (!rate) throw invalid("rate", "invalid_rate");
        auto date = in::optDate(b, "rate_date");
        if (!date) throw invalid("rate_date", "required");
        std::string source = in::optStr(b, "source", 100).value_or("");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto r = tx.one("INSERT INTO exchange_rates (establishment_id, from_currency, to_currency, rate, rate_date, source, "
                            "created_by) VALUES ($1,$2,$3,$4::numeric,$5::date,$6,$7) RETURNING id",
                            ctx.actor.establishmentId, from, to, rate->toString(), *date, source, ctx.actor.userId);
            audit(tx, ctx.actor, "exchange_rate.recorded", "exchange_rate", db::str(r[0]), b);
            return {{"id", db::str(r[0])}, {"from", from}, {"to", to}, {"rate", rate->toString()}, {"rate_date", *date},
                    {"source", source}};
        });
        ctx.json(201, out);
    });
}

}  // namespace nh
