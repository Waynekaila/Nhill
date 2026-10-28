// Creators' administration console API. Platform data only: administrators
// have no access to the business tables of establishments (RLS has no bypass).
// Statistics per establishment come from aggregate-only SQL functions
// (migration 0006): counts and sums per currency, never individual records.
// Periods are calendar days in the establishment's own time zone.
#include <algorithm>
#include <ctime>

#include "nhill/money.hpp"
#include "nhill/validation.hpp"
#include "services/entitlements.hpp"
#include "services/report.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"

namespace nh {

namespace v = nhill::validation;

void authenticateAdmin(Env& env, Ctx& ctx) {
    std::string h = ctx.header("Authorization");
    if (h.rfind("Bearer ", 0) != 0 || h.size() < 20) throw unauthorized();
    std::string hash = crypto::sha256Hex(h.substr(7));
    db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
        auto r = tx.opt("SELECT a.id, a.role, a.email, a.full_name FROM admin_sessions s JOIN admin_users a ON a.id = s.admin_id "
                        "WHERE s.token_hash = decode($1,'hex') AND s.revoked_at IS NULL AND s.expires_at > now() "
                        "AND a.status = 'active'",
                        hash);
        if (!r) throw unauthorized("invalid_token");
        ctx.actor.adminId = db::str((*r)[0]);
        ctx.actor.adminRole = db::str((*r)[1]);
        ctx.actor.email = db::str((*r)[2]);
        ctx.actor.fullName = db::str((*r)[3]);
    });
}

namespace {

std::string reason(const json& b) {
    std::string r = in::reqStr(b, "reason", 500);
    if (r.size() < 5) throw invalid("reason", "too_short");
    return r;
}

// Conditions of a plan (3.0.0). Only the keys present in `b` are written.
void applyPlanConditions(db::Tx& tx, const std::string& planId, const json& b) {
    auto present = [&](const char* k) { return b.contains(k); };
    std::optional<int> grace, trial;
    if (present("grace_days") && !b["grace_days"].is_null()) grace = in::optInt(b, "grace_days", 7, 0, 90);
    if (present("trial_days") && !b["trial_days"].is_null()) trial = in::optInt(b, "trial_days", 30, 1, 365);
    std::optional<std::string> reminders;
    if (present("reminder_days") && !b["reminder_days"].is_null()) {
        const json& a = b["reminder_days"];
        if (!a.is_array() || a.size() > 6) throw invalid("reminder_days", "array_of_at_most_6_required");
        std::string lit = "{";
        for (size_t i = 0; i < a.size(); ++i) {
            if (!a[i].is_number_integer() || a[i].get<int>() < 0 || a[i].get<int>() > 90)
                throw invalid("reminder_days", "days_between_0_and_90");
            lit += (i ? "," : "") + std::to_string(a[i].get<int>());
        }
        reminders = lit + "}";
    }
    std::optional<std::string> fallback;
    if (present("expiry_fallback_plan_id") && !b["expiry_fallback_plan_id"].is_null()) {
        fallback = in::reqUuid(b, "expiry_fallback_plan_id");
        if (*fallback == planId) throw invalid("expiry_fallback_plan_id", "same_plan");
        auto f = tx.opt("SELECT kind FROM plans WHERE id = $1", *fallback);
        if (!f) throw invalid("expiry_fallback_plan_id", "unknown_plan");
        if (db::str((*f)[0]) != "free" && db::str((*f)[0]) != "custom")
            throw invalid("expiry_fallback_plan_id", "free_or_custom_plan_required");
    }
    std::optional<bool> selfService, autoRenew, archived;
    if (present("self_service")) selfService = in::optBool(b, "self_service", true);
    if (present("auto_renew_allowed")) autoRenew = in::optBool(b, "auto_renew_allowed", true);
    if (present("archived")) archived = in::optBool(b, "archived", false);
    tx.q("UPDATE plans SET kind = COALESCE($2, kind), visibility = COALESCE($3, visibility), "
         "self_service = COALESCE($4, self_service), auto_renew_allowed = COALESCE($5, auto_renew_allowed), "
         "grace_days = CASE WHEN $6 THEN $7::integer ELSE grace_days END, "
         "reminder_days = CASE WHEN $8 THEN $9::int[] ELSE reminder_days END, "
         "trial_days = CASE WHEN $10 THEN $11::integer ELSE trial_days END, "
         "expiry_fallback_plan_id = CASE WHEN $12 THEN $13::uuid ELSE expiry_fallback_plan_id END, "
         "archived_at = CASE WHEN $14::boolean IS NULL THEN archived_at WHEN $14::boolean THEN COALESCE(archived_at, now()) "
         "ELSE NULL END, active = CASE WHEN $14::boolean THEN false ELSE active END WHERE id = $1",
         planId, in::optEnum(b, "kind", {"paid", "trial", "free", "custom"}), in::optEnum(b, "visibility", {"public", "private"}),
         selfService, autoRenew, present("grace_days"), grace, present("reminder_days"), reminders, present("trial_days"), trial,
         present("expiry_fallback_plan_id"), fallback, archived);
}

// Capability values of a plan: "entitlements" replaces them all; the 2.x fields
// (max_users, max_registers, max_warehouses, features.multi_warehouse) update one each.
void applyPlanEntitlements(db::Tx& tx, const std::string& planId, const json& b) {
    if (b.contains("entitlements")) {
        entitlements::savePlanEntitlements(tx, planId, entitlements::parsePlanEntitlements(tx, b["entitlements"]), true);
        return;
    }
    json legacy = json::object();
    if (b.contains("max_users")) legacy[entitlements::key::kUsers] = b["max_users"];
    if (b.contains("max_registers")) legacy[entitlements::key::kRegisters] = b["max_registers"];
    if (b.contains("max_warehouses")) legacy[entitlements::key::kWarehouses] = b["max_warehouses"];
    if (b.contains("features") && b["features"].is_object() && b["features"].contains("multi_warehouse"))
        legacy[entitlements::key::kMultiWarehouse] = b["features"]["multi_warehouse"];
    if (!legacy.empty()) entitlements::savePlanEntitlements(tx, planId, entitlements::parsePlanEntitlements(tx, legacy), false);
}



}  // namespace

