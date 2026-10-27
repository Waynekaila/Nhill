// Nhill POS: cash registers, cash sessions, sales receipts, returns, voids, cash
// movements, supervisor approvals and closing reports.
//
// Every operation is decided here, on the server: prices and taxes come from the
// catalogue, discounts above the establishment limit, price overrides, returns,
// voids and cash withdrawals need the corresponding permission or a one-time
// approval by a supervisor of the same establishment. Everything is audited.
// Receipts are sales documents (type "receipt"), so stock, payments and reports are
// shared with Nhill Business.
#include <map>
#include <set>

#include "nhill/money.hpp"
#include "pdf/pdf.hpp"
#include "services/sales.hpp"
#include "services/entitlements.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"

namespace nh {

std::string upperAscii(std::string s);

namespace {

bool has(const Actor& a, const char* perm) { return App::roleHas(a.role, perm); }

// POS settings row, cash method and walk-in customer are created on first use.
pqxx::row ensureSettings(db::Tx& tx, const Actor& a) {
    tx.q("INSERT INTO pos_settings (establishment_id) VALUES ($1) ON CONFLICT DO NOTHING", a.establishmentId);
    auto s = tx.one("SELECT walk_in_party_id, max_discount_bp, allow_price_override, receipt_footer, receipt_width_mm "
                    "FROM pos_settings WHERE establishment_id = $1 FOR UPDATE",
                    a.establishmentId);
    if (s[0].is_null()) {
        auto lang = tx.one("SELECT preferred_language FROM establishments WHERE id = $1", a.establishmentId);
        std::string name = db::str(lang[0]) == "en" ? "Walk-in customer" : "Client comptoir";
        auto p = tx.one("INSERT INTO parties (establishment_id, is_customer, kind, name, notes) VALUES ($1, true, "
                        "'individual', $2, 'Nhill POS') RETURNING id",
                        a.establishmentId, name);
        tx.q("UPDATE pos_settings SET walk_in_party_id = $2 WHERE establishment_id = $1", a.establishmentId, db::str(p[0]));
        s = tx.one("SELECT walk_in_party_id, max_discount_bp, allow_price_override, receipt_footer, receipt_width_mm "
                   "FROM pos_settings WHERE establishment_id = $1",
                   a.establishmentId);
    }
    if (tx.one("SELECT count(*) FROM pos_payment_methods WHERE kind = 'cash'")[0].as<int>() == 0) {
        auto lang = tx.one("SELECT preferred_language FROM establishments WHERE id = $1", a.establishmentId);
        tx.q("INSERT INTO pos_payment_methods (establishment_id, code, label, kind, sort_order) VALUES ($1, 'cash', $2, "
             "'cash', 0) ON CONFLICT DO NOTHING",
             a.establishmentId, db::str(lang[0]) == "en" ? "Cash" : "Espèces");
    }
    return s;
}

// Till limit of the plan (null = unlimited).
json registerLimitJson(db::Tx& tx, const Actor& a) {
    auto snap = entitlements::load(tx, a.establishmentId, a.establishmentKind);
    auto it = snap.limits.find(entitlements::key::kRegisters);
    return it == snap.limits.end() || !it->second ? json(nullptr) : json(*it->second);
}

// Without several warehouses in the plan, a till sells from the main warehouse.
std::string registerWarehouse(db::Tx& tx, const Actor& a, const json& b) {
    std::string main = db::str(tx.one("SELECT id FROM warehouses WHERE is_default")[0]);
    if (!b.contains("warehouse_id") || b["warehouse_id"].is_null()) return main;
    std::string wh = in::reqUuid(b, "warehouse_id");
    if (wh != main) entitlements::requireFeature(a, entitlements::key::kMultiWarehouse);
    return wh;
}

bool assigned(db::Tx& tx, const Actor& a, const std::string& registerId) {
    return has(a, "pos.supervise") ||
           !tx.q("SELECT 1 FROM pos_register_users WHERE register_id = $1 AND user_id = $2", registerId, a.userId).empty();
}

// Consumes a one-time supervisor approval; returns the approver id.
std::string consumeApproval(db::Tx& tx, const Actor& a, const json& b, const std::string& action,
                            const std::string& sessionId) {
    auto token = in::optStr(b, "approval_token", 100);
    if (!token) throw ApiError(403, "approval_required", "a supervisor approval is required", {{"action", action}});
    auto r = tx.opt("UPDATE pos_approvals SET used_at = now() WHERE token_hash = decode($1, 'hex') AND action = $2 "
                    "AND used_at IS NULL AND expires_at > now() AND requested_by = $3 AND (session_id IS NULL OR "
                    "session_id = $4) RETURNING approver_id",
                    crypto::sha256Hex(*token), action, a.userId, sessionId);
    if (!r) throw ApiError(403, "approval_invalid", "invalid, expired or already used approval");
    return db::str((*r)[0]);
}

// Approval needed unless the user holds the permission itself.
std::optional<std::string> authorize(db::Tx& tx, const Actor& a, const json& b, const char* perm, const std::string& action,
                                     const std::string& sessionId) {
    if (has(a, perm)) return std::nullopt;
    return consumeApproval(tx, a, b, action, sessionId);
}

struct Session {
    std::string id, registerId, registerCode, registerName, warehouseId, currency, status, openedBy;
    int number = 0;
};

Session loadSession(db::Tx& tx, const Actor& a, const std::string& id, bool lock, bool requireOpen = true) {
    auto r = tx.opt(std::string("SELECT s.id, s.register_id, r.code, r.name, r.warehouse_id, s.currency, s.status, s.opened_by, "
                                "s.number FROM pos_sessions s JOIN pos_registers r ON r.id = s.register_id WHERE s.id = $1") +
                        (lock ? " FOR UPDATE OF s" : ""),
                    id);
    if (!r) throw notFound("session");
    Session s{db::str((*r)[0]), db::str((*r)[1]), db::str((*r)[2]), db::str((*r)[3]), db::str((*r)[4]),
              db::str((*r)[5]), db::str((*r)[6]), db::str((*r)[7]), (*r)[8].as<int>()};
    if (requireOpen && s.status != "open") throw conflict("session_closed");
    // A cashier only works in the session they opened, on a register assigned to them.
    if (s.openedBy != a.userId && !has(a, "pos.supervise")) throw notFound("session");
    return s;
}

struct Method {
    std::string id, code, label, kind;
    std::optional<std::string> account;
};
std::map<std::string, Method> methods(db::Tx& tx, bool activeOnly) {
    std::map<std::string, Method> m;
    for (const auto& r : tx.q("SELECT id, code, label, kind, account_id FROM pos_payment_methods WHERE ($1 = false OR active) "
                              "ORDER BY sort_order, code",
                              activeOnly)) {
        Method x{db::str(r[0]), db::str(r[1]), db::str(r[2]), db::str(r[3]), std::nullopt};
        if (!r[4].is_null()) x.account = db::str(r[4]);
        m[x.code] = x;
    }
    return m;
}

int combineBp(int a, int b) { return 10000 - static_cast<int>((int64_t(10000 - a) * (10000 - b)) / 10000); }

// Records the payments (or refunds) of a POS document; returns the change given back.
int64_t recordPayments(db::Tx& tx, const Actor& a, const Session& s, const std::string& docId, const std::string& number,
                       const json& list, int64_t due, const std::string& direction) {
    if (!list.is_array()) throw invalid("payments", "array_required");
    auto ms = methods(tx, true);
    int64_t sum = 0, change = 0;
    struct P {
        Method m;
        int64_t amount;
    };
    std::vector<P> ps;
    for (size_t i = 0; i < list.size(); ++i) {
        const json& p = list[i];
        std::string f = "payments[" + std::to_string(i) + "].";
        std::string code = in::reqStr(p, "method", 24);
        auto it = ms.find(code);
        if (it == ms.end()) throw invalid(f + "method", "unknown_payment_method");
        int64_t amount = in::reqMoney(p, "amount", s.currency, false);
        if (auto t = in::optMoney(p, "tendered", s.currency)) {
            if (it->second.kind != "cash" || direction != "in") throw invalid(f + "tendered", "cash_only");
            if (*t < amount) throw invalid(f + "tendered", "less_than_amount");
            change += *t - amount;
        }
        sum += amount;
        ps.push_back({it->second, amount});
    }
    if (sum != due) throw invalid("payments", "payments_must_equal_total");
    for (const auto& p : ps)
        tx.q("INSERT INTO payments (establishment_id, direction, document_id, party_id, method, account_id, amount_minor, "
             "currency, reference, note, created_by) SELECT $1, $2, d.id, d.party_id, $3, $4, $5, $6, $7, $8, $9 "
             "FROM sales_documents d WHERE d.id = $10",
             a.establishmentId, direction, p.m.kind, p.m.account, p.amount, s.currency,
             "POS " + s.registerCode + "/" + std::to_string(s.number) + " " + number, "pos:" + p.m.code, a.userId, docId);
    return change;
}

// Session figures (sales, returns, takings per method, cash expected).
json summary(db::Tx& tx, const std::string& sessionId, bool withExpected) {
    auto s = tx.one("SELECT s.id, s.number, s.status, s.currency, nh_iso(s.opened_at), nh_iso(s.closed_at), "
                    "s.opening_float_minor, uo.full_name, uc.full_name, r.code, r.name, r.location, s.counted, s.expected, "
                    "s.cash_expected_minor, s.cash_counted_minor, s.cash_variance_minor, s.closing_note, s.register_id, "
                    "s.opened_by FROM pos_sessions s JOIN pos_registers r ON r.id = s.register_id JOIN users uo ON "
                    "uo.id = s.opened_by LEFT JOIN users uc ON uc.id = s.closed_by WHERE s.id = $1",
                    sessionId);
    const std::string cur = db::str(s[3]);
    auto m = [&](int64_t v) { return money(v, cur); };
    auto docs = tx.one("SELECT count(*) FILTER (WHERE doc_type = 'receipt'), "
                       "COALESCE(sum(total_minor) FILTER (WHERE doc_type = 'receipt'), 0), "
                       "count(*) FILTER (WHERE doc_type = 'credit_note'), "
                       "COALESCE(sum(total_minor) FILTER (WHERE doc_type = 'credit_note'), 0), "
                       "COALESCE(sum(discount_minor) FILTER (WHERE doc_type = 'receipt'), 0), "
                       "COALESCE(sum(net_minor) FILTER (WHERE doc_type = 'receipt'), 0) - "
                       "COALESCE(sum(net_minor) FILTER (WHERE doc_type = 'credit_note'), 0) "
                       "FROM sales_documents WHERE pos_session_id = $1 AND status <> 'draft'",
                       sessionId);
    json taxes = json::array();
    for (const auto& r : tx.q("SELECT (t->>'rate_bp')::int, sum(CASE WHEN d.doc_type = 'credit_note' THEN -1 ELSE 1 END * "
                              "(t->>'base_minor')::bigint), sum(CASE WHEN d.doc_type = 'credit_note' THEN -1 ELSE 1 END * "
                              "(t->>'tax_minor')::bigint) FROM sales_documents d, jsonb_array_elements(d.tax_breakdown) t "
                              "WHERE d.pos_session_id = $1 AND d.status <> 'draft' GROUP BY 1 ORDER BY 1",
                              sessionId))
        taxes.push_back({{"rate_bp", r[0].as<int>()}, {"base", m(r[1].as<int64_t>())}, {"tax", m(r[2].as<int64_t>())}});
    // Takings per payment method of the session (payments of its receipts and refunds).
    std::map<std::string, int64_t> net;
    json byMethod = json::array();
    auto ms = methods(tx, false);
    for (const auto& r : tx.q("SELECT substr(p.note, 5), COALESCE(sum(p.amount_minor) FILTER (WHERE p.direction = 'in'), 0), "
                              "COALESCE(sum(p.amount_minor) FILTER (WHERE p.direction = 'out'), 0) FROM payments p JOIN "
                              "sales_documents d ON d.id = p.document_id WHERE d.pos_session_id = $1 AND p.note LIKE 'pos:%' "
                              "GROUP BY 1 ORDER BY 1",
                              sessionId)) {
        std::string code = db::str(r[0]);
        int64_t in = r[1].as<int64_t>(), out = r[2].as<int64_t>();
        net[code] = in - out;
        auto it = ms.find(code);
        byMethod.push_back({{"method", code}, {"label", it == ms.end() ? code : it->second.label},
                            {"kind", it == ms.end() ? "other" : it->second.kind}, {"received", m(in)}, {"refunded", m(out)},
                            {"net", m(in - out)}});
    }
    auto mv = tx.one("SELECT COALESCE(sum(amount_minor) FILTER (WHERE kind = 'cash_in'), 0), "
                     "COALESCE(sum(amount_minor) FILTER (WHERE kind = 'cash_out'), 0) FROM pos_cash_movements "
                     "WHERE session_id = $1",
                     sessionId);
    json movements = json::array();
    for (const auto& r : tx.q("SELECT c.kind, c.amount_minor, c.reason, u.full_name, nh_iso(c.created_at), a.full_name "
                              "FROM pos_cash_movements c JOIN users u ON u.id = c.created_by LEFT JOIN users a ON "
                              "a.id = c.approved_by WHERE c.session_id = $1 ORDER BY c.created_at",
                              sessionId))
        movements.push_back({{"kind", db::str(r[0])}, {"amount", m(r[1].as<int64_t>())}, {"reason", db::str(r[2])},
                             {"by", db::str(r[3])}, {"at", db::str(r[4])}, {"approved_by", db::str(r[5])}});
    int64_t cashNet = 0;
    for (const auto& [code, v] : net) {
        auto it = ms.find(code);
        if (it != ms.end() && it->second.kind == "cash") cashNet += v;
    }
    int64_t cashExpected = s[6].as<int64_t>() + cashNet + mv[0].as<int64_t>() - mv[1].as<int64_t>();
    json out = {{"id", db::str(s[0])},
                {"number", s[1].as<int>()},
                {"status", db::str(s[2])},
                {"currency", cur},
                {"opened_at", db::str(s[4])},
                {"closed_at", s[5].is_null() ? json(nullptr) : json(db::str(s[5]))},
                {"opening_float", m(s[6].as<int64_t>())},
                {"opened_by_name", db::str(s[7])},
                {"closed_by_name", s[8].is_null() ? json(nullptr) : json(db::str(s[8]))},
                {"register", {{"id", db::str(s[18])}, {"code", db::str(s[9])}, {"name", db::str(s[10])}, {"location", db::str(s[11])}}},
                {"opened_by", db::str(s[19])},
                {"sales_count", docs[0].as<int>()},
                {"sales_total", m(docs[1].as<int64_t>())},
                {"returns_count", docs[2].as<int>()},
                {"returns_total", m(docs[3].as<int64_t>())},
                {"net_total", m(docs[1].as<int64_t>() - docs[3].as<int64_t>())},
                {"net_excl_tax", m(docs[5].as<int64_t>())},
                {"discounts", m(docs[4].as<int64_t>())},
                {"taxes", taxes},
                {"by_method", byMethod},
                {"cash_in", m(mv[0].as<int64_t>())},
                {"cash_out", m(mv[1].as<int64_t>())},
                {"cash_movements", movements},
                {"closing_note", db::str(s[17])}};
    if (withExpected) {
        out["cash_expected"] = m(cashExpected);
        json exp = json::object();
        for (const auto& [code, v] : net) exp[code] = m(v);
        // Cash methods share the drawer: their expected amount is the whole drawer.
        for (const auto& [code, mth] : ms)
            if (mth.kind == "cash") exp[code] = m(cashExpected);
        out["expected"] = exp;
        if (!s[12].is_null()) {
            json counted = json::parse(db::str(s[12]));
            json c = json::object();
            for (auto it = counted.begin(); it != counted.end(); ++it) c[it.key()] = m(it.value().get<int64_t>());
            out["counted"] = c;
            out["cash_counted"] = s[15].is_null() ? json(nullptr) : json(m(s[15].as<int64_t>()));
            out["cash_variance"] = s[16].is_null() ? json(nullptr) : json(m(s[16].as<int64_t>()));
        }
    }
    out["_cash_expected_minor"] = cashExpected;
    out["_net"] = net;
    return out;
}

json registerJson(const pqxx::row& r) {
    return {{"id", db::str(r[0])},          {"code", db::str(r[1])},         {"name", db::str(r[2])},
            {"location", db::str(r[3])},    {"warehouse_id", db::str(r[4])}, {"warehouse", db::str(r[5])},
            {"active", db::boolean(r[6])},  {"users", json::parse(db::str(r[7]))},
            {"open_session", r[8].is_null() ? json(nullptr)
                                            : json{{"id", db::str(r[8])}, {"number", r[9].as<int>()},
                                                   {"opened_by", db::str(r[10])}, {"opened_by_name", db::str(r[11])},
                                                   {"opened_at", db::str(r[12])}}}};
}
constexpr const char* kRegisterSelect =
    "SELECT r.id, r.code, r.name, r.location, r.warehouse_id, w.name, r.active, "
    "COALESCE((SELECT jsonb_agg(jsonb_build_object('id', u.id, 'full_name', u.full_name)) FROM pos_register_users ru "
    "JOIN users u ON u.id = ru.user_id WHERE ru.register_id = r.id), '[]'::jsonb), s.id, s.number, s.opened_by, "
    "uo.full_name, nh_iso(s.opened_at) FROM pos_registers r JOIN warehouses w ON w.id = r.warehouse_id "
    "LEFT JOIN pos_sessions s ON s.register_id = r.id AND s.status = 'open' LEFT JOIN users uo ON uo.id = s.opened_by ";

json methodsJson(db::Tx& tx, bool activeOnly) {
    json out = json::array();
    for (const auto& r : tx.q("SELECT m.id, m.code, m.label, m.kind, m.account_id, c.name, m.active, m.sort_order FROM "
                              "pos_payment_methods m LEFT JOIN cash_accounts c ON c.id = m.account_id WHERE ($1 = false OR "
                              "m.active) ORDER BY m.sort_order, m.code",
                              activeOnly))
        out.push_back({{"id", db::str(r[0])}, {"code", db::str(r[1])}, {"label", db::str(r[2])}, {"kind", db::str(r[3])},
                       {"account_id", r[4].is_null() ? json(nullptr) : json(db::str(r[4]))},
                       {"account", r[5].is_null() ? json(nullptr) : json(db::str(r[5]))}, {"active", db::boolean(r[6])},
                       {"sort_order", r[7].as<int>()}});
    return out;
}

// Cashiers read their own sessions; reporting roles (accountant, manager) read them all
// from Nhill Business without being allowed to sell.
void sellOrReport(const Actor& a) {
    if (!has(a, "pos.sell") && !has(a, "pos.report")) throw forbidden("permission_denied");
}

json posPermissions(const Actor& a) {
    json p = json::array();
    for (const char* x : {"pos.sell", "pos.refund", "pos.void", "pos.cash", "pos.supervise", "pos.manage", "pos.report"})
        if (has(a, x)) p.push_back(x);
    return p;
}

// ------------------------------------------------------------------ tickets (PDF)
std::string fmtMoney(const std::string& plain, const std::string& cur, const std::string& lang) {
    auto v = nhill::parseMoney(plain, minorUnits(cur));
    std::string s = nhill::formatDecimal(v.value_or(0), minorUnits(cur), lang);
    for (size_t p; (p = s.find("\xE2\x80\xAF")) != std::string::npos;) s.replace(p, 3, "\xC2\xA0");
    return s + " " + cur;
}

std::string localTime(db::Tx& tx, const std::string& iso, const std::string& est) {
    if (iso.empty()) return "";
    return db::str(tx.one("SELECT to_char($1::timestamptz AT TIME ZONE nh_tz($2::uuid), 'DD/MM/YYYY HH24:MI')", iso, est)[0]);
}

void identityBlocks(db::Tx& tx, const std::string& est, pdf::TicketView& v) {
    auto e = tx.one("SELECT e.trade_name, e.address, e.city, e.phone, o.mime, encode(o.data, 'hex') FROM establishments e "
                    "LEFT JOIN objects o ON o.id = e.logo_object_id WHERE e.id = $1",
                    est);
    if (!e[5].is_null()) {
        v.logoBytes = *crypto::fromHex(db::str(e[5]));
        v.logoMime = db::str(e[4]);
    }
    v.blocks.push_back({pdf::TicketBlock::Title, db::str(e[0])});
    std::string addr = db::str(e[1]) + (db::str(e[2]).empty() ? "" : (db::str(e[1]).empty() ? "" : ", ") + db::str(e[2]));
    if (!addr.empty()) v.blocks.push_back({pdf::TicketBlock::Center, addr});
    if (!db::str(e[3]).empty()) v.blocks.push_back({pdf::TicketBlock::Center, db::str(e[3])});
    std::string ids;
    for (const auto& r : tx.q("SELECT kind, value FROM establishment_identifiers WHERE establishment_id = $1 ORDER BY kind", est))
        {
            std::string k = upperAscii(db::str(r[0]));
            for (auto& c : k) if (c == '_') c = ' ';
            ids += (ids.empty() ? "" : " · ") + k + " " + db::str(r[1]);
        }
    if (!ids.empty()) v.blocks.push_back({pdf::TicketBlock::Center, ids});
}

std::string receiptPdf(Env& env, db::Tx& tx, const Actor& a, const std::string& docId) {
    json d = loadSalesDocument(tx, docId);
    auto x = tx.one("SELECT d.pos_session_id, s.number, r.code, r.name, u.full_name, nh_iso(d.issued_at), p.name, "
                    "ps.receipt_footer, ps.receipt_width_mm, ps.walk_in_party_id = d.party_id, rel.number "
                    "FROM sales_documents d JOIN pos_sessions s ON s.id = d.pos_session_id JOIN pos_registers r ON "
                    "r.id = s.register_id LEFT JOIN users u ON u.id = d.issued_by JOIN parties p ON p.id = d.party_id "
                    "LEFT JOIN pos_settings ps ON ps.establishment_id = d.establishment_id LEFT JOIN sales_documents rel "
                    "ON rel.id = d.related_document_id WHERE d.id = $1",
                    docId);
    const std::string lang = d["language"], cur = d["currency"];
    auto t = [&](const std::string& k) { return env.i18n.t(lang, k); };
    pdf::TicketView v;
    v.widthMm = x[8].is_null() ? 80.0f : static_cast<float>(x[8].as<int>());
    v.title = d["number"].get<std::string>();
    v.creationDateIso = db::str(x[5]);
    identityBlocks(tx, a.establishmentId, v);
    v.blocks.push_back({pdf::TicketBlock::Rule});
    bool credit = d["doc_type"] == "credit_note";
    v.blocks.push_back({pdf::TicketBlock::KVStrong, credit ? t("pos.return") : t("doc.receipt"), d["number"]});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.date"), localTime(tx, db::str(x[5]), a.establishmentId)});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.register"), db::str(x[2]) + " · " + t("pos.session") + " " +
                                                                    std::to_string(x[1].as<int>())});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.cashier"), db::str(x[4])});
    if (!x[9].is_null() && !db::boolean(x[9])) v.blocks.push_back({pdf::TicketBlock::KV, t("pos.customer"), db::str(x[6])});
    if (credit && !x[10].is_null()) v.blocks.push_back({pdf::TicketBlock::KV, t("pos.original"), db::str(x[10])});
    v.blocks.push_back({pdf::TicketBlock::Rule});
    for (const auto& l : d["lines"]) {
        std::string q = l["quantity"].get<std::string>();
        std::string detail = q + " × " + fmtMoney(l["unit_price"], cur, lang);
        if (l.value("discount_bp", 0) > 0) detail += " (−" + nhill::formatDecimal(l["discount_bp"].get<int>(), 2, lang, true) + " %)";
        v.blocks.push_back({pdf::TicketBlock::Item, l["description"], detail, fmtMoney(l["net"], cur, lang)});
    }
    v.blocks.push_back({pdf::TicketBlock::Rule});
    if (d["discount"] != money(0, cur)) v.blocks.push_back({pdf::TicketBlock::KV, t("pos.discount"), "−" + fmtMoney(d["discount"], cur, lang)});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.subtotal"), fmtMoney(d["net"], cur, lang)});
    for (const auto& tb : d["taxes"]) {
        std::string rate = nhill::formatDecimal(tb["rate_bp"].get<int>(), 2, lang, true) + " %";
        v.blocks.push_back({pdf::TicketBlock::KV, t("pos.tax") + " " + rate, fmtMoney(tb["tax"], cur, lang)});
    }
    v.blocks.push_back({pdf::TicketBlock::KVStrong, t("pos.total"), fmtMoney(d["total"], cur, lang)});
    auto ms = methods(tx, false);
    int64_t paid = 0;
    for (const auto& p : tx.q("SELECT substr(note, 5), amount_minor FROM payments WHERE document_id = $1 ORDER BY created_at",
                              docId)) {
        auto it = ms.find(db::str(p[0]));
        paid += p[1].as<int64_t>();
        v.blocks.push_back({pdf::TicketBlock::KV, (credit ? t("pos.refund") + " · " : "") + (it == ms.end() ? db::str(p[0]) : it->second.label),
                            fmtMoney(money(p[1].as<int64_t>(), cur), cur, lang)});
    }
    auto change = tx.opt("SELECT (details->>'change_minor')::bigint FROM audit_log WHERE establishment_id = $2 AND "
                         "entity = 'sales_document' AND entity_id = $1 AND action = 'pos.sale' LIMIT 1",
                         docId, a.establishmentId);
    if (change && !(*change)[0].is_null() && (*change)[0].as<int64_t>() > 0)
        v.blocks.push_back({pdf::TicketBlock::KV, t("pos.change"), fmtMoney(money((*change)[0].as<int64_t>(), cur), cur, lang)});
    v.blocks.push_back({pdf::TicketBlock::Rule});
    if (!x[7].is_null() && !db::str(x[7]).empty()) v.blocks.push_back({pdf::TicketBlock::Center, db::str(x[7])});
    v.blocks.push_back({pdf::TicketBlock::Center, t("pos.thanks")});
    v.blocks.push_back({pdf::TicketBlock::Space});
    v.blocks.push_back({pdf::TicketBlock::Muted, t("pos.receipt_notice")});
    v.blocks.push_back({pdf::TicketBlock::Muted, t("pos.generated") + (d["pdf_sha256"].is_string() ? " · " + d["pdf_sha256"].get<std::string>().substr(0, 12) : "")});
    return pdf::renderTicket(v, pdf::Fonts::fromDir(env.cfg.fontDir));
}

