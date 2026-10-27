// Team management: members, roles, invitations.
#include <algorithm>
#include <string_view>

#include "nhill/validation.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"

namespace nh {

namespace {

// Seats allowed: the plan decides; during a trial an individual business has
// one seat and a company the `trial_max_users` setting (default 5).
int seatLimit(db::Tx& tx, const std::string& est, const std::string& kind) {
    auto r = tx.one(
        "SELECT p.max_users, s.is_trial FROM subscriptions s LEFT JOIN plans p ON p.id = s.plan_id "
        "WHERE s.establishment_id = $1",
        est);
    if (!r[0].is_null()) return r[0].as<int>();
    if (db::boolean(r[1]) || r[0].is_null()) return kind == "individual" ? 1 : setting(tx, "trial_max_users", 5);
    return 1000000;
}

// Temporary password: 12 characters without look-alikes (0/O, 1/l/I), shown as
// xxxx-xxxx-xxxx (about 69 bits of entropy), always accepted by the password rule.
std::string temporaryPassword(const std::string& email) {
    static constexpr std::string_view alphabet = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (;;) {
        std::string r = crypto::randomFromAlphabet(alphabet, 12);
        std::string pw = r.substr(0, 4) + "-" + r.substr(4, 4) + "-" + r.substr(8, 4);
        if (nhill::validation::checkPassword(pw, email) == nhill::validation::PasswordIssue::None) return pw;
    }
}

}  // namespace

void registerMemberRoutes(App& app, Env& env) {
    // Forgotten password of a member: an owner / administrator gives a temporary password,
    // shown once, to hand over in person or by phone. The member must replace it at the
    // next sign-in; every session of the member is closed now. Rules: same establishment
    // only, never the owner (e-mail recovery), administrators by the owner only, not
    // oneself, and not an account also used in another establishment (its password is
    // not this establishment's to change: e-mail recovery).
    app.post("/api/v1/members/:id/password-reset", {Access::User, "users.manage"}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"));
        throttle(env.pool, "pwreset:admin:" + ctx.actor.userId, 10);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto m = tx.opt(
                "SELECT m.user_id, m.role, m.status, u.status, u.email FROM memberships m JOIN users u ON u.id = m.user_id "
                "WHERE m.id = $1 AND m.establishment_id = $2 FOR UPDATE OF m",
                id, ctx.actor.establishmentId);
            if (!m) throw notFound("member");
            const std::string userId = db::str((*m)[0]), role = db::str((*m)[1]), email = db::str((*m)[4]);
            if (userId == ctx.actor.userId) throw forbidden("cannot_modify_self");
            if (role == "owner") throw forbidden("owner_immutable");
            if (role == "admin" && ctx.actor.role != "owner") throw forbidden("owner_required");
            if (db::str((*m)[2]) != "active" || db::str((*m)[3]) != "active")
                throw conflict("member_disabled", "re-enable the access first");
            int elsewhere = tx.one(
                "SELECT count(*) FROM memberships m JOIN establishments e ON e.id = m.establishment_id "
                "WHERE m.user_id = $1 AND m.establishment_id <> $2 AND m.status = 'active' AND e.status <> 'closed'",
                userId, ctx.actor.establishmentId)[0].as<int>();
            if (elsewhere > 0)
                throw conflict("account_shared", "this account is also used in another establishment: e-mail recovery");
            const std::string temporary = temporaryPassword(email);
            const int hours = std::clamp(setting(tx, "temporary_password_hours", 72), 1, 168);
            auto r = tx.one(
                "UPDATE users SET password_hash = $2, password_changed_at = now(), must_change_password = true, "
                "temporary_password_expires_at = now() + make_interval(hours => $3) WHERE id = $1 "
                "RETURNING nh_iso(temporary_password_expires_at)",
                userId, crypto::hashPassword(temporary, env.cfg.argon2), hours);
            tx.q("UPDATE password_resets SET revoked_at = now() WHERE user_id = $1 AND used_at IS NULL AND revoked_at IS NULL",
                 userId);
            tx.q("UPDATE sessions SET revoked_at = now(), revoke_reason = 'password_reset_by_admin' "
                 "WHERE user_id = $1 AND revoked_at IS NULL",
                 userId);
            // Never the password itself in the history, the e-mail or the logs.
            audit(tx, ctx.actor, "member.password_reset", "membership", id, {{"email", email}});
            emailUser(tx, userId, "password_reset_by_admin",
                      {{"establishment", db::str(tx.one("SELECT trade_name FROM establishments WHERE id = $1",
                                                        ctx.actor.establishmentId)[0])}});
            return {{"temporary_password", temporary}, {"expires_at", db::str(r[0])}, {"must_change_password", true}};
        });
        jobs::kickEmails();
        ctx.json(200, out);
    });

    app.get("/api/v1/members", {Access::User, "users.manage"}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json members = json::array();
            for (const auto& r : tx.q(
                     "SELECT m.id, u.id, u.email, u.full_name, m.role, m.status, nh_iso(m.created_at), "
                     "(SELECT nh_iso(max(s.last_seen_at)) FROM sessions s WHERE s.user_id = u.id AND "
                     " s.establishment_id = m.establishment_id) FROM memberships m JOIN users u ON u.id = m.user_id "
                     "WHERE m.establishment_id = $1 ORDER BY m.role = 'owner' DESC, u.full_name",
                     ctx.actor.establishmentId))
                members.push_back({{"id", db::str(r[0])}, {"user_id", db::str(r[1])}, {"email", db::str(r[2])},
                                   {"full_name", db::str(r[3])}, {"role", db::str(r[4])}, {"status", db::str(r[5])},
                                   {"created_at", db::str(r[6])},
                                   {"last_seen_at", r[7].is_null() ? json(nullptr) : json(db::str(r[7]))}});
            json invitations = json::array();
            for (const auto& r : tx.q(
                     "SELECT id, email, role, nh_iso(expires_at), nh_iso(created_at) FROM invitations "
                     "WHERE establishment_id = $1 AND accepted_at IS NULL AND revoked_at IS NULL AND expires_at > now() "
                     "ORDER BY created_at DESC",
                     ctx.actor.establishmentId))
                invitations.push_back({{"id", db::str(r[0])}, {"email", db::str(r[1])}, {"role", db::str(r[2])},
                                       {"expires_at", db::str(r[3])}, {"created_at", db::str(r[4])}});
            int limit = seatLimit(tx, ctx.actor.establishmentId, ctx.actor.establishmentKind);
            return {{"members", members}, {"invitations", invitations}, {"seat_limit", limit}};
        });
        ctx.json(200, out);
    });

    // The activation code is returned once, to be handed over by the owner
    // (in person, SMS...). Only its hash is stored.
    app.post("/api/v1/invitations", {Access::User, "users.manage", true}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string email = in::reqEmail(b, "email");
        std::string role = in::reqEnum(b, "role", {"admin", "manager", "sales", "accountant", "viewer", "cash_manager", "cashier"});
        if (role == "admin" && ctx.actor.role != "owner") throw forbidden("owner_required");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            int used = tx.one(
                "SELECT (SELECT count(*) FROM memberships WHERE establishment_id = $1 AND status = 'active') + "
                "(SELECT count(*) FROM invitations WHERE establishment_id = $1 AND accepted_at IS NULL AND "
                " revoked_at IS NULL AND expires_at > now())",
                ctx.actor.establishmentId)[0].as<int>();
            int limit = seatLimit(tx, ctx.actor.establishmentId, ctx.actor.establishmentKind);
            if (used >= limit)
                throw conflict("user_limit_reached", "the current offer does not allow more users", {{"limit", limit}});
            if (tx.opt("SELECT 1 FROM memberships m JOIN users u ON u.id = m.user_id WHERE m.establishment_id = $1 "
                       "AND u.email = $2 AND m.status = 'active'",
                       ctx.actor.establishmentId, email))
                throw conflict("already_member");
            std::string code = crypto::randomDigits(8);
            int days = setting(tx, "invitation_days", 7);
            auto r = tx.one(
                "INSERT INTO invitations (establishment_id, email, role, activation_hash, expires_at, created_by) "
                "VALUES ($1, $2, $3, decode($4,'hex'), now() + make_interval(days => $5), $6) "
                "RETURNING id, nh_iso(expires_at)",
                ctx.actor.establishmentId, email, role, crypto::sha256Hex(code), days, ctx.actor.userId);
            audit(tx, ctx.actor, "member.invited", "invitation", db::str(r[0]), {{"email", email}, {"role", role}});
            return {{"id", db::str(r[0])},
                    {"email", email},
                    {"role", role},
                    {"activation_code", code.substr(0, 4) + "-" + code.substr(4)},
                    {"expires_at", db::str(r[1])}};
        });
        ctx.json(201, out);
    });

    app.del("/api/v1/invitations/:id", {Access::User, "users.manage"}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"));
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.q("UPDATE invitations SET revoked_at = now() WHERE id = $1 AND establishment_id = $2 "
                          "AND accepted_at IS NULL AND revoked_at IS NULL RETURNING id",
                          id, ctx.actor.establishmentId);
            if (r.empty()) throw notFound("invitation");
            audit(tx, ctx.actor, "member.invitation_revoked", "invitation", id);
        });
        ctx.noContent();
    });

    app.patch("/api/v1/members/:id", {Access::User, "users.manage"}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"));
        const auto& b = ctx.body();
        auto role = in::optEnum(b, "role", {"admin", "manager", "sales", "accountant", "viewer", "cash_manager", "cashier"});
        auto status = in::optEnum(b, "status", {"active", "disabled"});
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto m = tx.opt("SELECT user_id, role FROM memberships WHERE id = $1 AND establishment_id = $2 FOR UPDATE", id,
                            ctx.actor.establishmentId);
            if (!m) throw notFound("member");
            std::string current = db::str((*m)[1]);
            if (current == "owner") throw forbidden("owner_immutable");
            if ((current == "admin" || role == "admin") && ctx.actor.role != "owner") throw forbidden("owner_required");
            if (db::str((*m)[0]) == ctx.actor.userId) throw forbidden("cannot_modify_self");
            tx.q("UPDATE memberships SET role = COALESCE($2, role), status = COALESCE($3, status) WHERE id = $1", id, role,
                 status);
            if (status == "disabled")
                tx.q("UPDATE sessions SET revoked_at = now(), revoke_reason = 'member_disabled' WHERE user_id = $1 "
                     "AND establishment_id = $2 AND revoked_at IS NULL",
                     db::str((*m)[0]), ctx.actor.establishmentId);
            audit(tx, ctx.actor, "member.updated", "membership", id, b);
        });
        ctx.noContent();
    });
}

}  // namespace nh