void registerAdminRoutes(App& app, Env& env) {
    // ---------------------------------------------------------------- authentication
    // Step 1 of the first login: one-time setup token -> password + TOTP secret.
    app.post("/api/v1/admin/auth/setup", {Access::Public}, [&env](Ctx& ctx) {
        throttle(env.pool, "admin-setup:ip:" + ctx.actor.ip, 10);
        const auto& b = ctx.body();
        std::string email = in::reqEmail(b, "email");
        std::string token = in::reqStr(b, "setup_token", 100);
        if (!b.contains("password") || !b["password"].is_string()) throw invalid("password", "required");
        std::string password = b["password"];
        auto issue = v::checkPassword(password, email);
        if (issue != v::PasswordIssue::None) throw invalid("password", v::passwordIssueCode(issue));
        // 2.0.0: same 8-character minimum as every account. Console accounts keep the
        // mandatory TOTP second factor (platform-wide privileges), which is what makes
        // an 8-character password acceptable for them (NIST SP 800-63B-4, 3.1.1.2).
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto a = tx.opt("SELECT id, encode(setup_token_hash,'hex') FROM admin_users WHERE email = $1 AND status = 'pending' "
                            "AND setup_expires_at > now() FOR UPDATE",
                            email);
            if (!a || !crypto::constantTimeEquals(db::str((*a)[1]), crypto::sha256Hex(token))) throw unauthorized("invalid_setup_token");
            std::string secret = crypto::randomBytes(20);
            auto sealed = crypto::seal(env.cfg.dataKey, secret);
            tx.q("UPDATE admin_users SET password_hash = $2, totp_cipher = decode($3,'hex'), totp_nonce = decode($4,'hex') "
                 "WHERE id = $1",
                 db::str((*a)[0]), crypto::hashPassword(password, env.cfg.argon2), crypto::toHex(sealed.cipher),
                 crypto::toHex(sealed.nonce));
            std::string b32 = crypto::base32Encode(secret);
            return {{"totp_secret", b32},
                    {"otpauth_uri", "otpauth://totp/Nhill%20Admin:" + email + "?secret=" + b32 +
                                        "&issuer=Nhill&algorithm=SHA1&digits=6&period=30"}};
        });
        ctx.json(200, out);
    });

    // Step 2: proves the authenticator works, then activates the account.
    app.post("/api/v1/admin/auth/setup/confirm", {Access::Public}, [&env](Ctx& ctx) {
        throttle(env.pool, "admin-setup:ip:" + ctx.actor.ip, 10);
        const auto& b = ctx.body();
        std::string email = in::reqEmail(b, "email");
        std::string token = in::reqStr(b, "setup_token", 100);
        std::string code = in::reqStr(b, "totp_code", 6);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto a = tx.opt("SELECT id, encode(setup_token_hash,'hex'), encode(totp_cipher,'hex'), encode(totp_nonce,'hex') "
                            "FROM admin_users WHERE email = $1 AND status = 'pending' AND setup_expires_at > now() "
                            "AND totp_cipher IS NOT NULL FOR UPDATE",
                            email);
            if (!a || !crypto::constantTimeEquals(db::str((*a)[1]), crypto::sha256Hex(token))) throw unauthorized("invalid_setup_token");
            auto secret = crypto::open(env.cfg.dataKey, *crypto::fromHex(db::str((*a)[2])), *crypto::fromHex(db::str((*a)[3])));
            auto step = secret ? crypto::totpVerify(*secret, code, std::time(nullptr)) : std::nullopt;
            if (!step) throw unauthorized("invalid_totp");
            tx.q("UPDATE admin_users SET status = 'active', totp_enabled = true, setup_token_hash = NULL, "
                 "setup_expires_at = NULL, last_totp_step = $2 WHERE id = $1",
                 db::str((*a)[0]), *step);
            auditSystem(tx, std::nullopt, "admin.activated", "admin_user", db::str((*a)[0]), {}, "admin", db::str((*a)[0]));
        });
        ctx.noContent();
    });

    app.post("/api/v1/admin/auth/login", {Access::Public}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string email = in::reqEmail(b, "email");
        throttle(env.pool, "admin-login:" + email, 5);
        throttle(env.pool, "admin-login:ip:" + ctx.actor.ip, 20);
        if (!b.contains("password") || !b["password"].is_string()) throw invalid("password", "required");
        std::string code = in::reqStr(b, "totp_code", 6);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto a = tx.opt("SELECT id, password_hash, encode(totp_cipher,'hex'), encode(totp_nonce,'hex'), last_totp_step, role "
                            "FROM admin_users WHERE email = $1 AND status = 'active' AND totp_enabled FOR UPDATE",
                            email);
            bool ok = false;
            std::optional<int64_t> step;
            if (a && crypto::verifyPassword(db::str((*a)[1]), b["password"].get<std::string>())) {
                auto secret = crypto::open(env.cfg.dataKey, *crypto::fromHex(db::str((*a)[2])), *crypto::fromHex(db::str((*a)[3])));
                step = secret ? crypto::totpVerify(*secret, code, std::time(nullptr)) : std::nullopt;
                // A TOTP code is accepted once: replay within the window is refused.
                ok = step && ((*a)[4].is_null() || *step > (*a)[4].as<int64_t>());
            }
            if (!ok) return {{"_failed", true}};
            std::string token = crypto::randomToken(32);
            auto s = tx.one("INSERT INTO admin_sessions (admin_id, token_hash, expires_at, ip) VALUES ($1, decode($2,'hex'), "
                            "now() + make_interval(hours => $3), $4) RETURNING nh_iso(expires_at)",
                            db::str((*a)[0]), crypto::sha256Hex(token), env.cfg.adminSessionHours, ctx.actor.ip);
            tx.q("UPDATE admin_users SET last_totp_step = $2, last_login_at = now() WHERE id = $1", db::str((*a)[0]), *step);
            auditSystem(tx, std::nullopt, "admin.login", "admin_user", db::str((*a)[0]), {{"ip", ctx.actor.ip}}, "admin",
                        db::str((*a)[0]));
            return {{"token", token}, {"expires_at", db::str(s[0])}, {"role", db::str((*a)[5])}};
        });
        if (out.contains("_failed")) {
            db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
                auditSystem(tx, std::nullopt, "admin.login_failed", "admin_user", email, {{"ip", ctx.actor.ip}}, "anonymous");
            });
            throw unauthorized("invalid_credentials");
        }
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/auth/logout", {Access::Admin}, [&env](Ctx& ctx) {
        std::string hash = crypto::sha256Hex(ctx.header("Authorization").substr(7));
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.q("UPDATE admin_sessions SET revoked_at = now() WHERE token_hash = decode($1,'hex')", hash);
        });
        ctx.noContent();
    });

    app.get("/api/v1/admin/me", {Access::Admin}, [](Ctx& ctx) {
        ctx.json(200, {{"id", ctx.actor.adminId}, {"email", ctx.actor.email}, {"full_name", ctx.actor.fullName},
                       {"role", ctx.actor.adminRole}});
    });

    // A superadmin invites another administrator; the setup token is shown once.
    app.post("/api/v1/admin/admins", {Access::Admin, "", false, {"superadmin"}}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string email = in::reqEmail(b, "email");
        std::string name = in::reqStr(b, "full_name", 120);
        std::string role = in::reqEnum(b, "role", {"superadmin", "billing", "support", "auditor"});
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            std::string token = crypto::randomToken(24);
            auto r = tx.one("INSERT INTO admin_users (email, full_name, role, setup_token_hash, setup_expires_at) VALUES "
                            "($1,$2,$3,decode($4,'hex'), now() + interval '30 minutes') RETURNING id, nh_iso(setup_expires_at)",
                            email, name, role, crypto::sha256Hex(token));
            audit(tx, ctx.actor, "admin.invited", "admin_user", db::str(r[0]), {{"email", email}, {"role", role}});
            return {{"id", db::str(r[0])}, {"setup_token", token}, {"expires_at", db::str(r[1])}};
        });
        ctx.json(201, out);
    });

    // ---------------------------------------------------------------- establishments
    app.get("/api/v1/admin/establishments", {Access::Admin}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto search = ctx.query("search");
        auto status = ctx.query("status");
        auto sub = ctx.query("subscription_status");
        // erasable=true: establishments whose 90 days of export after the end of access are over.
        const bool erasable = ctx.query("erasable").value_or("") == "true";
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            std::string where = "WHERE ($1::text IS NULL OR e.trade_name ILIKE nh_like_contains($1) OR "
                                "e.email ILIKE nh_like_contains($1) OR e.id::text = $1) AND ($2::text IS NULL OR e.status = $2) AND ($3::text IS NULL OR "
                                "s.status = $3) ";
            if (erasable) where += "AND COALESCE((SELECT x.eligible FROM nh_establishment_erasure(e.id) x), false) ";
            int total = tx.one("SELECT count(*) FROM establishments e LEFT JOIN subscriptions s ON s.establishment_id = e.id " +
                                   where,
                               search, status, sub)[0].as<int>();
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT e.id, e.trade_name, e.kind, e.country_code, e.status, s.status, nh_iso(s.period_end), "
                     "nh_iso(e.created_at), (SELECT count(*) FROM memberships m WHERE m.establishment_id = e.id AND "
                     "m.status = 'active'), p.code FROM establishments e LEFT JOIN subscriptions s ON s.establishment_id = e.id "
                     "LEFT JOIN plans p ON p.id = s.plan_id " + where + "ORDER BY e.created_at DESC LIMIT $4 OFFSET $5",
                     search, status, sub, pg.limit, pg.offset))
                items.push_back({{"id", db::str(r[0])}, {"trade_name", db::str(r[1])}, {"kind", db::str(r[2])},
                                 {"country_code", db::str(r[3])}, {"status", db::str(r[4])},
                                 {"subscription_status", db::str(r[5])}, {"period_end", db::str(r[6])},
                                 {"created_at", db::str(r[7])}, {"members", r[8].as<int>()},
                                 {"plan", r[9].is_null() ? json(nullptr) : json(db::str(r[9]))}});
            return {{"items", items}, {"total", total}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/admin/establishments/:id", {Access::Admin}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "establishment");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto e = tx.one("SELECT id, trade_name, legal_name, kind, country_code, city, email, phone, status, status_reason, "
                            "vat_registered, compliance_profile_code, nh_iso(created_at) FROM establishments WHERE id = $1",
                            id);
            json codes = json::array();
            for (const auto& r : tx.q("SELECT status, nh_iso(created_at), nh_iso(expires_at), nh_iso(ended_at), ended_reason, "
                                      "created_by_type FROM establishment_codes WHERE establishment_id = $1 ORDER BY created_at "
                                      "DESC LIMIT 12",
                                      id))
                codes.push_back({{"status", db::str(r[0])}, {"created_at", db::str(r[1])}, {"expires_at", db::str(r[2])},
                                 {"ended_at", db::str(r[3])}, {"ended_reason", db::str(r[4])},
                                 {"created_by", db::str(r[5])}});
            json events = json::array();
            // Full history of the subscription: lifecycle, payments and every administrative action.
            int limit = 100;
            if (auto q = ctx.query("events")) {
                try {
                    limit = std::max(1, std::min(500, std::stoi(*q)));
                } catch (const std::exception&) {
                    throw invalid("events", "integer_required");
                }
            }
            for (const auto& r : tx.q("SELECT ev.kind, ev.from_status, ev.to_status, nh_iso(ev.period_start), nh_iso(ev.period_end), "
                                      "ev.actor_type, nh_iso(ev.created_at), ev.details, COALESCE(a.full_name, a.email) "
                                      "FROM subscription_events ev LEFT JOIN admin_users a ON a.id = ev.actor_id AND "
                                      "ev.actor_type = 'admin' WHERE ev.establishment_id = $1 ORDER BY ev.id DESC LIMIT $2",
                                      id, limit))
                events.push_back({{"kind", db::str(r[0])}, {"from", db::str(r[1])}, {"to", db::str(r[2])},
                                  {"period_start", db::str(r[3])}, {"period_end", db::str(r[4])},
                                  {"actor", db::str(r[5])}, {"at", db::str(r[6])}, {"details", json::parse(db::str(r[7]))},
                                  {"admin_name", r[8].is_null() ? json(nullptr) : json(db::str(r[8]))}});
            auto usage = tx.one("SELECT COALESCE(sum(documents_issued),0), COALESCE(sum(payments_recorded),0) FROM usage_daily "
                                "WHERE establishment_id = $1 AND day > current_date - 30",
                                id);
            auto members = tx.one("SELECT count(*) FROM memberships WHERE establishment_id = $1 AND status = 'active'", id);
            auto er = tx.one("SELECT eligible, reason, nh_iso(access_ended_at), nh_iso(erasable_from) "
                             "FROM nh_establishment_erasure($1)",
                             id);
            return {{"establishment",
                     {{"id", db::str(e[0])}, {"trade_name", db::str(e[1])}, {"legal_name", db::str(e[2])},
                      {"kind", db::str(e[3])}, {"country_code", db::str(e[4])}, {"city", db::str(e[5])},
                      {"email", db::str(e[6])}, {"phone", db::str(e[7])}, {"status", db::str(e[8])},
                      {"status_reason", db::str(e[9])}, {"vat_registered", db::boolean(e[10])},
                      {"profile", db::str(e[11])}, {"created_at", db::str(e[12])}, {"members", members[0].as<int>()}}},
                    {"subscription", subscriptions::view(tx, id)},
                    {"entitlements", [&] {
                         json j = entitlements::toJson(entitlements::load(tx, id, db::str(e[3])), entitlements::usage(tx, id));
                         return j;
                     }()},
                    {"subscription_events", events},
                    {"codes", codes},
                    {"erasure", {{"eligible", db::boolean(er[0])}, {"reason", db::str(er[1])},
                                 {"access_ended_at", er[2].is_null() ? json(nullptr) : json(db::str(er[2]))},
                                 {"erasable_from", er[3].is_null() ? json(nullptr) : json(db::str(er[3]))}}},
                    {"usage_30_days", {{"documents_issued", usage[0].as<int64_t>()}, {"payments_recorded", usage[1].as<int64_t>()}}}};
        });
        ctx.json(200, out);
    });

    auto setStatus = [&env](Ctx& ctx, const std::string& to) {
        std::string id = in::uuidParam(ctx.param("id"), "establishment");
        std::string why = reason(ctx.body());
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.one("SELECT status FROM establishments WHERE id = $1 FOR UPDATE", id);
            std::string from = db::str(r[0]);
            if (from == to) throw conflict("no_change");
            tx.q("UPDATE establishments SET status = $2, status_reason = $3 WHERE id = $1", id, to, why);
            if (to == "suspended")
                tx.q("UPDATE sessions SET revoked_at = now(), revoke_reason = 'establishment_suspended' WHERE "
                     "establishment_id = $1 AND revoked_at IS NULL",
                     id);
            Actor a = ctx.actor;
            a.establishmentId = id;
            audit(tx, a, to == "suspended" ? "admin.establishment_suspended" : "admin.establishment_reactivated",
                  "establishment", id, {{"reason", why}, {"from", from}});
            notify(tx, id, std::nullopt, to == "suspended" ? "establishment_suspended" : "establishment_reactivated",
                   {{"reason", why}});
        });
        ctx.noContent();
    };
    app.post("/api/v1/admin/establishments/:id/suspend", {Access::Admin, "", false, {"support"}},
             [setStatus](Ctx& ctx) { setStatus(ctx, "suspended"); });
    app.post("/api/v1/admin/establishments/:id/reactivate", {Access::Admin, "", false, {"support"}},
             [setStatus](Ctx& ctx) { setStatus(ctx, "active"); });

    // Permanent erasure (terms of service: data kept 90 days after the end of access, then
    // deleted). Superadmin only; the exact trade name must be typed to confirm. Before the
    // 90 days only at the customer's request, and never while the establishment has full
    // access. Paid subscription orders are archived for Nhill Solution's accounts.
    app.post("/api/v1/admin/establishments/:id/erase", {Access::Admin, "", false, {"superadmin"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "establishment");
        const json& b = ctx.body();
        std::string why = reason(b);
        std::string confirm = in::reqStr(b, "confirm_trade_name", 200);
        bool early = b.contains("customer_request") && b["customer_request"].is_boolean() && b["customer_request"].get<bool>();
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto e = tx.one("SELECT trade_name FROM establishments WHERE id = $1 FOR UPDATE", id);
            const std::string name = db::str(e[0]);
            if (confirm != name) throw invalid("confirm_trade_name", "does_not_match");
            auto er = tx.one("SELECT eligible, reason, nh_iso(erasable_from) FROM nh_establishment_erasure($1)", id);
            const bool eligible = db::boolean(er[0]);
            const std::string accessReason = db::str(er[1]);
            if (!eligible && (!early || accessReason == "access_active"))
                throw conflict("not_erasable", "the establishment cannot be erased yet",
                               {{"reason", accessReason}, {"erasable_from", er[2].is_null() ? json(nullptr) : json(db::str(er[2]))}});
            json summary = json::parse(db::str(tx.one("SELECT nh_erase_establishment($1, $2)::text", id, early)[0]));
            // Platform-level entry (the establishment's own trail is gone with it).
            Actor a = ctx.actor;
            a.establishmentId.clear();
            audit(tx, a, "admin.establishment_erased", "establishment", id,
                  {{"reason", why}, {"customer_request", early}, {"summary", summary}});
            return summary;
        });
        ctx.json(200, out);
    });

    // Revokes the current code and issues a new one. The new code is never
    // shown to the administrator: the owner reads it in the application.
    app.post("/api/v1/admin/establishments/:id/codes/reissue", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "establishment");
        std::string why = reason(ctx.body());
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.one("SELECT 1 FROM establishments WHERE id = $1", id);
            codes::rotate(env, tx, id, "revoked", "admin: " + why, "admin", ctx.actor.adminId);
            Actor a = ctx.actor;
            a.establishmentId = id;
            audit(tx, a, "admin.code_reissued", "establishment_code", id, {{"reason", why}});
            notify(tx, id, std::nullopt, "code_rotated", {{"by", "support"}});
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- plans
    app.get("/api/v1/admin/plans", {Access::Admin}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(std::string("SELECT ") + subscriptions::kPlanCols +
                                      " FROM plans ORDER BY tier, sort_order, code"))
                items.push_back(subscriptions::planJson(tx, r, false));
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/plans", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string code = in::reqStr(b, "code", 40);
        std::string currency = in::reqCurrency(b, "currency");
        int64_t price = in::reqMoney(b, "price", currency);
        json features = b.contains("features") && b["features"].is_object() ? b["features"] : json::object();
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.one(std::string("INSERT INTO plans (code, name_fr, name_en, description_fr, description_en, "
                                        "billing_period, price_minor, currency, tax_rate_bp, max_users, audience, features, "
                                        "active, sort_order, tier, max_registers, max_warehouses) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,"
                                        "$9,$10,$11,$12::jsonb,$13,$14,$15,$16,$17) RETURNING ") + subscriptions::kPlanCols,
                            code, in::reqStr(b, "name_fr", 80), in::reqStr(b, "name_en", 80),
                            in::optStr(b, "description_fr", 1000).value_or(""), in::optStr(b, "description_en", 1000).value_or(""),
                            in::reqEnum(b, "billing_period", {"monthly", "annual"}), price, currency,
                            in::optInt(b, "tax_rate_bp", 0, 0, 10000),
                            b.contains("max_users") && !b["max_users"].is_null()
                                ? std::optional<int>(in::optInt(b, "max_users", 1, 1, 100000))
                                : std::nullopt,
                            in::optEnum(b, "audience", {"individual", "company", "any"}).value_or("any"), features.dump(),
                            in::optBool(b, "active", true), in::optInt(b, "sort_order", 0, -1000, 1000),
                            in::optInt(b, "tier", 0, 0, 1000),
                            b.contains("max_registers") && !b["max_registers"].is_null()
                                ? std::optional<int>(in::optInt(b, "max_registers", 0, 0, 1000))
                                : std::nullopt,
                            b.contains("max_warehouses") && !b["max_warehouses"].is_null()
                                ? std::optional<int>(in::optInt(b, "max_warehouses", 1, 1, 1000))
                                : std::nullopt);
            std::string id = db::str(r[0]);
            applyPlanConditions(tx, id, b);
            applyPlanEntitlements(tx, id, b);
            audit(tx, ctx.actor, "admin.plan_created", "plan", id, b);
            return subscriptions::planJson(tx, tx.one(std::string("SELECT ") + subscriptions::kPlanCols + " FROM plans WHERE id = $1", id),
                                           false);
        });
        ctx.json(201, out);
    });

    // Price changes apply to future orders only; paid periods are never modified.
    app.patch("/api/v1/admin/plans/:id", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "plan");
        const auto& b = ctx.body();
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto cur = tx.one("SELECT currency FROM plans WHERE id = $1 FOR UPDATE", id);
            std::string currency = db::str(cur[0]);
            std::optional<std::string> features;
            if (b.contains("features") && b["features"].is_object()) features = b["features"].dump();
            std::optional<bool> active;
            if (b.contains("active")) active = in::optBool(b, "active", true);
            std::optional<int> maxUsers, tax, sort;
            if (b.contains("max_users") && !b["max_users"].is_null()) maxUsers = in::optInt(b, "max_users", 1, 1, 100000);
            if (b.contains("tax_rate_bp")) tax = in::optInt(b, "tax_rate_bp", 0, 0, 10000);
            if (b.contains("sort_order")) sort = in::optInt(b, "sort_order", 0, -1000, 1000);
            std::optional<int> tier, regs, whs;
            if (b.contains("tier")) tier = in::optInt(b, "tier", 0, 0, 1000);
            if (b.contains("max_registers") && !b["max_registers"].is_null()) regs = in::optInt(b, "max_registers", 0, 0, 1000);
            if (b.contains("max_warehouses") && !b["max_warehouses"].is_null()) whs = in::optInt(b, "max_warehouses", 1, 1, 1000);
            bool clearUsers = b.contains("max_users") && b["max_users"].is_null();
            bool clearRegs = b.contains("max_registers") && b["max_registers"].is_null();
            bool clearWhs = b.contains("max_warehouses") && b["max_warehouses"].is_null();
            auto r = tx.one(std::string("UPDATE plans SET name_fr = COALESCE($2, name_fr), name_en = COALESCE($3, name_en), "
                                        "description_fr = COALESCE($4, description_fr), description_en = COALESCE($5, "
                                        "description_en), price_minor = COALESCE($6, price_minor), tax_rate_bp = COALESCE($7, "
                                        "tax_rate_bp), max_users = CASE WHEN $12 THEN NULL ELSE COALESCE($8, max_users) END, "
                                        "features = COALESCE($9::jsonb, features), active = COALESCE($10, active), "
                                        "sort_order = COALESCE($11, sort_order), tier = COALESCE($13, tier), "
                                        "max_registers = CASE WHEN $14 THEN NULL ELSE COALESCE($15, max_registers) END, "
                                        "max_warehouses = CASE WHEN $16 THEN NULL ELSE COALESCE($17, max_warehouses) END "
                                        "WHERE id = $1 RETURNING ") + subscriptions::kPlanCols,
                            id, in::optStr(b, "name_fr", 80), in::optStr(b, "name_en", 80), in::optStr(b, "description_fr", 1000),
                            in::optStr(b, "description_en", 1000), in::optMoney(b, "price", currency), tax, maxUsers, features,
                            active, sort, clearUsers, tier, clearRegs, regs, clearWhs, whs);
            (void)r;
            applyPlanConditions(tx, id, b);
            applyPlanEntitlements(tx, id, b);
            audit(tx, ctx.actor, "admin.plan_updated", "plan", id, b);
            return subscriptions::planJson(tx, tx.one(std::string("SELECT ") + subscriptions::kPlanCols + " FROM plans WHERE id = $1", id),
                                           false);
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- subscriptions & payments
    app.get("/api/v1/admin/orders", {Access::Admin}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto status = ctx.query("status");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT o.id, e.trade_name, e.id, p.code, o.amount_minor, o.currency, o.status, o.provider, "
                     "o.provider_reference, nh_iso(o.created_at), nh_iso(o.paid_at) FROM subscription_orders o JOIN "
                     "establishments e ON e.id = o.establishment_id JOIN plans p ON p.id = o.plan_id WHERE ($1::text IS NULL "
                     "OR o.status = $1) ORDER BY o.created_at DESC LIMIT $2 OFFSET $3",
                     status, pg.limit, pg.offset))
                items.push_back({{"id", db::str(r[0])}, {"establishment", db::str(r[1])}, {"establishment_id", db::str(r[2])},
                                 {"plan", db::str(r[3])}, {"amount", money(r[4].as<int64_t>(), db::str(r[5]))},
                                 {"currency", db::str(r[5])}, {"status", db::str(r[6])}, {"provider", db::str(r[7])},
                                 {"provider_reference", db::str(r[8])}, {"created_at", db::str(r[9])},
                                 {"paid_at", db::str(r[10])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    // Manual reconciliation of a bank transfer / cash deposit, after checking
    // the bank statement. Requires the external reference; fully audited.
    app.post("/api/v1/admin/orders/:id/confirm-manual", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "order");
        const auto& b = ctx.body();
        std::string ref = in::reqStr(b, "reference", 100);
        std::string why = reason(b);
        bool applied = db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.q("UPDATE subscription_orders SET provider = 'manual' WHERE id = $1 AND provider IS NULL", id);
            bool ok = subscriptions::applyPaidOrder(env, tx, id, "admin", ctx.actor.adminId, ref);
            auditSystem(tx, std::nullopt, "admin.order_confirmed_manually", "subscription_order", id,
                        {{"reference", ref}, {"reason", why}}, "admin", ctx.actor.adminId);
            return ok;
        });
        ctx.json(200, {{"applied", applied}});
    });

    app.get("/api/v1/admin/webhook-events", {Access::Admin, "", false, {"billing", "auditor"}}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT id, provider, event_id, nh_iso(received_at), nh_iso(processed_at), outcome "
                                      "FROM payment_webhook_events ORDER BY id DESC LIMIT $1 OFFSET $2",
                                      pg.limit, pg.offset))
                items.push_back({{"id", r[0].as<int64_t>()}, {"provider", db::str(r[1])}, {"event_id", db::str(r[2])},
                                 {"received_at", db::str(r[3])}, {"processed_at", db::str(r[4])}, {"outcome", db::str(r[5])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- audit, stats
    // Platform-level events only; business events are listed without details.
    app.get("/api/v1/admin/audit", {Access::Admin, "", false, {"auditor", "support"}}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto est = ctx.query("establishment_id");
        if (est && !in::isUuid(*est)) est.reset();
        auto action = ctx.query("action");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT id, establishment_id, actor_type, actor_id, action, entity, entity_id, "
                     "CASE WHEN actor_type IN ('admin','system','provider','anonymous') OR action LIKE 'auth.%' OR action LIKE "
                     "'subscription.%' OR action LIKE 'establishment.code%' OR action LIKE 'admin.%' THEN details ELSE NULL END, "
                     "ip, nh_iso(created_at) FROM audit_log WHERE ($1::uuid IS NULL OR establishment_id = $1) AND "
                     "($2::text IS NULL OR action LIKE $2 || '%') ORDER BY id DESC LIMIT $3 OFFSET $4",
                     est, action, pg.limit, pg.offset))
                items.push_back({{"id", r[0].as<int64_t>()}, {"establishment_id", db::str(r[1])},
                                 {"actor_type", db::str(r[2])}, {"actor_id", db::str(r[3])}, {"action", db::str(r[4])},
                                 {"entity", db::str(r[5])}, {"entity_id", db::str(r[6])},
                                 {"details", r[7].is_null() ? json(nullptr) : json::parse(db::str(r[7]))},
                                 {"ip", db::str(r[8])}, {"created_at", db::str(r[9])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/admin/stats", {Access::Admin}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json byStatus = json::object(), bySub = json::object(), byKind = json::object(), revenue = json::object();
            for (const auto& r : tx.q("SELECT status, count(*) FROM establishments GROUP BY status"))
                byStatus[db::str(r[0])] = r[1].as<int>();
            for (const auto& r : tx.q("SELECT status, count(*) FROM subscriptions GROUP BY status"))
                bySub[db::str(r[0])] = r[1].as<int>();
            for (const auto& r : tx.q("SELECT kind, count(*) FROM establishments GROUP BY kind"))
                byKind[db::str(r[0])] = r[1].as<int>();
            for (const auto& r : tx.q("SELECT currency, sum(amount_minor) FROM subscription_orders WHERE status = 'paid' AND "
                                      "paid_at > now() - interval '30 days' GROUP BY currency"))
                revenue[db::str(r[0])] = money(r[1].as<int64_t>(), db::str(r[0]));
            auto u = tx.one("SELECT COALESCE(sum(documents_issued),0), COALESCE(sum(payments_recorded),0) FROM usage_daily "
                            "WHERE day > current_date - 30");
            auto n = tx.one("SELECT count(*) FROM establishments WHERE created_at > now() - interval '30 days'");
            auto active = tx.one("SELECT count(DISTINCT user_id) FROM sessions WHERE last_seen_at > now() - interval '1 day'");
            return {{"establishments_by_status", byStatus},
                    {"establishments_by_kind", byKind},
                    {"subscriptions_by_status", bySub},
                    {"new_establishments_30d", n[0].as<int>()},
                    {"documents_issued_30d", u[0].as<int64_t>()},
                    {"payments_recorded_30d", u[1].as<int64_t>()},
                    {"active_users_24h", active[0].as<int>()},
                    {"subscription_revenue_30d", revenue}};
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- announcements, settings, requests
    app.get("/api/v1/admin/announcements", {Access::Admin}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT id, kind, title_fr, title_en, body_fr, body_en, nh_iso(starts_at), nh_iso(ends_at), "
                                      "min_client_version FROM announcements ORDER BY starts_at DESC LIMIT 100"))
                items.push_back({{"id", db::str(r[0])}, {"kind", db::str(r[1])}, {"title_fr", db::str(r[2])},
                                 {"title_en", db::str(r[3])}, {"body_fr", db::str(r[4])}, {"body_en", db::str(r[5])},
                                 {"starts_at", db::str(r[6])}, {"ends_at", db::str(r[7])},
                                 {"min_client_version", db::str(r[8])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/announcements", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto r = tx.one("INSERT INTO announcements (kind, title_fr, title_en, body_fr, body_en, starts_at, ends_at, "
                            "min_client_version, created_by) VALUES ($1,$2,$3,$4,$5,COALESCE($6::timestamptz, now()),"
                            "$7::timestamptz,$8,$9) RETURNING id",
                            in::reqEnum(b, "kind", {"info", "maintenance", "release"}), in::reqStr(b, "title_fr", 200),
                            in::reqStr(b, "title_en", 200), in::optStr(b, "body_fr", 2000).value_or(""),
                            in::optStr(b, "body_en", 2000).value_or(""), in::optStr(b, "starts_at", 40),
                            in::optStr(b, "ends_at", 40), in::optStr(b, "min_client_version", 20), ctx.actor.adminId);
            audit(tx, ctx.actor, "admin.announcement_created", "announcement", db::str(r[0]), b);
            return {{"id", db::str(r[0])}};
        });
        ctx.json(201, out);
    });

    app.get("/api/v1/admin/settings", {Access::Admin}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT key, value, description, nh_iso(updated_at) FROM platform_settings ORDER BY key"))
                items.push_back({{"key", db::str(r[0])}, {"value", json::parse(db::str(r[1]))},
                                 {"description", db::str(r[2])}, {"updated_at", db::str(r[3])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.put("/api/v1/admin/settings/:key", {Access::Admin, "", false, {"superadmin"}}, [&env](Ctx& ctx) {
        std::string key = ctx.param("key");
        static const std::map<std::string, std::pair<int, int>> intKeys = {
            {"trial_days", {0, 365}},           {"grace_days", {0, 60}},         {"reminder_days_before", {1, 60}},
            {"code_rotation_days", {1, 365}},   {"code_check_max_per_15min", {3, 100}},
            {"login_max_per_15min", {3, 100}},  {"invitation_days", {1, 60}},   {"trial_max_users", {1, 1000}},
            {"password_reset_minutes", {5, 60}}, {"password_reset_max_attempts", {1, 10}},
            {"temporary_password_hours", {1, 168}}};
        const auto& b = ctx.body();
        if (!b.contains("value")) throw invalid("value", "required");
        json value = b["value"];
        if (auto it = intKeys.find(key); it != intKeys.end()) {
            if (!value.is_number_integer() || value.get<int>() < it->second.first || value.get<int>() > it->second.second)
                throw invalid("value", "out_of_range");
        } else if (key == "payment_instructions") {
            if (!value.is_object()) throw invalid("value", "must_be_object_fr_en");
        } else {
            throw notFound("setting");
        }
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.q("INSERT INTO platform_settings (key, value, updated_by) VALUES ($1, $2::jsonb, $3) ON CONFLICT (key) DO UPDATE "
                 "SET value = EXCLUDED.value, updated_at = now(), updated_by = EXCLUDED.updated_by",
                 key, value.dump(), ctx.actor.adminId);
            audit(tx, ctx.actor, "admin.setting_changed", "platform_setting", key, {{"value", value}});
        });
        ctx.noContent();
    });

    app.get("/api/v1/admin/attachment-requests", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(
                     "SELECT a.id, e.trade_name, a.establishment_id, a.requester_name, a.requester_email, a.requester_phone, "
                     "a.message, a.status, nh_iso(a.created_at) FROM attachment_requests a JOIN establishments e ON "
                     "e.id = a.establishment_id ORDER BY a.status = 'open' DESC, a.created_at DESC LIMIT 200"))
                items.push_back({{"id", db::str(r[0])}, {"establishment", db::str(r[1])},
                                 {"establishment_id", db::str(r[2])}, {"name", db::str(r[3])}, {"email", db::str(r[4])},
                                 {"phone", db::str(r[5])}, {"message", db::str(r[6])}, {"status", db::str(r[7])},
                                 {"created_at", db::str(r[8])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/attachment-requests/:id/resolve", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "request");
        const auto& b = ctx.body();
        std::string status = in::reqEnum(b, "status", {"approved", "rejected"});
        std::string note = reason(b);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.q("UPDATE attachment_requests SET status = $2, resolution_note = $3, resolved_by = $4, resolved_at = now() "
                          "WHERE id = $1 AND status = 'open' RETURNING establishment_id",
                          id, status, note, ctx.actor.adminId);
            if (r.empty()) throw notFound("request");
            Actor a = ctx.actor;
            a.establishmentId = db::str(r[0][0]);
            audit(tx, a, "admin.attachment_request_" + status, "attachment_request", id, {{"note", note}});
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- statistics & reports per establishment
    // Aggregates only (see migration 0006). Restricted to billing / auditor
    // (and superadmin); each consultation is itself recorded in the audit log.
    app.get("/api/v1/admin/establishments/:id/stats", {Access::Admin, "", false, {"billing", "auditor"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "establishment");
        Period p = period(ctx);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto e = tx.one("SELECT trade_name, kind, country_code, main_currency, status FROM establishments WHERE id = $1", id);
            json volumes = json::array();
            for (const auto& r : tx.q("SELECT * FROM nh_platform_establishment_volumes($1, $2::date, $3::date)", id, p.from, p.to)) {
                std::string c = db::str(r[0]);
                volumes.push_back({{"currency", c},
                                   {"invoices_issued", r[1].as<int64_t>()},
                                   {"invoiced_net", money(r[2].as<int64_t>(), c)},
                                   {"invoiced_total", money(r[3].as<int64_t>(), c)},
                                   {"credit_notes_issued", r[4].as<int64_t>()},
                                   {"credited_total", money(r[5].as<int64_t>(), c)},
                                   {"payments_in_count", r[6].as<int64_t>()},
                                   {"payments_in", money(r[7].as<int64_t>(), c)},
                                   {"payments_out", money(r[8].as<int64_t>(), c)},
                                   {"expenses_count", r[9].as<int64_t>()},
                                   {"expenses", money(r[10].as<int64_t>(), c)},
                                   {"purchases", money(r[11].as<int64_t>(), c)}});
            }
            json monthly = json::array();
            for (const auto& r : tx.q("SELECT * FROM nh_platform_establishment_monthly($1, $2::date, $3::date)", id, p.from, p.to)) {
                std::string c = db::str(r[1]);
                monthly.push_back({{"month", db::str(r[0])}, {"currency", c}, {"invoices_issued", r[2].as<int64_t>()},
                                   {"invoiced_net", money(r[3].as<int64_t>(), c)}, {"invoiced_total", money(r[4].as<int64_t>(), c)},
                                   {"credited_total", money(r[5].as<int64_t>(), c)}, {"payments_in", money(r[6].as<int64_t>(), c)},
                                   {"expenses", money(r[7].as<int64_t>(), c)}});
            }
            auto c = tx.one("SELECT * FROM nh_platform_establishment_counts($1)", id);
            json roles = json::object();
            for (const auto& r : tx.q("SELECT role, count(*) FROM memberships WHERE establishment_id = $1 AND status = 'active' "
                                      "GROUP BY role",
                                      id))
                roles[db::str(r[0])] = r[1].as<int>();
            auto u = tx.one(
                "SELECT (SELECT count(*) FROM sessions WHERE establishment_id = $1 AND created_at >= ($2::date::timestamp AT TIME ZONE nh_tz($1)) AND "
                "created_at < (($3::date + 1)::timestamp AT TIME ZONE nh_tz($1))), (SELECT count(DISTINCT user_id) FROM sessions WHERE establishment_id = $1 AND "
                "last_seen_at >= ($2::date::timestamp AT TIME ZONE nh_tz($1)) AND created_at < (($3::date + 1)::timestamp AT TIME ZONE nh_tz($1))), (SELECT nh_iso(max(created_at)) FROM audit_log WHERE "
                "establishment_id = $1 AND actor_type = 'user'), (SELECT count(*) FROM audit_log WHERE establishment_id = $1 "
                "AND actor_type = 'user' AND created_at >= ($2::date::timestamp AT TIME ZONE nh_tz($1)) AND created_at < (($3::date + 1)::timestamp AT TIME ZONE nh_tz($1)))",
                id, p.from, p.to);
            Actor a = ctx.actor;
            a.establishmentId = id;
            audit(tx, a, "admin.establishment_stats_viewed", "establishment", id, {{"from", p.from}, {"to", p.to}});
            return {{"establishment",
                     {{"id", id}, {"trade_name", db::str(e[0])}, {"kind", db::str(e[1])}, {"country_code", db::str(e[2])},
                      {"main_currency", db::str(e[3])}, {"status", db::str(e[4])}}},
                    {"period", {{"from", p.from}, {"to", p.to}}},
                    {"volumes", volumes},
                    {"monthly", monthly},
                    {"counts",
                     {{"products", c[0].as<int64_t>()}, {"customers", c[1].as<int64_t>()}, {"suppliers", c[2].as<int64_t>()},
                      {"documents", c[3].as<int64_t>()}, {"drafts", c[4].as<int64_t>()}}},
                    {"users",
                     {{"active_by_role", roles}, {"sign_ins", u[0].as<int64_t>()}, {"active_users", u[1].as<int64_t>()},
                      {"last_activity", u[2].is_null() ? json(nullptr) : json(db::str(u[2]))},
                      {"actions", u[3].as<int64_t>()}}},
                    {"subscription", subscriptions::view(tx, id)},
                    {"notice", "aggregates_only"}};
        });
        ctx.json(200, out);
    });

    // One line per establishment and currency over the period (JSON / CSV / PDF).
    app.get("/api/v1/admin/reports/establishments", {Access::Admin, "", false, {"billing", "auditor"}}, [&env](Ctx& ctx) {
        Period p = period(ctx);
        auto status = ctx.query("subscription_status");
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            Report r{"report.platform_establishments",
                     {"col.establishment", "col.kind", "col.country", "col.plan", "col.subscription", "col.users",
                      "col.currency", "col.invoices_issued", "col.invoiced_net", "col.invoiced_total", "col.credited_total",
                      "col.payments_in", "col.expenses"},
                     {Col::Text, Col::Text, Col::Text, Col::Text, Col::Text, Col::Int, Col::Text, Col::Int, Col::Money,
                      Col::Money, Col::Money, Col::Money, Col::Money},
                     {-1, -1, -1, -1, -1, -1, -1, -1, 6, 6, 6, 6, 6},
                     {},
                     true};
            r.orgName = "Nhill — Administration";
            const std::string lang = ctx.actor.lang;
            for (const auto& e : tx.q(
                     "SELECT e.id, e.trade_name, e.kind, e.country_code, COALESCE(pl.code, ''), COALESCE(s.status, ''), "
                     "(SELECT count(*) FROM memberships m WHERE m.establishment_id = e.id AND m.status = 'active') "
                     "FROM establishments e LEFT JOIN subscriptions s ON s.establishment_id = e.id LEFT JOIN plans pl ON "
                     "pl.id = s.plan_id WHERE ($1::text IS NULL OR s.status = $1) ORDER BY lower(e.trade_name) LIMIT 5000",
                     status)) {
                std::string id = db::str(e[0]);
                std::vector<std::string> base = {db::str(e[1]), env.i18n.t(lang, "kind." + db::str(e[2])), db::str(e[3]),
                                                 db::str(e[4]), env.i18n.t(lang, "subscription." + db::str(e[5])),
                                                 std::to_string(e[6].as<int>())};
                auto vols = tx.q("SELECT * FROM nh_platform_establishment_volumes($1, $2::date, $3::date)", id, p.from, p.to);
                if (vols.empty()) {
                    auto row = base;
                    for (const char* x : {"", "0", "", "", "", "", ""}) row.push_back(x);
                    r.rows.push_back(row);
                    continue;
                }
                for (const auto& v : vols) {
                    std::string c = db::str(v[0]);
                    auto row = base;
                    row.insert(row.end(), {c, std::to_string(v[1].as<int64_t>()), money(v[2].as<int64_t>(), c),
                                           money(v[3].as<int64_t>(), c), money(v[5].as<int64_t>(), c),
                                           money(v[7].as<int64_t>(), c), money(v[10].as<int64_t>(), c)});
                    r.rows.push_back(row);
                }
            }
            auditSystem(tx, std::nullopt, "admin.platform_report_generated", "report", "establishments",
                        {{"from", p.from}, {"to", p.to}, {"format", ctx.query("format").value_or("json")}}, "admin",
                        ctx.actor.adminId);
            sendReport(env, ctx, tx, r, "nhill_etablissements_" + p.from + "_" + p.to, &p);
        });
    });

    // Monthly report of one establishment (aggregates only).
    app.get("/api/v1/admin/reports/establishments/:id", {Access::Admin, "", false, {"billing", "auditor"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "establishment");
        Period p = period(ctx);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto e = tx.one("SELECT trade_name FROM establishments WHERE id = $1", id);
            Report r{"report.platform_establishment",
                     {"col.month", "col.currency", "col.invoices_issued", "col.invoiced_net", "col.invoiced_total",
                      "col.credited_total", "col.payments_in", "col.expenses"},
                     {Col::Text, Col::Text, Col::Int, Col::Money, Col::Money, Col::Money, Col::Money, Col::Money},
                     {-1, -1, -1, 1, 1, 1, 1, 1},
                     {}};
            r.orgName = "Nhill — Administration";
            r.extraSubtitle = db::str(e[0]);
            for (const auto& v : tx.q("SELECT * FROM nh_platform_establishment_monthly($1, $2::date, $3::date)", id, p.from, p.to)) {
                std::string c = db::str(v[1]);
                r.rows.push_back({db::str(v[0]), c, std::to_string(v[2].as<int64_t>()), money(v[3].as<int64_t>(), c),
                                  money(v[4].as<int64_t>(), c), money(v[5].as<int64_t>(), c), money(v[6].as<int64_t>(), c),
                                  money(v[7].as<int64_t>(), c)});
            }
            Actor a = ctx.actor;
            a.establishmentId = id;
            audit(tx, a, "admin.establishment_report_generated", "establishment", id,
                  {{"from", p.from}, {"to", p.to}, {"format", ctx.query("format").value_or("json")}});
            sendReport(env, ctx, tx, r, "nhill_etablissement_" + p.from + "_" + p.to, &p);
        });
    });
}

}  // namespace nh
