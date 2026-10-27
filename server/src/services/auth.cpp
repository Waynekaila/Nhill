// User authentication, sessions (access + refresh tokens) and profile.
#include <algorithm>
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

// Recovery codes are 8 digits: only an HMAC under a server key is stored, bound to the
// account, so that a copy of the database does not allow guessing them offline.
std::string resetCodeHash(const Env& env, const std::string& userId, const std::string& code) {
    return crypto::hmacSha256Hex(env.cfg.resetKey, userId + ":" + code);
}

std::string digitsOf(const std::string& s) {
    std::string out;
    for (char c : s)
        if (c >= '0' && c <= '9') out.push_back(c);
    return out;
}

std::string requirePasswordField(const json& b, const std::string& key) {
    if (!b.contains(key) || !b[key].is_string()) throw invalid(key, "required");
    return b[key].get<std::string>();
}

void checkNewPassword(const std::string& password, const std::string& email, const std::string& field) {
    auto issue = nhill::validation::checkPassword(password, email);
    if (issue != nhill::validation::PasswordIssue::None)
        throw invalid(field, nhill::validation::passwordIssueCode(issue));
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

void applyNewPassword(db::Tx& tx, const std::string& userId, const std::string& passwordHash,
                      const std::string& reason, const std::optional<std::string>& keepSessionId) {
    tx.q("UPDATE users SET password_hash = $2, password_changed_at = now(), must_change_password = false, "
         "temporary_password_expires_at = NULL WHERE id = $1",
         userId, passwordHash);
    // Every pending recovery secret becomes unusable, and every other session is closed.
    tx.q("UPDATE password_resets SET revoked_at = now() WHERE user_id = $1 AND used_at IS NULL AND revoked_at IS NULL",
         userId);
    tx.q("UPDATE sessions SET revoked_at = now(), revoke_reason = $2 WHERE user_id = $1 AND revoked_at IS NULL "
         "AND ($3::uuid IS NULL OR id <> $3::uuid)",
         userId, reason, keepSessionId);
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
        auto user = tx.opt(
            "SELECT id, password_hash, status, must_change_password, "
            "(temporary_password_expires_at IS NOT NULL AND temporary_password_expires_at <= now()) "
            "FROM users WHERE email = $1",
            email);
        bool ok = false;
        if (user) {
            ok = crypto::verifyPassword(db::str((*user)[1]), password) && db::str((*user)[2]) == "active";
        } else {
            crypto::verifyPassword(dummyHash(env), password);
        }
        if (!ok) return {{"_failed", true}};
        const std::string userId = db::str((*user)[0]);
        if (db::boolean((*user)[3])) {
            // Temporary password given by an administrator: no session, only a one-time
            // ticket to choose a new password (POST /auth/password/change-required).
            if (db::boolean((*user)[4])) return {{"_expired", userId}};
            tx.q("UPDATE password_resets SET revoked_at = now() WHERE user_id = $1 AND purpose = 'forced_change' "
                 "AND used_at IS NULL AND revoked_at IS NULL",
                 userId);
            std::string ticket = crypto::randomToken(32);
            auto t = tx.one(
                "INSERT INTO password_resets (user_id, purpose, secret_hash, expires_at, ip) "
                "VALUES ($1, 'forced_change', decode($2,'hex'), now() + interval '15 minutes', $3) "
                "RETURNING nh_iso(expires_at)",
                userId, crypto::sha256Hex(ticket), ctx.actor.ip);
            auditSystem(tx, establishmentId, "auth.password_change_required", "user", userId, {{"ip", ctx.actor.ip}},
                        "user", userId);
            return {{"password_change_required", true}, {"change_token", ticket}, {"expires_at", db::str(t[0])}};
        }
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
    if (out.contains("_expired")) {
        std::string userId = out["_expired"];
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auditSystem(tx, establishmentId, "auth.temporary_password_expired", "user", userId, {{"ip", ctx.actor.ip}},
                        "user", userId);
        });
        // The caller proved the temporary password: telling it has expired reveals nothing more.
        throw unauthorized("temporary_password_expired");
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
        checkNewPassword(np, ctx.actor.email, "new_password");
        throttle(env.pool, "pwchange:" + ctx.actor.userId, 5);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto row = tx.one("SELECT password_hash FROM users WHERE id = $1", ctx.actor.userId);
            if (!crypto::verifyPassword(db::str(row[0]), b["current_password"].get<std::string>()))
                throw unauthorized("invalid_credentials");
            // Every other session of this user is closed, pending recovery secrets are revoked.
            applyNewPassword(tx, ctx.actor.userId, crypto::hashPassword(np, env.cfg.argon2), "password_changed",
                             ctx.actor.sessionId);
            audit(tx, ctx.actor, "auth.password_changed", "user", ctx.actor.userId);
            emailUser(tx, ctx.actor.userId, "password_changed");
        });
        jobs::kickEmails();
        ctx.noContent();
    });

    // ---------------------------------------------------------------- forgotten password
    // Members of an establishment are helped by its administrator (temporary password,
    // POST /members/:id/password-reset). Accounts that no single establishment can reset
    // — owners and administrators, accounts used in several establishments — receive a
    // one-time code by e-mail. The answer is identical whether the address exists or not.
    app.post("/api/v1/auth/password/forgot", {Access::Public}, [&env](Ctx& ctx) {
        std::string email = in::reqEmail(ctx.body(), "email");
        throttle(env.pool, "pwforgot:ip:" + ctx.actor.ip, 10, 60);
        throttle(env.pool, "pwforgot:email:" + email, 3, 60);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto u = tx.opt(
                "SELECT u.id, COALESCE(bool_or(m.role IN ('owner','admin') AND e.id IS NOT NULL), false), count(e.id), "
                "min(e.trade_name) FROM users u "
                "LEFT JOIN memberships m ON m.user_id = u.id AND m.status = 'active' "
                "LEFT JOIN establishments e ON e.id = m.establishment_id AND e.status <> 'closed' "
                "WHERE u.email = $1 AND u.status = 'active' GROUP BY u.id",
                email);
            if (!u) return;  // unknown or disabled account: same answer, nothing is sent
            const std::string userId = db::str((*u)[0]);
            const bool manager = db::boolean((*u)[1]);
            const int establishments = (*u)[2].as<int>();
            if (manager || establishments >= 2) {
                tx.q("UPDATE password_resets SET revoked_at = now() WHERE user_id = $1 AND purpose = 'email_code' "
                     "AND used_at IS NULL AND revoked_at IS NULL",
                     userId);
                const std::string code = crypto::randomDigits(8);
                const int minutes = std::clamp(setting(tx, "password_reset_minutes", 20), 5, 60);
                auto r = tx.one(
                    "INSERT INTO password_resets (user_id, purpose, secret_hash, expires_at, ip) "
                    "VALUES ($1, 'email_code', decode($2,'hex'), now() + make_interval(mins => $3), $4) "
                    "RETURNING nh_iso(expires_at)",
                    userId, resetCodeHash(env, userId, code), minutes, ctx.actor.ip);
                // The code travels sealed in the outbox and is erased once sent.
                auto sealed = crypto::seal(env.cfg.dataKey, code);
                emailUser(tx, userId, "password_reset_code",
                          {{"code_box", crypto::toHex(sealed.cipher)}, {"code_nonce", crypto::toHex(sealed.nonce)},
                           {"minutes", minutes}, {"expires_at", db::str(r[0])}});
                auditSystem(tx, std::nullopt, "auth.password_reset_requested", "user", userId,
                            {{"ip", ctx.actor.ip}, {"delivery", "email_code"}}, "anonymous");
            } else if (establishments == 1) {
                emailUser(tx, userId, "password_reset_contact_admin", {{"establishment", db::str((*u)[3])}});
                auditSystem(tx, std::nullopt, "auth.password_reset_requested", "user", userId,
                            {{"ip", ctx.actor.ip}, {"delivery", "contact_admin"}}, "anonymous");
            }
        });
        jobs::kickEmails();
        ctx.json(202, {{"status", "accepted"}});
    });

    app.post("/api/v1/auth/password/reset", {Access::Public}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string email = in::reqEmail(b, "email");
        std::string code = digitsOf(in::reqStr(b, "code", 20));
        std::string np = requirePasswordField(b, "new_password");
        checkNewPassword(np, email, "new_password");
        throttle(env.pool, "pwreset:ip:" + ctx.actor.ip, 20);
        throttle(env.pool, "pwreset:email:" + email, 10);
        // Hashed before looking anything up: the time taken does not depend on the account.
        const std::string newHash = crypto::hashPassword(np, env.cfg.argon2);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto u = tx.opt("SELECT id FROM users WHERE email = $1 AND status = 'active'", email);
            const std::string userId = u ? db::str((*u)[0]) : std::string("00000000-0000-0000-0000-000000000000");
            const std::string presented = resetCodeHash(env, userId, code);
            if (!u || code.size() != 8) return {{"_failed", true}};
            auto r = tx.opt(
                "SELECT id, encode(secret_hash,'hex') FROM password_resets WHERE user_id = $1 AND purpose = 'email_code' "
                "AND used_at IS NULL AND revoked_at IS NULL AND expires_at > now() "
                "ORDER BY created_at DESC LIMIT 1 FOR UPDATE",
                userId);
            if (!r) return {{"_failed", true}};
            const std::string resetId = db::str((*r)[0]);
            if (!crypto::constantTimeEquals(db::str((*r)[1]), presented)) {
                // Each wrong code counts; the code is revoked after the allowed number of attempts.
                const int maxAttempts = std::clamp(setting(tx, "password_reset_max_attempts", 5), 1, 10);
                tx.q("UPDATE password_resets SET attempts = attempts + 1, "
                     "revoked_at = CASE WHEN attempts + 1 >= $2 THEN now() ELSE revoked_at END WHERE id = $1",
                     resetId, maxAttempts);
                auditSystem(tx, std::nullopt, "auth.password_reset_failed", "user", userId, {{"ip", ctx.actor.ip}},
                            "anonymous");
                return {{"_failed", true}};
            }
            tx.q("UPDATE password_resets SET used_at = now() WHERE id = $1", resetId);
            applyNewPassword(tx, userId, newHash, "password_reset");
            auditSystem(tx, std::nullopt, "auth.password_reset", "user", userId,
                        {{"ip", ctx.actor.ip}, {"method", "email_code"}}, "user", userId);
            emailUser(tx, userId, "password_changed");
            return {{"ok", true}};
        });
        if (out.contains("_failed")) throw badRequest("invalid_reset_code", "invalid or expired code");
        jobs::kickEmails();
        ctx.noContent();
    });

    // Sign-in with a temporary password: the ticket returned by /auth/login is exchanged,
    // with a new password, for the normal sign-in answer (session or establishment choice).
    app.post("/api/v1/auth/password/change-required", {Access::Public}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string ticket = in::reqStr(b, "change_token", 100);
        std::string np = requirePasswordField(b, "new_password");
        auto est = in::optUuid(b, "establishment_id");
        throttle(env.pool, "pwforced:ip:" + ctx.actor.ip, 20);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto r = tx.opt(
                "SELECT p.id, u.id, u.email, u.password_hash FROM password_resets p JOIN users u ON u.id = p.user_id "
                "WHERE p.secret_hash = decode($1,'hex') AND p.purpose = 'forced_change' AND p.used_at IS NULL "
                "AND p.revoked_at IS NULL AND p.expires_at > now() AND u.status = 'active' AND u.must_change_password "
                "FOR UPDATE OF p",
                crypto::sha256Hex(ticket));
            if (!r) return {{"_failed", true}};
            const std::string userId = db::str((*r)[1]), email = db::str((*r)[2]);
            checkNewPassword(np, email, "new_password");
            if (crypto::verifyPassword(db::str((*r)[3]), np)) throw invalid("new_password", "password_reused");
            tx.q("UPDATE password_resets SET used_at = now() WHERE id = $1", db::str((*r)[0]));
            applyNewPassword(tx, userId, crypto::hashPassword(np, env.cfg.argon2), "password_changed");
            auditSystem(tx, std::nullopt, "auth.password_changed", "user", userId,
                        {{"ip", ctx.actor.ip}, {"method", "temporary_password"}}, "user", userId);
            return {{"email", email}};
        });
        if (out.contains("_failed")) throw badRequest("invalid_change_token", "invalid or expired ticket");
        ctx.json(200, doLogin(env, ctx, out["email"].get<std::string>(), np, est));
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