std::string sessionPdf(Env& env, db::Tx& tx, const Actor& a, const std::string& sessionId, bool withExpected) {
    json s = summary(tx, sessionId, withExpected);
    auto lang = db::str(tx.one("SELECT document_language FROM establishment_settings WHERE establishment_id = $1",
                               a.establishmentId)[0]);
    const std::string cur = s["currency"];
    auto t = [&](const std::string& k) { return env.i18n.t(lang, k); };
    auto M = [&](const json& v) { return fmtMoney(v.get<std::string>(), cur, lang); };
    auto neg = [&](const json& v) { return v.get<std::string>() == money(0, cur) ? M(v) : "−" + M(v); };
    pdf::TicketView v;
    auto w = tx.opt("SELECT receipt_width_mm FROM pos_settings WHERE establishment_id = $1", a.establishmentId);
    v.widthMm = w ? static_cast<float>((*w)[0].as<int>()) : 80.0f;
    bool closed = s["status"] == "closed";
    v.title = std::string(closed ? "Z" : "X") + " " + s["register"]["code"].get<std::string>() + "-" + std::to_string(s["number"].get<int>());
    v.creationDateIso = closed ? s["closed_at"].get<std::string>() : db::str(tx.one("SELECT nh_iso(now())")[0]);
    identityBlocks(tx, a.establishmentId, v);
    v.blocks.push_back({pdf::TicketBlock::Rule});
    v.blocks.push_back({pdf::TicketBlock::Center, closed ? t("pos.z_report") : t("pos.x_report")});
    v.blocks.push_back({pdf::TicketBlock::KVStrong, t("pos.register") + " " + s["register"]["code"].get<std::string>(),
                        (closed ? "Z " : t("pos.session") + " ") + std::to_string(s["number"].get<int>())});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.opened"), localTime(tx, s["opened_at"], a.establishmentId)});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.opened_by"), s["opened_by_name"]});
    if (closed) {
        v.blocks.push_back({pdf::TicketBlock::KV, t("pos.closed"), localTime(tx, s["closed_at"], a.establishmentId)});
        v.blocks.push_back({pdf::TicketBlock::KV, t("pos.closed_by"), s["closed_by_name"]});
    }
    v.blocks.push_back({pdf::TicketBlock::Rule});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.sales_count"), std::to_string(s["sales_count"].get<int>())});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.gross_sales"), M(s["sales_total"])});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.returns_count"), std::to_string(s["returns_count"].get<int>())});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.returns"), neg(s["returns_total"])});
    v.blocks.push_back({pdf::TicketBlock::KVStrong, t("pos.net_sales"), M(s["net_total"])});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.discounts"), M(s["discounts"])});
    v.blocks.push_back({pdf::TicketBlock::Rule});
    v.blocks.push_back({pdf::TicketBlock::Text, t("pos.by_tax")});
    for (const auto& tx2 : s["taxes"])
        v.blocks.push_back({pdf::TicketBlock::KV, nhill::formatDecimal(tx2["rate_bp"].get<int>(), 2, lang, true) + " % · " + M(tx2["base"]),
                            M(tx2["tax"])});
    v.blocks.push_back({pdf::TicketBlock::Rule});
    v.blocks.push_back({pdf::TicketBlock::Text, t("pos.by_method")});
    for (const auto& bm : s["by_method"]) v.blocks.push_back({pdf::TicketBlock::KV, bm["label"], M(bm["net"])});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.float"), M(s["opening_float"])});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.cash_in"), M(s["cash_in"])});
    v.blocks.push_back({pdf::TicketBlock::KV, t("pos.cash_out"), neg(s["cash_out"])});
    if (withExpected) {
        v.blocks.push_back({pdf::TicketBlock::Rule});
        v.blocks.push_back({pdf::TicketBlock::KVStrong, t("pos.cash_expected"), M(s["cash_expected"])});
        if (s.contains("counted")) {
            for (auto it = s["counted"].begin(); it != s["counted"].end(); ++it)
                v.blocks.push_back({pdf::TicketBlock::KV, t("pos.counted") + " · " + it.key(), M(it.value())});
            if (!s["cash_variance"].is_null())
                v.blocks.push_back({pdf::TicketBlock::KVStrong, t("pos.variance"), M(s["cash_variance"])});
        }
    }
    if (!s["closing_note"].get<std::string>().empty()) v.blocks.push_back({pdf::TicketBlock::Muted, s["closing_note"]});
    v.blocks.push_back({pdf::TicketBlock::Space});
    v.blocks.push_back({pdf::TicketBlock::Muted, t("pos.generated")});
    return pdf::renderTicket(v, pdf::Fonts::fromDir(env.cfg.fontDir));
}

}  // namespace

