// User authentication, sessions (access + refresh tokens) and profile.
#include <mutex>

#include "nhill/validation.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh {

namespace {

std::string tokenHash(const std::string& token) { return crypto::sha256Hex(token); }

// Verifying against a dummy hash for unknown accounts keeps response times
// identical, so the endpoint does not reveal which e-mails exist.
const std::string& dummyHash(Env& env) {
    static std::once_flag once;
    static std::string h;
    std::call_once(once, [&] { h = crypto::hashPassword("nhill-dummy-password", env.cfg.argon2); });
    return h;
}

json permissionsFor(const std::string& role) {
    json p = json::array();
    for (const char* perm :
         {"settings.manage", "users.manage", "code.view", "code.rotate", "subscription.manage", "catalog.write",
          "parties.write", "sales.write", "sales.issue", "purchases.write", "stock.adjust", "expenses.write",
          "payments.write", "reports.read", "audit.read", "pos.sell", "pos.refund", "pos.void", "pos.cash",
          "pos.supervise", "pos.manage", "pos.report"})
        if (App::roleHas(role, perm)) p.push_back(perm);
    return p;
}

}  // namespace

json createSession(Env& env, db::Tx& tx, const std::string& userId, const std::string& establishmentId,
                   const Actor& a) {
    std::string access = crypto::randomToken(32);
    std::string refresh = crypto::randomToken(32);
    auto row = tx.one(
        "INSERT INTO sessions (user_id, establishment_id, access_hash, access_expires_at, refresh_hash, "
        "refresh_expires_at, user_agent, ip) VALUES ($1, $2, decode($3,'hex'), now() + make_interval(mins => $4), "
        "decode($5,'hex'), now() + make_interval(days => $6), $7, $8) "
        "RETURNING id, nh_iso(access_expires_at), nh_iso(refresh_expires_at)",
        userId, establishmentId, tokenHash(access), env.cfg.accessTokenMinutes, tokenHash(refresh),
        env.cfg.refreshTokenDays, a.userAgent, a.ip);
    return {{"token_type", "Bearer"},
            {"access_token", access},
            {"access_expires_at", db::str(row[1])},
            {"refresh_token", refresh},
            {"refresh_expires_at", db::str(row[2])},
            {"establishment_id", establishmentId}};
}

void authenticateUser(Env& env, Ctx& ctx) {
    std::string h = ctx.header("Authorization");
    if (h.rfind("Bearer ", 0) != 0 || h.size() < 20) throw unauthorized();
    std::string token = h.substr(7);
    db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
        auto row = tx.opt(
            "SELECT s.id, u.id, u.email, u.full_name, u.preferred_language, m.role, e.id, e.kind, e.status, "
            "COALESCE(sub.status, 'suspended'), (s.last_seen_at < now() - interval '5 minutes') "
            "FROM sessions s JOIN users u ON u.id = s.user_id "
            "JOIN memberships m ON m.user_id = u.id AND m.establishment_id = s.establishment_id "
            "JOIN establishments e ON e.id = s.establishment_id "
            "LEFT JOIN subscriptions sub ON sub.establishment_id = e.id "
            "WHERE s.access_hash = decode($1,'hex') AND s.revoked_at IS NULL AND s.access_expires_at > now() "
            "AND u.status = 'active' AND m.status = 'active'",
            tokenHash(token));
        if (!row) throw unauthorized("invalid_token");
        const auto& r = *row;
        ctx.actor.sessionId = db::str(r[0]);
        ctx.actor.userId = db::str(r[1]);
        ctx.actor.email = db::str(r[2]);
        ctx.actor.fullName = db::str(r[3]);
        if (ctx.header("Accept-Language").empty()) ctx.actor.lang = db::str(r[4]);
        ctx.actor.role = db::str(r[5]);
        ctx.actor.establishmentId = db::str(r[6]);
        ctx.actor.establishmentKind = db::str(r[7]);
        ctx.actor.establishmentStatus = db::str(r[8]);
        ctx.actor.subscriptionStatus = db::str(r[9]);
        if (db::boolean(r[10])) tx.q("UPDATE sessions SET last_seen_at = now() WHERE id = $1", ctx.actor.sessionId);
    });
}

