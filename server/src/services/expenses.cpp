// Expenses with categories and receipts (justificatifs).
#include "services/services.hpp"
#include "util/crypto.hpp"

namespace nh {

namespace {

constexpr const char* kExpenseSelect =
    "SELECT e.id, e.category_id, c.name, e.supplier_id, s.name, e.description, e.amount_minor, e.tax_minor, e.currency, "
    "e.expense_date::text, e.due_date::text, e.paid_minor, e.receipt_object_id IS NOT NULL, nh_iso(e.created_at) "
    "FROM expenses e LEFT JOIN expense_categories c ON c.id = e.category_id LEFT JOIN parties s ON s.id = e.supplier_id ";

json expenseJson(const pqxx::row& r) {
    auto nullable = [](const pqxx::field& f) { return f.is_null() ? json(nullptr) : json(db::str(f)); };
    std::string cur = db::str(r[8]);
    int64_t amount = r[6].as<int64_t>(), paid = r[11].as<int64_t>();
    return {{"id", db::str(r[0])},
            {"category_id", nullable(r[1])},
            {"category", nullable(r[2])},
            {"supplier_id", nullable(r[3])},
            {"supplier", nullable(r[4])},
            {"description", db::str(r[5])},
            {"amount", money(amount, cur)},
            {"tax", money(r[7].as<int64_t>(), cur)},
            {"currency", cur},
            {"expense_date", db::str(r[9])},
            {"due_date", nullable(r[10])},
            {"paid", money(paid, cur)},
            {"status", paid >= amount ? "paid" : paid > 0 ? "partially_paid" : "unpaid"},
            {"has_receipt", db::boolean(r[12])},
            {"created_at", db::str(r[13])}};
}

}  // namespace

void registerExpenseRoutes(App& app, Env& env) {
    app.get("/api/v1/expense-categories", {Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT id, name FROM expense_categories ORDER BY lower(name)"))
                items.push_back({{"id", db::str(r[0])}, {"name", db::str(r[1])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/expense-categories", {Access::User, "expenses.write", true}, [&env](Ctx& ctx) {
        std::string name = in::reqStr(ctx.body(), "name", 80);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto r = tx.one("INSERT INTO expense_categories (establishment_id, name) VALUES ($1,$2) RETURNING id",
                            ctx.actor.establishmentId, name);
            return {{"id", db::str(r[0])}, {"name", name}};
        });
        ctx.json(201, out);
    });

    app.get("/api/v1/expenses", {Access::User}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto from = ctx.query("from"), to = ctx.query("to");
        if (from && !in::isDate(*from)) from.reset();
        if (to && !in::isDate(*to)) to.reset();
        auto category = ctx.query("category_id");
        if (category && !in::isUuid(*category)) category.reset();
        bool unpaid = ctx.query("unpaid").value_or("") == "1";
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            std::string where = "WHERE ($1::date IS NULL OR e.expense_date >= $1::date) AND ($2::date IS NULL OR "
                                "e.expense_date <= $2::date) AND ($3::uuid IS NULL OR e.category_id = $3) ";
            if (unpaid) where += "AND e.paid_minor < e.amount_minor ";
            int total = tx.one("SELECT count(*) FROM expenses e " + where, from, to, category)[0].as<int>();
            json items = json::array();
            for (const auto& r : tx.q(std::string(kExpenseSelect) + where +
                                          "ORDER BY e.expense_date DESC, e.created_at DESC LIMIT $4 OFFSET $5",
                                      from, to, category, pg.limit, pg.offset))
                items.push_back(expenseJson(r));
            return {{"items", items}, {"total", total}};
        });
        ctx.json(200, out);
    });

    // Creates an expense; with "paid": {"method": ..., "account_id": ...} it is
    // recorded as paid in full in the same transaction.
    app.post("/api/v1/expenses", {Access::User, "expenses.write", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string currency = in::reqCurrency(b, "currency");
        int64_t amount = in::reqMoney(b, "amount", currency, false);
        int64_t tax = in::optMoney(b, "tax", currency).value_or(0);
        if (tax > amount) throw invalid("tax", "exceeds_amount");
        std::string description = in::reqStr(b, "description", 300);
        auto category = in::optUuid(b, "category_id");
        auto supplier = in::optUuid(b, "supplier_id");
        auto date = in::optDate(b, "expense_date");
        auto due = in::optDate(b, "due_date");
        std::optional<std::string> paidMethod, paidAccount;
        if (b.contains("paid") && b["paid"].is_object()) {
            paidMethod = in::reqEnum(b["paid"], "method", {"cash", "mobile_money", "card", "bank_transfer", "cheque", "other"});
            paidAccount = in::optUuid(b["paid"], "account_id");
        }
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                if (!tx.opt("SELECT 1 FROM currencies WHERE code = $1 AND active", currency))
                    throw invalid("currency", "unsupported_currency");
                std::string id = db::str(tx.one(
                    "INSERT INTO expenses (establishment_id, category_id, supplier_id, description, amount_minor, tax_minor, "
                    "currency, expense_date, due_date, paid_minor, created_by) VALUES ($1,$2,$3,$4,$5,$6,$7,"
                    "COALESCE($8::date, nh_local_today()),$9::date,$10,$11) RETURNING id",
                    ctx.actor.establishmentId, category, supplier, description, amount, tax, currency, date, due,
                    paidMethod ? amount : 0, ctx.actor.userId)[0]);
                if (paidMethod) {
                    if (paidAccount) {
                        auto a = tx.opt("SELECT currency FROM cash_accounts WHERE id = $1 AND active", *paidAccount);
                        if (!a || db::str((*a)[0]) != currency) throw invalid("paid.account_id", "account_currency_mismatch");
                    }
                    tx.q("INSERT INTO payments (establishment_id, direction, party_id, expense_id, method, account_id, "
                         "amount_minor, currency, paid_at, created_by) VALUES ($1,'out',$2,$3,$4,$5,$6,$7,"
                         "COALESCE($8::date::timestamptz, now()),$9)",
                         ctx.actor.establishmentId, supplier, id, *paidMethod, paidAccount, amount, currency, date,
                         ctx.actor.userId);
                }
                audit(tx, ctx.actor, "expense.created", "expense", id, {{"amount", money(amount, currency)}});
                return {201, expenseJson(tx.one(std::string(kExpenseSelect) + "WHERE e.id = $1", id))};
            });
        });
        ctx.json(status, body);
    });

    app.put("/api/v1/expenses/:id/receipt", {Access::User, "expenses.write", true}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "expense");
        const std::string& data = ctx.req.body;
        std::string mime = ctx.header("Content-Type");
        if (data.empty() || data.size() > 5 * 1024 * 1024) throw invalid("receipt", "size");
        if (mime.rfind("application/pdf", 0) == 0) {
            if (data.compare(0, 5, "%PDF-") != 0) throw invalid("receipt", "corrupted_pdf");
            mime = "application/pdf";
        } else {
            ImageInfo info = validateImage(data, mime);
            mime = info.mime;
        }
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            tx.one("SELECT id FROM expenses WHERE id = $1 FOR UPDATE", id);
            std::string oid = db::str(tx.one(
                "INSERT INTO objects (establishment_id, purpose, mime, byte_size, sha256, data, created_by) "
                "VALUES ($1,'receipt',$2,$3,$4,decode($5,'hex'),$6) RETURNING id",
                ctx.actor.establishmentId, mime, static_cast<int>(data.size()), crypto::sha256Hex(data), crypto::toHex(data),
                ctx.actor.userId)[0]);
            tx.q("UPDATE expenses SET receipt_object_id = $2 WHERE id = $1", id, oid);
            audit(tx, ctx.actor, "expense.receipt_attached", "expense", id);
        });
        ctx.noContent();
    });

    app.get("/api/v1/expenses/:id/receipt", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "expense");
        auto r = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return tx.opt("SELECT o.mime, encode(o.data,'hex') FROM expenses e JOIN objects o ON o.id = e.receipt_object_id "
                          "WHERE e.id = $1",
                          id);
        });
        if (!r) throw notFound("receipt");
        std::string mime = db::str((*r)[0]);
        ctx.file(*crypto::fromHex(db::str((*r)[1])), mime, mime == "application/pdf" ? "receipt.pdf" : "receipt", true);
    });
}

}  // namespace nh
