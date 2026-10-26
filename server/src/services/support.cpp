// Support tickets and technical error reports sent from Nhill Business and Nhill POS.
// A member sees the tickets they opened; owners and administrators of the
// establishment see all of its tickets. Console-internal notes are never returned.
#include "services/services.hpp"
#include "util/crypto.hpp"

#include <regex>

namespace nh {

namespace {

// Client reports are free text written by the applications (QML warnings, network
// errors): e-mail addresses, long digit runs (card or account numbers, phone numbers)
// and long opaque tokens are masked before storage, in the message and the context.
std::string redact(std::string s) {
    static const std::regex email(R"([A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,})");
    static const std::regex digits(R"(\d[\d ./-]{7,}\d)");
    static const std::regex token(R"([A-Za-z0-9_\-]{32,})");
    s = std::regex_replace(s, email, "<email>");
    s = std::regex_replace(s, token, "<token>");  // before digits: a token may end with digits
    s = std::regex_replace(s, digits, "<number>");
    return s;
}

json redactJson(const json& j) {
    if (j.is_string()) return redact(j.get<std::string>());
    if (j.is_object() || j.is_array()) {
        json out = j;
        for (auto it = out.begin(); it != out.end(); ++it) *it = redactJson(*it);
        return out;
    }
    return j;
}

bool seesAll(const Actor& a) { return App::roleHas(a.role, "users.manage"); }

json userTicketRow(const pqxx::row& r) {
    return {{"id", db::str(r[0])},          {"number", r[1].as<int64_t>()}, {"source", db::str(r[2])},
            {"category", db::str(r[3])},    {"severity", db::str(r[4])},    {"subject", db::str(r[5])},
            {"status", db::str(r[6])},      {"created_at", db::str(r[7])},  {"last_message_at", db::str(r[8])},
            {"created_by_name", db::str(r[9])}};
}
constexpr const char* kUserTicketSelect =
    "SELECT t.id, t.number, t.source, t.category, t.severity, t.subject, t.status, nh_iso(t.created_at), "
    "nh_iso(t.last_message_at), COALESCE(u.full_name, '') FROM support_tickets t LEFT JOIN users u ON u.id = t.created_by ";

// Visible ticket of the caller's establishment, or 404 (never reveals other tenants' tickets).
pqxx::row ticketFor(db::Tx& tx, const Actor& a, const std::string& id, bool lock = false) {
    auto r = tx.opt(std::string("SELECT id, status, number FROM support_tickets WHERE id = $1 AND establishment_id = $2 "
                                "AND ($3 OR created_by = $4)") +
                        (lock ? " FOR UPDATE" : ""),
                    id, a.establishmentId, seesAll(a), a.userId);
    if (!r) throw notFound("ticket");
    return *r;
}

}  // namespace

// Records an error event, grouped by fingerprint (best effort, never throws).
void recordErrorEvent(db::Pool& pool, const std::string& source, const std::string& level, const std::string& code,
                      const std::string& message, const json& context, const std::optional<std::string>& establishmentId,
                      const std::optional<std::string>& userId, const std::string& appVersion,
                      const std::string& platform) {
    try {
        std::string msg = message.substr(0, 2000);
        std::string fp = crypto::sha256Hex(source + "|" + code + "|" + msg.substr(0, 300));
        db::run(pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.q(
                "INSERT INTO error_events (source, level, code, message, fingerprint, context, establishment_id, user_id, "
                "app_version, platform) VALUES ($1,$2,$3,$4,$5,$6::jsonb,$7,$8,NULLIF($9,''),NULLIF($10,'')) "
                "ON CONFLICT (source, fingerprint) WHERE resolved_at IS NULL DO UPDATE SET occurrences = "
                "error_events.occurrences + 1, last_seen = now(), context = EXCLUDED.context, "
                "app_version = COALESCE(EXCLUDED.app_version, error_events.app_version) RETURNING (xmax = 0)",
                source, level, code.substr(0, 100), msg, fp, context.dump(), establishmentId, userId,
                appVersion.substr(0, 40), platform.substr(0, 40));
            if (!r.empty() && db::boolean(r[0][0]) && level != "warning")
                tx.q("INSERT INTO admin_notifications (kind, params, establishment_id) VALUES ('new_error', $1::jsonb, $2)",
                     json{{"source", source}, {"code", code}}.dump(), establishmentId);
        });
    } catch (...) {
    }
}