namespace {

// Logs a user in, optionally restricted to one establishment.
json doLogin(Env& env, Ctx& ctx, const std::string& email, const std::string& password,
             const std::optional<std::string>& establishmentId) {
    throttle(env.pool, "login:email:" + email, 8);
    throttle(env.pool, "login:ip:" + ctx.actor.ip, 40);
    json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
        auto user = tx.opt("SELECT id, password_hash, status FROM users WHERE email = $1", email);
        bool ok = false;
        if (user) {
            ok = crypto::verifyPassword(db::str((*user)[1]), password) && db::str((*user)[2]) == "active";
        } else {
            crypto::verifyPassword(dummyHash(env), password);
        }
        if (!ok) return {{"_failed", true}};
        const std::string userId = db::str((*user)[0]);
        if (crypto::passwordNeedsRehash(db::str((*user)[1]), env.cfg.argon2))
            tx.q("UPDATE users SET password_hash = $2 WHERE id = $1", userId,
                 crypto::hashPassword(password, env.cfg.argon2));
        auto ms = tx.q(
            "SELECT e.id, e.trade_name, m.role FROM memberships m JOIN establishments e ON e.id = m.establishment_id "
            "WHERE m.user_id = $1 AND m.status = 'active' AND e.status <> 'closed' ORDER BY e.trade_name",
            userId);
        std::optional<std::string> chosen;
        if (establishmentId) {
            for (const auto& m : ms)
                if (db::str(m[0]) == *establishmentId) chosen = *establishmentId;
            if (!chosen) throw unauthorized("invalid_credentials");
        } else if (ms.size() == 1) {
            chosen = db::str(ms[0][0]);
        } else if (ms.empty()) {
            throw forbidden("no_establishment");
        } else {
            json list = json::array();
            for (const auto& m : ms) list.push_back({{"id", db::str(m[0])}, {"name", db::str(m[1])}, {"role", db::str(m[2])}});
            return {{"choose_establishment", list}};
        }
        Actor a = ctx.actor;
        a.userId = userId;
        a.establishmentId = *chosen;
        audit(tx, a, "auth.login", "session", "", json::object());
        return createSession(env, tx, userId, *chosen, ctx.actor);
    });
    if (out.contains("_failed")) {
        // Recorded in its own transaction so the failure trail is never rolled back.
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auditSystem(tx, establishmentId, "auth.login_failed", "user", "", {{"ip", ctx.actor.ip}}, "anonymous");
        });
        throw unauthorized("invalid_credentials");
    }
    return out;
}

}  // namespace

json loginForEstablishment(Env& env, Ctx& ctx, const std::string& email, const std::string& password,
                           const std::string& establishmentId) {
    return doLogin(env, ctx, email, password, establishmentId);
}