std::string upperAscii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

void registerPosRoutes(App& app, Env& env) {
    // ---------------------------------------------------------------- configuration
    app.get("/api/v1/pos/config", {Access::User, "pos.sell", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto st = ensureSettings(tx, ctx.actor);
            json regs = json::array();
            for (const auto& r : tx.q(std::string(kRegisterSelect) +
                                          "WHERE r.active AND ($1 OR EXISTS (SELECT 1 FROM pos_register_users ru WHERE "
                                          "ru.register_id = r.id AND ru.user_id = $2)) ORDER BY r.code",
                                      has(ctx.actor, "pos.supervise"), ctx.actor.userId))
                regs.push_back(registerJson(r));
            auto mine = tx.opt("SELECT id FROM pos_sessions WHERE opened_by = $1 AND status = 'open'", ctx.actor.userId);
            auto e = tx.one("SELECT main_currency, trade_name FROM establishments WHERE id = $1", ctx.actor.establishmentId);
            return {{"registers", regs},
                    {"payment_methods", methodsJson(tx, true)},
                    {"settings", {{"walk_in_party_id", db::str(st[0])}, {"max_discount_bp", st[1].as<int>()},
                                  {"allow_price_override", db::boolean(st[2])}, {"receipt_footer", db::str(st[3])},
                                  {"receipt_width_mm", st[4].as<int>()}}},
                    {"permissions", posPermissions(ctx.actor)},
                    {"my_open_session", mine ? json(db::str((*mine)[0])) : json(nullptr)},
                    {"currency", db::str(e[0])},
                    {"establishment", db::str(e[1])},
                    {"registers_limit", has(ctx.actor, "pos.manage") ? registerLimitJson(tx, ctx.actor) : json(nullptr)}};
        });
        ctx.json(200, out);
    });

    // Catalogue for the register: active products with their price, tax and the stock of
    // the register's warehouse (same data as Nhill Business).
    app.get("/api/v1/pos/catalog", {Access::User, "pos.sell", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        auto reg = ctx.query("register_id");
        if (!reg || !in::isUuid(*reg)) throw invalid("register_id", "required");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto r = tx.one("SELECT warehouse_id FROM pos_registers WHERE id = $1 AND active", *reg);
            if (!assigned(tx, ctx.actor, *reg)) throw notFound("register");
            json items = json::array();
            for (const auto& p : tx.q(
                     "SELECT p.id, p.sku, p.barcode, p.name, p.unit, p.currency, p.sale_price_minor, COALESCE(t.rate_bp, 0), "
                     "p.track_stock, COALESCE(sl.quantity_milli, 0), c.name, p.kind FROM products p LEFT JOIN tax_rates t ON "
                     "t.id = p.tax_rate_id LEFT JOIN stock_levels sl ON sl.product_id = p.id AND sl.warehouse_id = $1 LEFT JOIN "
                     "product_categories c ON c.id = p.category_id WHERE p.active ORDER BY c.name NULLS LAST, p.name LIMIT 5000",
                     db::str(r[0])))
                items.push_back({{"id", db::str(p[0])}, {"sku", db::str(p[1])}, {"barcode", db::str(p[2])},
                                 {"name", db::str(p[3])}, {"unit", db::str(p[4])}, {"currency", db::str(p[5])},
                                 {"price", money(p[6].as<int64_t>(), db::str(p[5]))}, {"tax_rate_bp", p[7].as<int>()},
                                 {"track_stock", db::boolean(p[8])}, {"stock", qty(p[9].as<int64_t>())},
                                 {"category", db::str(p[10])}, {"kind", db::str(p[11])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- management (pos.manage)
    app.get("/api/v1/pos/registers", {Access::User, "pos.report", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            ensureSettings(tx, ctx.actor);
            json regs = json::array();
            for (const auto& r : tx.q(std::string(kRegisterSelect) + "ORDER BY r.active DESC, r.code")) regs.push_back(registerJson(r));
            int active = tx.one("SELECT count(*) FROM pos_registers WHERE active")[0].as<int>();
            auto st = tx.one("SELECT max_discount_bp, allow_price_override, receipt_footer, receipt_width_mm FROM pos_settings");
            return {{"items", regs},
                    {"limit", has(ctx.actor, "pos.manage") ? registerLimitJson(tx, ctx.actor) : json(nullptr)},
                    {"active_count", active},
                    {"payment_methods", methodsJson(tx, false)},
                    {"settings", {{"max_discount_bp", st[0].as<int>()}, {"allow_price_override", db::boolean(st[1])},
                                  {"receipt_footer", db::str(st[2])}, {"receipt_width_mm", st[3].as<int>()}}}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/pos/registers", {Access::User, "pos.manage", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string code = upperAscii(in::reqStr(b, "code", 12));
        std::string name = in::reqStr(b, "name", 80);
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            ensureSettings(tx, ctx.actor);
            tx.q("SELECT 1 FROM establishments WHERE id = $1 FOR UPDATE", ctx.actor.establishmentId);
            entitlements::requireCapacity(tx, ctx.actor, entitlements::key::kRegisters);
            std::string wh = registerWarehouse(tx, ctx.actor, b);
            auto r = tx.one("INSERT INTO pos_registers (establishment_id, code, name, location, warehouse_id) VALUES "
                            "($1,$2,$3,$4,$5) RETURNING id",
                            ctx.actor.establishmentId, code, name, in::optStr(b, "location", 120).value_or(""), wh);
            audit(tx, ctx.actor, "pos.register_created", "pos_register", db::str(r[0]), {{"code", code}});
            return registerJson(tx.one(std::string(kRegisterSelect) + "WHERE r.id = $1", db::str(r[0])));
        });
        ctx.json(201, out);
    });

    app.patch("/api/v1/pos/registers/:id", {Access::User, "pos.manage", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "register");
        const auto& b = ctx.body();
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto cur = tx.one("SELECT active FROM pos_registers WHERE id = $1 FOR UPDATE", id);
            std::optional<bool> active;
            if (b.contains("active")) active = in::optBool(b, "active", true);
            if (active && *active && !db::boolean(cur[0]))
                entitlements::requireCapacity(tx, ctx.actor, entitlements::key::kRegisters);
            std::optional<std::string> wh;
            if (b.contains("warehouse_id")) wh = registerWarehouse(tx, ctx.actor, b);
            if (active && !*active && !tx.q("SELECT 1 FROM pos_sessions WHERE register_id = $1 AND status = 'open'", id).empty())
                throw conflict("session_open", "close the cash session first");
            tx.q("UPDATE pos_registers SET name = COALESCE($2, name), location = COALESCE($3, location), "
                 "warehouse_id = COALESCE($4, warehouse_id), active = COALESCE($5, active) WHERE id = $1",
                 id, in::optStr(b, "name", 80), in::optStr(b, "location", 120), wh, active);
            audit(tx, ctx.actor, "pos.register_updated", "pos_register", id, b);
            return registerJson(tx.one(std::string(kRegisterSelect) + "WHERE r.id = $1", id));
        });
        ctx.json(200, out);
    });

    app.put("/api/v1/pos/registers/:id/users", {Access::User, "pos.manage", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "register");
        const auto& b = ctx.body();
        if (!b.contains("user_ids") || !b["user_ids"].is_array()) throw invalid("user_ids", "array_required");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            tx.one("SELECT 1 FROM pos_registers WHERE id = $1 FOR UPDATE", id);
            tx.q("DELETE FROM pos_register_users WHERE register_id = $1", id);
            json ids = json::array();
            for (const auto& u : b["user_ids"]) {
                if (!u.is_string() || !in::isUuid(u.get<std::string>())) throw invalid("user_ids", "invalid_uuid");
                // Only active members of this establishment can be assigned.
                if (tx.q("SELECT 1 FROM memberships WHERE establishment_id = $1 AND user_id = $2 AND status = 'active'",
                         ctx.actor.establishmentId, u.get<std::string>()).empty())
                    throw invalid("user_ids", "not_a_member");
                tx.q("INSERT INTO pos_register_users (establishment_id, register_id, user_id) VALUES ($1,$2,$3) "
                     "ON CONFLICT DO NOTHING",
                     ctx.actor.establishmentId, id, u.get<std::string>());
                ids.push_back(u);
            }
            audit(tx, ctx.actor, "pos.register_users_set", "pos_register", id, {{"user_ids", ids}});
            return registerJson(tx.one(std::string(kRegisterSelect) + "WHERE r.id = $1", id));
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/pos/payment-methods", {Access::User, "pos.manage", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string code = in::reqStr(b, "code", 24);
        for (auto& c : code) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            ensureSettings(tx, ctx.actor);
            auto r = tx.one("INSERT INTO pos_payment_methods (establishment_id, code, label, kind, account_id, sort_order) "
                            "VALUES ($1,$2,$3,$4,$5,$6) RETURNING id",
                            ctx.actor.establishmentId, code, in::reqStr(b, "label", 60),
                            in::reqEnum(b, "kind", {"cash", "card", "mobile_money", "bank_transfer", "cheque", "other"}),
                            in::optUuid(b, "account_id"), in::optInt(b, "sort_order", 10, 0, 1000));
            audit(tx, ctx.actor, "pos.payment_method_created", "pos_payment_method", db::str(r[0]), {{"code", code}});
            return {{"items", methodsJson(tx, false)}};
        });
        ctx.json(201, out);
    });

    app.patch("/api/v1/pos/payment-methods/:id", {Access::User, "pos.manage", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "method");
        const auto& b = ctx.body();
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto cur = tx.one("SELECT kind FROM pos_payment_methods WHERE id = $1 FOR UPDATE", id);
            std::optional<bool> active;
            if (b.contains("active")) active = in::optBool(b, "active", true);
            if (active && !*active && db::str(cur[0]) == "cash" &&
                tx.one("SELECT count(*) FROM pos_payment_methods WHERE kind = 'cash' AND active AND id <> $1", id)[0].as<int>() == 0)
                throw conflict("cash_method_required", "at least one cash method stays active");
            tx.q("UPDATE pos_payment_methods SET label = COALESCE($2, label), account_id = COALESCE($3, account_id), "
                 "active = COALESCE($4, active), sort_order = COALESCE($5, sort_order) WHERE id = $1",
                 id, in::optStr(b, "label", 60), in::optUuid(b, "account_id"), active,
                 b.contains("sort_order") ? std::optional<int>(in::optInt(b, "sort_order", 0, 0, 1000)) : std::nullopt);
            audit(tx, ctx.actor, "pos.payment_method_updated", "pos_payment_method", id, b);
            return {{"items", methodsJson(tx, false)}};
        });
        ctx.json(200, out);
    });

    app.patch("/api/v1/pos/settings", {Access::User, "pos.manage", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            ensureSettings(tx, ctx.actor);
            std::optional<int> bp, width;
            std::optional<bool> override;
            if (b.contains("max_discount_bp")) bp = in::optInt(b, "max_discount_bp", 0, 0, 10000);
            if (b.contains("receipt_width_mm")) {
                width = in::optInt(b, "receipt_width_mm", 80, 58, 80);
                if (*width != 58 && *width != 80) throw invalid("receipt_width_mm", "58_or_80");
            }
            if (b.contains("allow_price_override")) override = in::optBool(b, "allow_price_override", false);
            std::optional<std::string> footer;
            if (b.contains("receipt_footer")) footer = in::optStr(b, "receipt_footer", 300).value_or("");
            tx.q("UPDATE pos_settings SET max_discount_bp = COALESCE($2, max_discount_bp), allow_price_override = "
                 "COALESCE($3, allow_price_override), receipt_footer = COALESCE($4, receipt_footer), receipt_width_mm = "
                 "COALESCE($5, receipt_width_mm), updated_at = now() WHERE establishment_id = $1",
                 ctx.actor.establishmentId, bp, override, footer, width);
            audit(tx, ctx.actor, "pos.settings_updated", "pos_settings", ctx.actor.establishmentId, b);
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- sales promo codes
    app.get("/api/v1/pos/promo-codes", {Access::User, "pos.manage", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT p.id, p.code, p.description, p.percent_bp, p.min_amount_minor, p.currency, "
                                      "nh_iso(p.valid_from), nh_iso(p.valid_until), p.max_redemptions, p.active, "
                                      "(SELECT count(*) FROM sales_promo_redemptions r WHERE r.promo_id = p.id)::int "
                                      "FROM sales_promo_codes p ORDER BY p.active DESC, p.created_at DESC"))
                items.push_back({{"id", db::str(r[0])}, {"code", db::str(r[1])}, {"description", db::str(r[2])},
                                 {"percent_bp", r[3].as<int>()},
                                 {"min_amount", r[5].is_null() ? json(nullptr) : json(money(r[4].as<int64_t>(), db::str(r[5])))},
                                 {"currency", r[5].is_null() ? json(nullptr) : json(db::str(r[5]))},
                                 {"valid_from", db::str(r[6])}, {"valid_until", r[7].is_null() ? json(nullptr) : json(db::str(r[7]))},
                                 {"max_redemptions", r[8].is_null() ? json(nullptr) : json(r[8].as<int>())},
                                 {"active", db::boolean(r[9])}, {"used", r[10].as<int>()}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/pos/promo-codes", {Access::User, "pos.manage", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string code = upperAscii(in::reqStr(b, "code", 32));
        std::optional<std::string> cur;
        std::optional<int64_t> minAmount;
        if (b.contains("min_amount")) {
            cur = in::reqCurrency(b, "currency");
            minAmount = in::reqMoney(b, "min_amount", *cur);
        }
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            if (cur) entitlements::requireCurrency(tx, ctx.actor, *cur);
            auto r = tx.one("INSERT INTO sales_promo_codes (establishment_id, code, description, percent_bp, min_amount_minor, "
                            "currency, valid_from, valid_until, max_redemptions, created_by) VALUES ($1,$2,$3,$4,COALESCE($5,0),"
                            "$6,COALESCE($7::date::timestamp AT TIME ZONE nh_tz($1), now()),$8::date::timestamp AT TIME ZONE "
                            "nh_tz($1),$9,$10) RETURNING id",
                            ctx.actor.establishmentId, code, in::optStr(b, "description", 200).value_or(""),
                            in::optInt(b, "percent_bp", 0, 1, 10000), minAmount, cur, in::optDate(b, "valid_from"),
                            in::optDate(b, "valid_until"),
                            b.contains("max_redemptions") && !b["max_redemptions"].is_null()
                                ? std::optional<int>(in::optInt(b, "max_redemptions", 1, 1, 1000000))
                                : std::nullopt,
                            ctx.actor.userId);
            audit(tx, ctx.actor, "pos.promo_created", "sales_promo_code", db::str(r[0]), b);
            return {{"id", db::str(r[0])}};
        });
        ctx.json(201, out);
    });

    app.patch("/api/v1/pos/promo-codes/:id", {Access::User, "pos.manage", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "promo_code");
        const auto& b = ctx.body();
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            std::optional<bool> active;
            if (b.contains("active")) active = in::optBool(b, "active", true);
            auto r = tx.q("UPDATE sales_promo_codes SET active = COALESCE($2, active), description = COALESCE($3, description) "
                          "WHERE id = $1 RETURNING id",
                          id, active, in::optStr(b, "description", 200));
            if (r.empty()) throw notFound("promo_code");
            audit(tx, ctx.actor, "pos.promo_updated", "sales_promo_code", id, b);
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- approvals
    // A supervisor types their own password on the register to authorise one operation.
    app.post("/api/v1/pos/approvals", {Access::User, "pos.sell", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string email = in::reqEmail(b, "email");
        throttle(env.pool, "pos-approval:" + ctx.actor.userId, 10);
        throttle(env.pool, "pos-approval:" + email, 8);
        if (!b.contains("password") || !b["password"].is_string()) throw invalid("password", "required");
        std::string action = in::reqEnum(b, "action", {"discount", "price_override", "return", "void", "cash_out", "close_other"});
        auto sessionId = in::optUuid(b, "session_id");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto u = tx.opt("SELECT u.id, u.password_hash, m.role, u.full_name FROM users u JOIN memberships m ON "
                            "m.user_id = u.id AND m.establishment_id = $2 AND m.status = 'active' WHERE u.email = $1 AND "
                            "u.status = 'active'",
                            email, ctx.actor.establishmentId);
            bool ok = u && crypto::verifyPassword(db::str((*u)[1]), b["password"].get<std::string>());
            if (!ok || !App::roleHas(db::str((*u)[2]), "pos.supervise")) {
                audit(tx, ctx.actor, "pos.approval_refused", "pos_approval", email, {{"action", action}});
                return {{"_refused", true}};
            }
            if (db::str((*u)[0]) == ctx.actor.userId) throw conflict("approval_self", "another person must approve");
            if (sessionId) tx.one("SELECT 1 FROM pos_sessions WHERE id = $1 AND status = 'open'", *sessionId);
            std::string token = crypto::randomToken(24);
            auto r = tx.one("INSERT INTO pos_approvals (establishment_id, token_hash, action, approver_id, requested_by, "
                            "session_id, expires_at) VALUES ($1, decode($2,'hex'), $3, $4, $5, $6, now() + "
                            "make_interval(mins => nh_setting_int('pos_approval_minutes', 3))) RETURNING id, nh_iso(expires_at)",
                            ctx.actor.establishmentId, crypto::sha256Hex(token), action, db::str((*u)[0]), ctx.actor.userId,
                            sessionId);
            audit(tx, ctx.actor, "pos.approval_granted", "pos_approval", db::str(r[0]),
                  {{"action", action}, {"approver", db::str((*u)[3])}});
            return {{"token", token}, {"expires_at", db::str(r[1])}, {"approver", db::str((*u)[3])}};
        });
        if (out.contains("_refused")) throw unauthorized("approval_refused");
        ctx.json(201, out);
    });

    // ---------------------------------------------------------------- sessions
    app.post("/api/v1/pos/sessions", {Access::User, "pos.sell", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string reg = in::reqUuid(b, "register_id");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            ensureSettings(tx, ctx.actor);
            auto r = tx.opt("SELECT active FROM pos_registers WHERE id = $1 FOR UPDATE", reg);
            if (!r || !assigned(tx, ctx.actor, reg)) throw notFound("register");
            if (!db::boolean((*r)[0])) throw conflict("register_inactive");
            if (!tx.q("SELECT 1 FROM pos_sessions WHERE register_id = $1 AND status = 'open'", reg).empty())
                throw conflict("session_already_open", "a cash session is already open on this register");
            if (!tx.q("SELECT 1 FROM pos_sessions WHERE opened_by = $1 AND status = 'open'", ctx.actor.userId).empty())
                throw conflict("user_session_open", "you already have an open cash session");
            std::string cur = db::str(tx.one("SELECT main_currency FROM establishments WHERE id = $1", ctx.actor.establishmentId)[0]);
            int64_t fl = in::optMoney(b, "opening_float", cur).value_or(0);
            if (fl < 0) throw invalid("opening_float", "must_be_positive");
            int number = tx.one("SELECT COALESCE(max(number), 0) + 1 FROM pos_sessions WHERE register_id = $1", reg)[0].as<int>();
            auto s = tx.one("INSERT INTO pos_sessions (establishment_id, register_id, number, currency, opened_by, "
                            "opening_float_minor) VALUES ($1,$2,$3,$4,$5,$6) RETURNING id",
                            ctx.actor.establishmentId, reg, number, cur, ctx.actor.userId, fl);
            audit(tx, ctx.actor, "pos.session_opened", "pos_session", db::str(s[0]),
                  {{"register_id", reg}, {"number", number}, {"opening_float", money(fl, cur)}});
            return summary(tx, db::str(s[0]), true);
        });
        out.erase("_cash_expected_minor");
        out.erase("_net");
        ctx.json(201, out);
    });

    app.get("/api/v1/pos/sessions", {Access::User, "", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        sellOrReport(ctx.actor);
        auto pg = page(ctx);
        auto reg = ctx.query("register_id");
        if (reg && !in::isUuid(*reg)) throw invalid("register_id", "invalid_uuid");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            bool all = has(ctx.actor, "pos.report");
            json items = json::array();
            for (const auto& r : tx.q("SELECT s.id FROM pos_sessions s WHERE ($1 OR s.opened_by = $2) AND ($3::uuid IS NULL OR "
                                      "s.register_id = $3) ORDER BY s.opened_at DESC LIMIT $4 OFFSET $5",
                                      all, ctx.actor.userId, reg, pg.limit, pg.offset)) {
                json s = summary(tx, db::str(r[0]), has(ctx.actor, "pos.supervise"));
                s.erase("_cash_expected_minor");
                s.erase("_net");
                s.erase("cash_movements");
                items.push_back(s);
            }
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/pos/sessions/:id", {Access::User, "", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        sellOrReport(ctx.actor);
        std::string id = in::uuidParam(ctx.param("id"), "session");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto s = tx.opt("SELECT opened_by FROM pos_sessions WHERE id = $1", id);
            if (!s || (db::str((*s)[0]) != ctx.actor.userId && !has(ctx.actor, "pos.report"))) throw notFound("session");
            json j = summary(tx, id, has(ctx.actor, "pos.supervise"));
            j.erase("_cash_expected_minor");
            j.erase("_net");
            json docs = json::array();
            for (const auto& r : tx.q("SELECT d.id, d.doc_type, d.number, d.total_minor, d.currency, d.status, nh_iso(d.issued_at), "
                                      "p.name, d.related_document_id FROM sales_documents d JOIN parties p ON p.id = d.party_id "
                                      "WHERE d.pos_session_id = $1 AND d.status <> 'draft' ORDER BY d.issued_at DESC LIMIT 500",
                                      id))
                docs.push_back({{"id", db::str(r[0])}, {"doc_type", db::str(r[1])}, {"number", db::str(r[2])},
                                {"total", money(r[3].as<int64_t>(), db::str(r[4]))}, {"currency", db::str(r[4])},
                                {"status", db::str(r[5])}, {"issued_at", db::str(r[6])}, {"customer", db::str(r[7])},
                                {"related_document_id", r[8].is_null() ? json(nullptr) : json(db::str(r[8]))}});
            j["documents"] = docs;
            return j;
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/pos/sessions/:id/cash-movements", {Access::User, "pos.sell", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "session");
        const auto& b = ctx.body();
        std::string kind = in::reqEnum(b, "kind", {"cash_in", "cash_out"});
        std::string reason = in::reqStr(b, "reason", 300);
        if (reason.size() < 3) throw invalid("reason", "too_short");
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                Session s = loadSession(tx, ctx.actor, id, true);
                int64_t amount = in::reqMoney(b, "amount", s.currency, false);
                std::optional<std::string> approver;
                if (kind == "cash_out") {
                    approver = authorize(tx, ctx.actor, b, "pos.cash", "cash_out", id);
                    json sm = summary(tx, id, true);
                    if (amount > sm["_cash_expected_minor"].get<int64_t>()) throw conflict("cash_insufficient");
                }
                auto r = tx.one("INSERT INTO pos_cash_movements (establishment_id, session_id, kind, amount_minor, reason, "
                                "created_by, approved_by) VALUES ($1,$2,$3,$4,$5,$6,$7) RETURNING id",
                                ctx.actor.establishmentId, id, kind, amount, reason, ctx.actor.userId, approver);
                audit(tx, ctx.actor, "pos." + kind, "pos_cash_movement", db::str(r[0]),
                      {{"session_id", id}, {"amount", money(amount, s.currency)}, {"reason", reason},
                       {"approved_by", approver ? json(*approver) : json(nullptr)}});
                return {201, {{"id", db::str(r[0])}}};
            });
        });
        ctx.json(status, body);
    });

    // Closing: the cashier declares the counted amounts; expected amounts and the cash
    // variance are computed by the server (shown to supervisors only).
    app.post("/api/v1/pos/sessions/:id/close", {Access::User, "pos.sell", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "session");
        const auto& b = ctx.body();
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                auto own = tx.opt("SELECT opened_by FROM pos_sessions WHERE id = $1", id);
                if (!own) throw notFound("session");
                bool mine = db::str((*own)[0]) == ctx.actor.userId;
                std::optional<std::string> approver;
                if (!mine && !has(ctx.actor, "pos.supervise")) throw notFound("session");
                Session s = loadSession(tx, ctx.actor, id, true);
                if (!b.contains("counted") || !b["counted"].is_object()) throw invalid("counted", "required");
                json sm = summary(tx, id, true);
                auto ms = methods(tx, false);
                json counted = json::object();
                int64_t cashCounted = 0;
                bool cashSeen = false;
                for (auto it = b["counted"].begin(); it != b["counted"].end(); ++it) {
                    auto mt = ms.find(it.key());
                    if (mt == ms.end()) throw invalid("counted." + it.key(), "unknown_payment_method");
                    json tmp = {{"v", it.value()}};
                    int64_t v = in::reqMoney(tmp, "v", s.currency);
                    counted[it.key()] = v;
                    if (mt->second.kind == "cash") {
                        cashCounted += v;
                        cashSeen = true;
                    }
                }
                if (!cashSeen) throw invalid("counted", "cash_count_required");
                int64_t expected = sm["_cash_expected_minor"].get<int64_t>();
                json expectedMap = json::object();
                for (const auto& [code, v] : sm["_net"].get<std::map<std::string, int64_t>>()) expectedMap[code] = v;
                tx.q("UPDATE pos_sessions SET status = 'closed', closed_by = $2, closed_at = now(), counted = $3::jsonb, "
                     "expected = $4::jsonb, cash_expected_minor = $5, cash_counted_minor = $6, cash_variance_minor = $7, "
                     "closing_note = $8 WHERE id = $1",
                     id, ctx.actor.userId, counted.dump(), expectedMap.dump(), expected, cashCounted, cashCounted - expected,
                     in::optStr(b, "note", 500).value_or(""));
                audit(tx, ctx.actor, "pos.session_closed", "pos_session", id,
                      {{"number", s.number}, {"register", s.registerCode}, {"cash_expected", money(expected, s.currency)},
                       {"cash_counted", money(cashCounted, s.currency)},
                       {"cash_variance", money(cashCounted - expected, s.currency)}, {"by_owner", mine}});
                if (cashCounted != expected)
                    notify(tx, ctx.actor.establishmentId, std::nullopt, "pos_cash_variance",
                           {{"register", s.registerCode}, {"number", s.number},
                            {"variance", money(cashCounted - expected, s.currency)}, {"currency", s.currency}});
                json out = summary(tx, id, has(ctx.actor, "pos.supervise"));
                out.erase("_cash_expected_minor");
                out.erase("_net");
                return {200, out};
            });
        });
        ctx.json(status, body);
    });

    app.get("/api/v1/pos/sessions/:id/report.pdf", {Access::User, "", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        sellOrReport(ctx.actor);
        std::string id = in::uuidParam(ctx.param("id"), "session");
        std::string pdfBytes, name;
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto s = tx.opt("SELECT opened_by, status, number, (SELECT code FROM pos_registers WHERE id = register_id) "
                            "FROM pos_sessions WHERE id = $1",
                            id);
            if (!s || (db::str((*s)[0]) != ctx.actor.userId && !has(ctx.actor, "pos.report"))) throw notFound("session");
            pdfBytes = sessionPdf(env, tx, ctx.actor, id, has(ctx.actor, "pos.supervise"));
            name = std::string(db::str((*s)[1]) == "closed" ? "Z" : "X") + "-" + db::str((*s)[3]) + "-" + db::str((*s)[2]) + ".pdf";
            audit(tx, ctx.actor, "pos.report_printed", "pos_session", id);
        });
        ctx.file(pdfBytes, "application/pdf", name);
    });

    // ---------------------------------------------------------------- sales
    app.post("/api/v1/pos/sales", {Access::User, "pos.sell", true, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string sid = in::reqUuid(b, "session_id");
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                Session s = loadSession(tx, ctx.actor, sid, true);
                if (!assigned(tx, ctx.actor, s.registerId)) throw notFound("register");
                auto st = ensureSettings(tx, ctx.actor);
                const int maxBp = st[1].as<int>();
                const bool allowOverride = db::boolean(st[2]);
                std::string party = db::str(st[0]);
                if (auto c = in::optUuid(b, "customer_id")) {
                    auto p = tx.opt("SELECT is_customer, active FROM parties WHERE id = $1", *c);
                    if (!p || !db::boolean((*p)[0]) || !db::boolean((*p)[1])) throw invalid("customer_id", "not_an_active_customer");
                    party = *c;
                }
                if (!b.contains("lines") || !b["lines"].is_array() || b["lines"].empty()) throw invalid("lines", "at_least_one_line");
                // Discount and price checks (server-side catalogue prices).
                bool needDiscount = false, needOverride = false;
                json lines = json::array();
                for (size_t i = 0; i < b["lines"].size(); ++i) {
                    const json& l = b["lines"][i];
                    std::string f = "lines[" + std::to_string(i) + "].";
                    std::string pid = in::reqUuid(l, "product_id");
                    auto p = tx.opt("SELECT sale_price_minor, currency, active FROM products WHERE id = $1", pid);
                    if (!p || !db::boolean((*p)[2])) throw invalid(f + "product_id", "unknown_product");
                    json nl = {{"product_id", pid}, {"quantity", l.value("quantity", json("1"))}};
                    int bp = in::optInt(l, "discount_bp", 0, 0, 10000);
                    if (bp > maxBp) needDiscount = true;
                    nl["discount_bp"] = bp;
                    if (l.contains("unit_price") && !l["unit_price"].is_null()) {
                        int64_t up = in::reqMoney(l, "unit_price", s.currency);
                        if (db::str((*p)[1]) != s.currency || up != (*p)[0].as<int64_t>()) {
                            if (!allowOverride) throw invalid(f + "unit_price", "price_override_disabled");
                            needOverride = true;
                        }
                        nl["unit_price"] = l["unit_price"];
                    } else if (db::str((*p)[1]) != s.currency) {
                        throw invalid(f + "unit_price", "product_currency_differs");
                    }
                    lines.push_back(nl);
                }
                std::optional<std::string> approver;
                if (needDiscount) approver = authorize(tx, ctx.actor, b, "pos.supervise", "discount", sid);
                if (needOverride && !approver) approver = authorize(tx, ctx.actor, b, "pos.supervise", "price_override", sid);
                auto es = tx.one("SELECT e.main_currency, s.document_language, s.tax_rounding FROM establishments e "
                                 "JOIN establishment_settings s ON s.establishment_id = e.id WHERE e.id = $1",
                                 ctx.actor.establishmentId);
                const std::string rounding = db::str(es[2]);
                // Establishment promo code: percentage combined with each line discount.
                std::optional<std::string> promoId;
                int promoBp = 0;
                if (auto code = in::optStr(b, "promo_code", 32)) {
                    throttle(env.pool, "pos-promo:" + ctx.actor.establishmentId, 60);
                    auto pr = tx.opt("SELECT id, percent_bp, min_amount_minor, currency, max_redemptions, "
                                     "(SELECT count(*) FROM sales_promo_redemptions r WHERE r.promo_id = p.id) FROM "
                                     "sales_promo_codes p WHERE code = $1 AND active AND valid_from <= now() AND "
                                     "(valid_until IS NULL OR valid_until > now()) FOR UPDATE",
                                     upperAscii(*code));
                    if (!pr) throw conflict("promo_invalid");
                    if (!(*pr)[4].is_null() && (*pr)[5].as<int>() >= (*pr)[4].as<int>()) throw conflict("promo_exhausted");
                    auto [pl, pt] = sales::priceLines(tx, json{{"lines", lines}}, s.currency, rounding);
                    if (!(*pr)[3].is_null() && (db::str((*pr)[3]) != s.currency || pt.totalMinor < (*pr)[2].as<int64_t>()))
                        throw conflict("promo_not_applicable");
                    promoId = db::str((*pr)[0]);
                    promoBp = (*pr)[1].as<int>();
                    for (auto& l : lines) l["discount_bp"] = combineBp(l["discount_bp"].get<int>(), promoBp);
                }
                auto [lc, totals] = sales::priceLines(tx, json{{"lines", lines}}, s.currency, rounding);
                std::string id = db::str(tx.one(
                    "INSERT INTO sales_documents (establishment_id, doc_type, party_id, currency, language, main_currency, "
                    "tax_rounding, gross_minor, discount_minor, net_minor, tax_minor, total_minor, tax_breakdown, notes, "
                    "created_by, pos_session_id) VALUES ($1,'receipt',$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12::jsonb,$13,$14,$15) "
                    "RETURNING id",
                    ctx.actor.establishmentId, party, s.currency, db::str(es[1]), db::str(es[0]), rounding, totals.grossMinor,
                    totals.discountMinor, totals.netMinor, totals.taxMinor, totals.totalMinor,
                    sales::breakdownJson(totals).dump(), in::optStr(b, "note", 300).value_or(""), ctx.actor.userId, sid)[0]);
                sales::writeLines(tx, ctx.actor.establishmentId, id, lc, totals);
                sales::history(tx, ctx.actor, id, std::nullopt, "draft");
                json doc = sales::issue(env, tx, ctx.actor, id, json::object(), s.warehouseId);
                int64_t change = recordPayments(tx, ctx.actor, s, id, doc["number"], b.value("payments", json::array()),
                                                totals.totalMinor, "in");
                std::string newStatus = invoiceStatusFor(totals.totalMinor, totals.totalMinor, 0);
                tx.q("UPDATE sales_documents SET paid_minor = total_minor, status = $2 WHERE id = $1", id, newStatus);
                sales::history(tx, ctx.actor, id, std::string("issued"), newStatus, "pos");
                if (promoId)
                    tx.q("INSERT INTO sales_promo_redemptions (establishment_id, promo_id, document_id, discount_minor) "
                         "VALUES ($1,$2,$3,$4)",
                         ctx.actor.establishmentId, *promoId, id, totals.discountMinor);
                audit(tx, ctx.actor, "pos.sale", "sales_document", id,
                      {{"number", doc["number"]}, {"session_id", sid}, {"total", money(totals.totalMinor, s.currency)},
                       {"change_minor", change}, {"approved_by", approver ? json(*approver) : json(nullptr)},
                       {"promo", promoId ? json(*promoId) : json(nullptr)}});
                return {201, {{"receipt", loadSalesDocument(tx, id)}, {"change", money(change, s.currency)}}};
            });
        });
        ctx.json(status, body);
    });

    app.get("/api/v1/pos/sales/:id/receipt.pdf", {Access::User, "pos.sell", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "document");
        std::string bytes, number;
        db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            auto d = tx.opt("SELECT d.number, s.opened_by FROM sales_documents d JOIN pos_sessions s ON s.id = d.pos_session_id "
                            "WHERE d.id = $1 AND d.status <> 'draft'",
                            id);
            if (!d || (db::str((*d)[1]) != ctx.actor.userId && !has(ctx.actor, "pos.report") &&
                       !has(ctx.actor, "pos.supervise")))
                throw notFound("document");
            number = db::str((*d)[0]);
            bytes = receiptPdf(env, tx, ctx.actor, id);
        });
        ctx.file(bytes, "application/pdf", number + ".pdf");
    });

    // Return (partial) or void (whole receipt) through a credit note, with refunds.
    auto refund = [&env](Ctx& ctx, bool isVoid) {
        std::string rid = in::uuidParam(ctx.param("id"), "document");
        const auto& b = ctx.body();
        std::string sid = in::reqUuid(b, "session_id");
        std::string reason = in::reqStr(b, "reason", 300);
        if (reason.size() < 3) throw invalid("reason", "too_short");
        auto [status, body] = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) {
            return idempotent(tx, ctx, [&]() -> Result {
                Session s = loadSession(tx, ctx.actor, sid, true);
                auto r = tx.opt("SELECT doc_type, status, currency, total_minor, credited_minor, pos_session_id, party_id, "
                                "language FROM sales_documents WHERE id = $1 FOR UPDATE",
                                rid);
                if (!r || db::str((*r)[0]) != "receipt" || (*r)[5].is_null()) throw notFound("document");
                if (db::str((*r)[2]) != s.currency) throw invalid("session_id", "currency_mismatch");
                if (db::str((*r)[1]) == "credited") throw conflict("already_refunded");
                std::optional<std::string> approver;
                if (isVoid) {
                    if (db::str((*r)[5]) != sid) throw conflict("void_other_session", "a receipt can only be voided during its session");
                    if ((*r)[4].as<int64_t>() > 0) throw conflict("already_partially_returned");
                    approver = authorize(tx, ctx.actor, b, "pos.void", "void", sid);
                } else {
                    approver = authorize(tx, ctx.actor, b, "pos.refund", "return", sid);
                }
                // Quantities still returnable per original line.
                std::map<std::string, int64_t> returned;
                for (const auto& x : tx.q("SELECT l.description || '|' || l.unit_price_minor || '|' || COALESCE(l.product_id::text,''), "
                                          "sum(l.quantity_milli) FROM sales_document_lines l JOIN sales_documents c ON "
                                          "c.id = l.document_id WHERE c.related_document_id = $1 AND c.doc_type = 'credit_note' "
                                          "AND c.status <> 'draft' GROUP BY 1",
                                          rid))
                    returned[db::str(x[0])] = x[1].as<int64_t>();
                std::map<int, int64_t> wanted;
                if (!isVoid) {
                    if (!b.contains("lines") || !b["lines"].is_array() || b["lines"].empty()) throw invalid("lines", "required");
                    for (const auto& l : b["lines"]) {
                        int no = in::optInt(l, "line_no", 0, 1, 500);
                        wanted[no] += in::reqQty(l, "quantity");
                    }
                }
                json lines = json::array();
                for (const auto& l : tx.q("SELECT line_no, product_id, description, quantity_milli, unit, unit_price_minor, "
                                          "discount_bp, tax_code FROM sales_document_lines WHERE document_id = $1 ORDER BY line_no",
                                          rid)) {
                    int no = l[0].as<int>();
                    std::string key = db::str(l[2]) + "|" + std::to_string(l[5].as<int64_t>()) + "|" + db::str(l[1]);
                    int64_t left = l[3].as<int64_t>() - returned[key];
                    int64_t q = isVoid ? left : (wanted.count(no) ? wanted[no] : 0);
                    if (q <= 0) continue;
                    if (q > left) throw invalid("lines", "quantity_exceeds_sold");
                    json lj = {{"description", db::str(l[2])}, {"quantity", qty(q)}, {"unit", db::str(l[4])},
                               {"unit_price", money(l[5].as<int64_t>(), s.currency)}, {"discount_bp", l[6].as<int>()}};
                    if (!l[1].is_null()) lj["product_id"] = db::str(l[1]);
                    auto tr = tx.opt("SELECT id FROM tax_rates WHERE code = $1", db::str(l[7]));
                    if (tr) lj["tax_rate_id"] = db::str((*tr)[0]);
                    lines.push_back(lj);
                    wanted.erase(no);
                }
                if (!wanted.empty()) throw invalid("lines", "unknown_line");
                if (lines.empty()) throw conflict("nothing_to_return");
                auto es = tx.one("SELECT main_currency, (SELECT tax_rounding FROM establishment_settings WHERE "
                                 "establishment_id = $1) FROM establishments WHERE id = $1",
                                 ctx.actor.establishmentId);
                auto [lc, totals] = sales::priceLines(tx, json{{"lines", lines}}, s.currency, db::str(es[1]));
                bool restock = isVoid ? true : in::optBool(b, "restock", true);
                std::string cid = db::str(tx.one(
                    "INSERT INTO sales_documents (establishment_id, doc_type, party_id, currency, language, main_currency, "
                    "tax_rounding, gross_minor, discount_minor, net_minor, tax_minor, total_minor, tax_breakdown, notes, "
                    "related_document_id, restock, created_by, pos_session_id) VALUES ($1,'credit_note',$2,$3,$4,$5,$6,$7,$8,$9,"
                    "$10,$11,$12::jsonb,$13,$14,$15,$16,$17) RETURNING id",
                    ctx.actor.establishmentId, db::str((*r)[6]), s.currency, db::str((*r)[7]), db::str(es[0]), db::str(es[1]),
                    totals.grossMinor, totals.discountMinor, totals.netMinor, totals.taxMinor, totals.totalMinor,
                    sales::breakdownJson(totals).dump(), (isVoid ? "VOID: " : "RETURN: ") + reason, rid, restock,
                    ctx.actor.userId, sid)[0]);
                sales::writeLines(tx, ctx.actor.establishmentId, cid, lc, totals);
                sales::history(tx, ctx.actor, cid, std::nullopt, "draft");
                json doc = sales::issue(env, tx, ctx.actor, cid, json::object(), s.warehouseId);
                // Refunds: given explicitly, or (void) by the original methods.
                json refunds = b.value("refunds", json::array());
                if (isVoid && refunds.empty()) {
                    for (const auto& p : tx.q("SELECT substr(note, 5), amount_minor FROM payments WHERE document_id = $1 AND "
                                              "direction = 'in'",
                                              rid))
                        refunds.push_back({{"method", db::str(p[0])}, {"amount", money(p[1].as<int64_t>(), s.currency)}});
                } else if (refunds.empty() && totals.totalMinor > 0) {
                    // Return without explicit refunds: the whole credit note is paid back in cash.
                    for (const auto& [code, mth] : methods(tx, true))
                        if (mth.kind == "cash") {
                            refunds.push_back({{"method", code}, {"amount", money(totals.totalMinor, s.currency)}});
                            break;
                        }
                }
                if (totals.totalMinor > 0) recordPayments(tx, ctx.actor, s, cid, doc["number"], refunds, totals.totalMinor, "out");
                audit(tx, ctx.actor, isVoid ? "pos.void" : "pos.return", "sales_document", cid,
                      {{"receipt_id", rid}, {"number", doc["number"]}, {"reason", reason},
                       {"total", money(totals.totalMinor, s.currency)}, {"restock", restock},
                       {"approved_by", approver ? json(*approver) : json(nullptr)}});
                return {201, {{"credit_note", loadSalesDocument(tx, cid)}, {"receipt", loadSalesDocument(tx, rid)}}};
            });
        });
        ctx.json(status, body);
    };
    app.post("/api/v1/pos/sales/:id/return", {Access::User, "pos.sell", true, {}, entitlements::key::kPos}, [refund](Ctx& ctx) { refund(ctx, false); });
    app.post("/api/v1/pos/sales/:id/void", {Access::User, "pos.sell", true, {}, entitlements::key::kPos}, [refund](Ctx& ctx) { refund(ctx, true); });

    // Find a receipt by its number (returns at another register / another day).
    app.get("/api/v1/pos/receipts", {Access::User, "pos.sell", false, {}, entitlements::key::kPos}, [&env](Ctx& ctx) {
        auto number = ctx.query("number");
        if (!number || number->size() < 3) throw invalid("number", "required");
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto r = tx.opt("SELECT id FROM sales_documents WHERE doc_type = 'receipt' AND number = $1", upperAscii(*number));
            if (!r) throw notFound("document");
            return loadSalesDocument(tx, db::str((*r)[0]));
        });
        ctx.json(200, out);
    });
}

}  // namespace nh
