// Nhill Console — platform operations added in migration 0008:
// plan prices, subscription actions, payment providers, promo codes, transactions,
// support tickets, technical errors and console notifications.
// Every write is restricted to an administrator role and recorded in the audit log.
#include <set>

#include "services/providers.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"

namespace nh {

namespace {

std::string reasonOf(const json& b) {
    std::string r = in::reqStr(b, "reason", 500);
    if (r.size() < 5) throw invalid("reason", "too_short");
    return r;
}

std::optional<int> optNullableInt(const json& b, const char* key, int min, int max) {
    if (!b.contains(key) || b[key].is_null()) return std::nullopt;
    return in::optInt(b, key, min, min, max);
}

std::string currenciesArray(const json& b, const char* key) {
    std::string out = "{";
    if (b.contains(key)) {
        if (!b[key].is_array()) throw invalid(key, "array_required");
        bool first = true;
        for (const auto& c : b[key]) {
            json tmp = {{"c", c}};
            std::string cur = in::reqCurrency(tmp, "c");
            out += (first ? "" : ",") + cur;
            first = false;
        }
    }
    return out + "}";
}

json ticketRow(const pqxx::row& r) {
    return {{"id", db::str(r[0])},          {"number", r[1].as<int64_t>()},  {"establishment_id", db::str(r[2])},
            {"establishment", db::str(r[3])}, {"source", db::str(r[4])},     {"category", db::str(r[5])},
            {"severity", db::str(r[6])},    {"subject", db::str(r[7])},      {"status", db::str(r[8])},
            {"app_version", db::str(r[9])}, {"platform", db::str(r[10])},
            {"assigned_to", r[11].is_null() ? json(nullptr) : json(db::str(r[11]))},
            {"assigned_name", r[12].is_null() ? json(nullptr) : json(db::str(r[12]))},
            {"created_at", db::str(r[13])}, {"last_message_at", db::str(r[14])},
            {"resolved_at", r[15].is_null() ? json(nullptr) : json(db::str(r[15]))},
            {"created_by_name", r[16].is_null() ? json(nullptr) : json(db::str(r[16]))}};
}
constexpr const char* kTicketSelect =
    "SELECT t.id, t.number, t.establishment_id, e.trade_name, t.source, t.category, t.severity, t.subject, t.status, "
    "COALESCE(t.app_version, ''), COALESCE(t.platform, ''), t.assigned_to, a.full_name, nh_iso(t.created_at), "
    "nh_iso(t.last_message_at), nh_iso(t.resolved_at), u.full_name FROM support_tickets t "
    "JOIN establishments e ON e.id = t.establishment_id LEFT JOIN admin_users a ON a.id = t.assigned_to "
    "LEFT JOIN users u ON u.id = t.created_by ";

json promoRow(const pqxx::row& r) {
    std::string cur = db::str(r[5]);
    return {{"id", db::str(r[0])},
            {"code", db::str(r[1])},
            {"description", db::str(r[2])},
            {"discount_kind", db::str(r[3])},
            {"percent_bp", r[4].is_null() ? json(nullptr) : json(r[4].as<int>())},
            {"currency", r[5].is_null() ? json(nullptr) : json(cur)},
            {"amount", r[6].is_null() ? json(nullptr) : json(money(r[6].as<int64_t>(), cur))},
            {"valid_from", db::str(r[7])},
            {"valid_until", r[8].is_null() ? json(nullptr) : json(db::str(r[8]))},
            {"max_redemptions", r[9].is_null() ? json(nullptr) : json(r[9].as<int>())},
            {"max_per_establishment", r[10].as<int>()},
            {"plan_ids", r[11].is_null() ? json(nullptr) : json::parse(db::str(r[11]))},
            {"min_duration_months", r[12].is_null() ? json(nullptr) : json(r[12].as<int>())},
            {"new_customers_only", db::boolean(r[13])},
            {"audience", db::str(r[14])},
            {"active", db::boolean(r[15])},
            {"redemptions_confirmed", r[16].as<int>()},
            {"redemptions_reserved", r[17].as<int>()},
            {"created_at", db::str(r[18])}};
}
constexpr const char* kPromoSelect =
    "SELECT p.id, p.code, p.description, p.discount_kind, p.percent_bp, p.currency, p.amount_minor, nh_iso(p.valid_from), "
    "nh_iso(p.valid_until), p.max_redemptions, p.max_per_establishment, to_jsonb(p.plan_ids), p.min_duration_months, "
    "p.new_customers_only, p.audience, p.active, "
    "(SELECT count(*) FROM promo_redemptions r WHERE r.promo_code_id = p.id AND r.status = 'confirmed')::int, "
    "(SELECT count(*) FROM promo_redemptions r WHERE r.promo_code_id = p.id AND r.status = 'reserved')::int, "
    "nh_iso(p.created_at) FROM promo_codes p ";

std::optional<std::string> planIdsArray(const json& b) {
    if (!b.contains("plan_ids") || b["plan_ids"].is_null()) return std::nullopt;
    if (!b["plan_ids"].is_array() || b["plan_ids"].empty()) throw invalid("plan_ids", "array_required");
    std::string out = "{";
    bool first = true;
    for (const auto& id : b["plan_ids"]) {
        if (!id.is_string() || !in::isUuid(id.get<std::string>())) throw invalid("plan_ids", "invalid_uuid");
        out += (first ? "" : ",") + id.get<std::string>();
        first = false;
    }
    return out + "}";
}

std::optional<std::string> optTimestamp(const json& b, const char* key) {
    auto s = in::optStr(b, key, 40);
    if (!s) return std::nullopt;
    if (in::isDate(*s)) return *s + "T00:00:00Z";
    // ISO 8601 date-time; PostgreSQL validates the value (22007 -> 422).
    if (s->size() < 16 || (*s)[4] != '-' || (*s)[10] != 'T') throw invalid(key, "invalid_datetime");
    return s;
}

// Validates provider settings against the field specification of its kind.
json cleanSettings(const std::string& kind, const json& settings, json& secrets) {
    json spec;
    for (const auto& k : providers::kinds())
        if (k["kind"] == kind) spec = k["fields"];
    std::set<std::string> plain, secret;
    for (const auto& f : spec) (f["secret"].get<bool>() ? secret : plain).insert(f["name"].get<std::string>());
    json out = json::object();
    if (settings.is_object())
        for (auto it = settings.begin(); it != settings.end(); ++it) {
            if (!plain.count(it.key())) throw invalid("settings." + it.key(), "unknown_field");
            if (!it.value().is_string() || it.value().get<std::string>().size() > 500)
                throw invalid("settings." + it.key(), "invalid_text");
            std::string v = it.value().get<std::string>();
            if ((it.key().find("url") != std::string::npos) && !v.empty() && v.rfind("https://", 0) != 0)
                throw invalid("settings." + it.key(), "https_required");
            out[it.key()] = v;
        }
    if (secrets.is_object())
        for (auto it = secrets.begin(); it != secrets.end(); ++it)
            if (!secret.count(it.key())) throw invalid("secrets." + it.key(), "unknown_field");
    return out;
}

void requireComplete(const providers::Provider& p) {
    for (const auto& k : providers::kinds()) {
        if (k["kind"] != p.kind) continue;
        if (!k["implemented"].get<bool>()) throw conflict("provider_not_implemented");
        for (const auto& f : k["fields"]) {
            if (!f["required"].get<bool>()) continue;
            std::string n = f["name"];
            bool set = f["secret"].get<bool>()
                           ? std::find(p.secretFields.begin(), p.secretFields.end(), n) != p.secretFields.end()
                           : (p.settings.contains(n) && !p.settings[n].get<std::string>().empty());
            if (!set) throw conflict("provider_incomplete", "missing field: " + n, {{"field", n}});
        }
    }
}

}  // namespace

void registerConsoleRoutes(App& app, Env& env) {
    // ---------------------------------------------------------------- plan prices
    app.post("/api/v1/admin/plans/:id/prices", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string planId = in::uuidParam(ctx.param("id"), "plan");
        const auto& b = ctx.body();
        int months = in::optInt(b, "duration_months", 1, 1, 36);
        static const std::set<int> allowed = {1, 3, 6, 12, 24, 36};
        if (!allowed.count(months)) throw invalid("duration_months", "unsupported_duration");
        std::string currency = in::reqCurrency(b, "currency");
        int64_t price = in::reqMoney(b, "price", currency);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            tx.one("SELECT 1 FROM plans WHERE id = $1", planId);
            auto r = tx.one("INSERT INTO plan_prices (plan_id, duration_months, currency, price_minor) VALUES ($1,$2,$3,$4) "
                            "ON CONFLICT (plan_id, duration_months, currency) DO UPDATE SET price_minor = EXCLUDED.price_minor, "
                            "active = true RETURNING id",
                            planId, months, currency, price);
            audit(tx, ctx.actor, "admin.plan_price_set", "plan_price", db::str(r[0]),
                  {{"plan_id", planId}, {"duration_months", months}, {"currency", currency}, {"price", money(price, currency)}});
            return {{"id", db::str(r[0])}};
        });
        ctx.json(201, out);
    });

    // Price changes apply to future orders only (orders freeze their amounts).
    app.patch("/api/v1/admin/plan-prices/:id", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "price");
        const auto& b = ctx.body();
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto cur = tx.one("SELECT currency FROM plan_prices WHERE id = $1 FOR UPDATE", id);
            std::optional<bool> active;
            if (b.contains("active")) active = in::optBool(b, "active", true);
            tx.q("UPDATE plan_prices SET price_minor = COALESCE($2, price_minor), active = COALESCE($3, active) WHERE id = $1",
                 id, in::optMoney(b, "price", db::str(cur[0])), active);
            audit(tx, ctx.actor, "admin.plan_price_updated", "plan_price", id, b);
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- subscription actions
    // Support / billing gestures, always with a written reason. Never touches the
    // establishment code nor its identity.
    app.post("/api/v1/admin/establishments/:id/subscription", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string est = in::uuidParam(ctx.param("id"), "establishment");
        const auto& b = ctx.body();
        std::string action = in::reqEnum(b, "action", {"extend", "suspend", "reactivate", "cancel", "set_plan"});
        std::string why = reasonOf(b);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto s = tx.one("SELECT id, status FROM subscriptions WHERE establishment_id = $1 FOR UPDATE", est);
            std::string subId = db::str(s[0]), from = db::str(s[1]), to = from;
            json details = {{"reason", why}, {"action", action}};
            if (action == "extend") {
                int days = in::optInt(b, "days", 0, 0, 3660);
                int months = in::optInt(b, "months", 0, 0, 120);
                if (days == 0 && months == 0) throw invalid("days", "required");
                // From the later of now and the current end: suspended time is not given back twice.
                tx.q("UPDATE subscriptions SET period_end = GREATEST(period_end, now()) + make_interval(months => $2, days => $3), "
                     "status = CASE WHEN status IN ('past_due','suspended') THEN 'active' ELSE status END, grace_until = NULL, "
                     "reminder_sent_for = NULL, canceled_at = NULL WHERE id = $1",
                     subId, months, days);
                if (from == "past_due" || from == "suspended" || from == "canceled") to = "active";
                if (from == "canceled") tx.q("UPDATE subscriptions SET status = 'active' WHERE id = $1", subId);
                details["days"] = days;
                details["months"] = months;
            } else if (action == "suspend") {
                if (from == "suspended") throw conflict("no_change");
                tx.q("UPDATE subscriptions SET status = 'suspended' WHERE id = $1", subId);
                to = "suspended";
            } else if (action == "reactivate") {
                if (from != "suspended" && from != "canceled") throw conflict("no_change");
                auto r = tx.one("UPDATE subscriptions SET status = CASE WHEN period_end > now() THEN "
                                "CASE WHEN is_trial THEN 'trialing' ELSE 'active' END ELSE 'past_due' END, "
                                "grace_until = CASE WHEN period_end > now() THEN NULL ELSE now() + "
                                "make_interval(days => nh_setting_int('grace_days', 7)) END, canceled_at = NULL "
                                "WHERE id = $1 RETURNING status",
                                subId);
                to = db::str(r[0]);
            } else if (action == "cancel") {
                if (from == "canceled") throw conflict("no_change");
                tx.q("UPDATE subscriptions SET status = 'canceled', canceled_at = now() WHERE id = $1", subId);
                for (const auto& o : tx.q("UPDATE subscription_orders SET status = 'canceled', failure_reason = 'admin_cancel' "
                                          "WHERE establishment_id = $1 AND status = 'pending' RETURNING id",
                                          est))
                    subscriptions::releasePromo(tx, db::str(o[0]));
                to = "canceled";
            } else {
                std::string priceId = in::reqUuid(b, "price_id");
                auto p = tx.one("SELECT pp.plan_id, p.code FROM plan_prices pp JOIN plans p ON p.id = pp.plan_id WHERE pp.id = $1",
                                priceId);
                tx.q("UPDATE subscriptions SET plan_id = $2, price_id = $3, pending_price_id = NULL, pending_from = NULL "
                     "WHERE id = $1",
                     subId, db::str(p[0]), priceId);
                details["plan"] = db::str(p[1]);
            }
            auto r = tx.one("SELECT period_start, period_end FROM subscriptions WHERE id = $1", subId);
            tx.q("INSERT INTO subscription_events (subscription_id, establishment_id, kind, from_status, to_status, "
                 "period_start, period_end, details, actor_type, actor_id) VALUES ($1,$2,$3,$4,$5,$6,$7,$8::jsonb,'admin',$9)",
                 subId, est, "admin_" + action, from, to, db::str(r[0]), db::str(r[1]), details.dump(), ctx.actor.adminId);
            Actor a = ctx.actor;
            a.establishmentId = est;
            audit(tx, a, "admin.subscription_" + action, "subscription", subId, details);
            notify(tx, est, std::nullopt, "subscription_admin_" + action, {{"reason", why}});
            if (action == "suspend" || action == "cancel")
                tx.q("UPDATE sessions SET revoked_at = now(), revoke_reason = 'subscription_' || $2 WHERE "
                     "establishment_id = $1 AND revoked_at IS NULL AND $2 = 'cancel'",
                     est, action);
            return {{"subscription", subscriptions::view(tx, est)}};
        });
        ctx.json(200, out);
    });

    // A payment request on behalf of an establishment (e.g. invoice sent by e-mail).
    app.post("/api/v1/admin/establishments/:id/orders", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string est = in::uuidParam(ctx.param("id"), "establishment");
        const auto& b = ctx.body();
        std::string priceId = in::reqUuid(b, "price_id");
        auto promo = in::optStr(b, "promo_code", 32);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto e = tx.one("SELECT kind FROM establishments WHERE id = $1", est);
            Actor a = ctx.actor;
            a.establishmentId = est;
            a.establishmentKind = db::str(e[0]);
            json created = subscriptions::createOrder(env, tx, a, priceId, promo, "admin");
            return {{"order", subscriptions::orderJson(tx, created["id"])}};
        });
        ctx.json(201, out);
    });

    // ---------------------------------------------------------------- transactions
    app.get("/api/v1/admin/transactions", {Access::Admin, "", false, {"billing", "auditor"}}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto status = ctx.query("status");
        auto provider = ctx.query("provider");
        auto est = ctx.query("establishment_id");
        if (est && !in::isUuid(*est)) throw invalid("establishment_id", "invalid_uuid");
        auto from = ctx.query("from"), to = ctx.query("to");
        if (from && !in::isDate(*from)) throw invalid("from", "invalid_date");
        if (to && !in::isDate(*to)) throw invalid("to", "invalid_date");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            std::string where = "WHERE ($1::text IS NULL OR o.status = $1) AND ($2::text IS NULL OR o.provider = $2) "
                                "AND ($3::uuid IS NULL OR o.establishment_id = $3) AND ($4::date IS NULL OR o.created_at >= $4::date) "
                                "AND ($5::date IS NULL OR o.created_at < $5::date + 1) ";
            json items = json::array();
            for (const auto& r : tx.q("SELECT o.id, e.trade_name, o.establishment_id FROM subscription_orders o "
                                      "JOIN establishments e ON e.id = o.establishment_id " + where +
                                      "ORDER BY o.created_at DESC LIMIT $6 OFFSET $7",
                                      status, provider, est, from, to, pg.limit, pg.offset)) {
                json o = subscriptions::orderJson(tx, db::str(r[0]));
                o["establishment"] = db::str(r[1]);
                o["establishment_id"] = db::str(r[2]);
                items.push_back(o);
            }
            json totals = json::array();
            for (const auto& r : tx.q("SELECT o.currency, count(*), COALESCE(sum(o.amount_minor),0), "
                                      "COALESCE(sum(o.discount_minor),0) FROM subscription_orders o " + where +
                                      "AND o.status = 'paid' GROUP BY o.currency ORDER BY o.currency",
                                      std::optional<std::string>(), provider, est, from, to))
                totals.push_back({{"currency", db::str(r[0])}, {"count", r[1].as<int>()},
                                  {"collected", money(r[2].as<int64_t>(), db::str(r[0]))},
                                  {"discounts", money(r[3].as<int64_t>(), db::str(r[0]))}});
            return {{"items", items}, {"paid_totals", totals}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/admin/transactions/:id", {Access::Admin, "", false, {"billing", "auditor"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "order");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json o = subscriptions::orderJson(tx, id);
            auto r = tx.one("SELECT o.establishment_id, e.trade_name, o.provider_reference, o.provider_session FROM "
                            "subscription_orders o JOIN establishments e ON e.id = o.establishment_id WHERE o.id = $1",
                            id);
            o["establishment_id"] = db::str(r[0]);
            o["establishment"] = db::str(r[1]);
            o["provider_reference"] = db::str(r[2]);
            o["provider_session"] = db::str(r[3]);
            json events = json::array();
            for (const auto& e : tx.q("SELECT provider, event_id, nh_iso(received_at), outcome FROM payment_webhook_events "
                                      "WHERE payload::text LIKE '%' || $1 || '%' ORDER BY id DESC LIMIT 20",
                                      id))
                events.push_back({{"provider", db::str(e[0])}, {"event_id", db::str(e[1])}, {"received_at", db::str(e[2])},
                                  {"outcome", db::str(e[3])}});
            o["provider_events"] = events;
            return o;
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- payment providers
    app.get("/api/v1/admin/payment-providers", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT code FROM payment_providers ORDER BY sort_order, code"))
                items.push_back(providers::toJson(env, *providers::byCode(env, tx, db::str(r[0]), false)));
            return {{"items", items}, {"kinds", providers::kinds()},
                    {"public_base_url", env.cfg.publicBaseUrl.empty() ? json(nullptr) : json(env.cfg.publicBaseUrl)}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/payment-providers", {Access::Admin, "", false, {"superadmin"}}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string code = in::reqStr(b, "code", 40);
        for (auto& c : code) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        std::string kind = in::reqStr(b, "kind", 20);
        if (!providers::isKnownKind(kind)) throw invalid("kind", "unknown_kind");
        json secrets = b.contains("secrets") && b["secrets"].is_object() ? b["secrets"] : json::object();
        json settings = cleanSettings(kind, b.value("settings", json::object()), secrets);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto sealed = providers::sealSecrets(env, json::object(), secrets);
            std::string fields = "{";
            for (size_t i = 0; i < sealed.fields.size(); ++i) fields += (i ? "," : "") + sealed.fields[i];
            fields += "}";
            auto r = tx.one("INSERT INTO payment_providers (code, kind, display_name, environment, currencies, settings, "
                            "secret_cipher, secret_nonce, secret_fields, updated_by) VALUES ($1,$2,$3,$4,$5::char(3)[],$6::jsonb,"
                            "decode($7,'hex'),decode($8,'hex'),$9::text[],$10) RETURNING id",
                            code, kind, in::reqStr(b, "display_name", 80),
                            in::optEnum(b, "environment", {"test", "live"}).value_or("test"), currenciesArray(b, "currencies"),
                            settings.dump(), sealed.cipherHex, sealed.nonceHex, fields, ctx.actor.adminId);
            // Only the names of the secret fields are audited, never their values.
            audit(tx, ctx.actor, "admin.payment_provider_created", "payment_provider", db::str(r[0]),
                  {{"code", code}, {"kind", kind}, {"secret_fields", sealed.fields}});
            return providers::toJson(env, *providers::byId(env, tx, db::str(r[0]), false));
        });
        ctx.json(201, out);
    });

    app.patch("/api/v1/admin/payment-providers/:id", {Access::Admin, "", false, {"superadmin"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "provider");
        const auto& b = ctx.body();
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            tx.one("SELECT 1 FROM payment_providers WHERE id = $1 FOR UPDATE", id);
            auto p = *providers::byId(env, tx, id, true);
            json secrets = b.contains("secrets") && b["secrets"].is_object() ? b["secrets"] : json::object();
            json settings = p.settings;
            if (b.contains("settings")) {
                json upd = cleanSettings(p.kind, b["settings"], secrets);
                for (auto it = upd.begin(); it != upd.end(); ++it) settings[it.key()] = it.value();
            } else {
                cleanSettings(p.kind, json::object(), secrets);
            }
            auto sealed = providers::sealSecrets(env, p.secrets, secrets);
            std::string fields = "{";
            for (size_t i = 0; i < sealed.fields.size(); ++i) fields += (i ? "," : "") + sealed.fields[i];
            fields += "}";
            std::optional<bool> enabled;
            if (b.contains("enabled")) enabled = in::optBool(b, "enabled", false);
            std::optional<std::string> currencies;
            if (b.contains("currencies")) currencies = currenciesArray(b, "currencies");
            tx.q("UPDATE payment_providers SET display_name = COALESCE($2, display_name), environment = COALESCE($3, environment), "
                 "currencies = COALESCE($4::char(3)[], currencies), settings = $5::jsonb, secret_cipher = decode($6,'hex'), "
                 "secret_nonce = decode($7,'hex'), secret_fields = $8::text[], enabled = COALESCE($9, enabled), "
                 "sort_order = COALESCE($10, sort_order), updated_by = $11 WHERE id = $1",
                 id, in::optStr(b, "display_name", 80), in::optEnum(b, "environment", {"test", "live"}), currencies,
                 settings.dump(), sealed.cipherHex, sealed.nonceHex, fields, enabled,
                 b.contains("sort_order") ? std::optional<int>(in::optInt(b, "sort_order", 0, -100, 100)) : std::nullopt,
                 ctx.actor.adminId);
            auto after = *providers::byId(env, tx, id, false);
            if (after.enabled) requireComplete(after);
            json changed = json::object();
            for (auto it = b.begin(); it != b.end(); ++it)
                if (it.key() != "secrets") changed[it.key()] = it.value();
            json secretNames = json::array();
            for (auto it = secrets.begin(); it != secrets.end(); ++it) secretNames.push_back(it.key());
            changed["secrets_changed"] = secretNames;
            audit(tx, ctx.actor, "admin.payment_provider_updated", "payment_provider", id, changed);
            return providers::toJson(env, after);
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- promo codes
    app.get("/api/v1/admin/promo-codes", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(std::string(kPromoSelect) + "ORDER BY p.active DESC, p.created_at DESC LIMIT 500"))
                items.push_back(promoRow(r));
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/promo-codes", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string code = in::reqStr(b, "code", 32);
        for (auto& c : code) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        std::string kind = in::reqEnum(b, "discount_kind", {"percent", "fixed"});
        std::optional<int> bp;
        std::optional<std::string> currency;
        std::optional<int64_t> amount;
        if (kind == "percent") bp = in::optInt(b, "percent_bp", 0, 1, 10000);
        else {
            currency = in::reqCurrency(b, "currency");
            amount = in::reqMoney(b, "amount", *currency, false);
        }
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto r = tx.one("INSERT INTO promo_codes (code, description, discount_kind, percent_bp, amount_minor, currency, "
                            "valid_from, valid_until, max_redemptions, max_per_establishment, plan_ids, min_duration_months, "
                            "new_customers_only, audience, created_by) VALUES ($1,$2,$3,$4,$5,$6,COALESCE($7::timestamptz, now()),"
                            "$8::timestamptz,$9,$10,$11::uuid[],$12,$13,$14,$15) RETURNING id",
                            code, in::optStr(b, "description", 300).value_or(""), kind, bp, amount, currency,
                            optTimestamp(b, "valid_from"), optTimestamp(b, "valid_until"),
                            optNullableInt(b, "max_redemptions", 1, 1000000), in::optInt(b, "max_per_establishment", 1, 1, 100),
                            planIdsArray(b), optNullableInt(b, "min_duration_months", 1, 36),
                            in::optBool(b, "new_customers_only", false),
                            in::optEnum(b, "audience", {"individual", "company", "any"}).value_or("any"), ctx.actor.adminId);
            audit(tx, ctx.actor, "admin.promo_created", "promo_code", db::str(r[0]), b);
            return promoRow(tx.one(std::string(kPromoSelect) + "WHERE p.id = $1", db::str(r[0])));
        });
        ctx.json(201, out);
    });

    // The code and the discount itself are immutable once created (orders reference
    // them); validity, limits and activation can change.
    app.patch("/api/v1/admin/promo-codes/:id", {Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "promo_code");
        const auto& b = ctx.body();
        for (const char* k : {"code", "discount_kind", "percent_bp", "amount", "currency"})
            if (b.contains(k)) throw invalid(k, "immutable");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            tx.one("SELECT 1 FROM promo_codes WHERE id = $1 FOR UPDATE", id);
            std::optional<bool> active, newOnly;
            if (b.contains("active")) active = in::optBool(b, "active", true);
            if (b.contains("new_customers_only")) newOnly = in::optBool(b, "new_customers_only", false);
            bool clearUntil = b.contains("valid_until") && b["valid_until"].is_null();
            bool clearMax = b.contains("max_redemptions") && b["max_redemptions"].is_null();
            tx.q("UPDATE promo_codes SET description = COALESCE($2, description), "
                 "valid_until = CASE WHEN $3 THEN NULL ELSE COALESCE($4::timestamptz, valid_until) END, "
                 "max_redemptions = CASE WHEN $5 THEN NULL ELSE COALESCE($6, max_redemptions) END, "
                 "max_per_establishment = COALESCE($7, max_per_establishment), active = COALESCE($8, active), "
                 "new_customers_only = COALESCE($9, new_customers_only) WHERE id = $1",
                 id, in::optStr(b, "description", 300), clearUntil, optTimestamp(b, "valid_until"), clearMax,
                 optNullableInt(b, "max_redemptions", 1, 1000000),
                 b.contains("max_per_establishment") ? std::optional<int>(in::optInt(b, "max_per_establishment", 1, 1, 100))
                                                     : std::nullopt,
                 active, newOnly);
            audit(tx, ctx.actor, "admin.promo_updated", "promo_code", id, b);
            return promoRow(tx.one(std::string(kPromoSelect) + "WHERE p.id = $1", id));
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/admin/promo-codes/:id/redemptions", {Access::Admin, "", false, {"billing", "auditor"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "promo_code");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT r.order_id, e.trade_name, r.status, r.discount_minor, r.currency, "
                                      "nh_iso(r.created_at), nh_iso(r.settled_at) FROM promo_redemptions r JOIN establishments e "
                                      "ON e.id = r.establishment_id WHERE r.promo_code_id = $1 ORDER BY r.created_at DESC LIMIT 500",
                                      id))
                items.push_back({{"order_id", db::str(r[0])}, {"establishment", db::str(r[1])}, {"status", db::str(r[2])},
                                 {"discount", money(r[3].as<int64_t>(), db::str(r[4]))}, {"currency", db::str(r[4])},
                                 {"created_at", db::str(r[5])}, {"settled_at", db::str(r[6])}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- support tickets
    app.get("/api/v1/admin/tickets", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto status = ctx.query("status");
        auto category = ctx.query("category");
        auto est = ctx.query("establishment_id");
        if (est && !in::isUuid(*est)) throw invalid("establishment_id", "invalid_uuid");
        bool open = ctx.query("open").value_or("") == "1";
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(std::string(kTicketSelect) +
                                          "WHERE ($1::text IS NULL OR t.status = $1) AND ($2::text IS NULL OR t.category = $2) "
                                          "AND ($3::uuid IS NULL OR t.establishment_id = $3) AND (NOT $4 OR t.status NOT IN "
                                          "('resolved','closed')) ORDER BY (t.severity = 'critical') DESC, t.last_message_at DESC "
                                          "LIMIT $5 OFFSET $6",
                                      status, category, est, open, pg.limit, pg.offset))
                items.push_back(ticketRow(r));
            auto c = tx.one("SELECT count(*) FILTER (WHERE status = 'open'), count(*) FILTER (WHERE status = 'in_progress'), "
                            "count(*) FILTER (WHERE status = 'waiting_customer') FROM support_tickets");
            return {{"items", items},
                    {"counts", {{"open", c[0].as<int>()}, {"in_progress", c[1].as<int>()}, {"waiting_customer", c[2].as<int>()}}}};
        });
        ctx.json(200, out);
    });

    app.get("/api/v1/admin/tickets/:id", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "ticket");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json t = ticketRow(tx.one(std::string(kTicketSelect) + "WHERE t.id = $1", id));
            json msgs = json::array();
            for (const auto& r : tx.q("SELECT m.id, m.author_type, COALESCE(u.full_name, a.full_name, ''), m.body, m.internal, "
                                      "nh_iso(m.created_at) FROM support_ticket_messages m LEFT JOIN users u ON "
                                      "m.author_type = 'user' AND u.id = m.author_id LEFT JOIN admin_users a ON "
                                      "m.author_type = 'admin' AND a.id = m.author_id WHERE m.ticket_id = $1 ORDER BY m.id",
                                      id))
                msgs.push_back({{"id", r[0].as<int64_t>()}, {"author_type", db::str(r[1])}, {"author", db::str(r[2])},
                                {"body", db::str(r[3])}, {"internal", db::boolean(r[4])}, {"created_at", db::str(r[5])}});
            t["messages"] = msgs;
            return t;
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/tickets/:id/messages", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "ticket");
        const auto& b = ctx.body();
        std::string body = in::reqStr(b, "body", 10000);
        bool internal = in::optBool(b, "internal", false);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto t = tx.one("SELECT establishment_id, status, created_by, number FROM support_tickets WHERE id = $1 FOR UPDATE", id);
            tx.q("INSERT INTO support_ticket_messages (ticket_id, author_type, author_id, body, internal) VALUES "
                 "($1,'admin',$2,$3,$4)",
                 id, ctx.actor.adminId, body, internal);
            if (!internal) {
                tx.q("UPDATE support_tickets SET last_message_at = now(), status = CASE WHEN status IN ('open') "
                     "THEN 'in_progress' ELSE status END, assigned_to = COALESCE(assigned_to, $2) WHERE id = $1",
                     id, ctx.actor.adminId);
                std::optional<std::string> user;
                if (!t[2].is_null()) user = db::str(t[2]);
                notify(tx, db::str(t[0]), user, "ticket_replied", {{"ticket_id", id}, {"number", t[3].as<int64_t>()}});
            }
            Actor a = ctx.actor;
            a.establishmentId = db::str(t[0]);
            audit(tx, a, internal ? "admin.ticket_note" : "admin.ticket_replied", "support_ticket", id);
        });
        ctx.json(201, {{"ok", true}});
    });

    app.patch("/api/v1/admin/tickets/:id", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "ticket");
        const auto& b = ctx.body();
        auto status = in::optEnum(b, "status", {"open", "in_progress", "waiting_customer", "resolved", "closed"});
        auto severity = in::optEnum(b, "severity", {"low", "normal", "high", "critical"});
        auto assigned = in::optUuid(b, "assigned_to");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto t = tx.one("SELECT establishment_id, status, created_by, number FROM support_tickets WHERE id = $1 FOR UPDATE", id);
            if (assigned) tx.one("SELECT 1 FROM admin_users WHERE id = $1 AND status = 'active'", *assigned);
            tx.q("UPDATE support_tickets SET status = COALESCE($2, status), severity = COALESCE($3, severity), "
                 "assigned_to = COALESCE($4::uuid, assigned_to), resolved_at = CASE WHEN $2 IN ('resolved','closed') THEN "
                 "COALESCE(resolved_at, now()) WHEN $2 IS NOT NULL THEN NULL ELSE resolved_at END WHERE id = $1",
                 id, status, severity, assigned);
            if (status && *status != db::str(t[1])) {
                tx.q("INSERT INTO support_ticket_messages (ticket_id, author_type, author_id, body) VALUES ($1,'system',$2,$3)",
                     id, ctx.actor.adminId, "status: " + db::str(t[1]) + " -> " + *status);
                std::optional<std::string> user;
                if (!t[2].is_null()) user = db::str(t[2]);
                notify(tx, db::str(t[0]), user, "ticket_status", {{"ticket_id", id}, {"number", t[3].as<int64_t>()},
                                                                  {"status", *status}});
            }
            Actor a = ctx.actor;
            a.establishmentId = db::str(t[0]);
            audit(tx, a, "admin.ticket_updated", "support_ticket", id, b);
            return ticketRow(tx.one(std::string(kTicketSelect) + "WHERE t.id = $1", id));
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- technical errors
    app.get("/api/v1/admin/errors", {Access::Admin, "", false, {"support", "auditor"}}, [&env](Ctx& ctx) {
        auto pg = page(ctx);
        auto source = ctx.query("source");
        bool all = ctx.query("include_resolved").value_or("") == "1";
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT x.id, x.source, x.level, x.code, x.message, x.context, x.establishment_id, "
                                      "e.trade_name, x.app_version, x.platform, x.occurrences, nh_iso(x.first_seen), "
                                      "nh_iso(x.last_seen), nh_iso(x.resolved_at) FROM error_events x LEFT JOIN establishments e "
                                      "ON e.id = x.establishment_id WHERE ($1::text IS NULL OR x.source = $1) AND ($2 OR "
                                      "x.resolved_at IS NULL) ORDER BY x.last_seen DESC LIMIT $3 OFFSET $4",
                                      source, all, pg.limit, pg.offset))
                items.push_back({{"id", r[0].as<int64_t>()}, {"source", db::str(r[1])}, {"level", db::str(r[2])},
                                 {"code", db::str(r[3])}, {"message", db::str(r[4])}, {"context", json::parse(db::str(r[5]))},
                                 {"establishment_id", r[6].is_null() ? json(nullptr) : json(db::str(r[6]))},
                                 {"establishment", r[7].is_null() ? json(nullptr) : json(db::str(r[7]))},
                                 {"app_version", db::str(r[8])}, {"platform", db::str(r[9])},
                                 {"occurrences", r[10].as<int>()}, {"first_seen", db::str(r[11])},
                                 {"last_seen", db::str(r[12])},
                                 {"resolved_at", r[13].is_null() ? json(nullptr) : json(db::str(r[13]))}});
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/errors/:id/resolve", {Access::Admin, "", false, {"support"}}, [&env](Ctx& ctx) {
        auto id = ctx.param("id");
        if (id.empty() || id.size() > 18 || id.find_first_not_of("0123456789") != std::string::npos) throw notFound("error");
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.q("UPDATE error_events SET resolved_at = now(), resolved_by = $2 WHERE id = $1::bigint AND "
                          "resolved_at IS NULL RETURNING id",
                          id, ctx.actor.adminId);
            if (r.empty()) throw notFound("error");
            audit(tx, ctx.actor, "admin.error_resolved", "error_event", id);
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- notifications
    app.get("/api/v1/admin/notifications", {Access::Admin}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q("SELECT n.id, n.kind, n.params, n.establishment_id, e.trade_name, nh_iso(n.created_at), "
                                      "n.read_at IS NOT NULL FROM admin_notifications n LEFT JOIN establishments e ON "
                                      "e.id = n.establishment_id ORDER BY n.id DESC LIMIT 100"))
                items.push_back({{"id", r[0].as<int64_t>()}, {"kind", db::str(r[1])}, {"params", json::parse(db::str(r[2]))},
                                 {"establishment_id", r[3].is_null() ? json(nullptr) : json(db::str(r[3]))},
                                 {"establishment", r[4].is_null() ? json(nullptr) : json(db::str(r[4]))},
                                 {"created_at", db::str(r[5])}, {"read", db::boolean(r[6])}});
            int unread = tx.one("SELECT count(*) FROM admin_notifications WHERE read_at IS NULL")[0].as<int>();
            return {{"items", items}, {"unread", unread}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/notifications/read", {Access::Admin}, [&env](Ctx& ctx) {
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.q("UPDATE admin_notifications SET read_at = now(), read_by = $1 WHERE read_at IS NULL", ctx.actor.adminId);
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- overview
    app.get("/api/v1/admin/overview", {Access::Admin}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto c = tx.one(
                "SELECT (SELECT count(*) FROM support_tickets WHERE status IN ('open','in_progress')), "
                "(SELECT count(*) FROM support_tickets WHERE status IN ('open','in_progress') AND severity = 'critical'), "
                "(SELECT count(*) FROM error_events WHERE resolved_at IS NULL), "
                "(SELECT count(*) FROM subscription_orders WHERE status = 'pending'), "
                "(SELECT count(*) FROM subscriptions WHERE failed_payments > 0 AND status IN ('active','past_due')), "
                "(SELECT count(*) FROM admin_notifications WHERE read_at IS NULL), "
                "(SELECT count(*) FROM payment_providers WHERE enabled), "
                "(SELECT count(*) FROM promo_codes WHERE active AND (valid_until IS NULL OR valid_until > now()))");
            json revenue = json::array();
            for (const auto& r : tx.q("SELECT currency, COALESCE(sum(amount_minor),0) FROM subscription_orders WHERE status = 'paid' "
                                      "AND paid_at >= date_trunc('month', now()) GROUP BY currency ORDER BY currency"))
                revenue.push_back({{"currency", db::str(r[0])}, {"amount", money(r[1].as<int64_t>(), db::str(r[0]))}});
            return {{"open_tickets", c[0].as<int>()},       {"critical_tickets", c[1].as<int>()},
                    {"unresolved_errors", c[2].as<int>()},  {"pending_orders", c[3].as<int>()},
                    {"failed_payment_subscriptions", c[4].as<int>()},
                    {"unread_notifications", c[5].as<int>()}, {"enabled_providers", c[6].as<int>()},
                    {"active_promo_codes", c[7].as<int>()}, {"revenue_this_month", revenue}};
        });
        ctx.json(200, out);
    });
}

}  // namespace nh