void registerAuthRoutes(App& app, Env& env) {
    app.post("/api/v1/auth/login", {Access::Public}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string email = in::reqEmail(b, "email");
        if (!b.contains("password") || !b["password"].is_string()) throw invalid("password", "required");
        auto est = in::optUuid(b, "establishment_id");
        ctx.json(200, doLogin(env, ctx, email, b["password"].get<std::string>(), est));
    });

    app.post("/api/v1/auth/refresh", {Access::Public}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        if (!b.contains("refresh_token") || !b["refresh_token"].is_string()) throw invalid("refresh_token", "required");
        std::string presented = crypto::sha256Hex(b["refresh_token"].get<std::string>());
        throttle(env.pool, "refresh:ip:" + ctx.actor.ip, 120);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto s = tx.opt(
                "SELECT s.id, s.user_id, s.establishment_id FROM sessions s JOIN users u ON u.id = s.user_id "
                "JOIN memberships m ON m.user_id = s.user_id AND m.establishment_id = s.establishment_id "
                "WHERE s.refresh_hash = decode($1,'hex') AND s.revoked_at IS NULL AND s.refresh_expires_at > now() "
                "AND u.status = 'active' AND m.status = 'active' FOR UPDATE OF s",
                presented);
            if (!s) {
                // Reuse of an already-rotated refresh token: assume theft, revoke the session.
                auto reused = tx.opt(
                    "UPDATE sessions SET revoked_at = now(), revoke_reason = 'refresh_token_reuse' "
                    "WHERE previous_refresh_hash = decode($1,'hex') AND revoked_at IS NULL "
                    "RETURNING establishment_id, user_id",
                    presented);
                if (reused)
                    auditSystem(tx, db::str((*reused)[0]), "auth.refresh_reuse_detected", "user",
                                db::str((*reused)[1]), {{"ip", ctx.actor.ip}});
                return nullptr;
            }
            std::string access = crypto::randomToken(32), refresh = crypto::randomToken(32);
            auto row = tx.one(
                "UPDATE sessions SET previous_refresh_hash = refresh_hash, refresh_hash = decode($2,'hex'), "
                "access_hash = decode($3,'hex'), access_expires_at = now() + make_interval(mins => $4), "
                "refresh_expires_at = now() + make_interval(days => $5), last_seen_at = now() WHERE id = $1 "
                "RETURNING nh_iso(access_expires_at), nh_iso(refresh_expires_at)",
                db::str((*s)[0]), crypto::sha256Hex(refresh), crypto::sha256Hex(access), env.cfg.accessTokenMinutes,
                env.cfg.refreshTokenDays);
            return {{"token_type", "Bearer"},
                    {"access_token", access},
                    {"access_expires_at", db::str(row[0])},
                    {"refresh_token", refresh},
                    {"refresh_expires_at", db::str(row[1])},
                    {"establishment_id", db::str((*s)[2])}};
        });
        if (out.is_null()) throw unauthorized("invalid_refresh_token");
        ctx.json(200, out);
    });

    app.post("/api/v1/auth/logout", {Access::User}, [&env](Ctx& ctx) {
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.q("UPDATE sessions SET revoked_at = now(), revoke_reason = 'logout' WHERE id = $1",
                 ctx.actor.sessionId);
        });
        ctx.noContent();
    });

    app.get("/api/v1/me", {Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, ctx.actor.establishmentId, [&](db::Tx& tx) -> json {
            auto u = tx.one("SELECT email, full_name, phone, preferred_language FROM users WHERE id = $1",
                            ctx.actor.userId);
            auto e = tx.one(
                "SELECT id, trade_name, kind, country_code, main_currency, preferred_language, status, "
                "compliance_profile_code, vat_registered, logo_object_id IS NOT NULL FROM establishments WHERE id = $1",
                ctx.actor.establishmentId);
            auto others = tx.q(
                "SELECT e.id, e.trade_name FROM memberships m JOIN establishments e ON e.id = m.establishment_id "
                "WHERE m.user_id = $1 AND m.status = 'active' AND e.status <> 'closed' AND e.id <> $2",
                ctx.actor.userId, ctx.actor.establishmentId);
            json other = json::array();
            for (const auto& o : others) other.push_back({{"id", db::str(o[0])}, {"name", db::str(o[1])}});
            return {
                {"user",
                 {{"id", ctx.actor.userId},
                  {"email", db::str(u[0])},
                  {"full_name", db::str(u[1])},
                  {"phone", db::optStr(u[2]) ? json(db::str(u[2])) : json(nullptr)},
                  {"preferred_language", db::str(u[3])}}},
                {"role", ctx.actor.role},
                {"permissions", permissionsFor(ctx.actor.role)},
                {"establishment",
                 {{"id", db::str(e[0])},
                  {"trade_name", db::str(e[1])},
                  {"kind", db::str(e[2])},
                  {"country_code", db::str(e[3])},
                  {"main_currency", db::str(e[4])},
                  {"preferred_language", db::str(e[5])},
                  {"status", db::str(e[6])},
                  {"profile", db::str(e[7])},
                  {"vat_registered", db::boolean(e[8])},
                  {"has_logo", db::boolean(e[9])}}},
                // The plan and its options are shown to owners / administrators only; other
                // members only learn whether they can write (read-only mode explained).
                {"subscription", App::roleHas(ctx.actor.role, "subscription.manage")
                                     ? subscriptions::view(tx, ctx.actor.establishmentId)
                                     : json{{"status", ctx.actor.subscriptionStatus},
                                            {"write_allowed", ctx.actor.subscriptionStatus == "trialing" ||
                                                                  ctx.actor.subscriptionStatus == "active" ||
                                                                  ctx.actor.subscriptionStatus == "past_due"}}},
                {"other_establishments", other}};
        });
        ctx.json(200, out);
    });

    app.patch("/api/v1/me", {Access::User}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        auto name = in::optStr(b, "full_name", 120);
        auto lang = in::optEnum(b, "preferred_language", {"fr", "en"});
        auto phone = in::optPhone(b, "phone");
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.q("UPDATE users SET full_name = COALESCE($2, full_name), preferred_language = COALESCE($3, "
                 "preferred_language), phone = COALESCE($4, phone) WHERE id = $1",
                 ctx.actor.userId, name, lang, phone);
        });
        ctx.noContent();
    });

    app.post("/api/v1/me/password", {Access::User}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        if (!b.contains("current_password") || !b["current_password"].is_string()) throw invalid("current_password", "required");
        if (!b.contains("new_password") || !b["new_password"].is_string()) throw invalid("new_password", "required");
        std::string np = b["new_password"];
        auto issue = nhill::validation::checkPassword(np, ctx.actor.email);
        if (issue != nhill::validation::PasswordIssue::None)
            throw invalid("new_password", nhill::validation::passwordIssueCode(issue));
        throttle(env.pool, "pwchange:" + ctx.actor.userId, 5);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto row = tx.one("SELECT password_hash FROM users WHERE id = $1", ctx.actor.userId);
            if (!crypto::verifyPassword(db::str(row[0]), b["current_password"].get<std::string>()))
                throw unauthorized("invalid_credentials");
            tx.q("UPDATE users SET password_hash = $2, password_changed_at = now() WHERE id = $1", ctx.actor.userId,
                 crypto::hashPassword(np, env.cfg.argon2));
            // Every other session of this user is closed.
            tx.q("UPDATE sessions SET revoked_at = now(), revoke_reason = 'password_changed' "
                 "WHERE user_id = $1 AND id <> $2 AND revoked_at IS NULL",
                 ctx.actor.userId, ctx.actor.sessionId);
            audit(tx, ctx.actor, "auth.password_changed", "user", ctx.actor.userId);
        });
        ctx.noContent();
    });

    app.post("/api/v1/me/switch", {Access::User}, [&env](Ctx& ctx) {
        // Switches to another establishment of the same user without re-entering the password.
        std::string target = in::reqUuid(ctx.body(), "establishment_id");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto m = tx.opt(
                "SELECT 1 FROM memberships m JOIN establishments e ON e.id = m.establishment_id WHERE m.user_id = $1 "
                "AND m.establishment_id = $2 AND m.status = 'active' AND e.status <> 'closed'",
                ctx.actor.userId, target);
            if (!m) throw forbidden("not_a_member");
            tx.q("UPDATE sessions SET revoked_at = now(), revoke_reason = 'switch' WHERE id = $1", ctx.actor.sessionId);
            return createSession(env, tx, ctx.actor.userId, target, ctx.actor);
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/notifications", {Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            bool manager = ctx.actor.role == "owner" || ctx.actor.role == "admin";
            auto r = tx.q(
                "SELECT id, kind, params, nh_iso(created_at), read_at IS NOT NULL FROM notifications "
                "WHERE establishment_id = $1 AND (user_id = $2 OR (user_id IS NULL AND $3)) "
                "ORDER BY created_at DESC LIMIT 50",
                ctx.actor.establishmentId, ctx.actor.userId, manager);
            json items = json::array();
            for (const auto& n : r)
                items.push_back({{"id", db::str(n[0])},
                                 {"kind", db::str(n[1])},
                                 {"params", json::parse(db::str(n[2]))},
                                 {"created_at", db::str(n[3])},
                                 {"read", db::boolean(n[4])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/notifications/:id/read", {Access::User}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"));
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.q("UPDATE notifications SET read_at = COALESCE(read_at, now()) WHERE id = $1 AND establishment_id = $2",
                 id, ctx.actor.establishmentId);
        });
        ctx.noContent();
    });
}

}  // namespace nh
