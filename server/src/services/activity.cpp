// Inside one establishment: activity journal ("who did what, when") and the
// per-employee activity report. Every write of the API is recorded in
// audit_log (append-only) with the user who made it.
//  - owner / admin / accountant (permission audit.read): the whole journal,
//    filterable by employee, action and period, exportable as CSV or PDF;
//  - every other user: only their own actions ("my activity").
#include <map>
#include <regex>

#include "nhill/money.hpp"
#include "services/report.hpp"
#include "services/sales.hpp"
#include "services/services.hpp"

namespace nh {

namespace {

// Short, readable summary of the JSON details (scalars only, no nested objects).
std::string detailsSummary(Env& env, const std::string& lang, const json& d) {
    if (!d.is_object()) return "";
    // Secrets, bulky values and technical identifiers (UUIDs, amounts in minor units) are not
    // shown to users: they are meaningless in the activity log (2.0.0).
    static const std::set<std::string> skip = {"password", "token", "refresh_token", "lines", "legal_ids", "user_id", "action",
                                               "session_id", "register_id", "receipt_id", "approved_by", "promo", "user_ids",
                                               "change_minor"};
    static const std::set<std::string> kDecimalKeys = {"amount", "total", "rate", "paid", "counted", "expected",
                                                       "difference", "opening_float", "cash_counted"};
    std::string out;
    for (auto it = d.begin(); it != d.end(); ++it) {
        if (skip.count(it.key()) || it->is_object() || it->is_array() || it->is_null()) continue;
        std::string v = it->is_string() ? it->get<std::string>() : it->dump();
        if (it->is_boolean()) v = env.i18n.t(lang, it->get<bool>() ? "detail.yes" : "detail.no");
        if (it.key() == "doc_type" || it.key() == "type") {
            // Document types are shown in the user's language ("invoice" -> "Facture").
            std::string t = env.i18n.t(lang, "doctype." + v);
            if (t != "doctype." + v) v = t;
        }
        if (it->is_string() && kDecimalKeys.count(it.key())) {
            // Amounts and rates are stored as exact decimal strings ("95000.00"): shown with
            // the user's digit grouping ("95 000,00"), never rounded (2.0.0).
            const int scale = it.key() == "rate" ? 10 : 2;
            if (auto n = nhill::parseDecimal(v, scale)) v = nhill::formatDecimal(*n, scale, lang, it.key() == "rate");
        }
        if (it.key() == "role") v = env.i18n.t(lang, "role." + v);
        if (v.empty()) continue;
        if (v.size() > 60) v = v.substr(0, 57) + "...";
        if (!out.empty()) out += " · ";
        // Readable label when known ("amount" -> "Montant"), technical key otherwise.
        std::string label = env.i18n.t(lang, "detail." + it.key());
        if (label == "detail." + it.key()) label = it.key();
        out += label + " : " + v;
        if (out.size() > 180) {
            out = out.substr(0, 177) + "...";
            break;
        }
    }
    return out;
}

struct Filters {
    std::optional<std::string> user, action, from, to;
};

Filters readFilters(const Ctx& ctx, bool seeAll) {
    Filters f;
    f.user = ctx.query("user_id");
    if (f.user && !in::isUuid(*f.user)) throw invalid("user_id", "invalid");
    if (!seeAll) f.user = ctx.actor.userId;  // own actions only
    f.action = ctx.query("action");
    static const std::regex actionRe("^[a-z_.]{1,60}$");
    if (f.action && (f.action->empty() || !std::regex_match(*f.action, actionRe))) f.action.reset();
    f.from = ctx.query("from");
    f.to = ctx.query("to");
    if (f.from && !in::isDate(*f.from)) throw invalid("from", "invalid_date");
    if (f.to && !in::isDate(*f.to)) throw invalid("to", "invalid_date");
    if (f.from && f.to && *f.from > *f.to) throw badRequest("invalid_period");
    return f;
}

constexpr const char* kActivityWhere =
    "WHERE a.establishment_id = $1 AND ($2::uuid IS NULL OR a.actor_id = $2) AND ($3::text IS NULL OR "
    "a.action LIKE $3 || '%') AND ($4::date IS NULL OR a.created_at >= nh_local_start($4::date)) AND ($5::date IS NULL OR "
    "a.created_at < nh_local_start($5::date + 1)) ";

// Readable reference of the object concerned (document number, product, contact...),
// resolved inside the establishment (row-level security applies).
constexpr const char* kActivitySelect =
    "SELECT a.id, a.actor_type, a.actor_id, COALESCE(u.full_name, ''), a.action, COALESCE(a.entity, ''), "
    "COALESCE(a.entity_id, ''), COALESCE(d.number, CASE WHEN pr.id IS NOT NULL THEN pr.sku || ' · ' || pr.name END, "
    "pt.name, NULLIF(pu.supplier_reference, ''), ''), a.details, COALESCE(a.ip, ''), nh_iso(a.created_at), "
    "to_char(a.created_at AT TIME ZONE nh_tz(a.establishment_id), 'YYYY-MM-DD HH24:MI') FROM audit_log a "
    "LEFT JOIN users u ON a.actor_type = 'user' AND u.id = a.actor_id "
    "LEFT JOIN sales_documents d ON a.entity = 'sales_document' AND d.id::text = a.entity_id "
    "LEFT JOIN products pr ON a.entity = 'product' AND pr.id::text = a.entity_id "
    "LEFT JOIN parties pt ON a.entity = 'party' AND pt.id::text = a.entity_id "
    "LEFT JOIN purchases pu ON a.entity = 'purchase' AND pu.id::text = a.entity_id ";

std::string actorLabel(Env& env, const std::string& lang, const std::string& type, const std::string& name) {
    if (type == "user") return name;
    return env.i18n.t(lang, "actor." + type);
}

struct StaffLine {
    int64_t invoices = 0, invoiced = 0, creditNotes = 0, credited = 0, paymentsIn = 0, paidIn = 0, expenses = 0,
            expensesAmount = 0;
};

}  // namespace

void registerActivityRoutes(App& app, Env& env) {
    app.get("/api/v1/activity", {Access::User}, [&env](Ctx& ctx) {
        const bool seeAll = App::roleHas(ctx.actor.role, "audit.read");
        Filters f = readFilters(ctx, seeAll);
        const std::string format = ctx.query("format").value_or("json");
        const std::string lang = ctx.actor.lang;
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            if (format == "csv" || format == "pdf") {
                Report r{"report.activity",
                         {"col.datetime", "col.user", "col.action", "col.object", "col.reference", "col.details"},
                         {Col::Text, Col::Text, Col::Text, Col::Text, Col::Text, Col::Text},
                         {-1, -1, -1, -1, -1, -1},
                         {},
                         true};
                // Exports are capped to keep the PDF readable; narrow the period beyond that.
                for (const auto& row : tx.q(std::string(kActivitySelect) + kActivityWhere + "ORDER BY a.id DESC LIMIT 5000",
                                            ctx.actor.establishmentId, f.user, f.action, f.from, f.to))
                    r.rows.push_back({db::str(row[11]), actorLabel(env, lang, db::str(row[1]), db::str(row[3])),
                                      env.i18n.t(lang, "action." + db::str(row[4])),
                                      db::str(row[5]).empty() ? "" : env.i18n.t(lang, "entity." + db::str(row[5])),
                                      db::str(row[7]), detailsSummary(env, lang, json::parse(db::str(row[8])))});
                Period p{f.from.value_or(""), f.to.value_or("")};
                if (f.user && seeAll) {
                    auto u = tx.opt("SELECT full_name FROM users WHERE id = $1", *f.user);
                    if (u) r.extraSubtitle = env.i18n.t(lang, "col.user") + " : " + db::str((*u)[0]);
                } else if (!seeAll) {
                    r.extraSubtitle = env.i18n.t(lang, "col.user") + " : " + ctx.actor.fullName;
                }
                audit(tx, ctx.actor, "activity.exported", "audit_log", "",
                      {{"format", format}, {"user_id", f.user.value_or("")}, {"action", f.action.value_or("")}});
                sendReport(env, ctx, tx, r, "journal_activite", f.from && f.to ? &p : nullptr);
                return;
            }
            auto pg = page(ctx);
            int total = tx.one(std::string("SELECT count(*) FROM audit_log a ") + kActivityWhere, ctx.actor.establishmentId,
                               f.user, f.action, f.from, f.to)[0]
                            .as<int>();
            json items = json::array();
            for (const auto& row : tx.q(std::string(kActivitySelect) + kActivityWhere + "ORDER BY a.id DESC LIMIT $6 OFFSET $7",
                                        ctx.actor.establishmentId, f.user, f.action, f.from, f.to, pg.limit, pg.offset)) {
                std::string action = db::str(row[4]);
                items.push_back({{"id", row[0].as<int64_t>()},
                                 {"actor_type", db::str(row[1])},
                                 {"actor_id", db::str(row[2])},
                                 {"actor", actorLabel(env, lang, db::str(row[1]), db::str(row[3]))},
                                 {"action", action},
                                 {"action_label", env.i18n.t(lang, "action." + action)},
                                 {"entity", db::str(row[5])},
                                 {"entity_id", db::str(row[6])},
                                 {"reference", db::str(row[7])},
                                 {"summary", detailsSummary(env, lang, json::parse(db::str(row[8])))},
                                 {"ip", seeAll ? db::str(row[9]) : ""},
                                 {"created_at", db::str(row[10])}});
            }
            json people = json::array();
            if (seeAll)
                for (const auto& m : tx.q("SELECT u.id, u.full_name, m.role, m.status FROM memberships m JOIN users u ON "
                                          "u.id = m.user_id WHERE m.establishment_id = $1 ORDER BY u.full_name",
                                          ctx.actor.establishmentId))
                    people.push_back({{"id", db::str(m[0])}, {"full_name", db::str(m[1])}, {"role", db::str(m[2])},
                                      {"status", db::str(m[3])}});
            ctx.json(200, {{"items", items}, {"total", total}, {"scope", seeAll ? "establishment" : "self"},
                           {"members", people}});
        });
    });

    // What each employee did over a period: documents issued, money recorded,
    // expenses, stock corrections. Amounts per currency, never converted.
    app.get("/api/v1/reports/staff", {Access::User, "audit.read"}, [&env](Ctx& ctx) {
        Period p = period(ctx);
        const std::string lang = ctx.actor.lang;
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            struct Person {
                std::string name, role;
                int64_t created = 0, adjustments = 0;
                std::string last;
                std::map<std::string, StaffLine> byCur;
            };
            std::map<std::string, Person> people;
            std::vector<std::string> order;
            for (const auto& m : tx.q("SELECT u.id, u.full_name, m.role FROM memberships m JOIN users u ON u.id = m.user_id "
                                      "WHERE m.establishment_id = $1 ORDER BY m.role = 'owner' DESC, u.full_name",
                                      ctx.actor.establishmentId)) {
                people[db::str(m[0])] = Person{db::str(m[1]), db::str(m[2])};
                order.push_back(db::str(m[0]));
            }
            auto has = [&](const pqxx::field& f) { return !f.is_null() && people.count(db::str(f)); };
            for (const auto& r : tx.q("SELECT issued_by, currency, count(*) FILTER (WHERE doc_type = 'invoice'), "
                                      "COALESCE(sum(total_minor) FILTER (WHERE doc_type = 'invoice'), 0), "
                                      "count(*) FILTER (WHERE doc_type = 'credit_note'), "
                                      "COALESCE(sum(total_minor) FILTER (WHERE doc_type = 'credit_note'), 0) "
                                      "FROM sales_documents WHERE status <> 'draft' AND doc_type IN ('invoice','credit_note') "
                                      "AND issue_date BETWEEN $1::date AND $2::date GROUP BY 1, 2",
                                      p.from, p.to)) {
                if (!has(r[0])) continue;
                auto& l = people[db::str(r[0])].byCur[db::str(r[1])];
                l.invoices = r[2].as<int64_t>();
                l.invoiced = r[3].as<int64_t>();
                l.creditNotes = r[4].as<int64_t>();
                l.credited = r[5].as<int64_t>();
            }
            for (const auto& r : tx.q("SELECT created_by, currency, count(*), sum(amount_minor) FROM payments WHERE "
                                      "direction = 'in' AND paid_at >= nh_local_start($1::date) AND paid_at < nh_local_start($2::date + 1) GROUP BY 1, 2",
                                      p.from, p.to)) {
                if (!has(r[0])) continue;
                auto& l = people[db::str(r[0])].byCur[db::str(r[1])];
                l.paymentsIn = r[2].as<int64_t>();
                l.paidIn = r[3].as<int64_t>();
            }
            for (const auto& r : tx.q("SELECT created_by, currency, count(*), sum(amount_minor) FROM expenses WHERE "
                                      "expense_date BETWEEN $1::date AND $2::date GROUP BY 1, 2",
                                      p.from, p.to)) {
                if (!has(r[0])) continue;
                auto& l = people[db::str(r[0])].byCur[db::str(r[1])];
                l.expenses = r[2].as<int64_t>();
                l.expensesAmount = r[3].as<int64_t>();
            }
            for (const auto& r : tx.q("SELECT created_by, count(*) FROM sales_documents WHERE created_at >= nh_local_start($1::date) AND "
                                      "created_at < nh_local_start($2::date + 1) GROUP BY 1",
                                      p.from, p.to))
                if (has(r[0])) people[db::str(r[0])].created = r[1].as<int64_t>();
            for (const auto& r : tx.q("SELECT created_by, count(*) FROM stock_movements WHERE source_type IN ('manual', "
                                      "'transfer') AND created_at >= nh_local_start($1::date) AND created_at < nh_local_start($2::date + 1) GROUP BY 1",
                                      p.from, p.to))
                if (has(r[0])) people[db::str(r[0])].adjustments = r[1].as<int64_t>();
            for (const auto& r : tx.q("SELECT actor_id, to_char(max(created_at) AT TIME ZONE nh_tz($1::uuid), 'YYYY-MM-DD HH24:MI') "
                                      "FROM audit_log WHERE establishment_id = $1 AND actor_type = 'user' GROUP BY 1",
                                      ctx.actor.establishmentId))
                if (has(r[0])) people[db::str(r[0])].last = db::str(r[1]);

            Report r{"report.staff",
                     {"col.employee", "col.role", "col.documents_created", "col.currency", "col.invoices_issued",
                      "col.invoiced_total", "col.credit_notes", "col.credited_total", "col.payments_in_count",
                      "col.payments_in", "col.expenses_count", "col.expenses", "col.stock_corrections",
                      "col.last_activity"},
                     {Col::Text, Col::Text, Col::Int, Col::Text, Col::Int, Col::Money, Col::Int, Col::Money, Col::Int,
                      Col::Money, Col::Int, Col::Money, Col::Int, Col::Text},
                     {-1, -1, -1, -1, -1, 3, -1, 3, -1, 3, -1, 3, -1, -1},
                     {},
                     true};
            for (const auto& id : order) {
                const Person& pp = people[id];
                std::string role = env.i18n.t(lang, "role." + pp.role);
                if (pp.byCur.empty()) {
                    r.rows.push_back({pp.name, role, std::to_string(pp.created), "", "0", "", "0", "", "0", "", "0", "",
                                      std::to_string(pp.adjustments), pp.last});
                    continue;
                }
                bool first = true;
                for (const auto& [cur, l] : pp.byCur) {
                    r.rows.push_back({pp.name, role, first ? std::to_string(pp.created) : "", cur, std::to_string(l.invoices),
                                      money(l.invoiced, cur), std::to_string(l.creditNotes), money(l.credited, cur),
                                      std::to_string(l.paymentsIn), money(l.paidIn, cur), std::to_string(l.expenses),
                                      money(l.expensesAmount, cur), first ? std::to_string(pp.adjustments) : "",
                                      first ? pp.last : ""});
                    first = false;
                }
            }
            sendReport(env, ctx, tx, r, "activite_par_employe_" + p.from + "_" + p.to, &p);
        });
    });
}

}  // namespace nh