void registerSupportRoutes(App& app, Env& env) {
    app.get("/api/v1/support/tickets", {Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(std::string(kUserTicketSelect) +
                                          "WHERE t.establishment_id = $1 AND ($2 OR t.created_by = $3) "
                                          "ORDER BY t.last_message_at DESC LIMIT 200",
                                      ctx.actor.establishmentId, seesAll(ctx.actor), ctx.actor.userId))
                items.push_back(userTicketRow(r));
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    // Opening a ticket is allowed even when the subscription is suspended (support matters most then).
    app.post("/api/v1/support/tickets", {Access::User}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string category = in::reqEnum(b, "category", {"bug", "question", "billing", "feature"});
        std::string severity = in::optEnum(b, "severity", {"low", "normal", "high", "critical"}).value_or("normal");
        std::string source = in::optEnum(b, "source", {"business", "pos"}).value_or("business");
        std::string subject = in::reqStr(b, "subject", 160);
        if (subject.size() < 3) throw invalid("subject", "too_short");
        std::string body = in::reqStr(b, "body", 10000);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            int today = tx.one("SELECT count(*) FROM support_tickets WHERE establishment_id = $1 AND created_at > now() - "
                               "interval '1 day'",
                               ctx.actor.establishmentId)[0].as<int>();
            if (today >= setting(tx, "tickets_max_per_day", 20)) throw tooMany();
            auto r = tx.one("INSERT INTO support_tickets (establishment_id, created_by, source, category, severity, subject, "
                            "app_version, platform) VALUES ($1,$2,$3,$4,$5,$6,$7,$8) RETURNING id, number",
                            ctx.actor.establishmentId, ctx.actor.userId, source, category, severity, subject,
                            in::optStr(b, "app_version", 40), in::optStr(b, "platform", 40));
            std::string id = db::str(r[0]);
            tx.q("INSERT INTO support_ticket_messages (ticket_id, author_type, author_id, body) VALUES ($1,'user',$2,$3)", id,
                 ctx.actor.userId, body);
            tx.q("INSERT INTO admin_notifications (kind, params, establishment_id) VALUES ('ticket_opened', $1::jsonb, $2)",
                 json{{"ticket_id", id}, {"number", r[1].as<int64_t>()}, {"severity", severity}, {"category", category}}.dump(),
                 ctx.actor.establishmentId);
            audit(tx, ctx.actor, "support.ticket_opened", "support_ticket", id, {{"category", category}, {"severity", severity}});
            return userTicketRow(tx.one(std::string(kUserTicketSelect) + "WHERE t.id = $1", id));
        });
        ctx.json(201, out);
    });

    app.get("/api/v1/support/tickets/:id", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "ticket");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            ticketFor(tx, ctx.actor, id);
            json t = userTicketRow(tx.one(std::string(kUserTicketSelect) + "WHERE t.id = $1", id));
            json msgs = json::array();
            for (const auto& r : tx.q("SELECT m.author_type, CASE WHEN m.author_type = 'user' THEN COALESCE(u.full_name,'') "
                                      "ELSE 'Nhill' END, m.body, nh_iso(m.created_at) FROM support_ticket_messages m LEFT JOIN "
                                      "users u ON m.author_type = 'user' AND u.id = m.author_id WHERE m.ticket_id = $1 AND "
                                      "NOT m.internal ORDER BY m.id",
                                      id))
                msgs.push_back({{"author_type", db::str(r[0])}, {"author", db::str(r[1])}, {"body", db::str(r[2])},
                                {"created_at", db::str(r[3])}});
            t["messages"] = msgs;
            return t;
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/support/tickets/:id/messages", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "ticket");
        std::string body = in::reqStr(ctx.body(), "body", 10000);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto t = ticketFor(tx, ctx.actor, id, true);
            if (db::str(t[1]) == "closed") throw conflict("ticket_closed");
            tx.q("INSERT INTO support_ticket_messages (ticket_id, author_type, author_id, body) VALUES ($1,'user',$2,$3)", id,
                 ctx.actor.userId, body);
            // A reply re-opens a resolved ticket / answers a pending question.
            tx.q("UPDATE support_tickets SET last_message_at = now(), resolved_at = NULL, status = CASE WHEN status IN "
                 "('resolved','waiting_customer') THEN 'open' ELSE status END WHERE id = $1",
                 id);
            tx.q("INSERT INTO admin_notifications (kind, params, establishment_id) VALUES ('ticket_reply', $1::jsonb, $2)",
                 json{{"ticket_id", id}, {"number", t[2].as<int64_t>()}}.dump(), ctx.actor.establishmentId);
            audit(tx, ctx.actor, "support.ticket_replied", "support_ticket", id);
        });
        ctx.json(201, {{"ok", true}});
    });

    app.post("/api/v1/support/tickets/:id/close", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "ticket");
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            ticketFor(tx, ctx.actor, id, true);
            tx.q("UPDATE support_tickets SET status = 'closed', resolved_at = COALESCE(resolved_at, now()) WHERE id = $1", id);
            tx.q("INSERT INTO support_ticket_messages (ticket_id, author_type, author_id, body) VALUES "
                 "($1,'system',$2,'status: closed by the customer')",
                 id, ctx.actor.userId);
            audit(tx, ctx.actor, "support.ticket_closed", "support_ticket", id);
        });
        ctx.noContent();
    });

    // Technical error reports from the applications: personal data is masked (redact),
    // the context object is capped and kept for diagnosis only.
    app.post("/api/v1/client-errors", {Access::User}, [&env](Ctx& ctx) {
        throttle(env.pool, "client-errors:" + ctx.actor.userId, 60);
        const auto& b = ctx.body();
        std::string source = in::reqEnum(b, "source", {"business", "console", "pos"});
        std::string level = in::optEnum(b, "level", {"warning", "error", "fatal"}).value_or("error");
        std::string code = in::reqStr(b, "code", 100);
        std::string message = redact(in::reqStr(b, "message", 2000));
        json context = b.contains("context") && b["context"].is_object() ? redactJson(b["context"]) : json::object();
        if (context.dump().size() > 4000) context = {{"truncated", true}};
        recordErrorEvent(env.pool, source, level, code, message, context, ctx.actor.establishmentId, ctx.actor.userId,
                         in::optStr(b, "app_version", 40).value_or(""), in::optStr(b, "platform", 40).value_or(""));
        ctx.json(202, {{"ok", true}});
    });
}

}  // namespace nh
