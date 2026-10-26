// Subscriptions: the single source of truth for usage rights.
//
// An order freezes a price (plan_prices row), a kind and every amount at creation:
//   new       : no paid plan yet, trial, suspended or canceled — starts now (after a
//               running trial);
//   renewal   : same tier — starts at the current period end (contract dates never
//               shift because of the grace period);
//   upgrade   : higher tier — starts now; the unused part of the current period is
//               credited pro rata (same currency only);
//   downgrade : lower tier — paid now, effective at the current period end;
//   admin     : created by a billing administrator.
// An order becomes paid only through a verified provider event or an audited manual
// confirmation. Promo codes are reserved by the order and consumed when it is paid.
#include <ctime>

#include "nhill/money.hpp"
#include "services/providers.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh {

namespace {

json pricesOf(db::Tx& tx, const std::string& planId, bool activeOnly) {
    json out = json::array();
    for (const auto& r : tx.q("SELECT id, duration_months, currency, price_minor, active FROM plan_prices WHERE plan_id = $1 "
                              "AND ($2 = false OR active) ORDER BY currency, duration_months",
                              planId, activeOnly))
        out.push_back({{"id", db::str(r[0])},
                       {"duration_months", r[1].as<int>()},
                       {"currency", db::str(r[2])},
                       {"price", money(r[3].as<int64_t>(), db::str(r[2]))},
                       {"active", db::boolean(r[4])}});
    return out;
}

}  // namespace

namespace subscriptions {

json planJson(db::Tx& tx, const pqxx::row& r, bool activePricesOnly) {
    auto optInt = [](const pqxx::field& f) { return f.is_null() ? json(nullptr) : json(f.as<int>()); };
    return {{"id", db::str(r[0])},
            {"code", db::str(r[1])},
            {"name_fr", db::str(r[2])},
            {"name_en", db::str(r[3])},
            {"description_fr", db::str(r[4])},
            {"description_en", db::str(r[5])},
            {"billing_period", db::str(r[6])},
            {"price", money(r[7].as<int64_t>(), db::str(r[8]))},
            {"currency", db::str(r[8])},
            {"tax_rate_bp", r[9].as<int>()},
            {"max_users", optInt(r[10])},
            {"audience", db::str(r[11])},
            {"features", json::parse(db::str(r[12]))},
            {"active", db::boolean(r[13])},
            {"sort_order", r[14].as<int>()},
            {"tier", r[15].as<int>()},
            {"max_registers", optInt(r[16])},
            {"max_warehouses", optInt(r[17])},
            {"prices", pricesOf(tx, db::str(r[0]), activePricesOnly)}};
}

json view(db::Tx& tx, const std::string& est) {
    auto r = tx.opt(
        "SELECT s.status, s.is_trial, nh_iso(s.period_start), nh_iso(s.period_end), nh_iso(s.grace_until), "
        "p.id, p.code, p.name_fr, p.name_en, p.billing_period, p.max_users, p.features, "
        "s.auto_renew_consent_at IS NOT NULL, "
        "GREATEST(0, ceil(extract(epoch FROM (COALESCE(s.grace_until, s.period_end) - now())) / 86400))::int, "
        "p.tier, p.max_registers, pp.duration_months, pp.currency, pp.price_minor, s.failed_payments, "
        "np.code, np.name_fr, np.name_en, nh_iso(s.pending_from) "
        "FROM subscriptions s LEFT JOIN plans p ON p.id = s.plan_id LEFT JOIN plan_prices pp ON pp.id = s.price_id "
        "LEFT JOIN plan_prices npp ON npp.id = s.pending_price_id LEFT JOIN plans np ON np.id = npp.plan_id "
        "WHERE s.establishment_id = $1",
        est);
    if (!r) return nullptr;
    const auto& s = *r;
    std::string status = db::str(s[0]);
    json plan = nullptr;
    if (!s[5].is_null())
        plan = {{"id", db::str(s[5])},           {"code", db::str(s[6])},
                {"name_fr", db::str(s[7])},      {"name_en", db::str(s[8])},
                {"billing_period", db::str(s[9])},
                {"max_users", s[10].is_null() ? json(nullptr) : json(s[10].as<int>())},
                {"features", json::parse(db::str(s[11]))},
                {"tier", s[14].as<int>()},
                {"max_registers", s[15].is_null() ? json(nullptr) : json(s[15].as<int>())}};
    json price = nullptr;
    if (!s[16].is_null())
        price = {{"duration_months", s[16].as<int>()}, {"currency", db::str(s[17])},
                 {"price", money(s[18].as<int64_t>(), db::str(s[17]))}};
    json pending = nullptr;
    if (!s[20].is_null())
        pending = {{"plan_code", db::str(s[20])}, {"name_fr", db::str(s[21])}, {"name_en", db::str(s[22])},
                   {"effective_at", db::str(s[23])}};
    return {{"status", status},
            {"is_trial", db::boolean(s[1])},
            {"period_start", db::str(s[2])},
            {"period_end", db::str(s[3])},
            {"grace_until", s[4].is_null() ? json(nullptr) : json(db::str(s[4]))},
            {"plan", plan},
            {"price", price},
            {"pending_change", pending},
            {"failed_payments", s[19].as<int>()},
            {"auto_renew_consent", db::boolean(s[12])},
            {"days_left", s[13].as<int>()},
            {"write_allowed", status == "trialing" || status == "active" || status == "past_due"}};
}

// ------------------------------------------------------------------ quote
Quote quote(db::Tx& tx, const std::string& est, const std::string& establishmentKind, const std::string& priceId,
            const std::optional<std::string>& promoCode, bool forOrder) {
    Quote q;
    auto p = tx.opt(
        "SELECT pp.id, pp.plan_id, pp.duration_months, pp.currency, pp.price_minor, p.code, p.tax_rate_bp, p.max_users, "
        "p.audience, p.tier, p.name_fr, p.name_en "
        "FROM plan_prices pp JOIN plans p ON p.id = pp.plan_id WHERE pp.id = $1 AND pp.active AND p.active",
        priceId);
    if (!p) throw notFound("plan");
    q.priceId = db::str((*p)[0]);
    q.planId = db::str((*p)[1]);
    q.durationMonths = (*p)[2].as<int>();
    q.currency = db::str((*p)[3]);
    q.subtotal = (*p)[4].as<int64_t>();
    q.planCode = db::str((*p)[5]);
    q.planName = db::str((*p)[10]);
    int taxBp = (*p)[6].as<int>();
    std::string audience = db::str((*p)[8]);
    int tier = (*p)[9].as<int>();
    if (audience != "any" && audience != establishmentKind) throw conflict("plan_audience_mismatch");
    if (!(*p)[7].is_null()) {
        int members = tx.one("SELECT count(*) FROM memberships WHERE establishment_id = $1 AND status = 'active'", est)[0]
                          .as<int>();
        if (members > (*p)[7].as<int>()) throw conflict("plan_user_limit", "too many active users for this plan");
    }

    auto s = tx.one(
        "SELECT s.status, s.is_trial, s.plan_id IS NOT NULL, COALESCE(cp.tier, 0), "
        "extract(epoch FROM GREATEST(s.period_end - now(), interval '0'))::bigint, "
        "extract(epoch FROM (s.period_end - s.period_start))::bigint, s.pending_price_id IS NOT NULL, "
        "nh_iso(s.period_end), nh_iso(GREATEST(now(), s.period_end)) "
        "FROM subscriptions s LEFT JOIN plans cp ON cp.id = s.plan_id WHERE s.establishment_id = $1" +
            std::string(forOrder ? " FOR UPDATE OF s" : ""),
        est);
    std::string status = db::str(s[0]);
    bool trial = db::boolean(s[1]);
    bool hasPlan = db::boolean(s[2]);
    int curTier = s[3].as<int>();
    int64_t remaining = s[4].as<int64_t>(), total = s[5].as<int64_t>();
    if (db::boolean(s[6])) throw conflict("pending_plan_change", "a plan change is already scheduled");
    bool running = status == "active" || status == "past_due";
    if (!hasPlan || trial || !running) q.kind = "new";
    else if (tier > curTier) q.kind = "upgrade";
    else if (tier < curTier) q.kind = "downgrade";
    else q.kind = "renewal";
    q.startsAt = q.kind == "upgrade" ? "now" : (q.kind == "new" ? (trial ? db::str(s[8]) : "now") : db::str(s[7]));

    // Promo code (validated here, reserved by the order).
    if (promoCode && !promoCode->empty()) {
        std::string code = *promoCode;
        for (auto& c : code) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        auto pr = tx.opt(
            "SELECT id, discount_kind, percent_bp, amount_minor, currency, max_redemptions, max_per_establishment, "
            "plan_ids IS NULL OR $2::uuid = ANY(plan_ids), COALESCE(min_duration_months, 0), new_customers_only, audience, "
            "description, valid_until IS NOT NULL AND valid_until <= now() "
            "FROM promo_codes WHERE code = $1 AND active AND valid_from <= now()" +
                std::string(forOrder ? " FOR UPDATE" : ""),
            code, q.planId);
        if (!pr) throw conflict("promo_invalid", "unknown or inactive promo code");
        const auto& c = *pr;
        if (db::boolean(c[12])) throw conflict("promo_expired");
        if (!db::boolean(c[7]) || q.durationMonths < c[8].as<int>()) throw conflict("promo_not_applicable");
        std::string pa = db::str(c[10]);
        if (pa != "any" && pa != establishmentKind) throw conflict("promo_not_applicable");
        std::string promoId = db::str(c[0]);
        if (db::boolean(c[9]) &&
            tx.one("SELECT count(*) FROM subscription_orders WHERE establishment_id = $1 AND status = 'paid'", est)[0]
                    .as<int>() > 0)
            throw conflict("promo_new_customers_only");
        if (!c[5].is_null()) {
            int used = tx.one("SELECT count(*) FROM promo_redemptions WHERE promo_code_id = $1 AND status <> 'released'",
                              promoId)[0]
                           .as<int>();
            if (used >= c[5].as<int>()) throw conflict("promo_exhausted");
        }
        // Reservations of this establishment's own open orders do not count: they are
        // released when the new order replaces them.
        int mine = tx.one("SELECT count(*) FROM promo_redemptions r JOIN subscription_orders o ON o.id = r.order_id "
                          "WHERE r.promo_code_id = $1 AND r.establishment_id = $2 AND (r.status = 'confirmed' OR "
                          "(r.status = 'reserved' AND o.status <> 'pending'))",
                          promoId, est)[0]
                       .as<int>();
        if (mine >= c[6].as<int>()) throw conflict("promo_already_used");
        if (db::str(c[1]) == "percent") {
            q.discount = *nhill::mulDiv(q.subtotal, c[2].as<int>(), 10000);
        } else {
            if (db::str(c[4]) != q.currency) throw conflict("promo_currency_mismatch");
            q.discount = std::min<int64_t>(c[3].as<int64_t>(), q.subtotal);
        }
        q.promoId = promoId;
        q.promoCode = code;
        q.promoDescription = db::str(c[11]);
    }

    // Pro-rata credit of the unused current period on an upgrade.
    if (q.kind == "upgrade" && remaining > 0 && total > 0) {
        auto last = tx.opt("SELECT COALESCE(subtotal_minor, amount_minor) - discount_minor - credit_minor, currency "
                           "FROM subscription_orders WHERE establishment_id = $1 AND status = 'paid' "
                           "ORDER BY paid_at DESC LIMIT 1",
                           est);
        if (last && db::str((*last)[1]) == q.currency) {
            int64_t paid = std::max<int64_t>(0, (*last)[0].as<int64_t>());
            q.credit = *nhill::mulDiv(paid, std::min(remaining, total), total);
        } else if (last) {
            q.notes.push_back("credit_currency_mismatch");
        }
    }
    q.credit = std::min<int64_t>(q.credit, q.subtotal - q.discount);
    int64_t base = q.subtotal - q.discount - q.credit;
    q.tax = *nhill::mulDiv(base, taxBp, 10000);
    q.total = base + q.tax;
    return q;
}

json quoteJson(const Quote& q) {
    json j = {{"kind", q.kind},
              {"price_id", q.priceId},
              {"plan_id", q.planId},
              {"plan_code", q.planCode},
              {"duration_months", q.durationMonths},
              {"currency", q.currency},
              {"subtotal", money(q.subtotal, q.currency)},
              {"discount", money(q.discount, q.currency)},
              {"credit", money(q.credit, q.currency)},
              {"tax", money(q.tax, q.currency)},
              {"total", money(q.total, q.currency)},
              {"starts_at", q.startsAt},
              {"notes", q.notes}};
    if (!q.promoCode.empty()) j["promo"] = {{"code", q.promoCode}, {"description", q.promoDescription}};
    return j;
}

// Releases the promo reservation of an order that will not be paid.
void releasePromo(db::Tx& tx, const std::string& orderId) {
    tx.q("UPDATE promo_redemptions SET status = 'released', settled_at = now() WHERE order_id = $1 AND status = 'reserved'",
         orderId);
}

// ------------------------------------------------------------------ order creation
json createOrder(Env& env, db::Tx& tx, const Actor& actor, const std::string& priceId,
                 const std::optional<std::string>& promoCode, const std::string& createdByType) {
    // Replaces any open order of the establishment (one payable request at a time).
    for (const auto& r : tx.q("UPDATE subscription_orders SET status = 'canceled', failure_reason = 'replaced' "
                              "WHERE establishment_id = $1 AND status = 'pending' RETURNING id",
                              actor.establishmentId))
        releasePromo(tx, db::str(r[0]));
    Quote q = quote(tx, actor.establishmentId, actor.establishmentKind, priceId, promoCode, true);
    int hours = setting(tx, "order_expiry_hours", 48);
    auto r = tx.one(
        "INSERT INTO subscription_orders (establishment_id, plan_id, amount_minor, currency, created_by, kind, price_id, "
        "duration_months, subtotal_minor, discount_minor, credit_minor, tax_minor, promo_code_id, expires_at) "
        "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13, now() + make_interval(hours => $14)) RETURNING id",
        actor.establishmentId, q.planId, q.total, q.currency,
        actor.userId.empty() ? std::nullopt : std::optional<std::string>(actor.userId),
        createdByType == "admin" ? std::string("admin") : q.kind, q.priceId, q.durationMonths, q.subtotal, q.discount,
        q.credit, q.tax, q.promoId.empty() ? std::nullopt : std::optional<std::string>(q.promoId), hours);
    std::string id = db::str(r[0]);
    if (!q.promoId.empty())
        tx.q("INSERT INTO promo_redemptions (promo_code_id, establishment_id, order_id, discount_minor, currency) "
             "VALUES ($1,$2,$3,$4,$5)",
             q.promoId, actor.establishmentId, id, q.discount, q.currency);
    json details = quoteJson(q);
    if (createdByType == "admin") auditSystem(tx, actor.establishmentId, "admin.order_created", "subscription_order", id,
                                              details, "admin", actor.adminId);
    else audit(tx, actor, "subscription.order_created", "subscription_order", id, details);
    (void)env;
    return {{"id", id}, {"quote", details}, {"total_minor", q.total}};
}

json orderJson(db::Tx& tx, const std::string& id) {
    auto r = tx.one(
        "SELECT o.id, p.code, o.amount_minor, o.currency, o.status, nh_iso(o.created_at), nh_iso(o.paid_at), o.kind, "
        "o.duration_months, COALESCE(o.subtotal_minor, o.amount_minor), o.discount_minor, o.credit_minor, o.tax_minor, "
        "pc.code, pv.code, pv.display_name, o.checkout_url, nh_iso(o.expires_at), o.failure_reason, p.name_fr, p.name_en "
        "FROM subscription_orders o JOIN plans p ON p.id = o.plan_id LEFT JOIN promo_codes pc ON pc.id = o.promo_code_id "
        "LEFT JOIN payment_providers pv ON pv.id = o.provider_id WHERE o.id = $1",
        id);
    std::string cur = db::str(r[3]);
    auto m = [&](int i) { return money(r[i].as<int64_t>(), cur); };
    return {{"id", db::str(r[0])},          {"plan_code", db::str(r[1])},     {"plan_name_fr", db::str(r[19])},
            {"plan_name_en", db::str(r[20])}, {"amount", m(2)},               {"currency", cur},
            {"status", db::str(r[4])},      {"created_at", db::str(r[5])},
            {"paid_at", r[6].is_null() ? json(nullptr) : json(db::str(r[6]))},
            {"kind", db::str(r[7])},
            {"duration_months", r[8].is_null() ? json(nullptr) : json(r[8].as<int>())},
            {"subtotal", m(9)},             {"discount", m(10)},              {"credit", m(11)},
            {"tax", m(12)},
            {"promo_code", r[13].is_null() ? json(nullptr) : json(db::str(r[13]))},
            {"provider", r[14].is_null() ? json(nullptr) : json(db::str(r[14]))},
            {"provider_name", r[15].is_null() ? json(nullptr) : json(db::str(r[15]))},
            {"checkout_url", r[16].is_null() ? json(nullptr) : json(db::str(r[16]))},
            {"expires_at", r[17].is_null() ? json(nullptr) : json(db::str(r[17]))},
            {"failure_reason", r[18].is_null() ? json(nullptr) : json(db::str(r[18]))}};
}

// ------------------------------------------------------------------ paid / failed
bool applyPaidOrder(Env&, db::Tx& tx, const std::string& orderId, const std::string& actorType,
                    const std::optional<std::string>& actorId, const std::string& providerReference) {
    auto o = tx.opt(
        "SELECT o.establishment_id, o.plan_id, o.status, p.billing_period, p.code, o.kind, "
        "COALESCE(o.duration_months, CASE p.billing_period WHEN 'annual' THEN 12 ELSE 1 END), o.price_id "
        "FROM subscription_orders o JOIN plans p ON p.id = o.plan_id WHERE o.id = $1 FOR UPDATE OF o",
        orderId);
    if (!o) throw notFound("order");
    std::string est = db::str((*o)[0]);
    std::string status = db::str((*o)[2]);
    if (status == "paid") return false;
    // A confirmed payment is honoured even if the request expired or was replaced in the meantime.
    if (status == "refunded") throw conflict("order_not_payable");
    std::string kind = db::str((*o)[5]);
    int months = (*o)[6].as<int>();
    std::optional<std::string> priceId;
    if (!(*o)[7].is_null()) priceId = db::str((*o)[7]);

    auto s = tx.one("SELECT id, status, is_trial FROM subscriptions WHERE establishment_id = $1 FOR UPDATE", est);
    std::string subId = db::str(s[0]);
    std::string from = db::str(s[1]);
    bool running = (from == "active" || from == "past_due") && !db::boolean(s[2]);
    pqxx::row upd;
    std::string eventKind;
    if (kind == "upgrade" && running) {
        upd = tx.one("UPDATE subscriptions SET period_start = now(), period_end = now() + make_interval(months => $2), "
                     "status = 'active', plan_id = $3, price_id = $4, grace_until = NULL, reminder_sent_for = NULL, "
                     "failed_payments = 0, last_failure_at = NULL WHERE id = $1 "
                     "RETURNING nh_iso(period_start), nh_iso(period_end), period_start, period_end",
                     subId, months, db::str((*o)[1]), priceId);
        eventKind = "upgraded";
    } else if (kind == "downgrade" && running) {
        // Paid now, effective when the current (higher) period ends.
        upd = tx.one("UPDATE subscriptions SET pending_price_id = $2, pending_from = period_end, "
                     "period_end = period_end + make_interval(months => $3), status = 'active', grace_until = NULL, "
                     "reminder_sent_for = NULL, failed_payments = 0, last_failure_at = NULL WHERE id = $1 "
                     "RETURNING nh_iso(pending_from), nh_iso(period_end), pending_from, period_end",
                     subId, priceId, months);
        eventKind = "downgrade_scheduled";
    } else {
        // New period start:
        //  - during/after a trial: when the trial ends (or now if it has ended);
        //  - active or in grace: exactly at the previous period end;
        //  - suspended/canceled: now (the suspended time is not billed).
        upd = tx.one(
            "UPDATE subscriptions SET "
            " period_start = CASE WHEN is_trial THEN GREATEST(now(), period_end) "
            "                     WHEN status IN ('active','past_due') THEN period_end ELSE now() END, "
            " period_end = (CASE WHEN is_trial THEN GREATEST(now(), period_end) "
            "                    WHEN status IN ('active','past_due') THEN period_end ELSE now() END) "
            "              + make_interval(months => $2), "
            " status = 'active', is_trial = false, plan_id = $3, price_id = $4, pending_plan_id = NULL, "
            " grace_until = NULL, reminder_sent_for = NULL, failed_payments = 0, last_failure_at = NULL "
            "WHERE id = $1 RETURNING nh_iso(period_start), nh_iso(period_end), period_start, period_end",
            subId, months, db::str((*o)[1]), priceId);
        eventKind = running ? "renewed" : "activated";
    }
    tx.q("UPDATE subscription_orders SET status = 'paid', paid_at = now(), failure_reason = NULL, "
         "provider_reference = COALESCE(provider_reference, NULLIF($2, '')) WHERE id = $1",
         orderId, providerReference);
    tx.q("UPDATE promo_redemptions SET status = 'confirmed', settled_at = now() WHERE order_id = $1", orderId);
    tx.q("INSERT INTO subscription_events (subscription_id, establishment_id, kind, from_status, to_status, "
         "period_start, period_end, details, actor_type, actor_id) VALUES ($1,$2,$3,$4,'active',$5,$6,$7::jsonb,$8,$9)",
         subId, est, eventKind, from, db::str(upd[2]), db::str(upd[3]),
         json{{"order_id", orderId}, {"plan", db::str((*o)[4])}, {"duration_months", months}, {"order_kind", kind}}.dump(),
         actorType, actorId);
    notify(tx, est, std::nullopt, kind == "downgrade" && running ? "plan_change_scheduled" : "subscription_activated",
           {{"plan", db::str((*o)[4])}, {"period_end", db::str(upd[1])}, {"effective_at", db::str(upd[0])}});
    tx.q("INSERT INTO admin_notifications (kind, params, establishment_id) VALUES ('order_paid', $1::jsonb, $2)",
         json{{"order_id", orderId}, {"plan", db::str((*o)[4])}, {"kind", kind}}.dump(), est);
    auditSystem(tx, est, "subscription.paid", "subscription_order", orderId,
                {{"period_start", db::str(upd[0])}, {"period_end", db::str(upd[1])}, {"kind", kind}}, actorType, actorId);
    return true;
}

bool failOrder(db::Tx& tx, const std::string& orderId, const std::string& reason, const std::string& actorType) {
    auto o = tx.opt("UPDATE subscription_orders SET status = 'failed', failure_reason = $2 WHERE id = $1 AND "
                    "status = 'pending' RETURNING establishment_id",
                    orderId, reason.substr(0, 200));
    if (!o) return false;
    std::string est = db::str((*o)[0]);
    releasePromo(tx, orderId);
    tx.q("UPDATE subscriptions SET failed_payments = failed_payments + 1, last_failure_at = now() "
         "WHERE establishment_id = $1",
         est);
    notify(tx, est, std::nullopt, "payment_failed", {{"order_id", orderId}});
    tx.q("INSERT INTO admin_notifications (kind, params, establishment_id) VALUES ('payment_failed', $1::jsonb, $2)",
         json{{"order_id", orderId}, {"reason", reason}}.dump(), est);
    auditSystem(tx, est, "subscription.payment_failed", "subscription_order", orderId, {{"reason", reason}}, actorType);
    return true;
}

}  // namespace subscriptions

// ------------------------------------------------------------------ routes
namespace {

// Legacy input: {plan_id} = the plan's main price.
std::string resolvePriceId(db::Tx& tx, const json& b) {
    if (auto pid = in::optUuid(b, "price_id")) return *pid;
    std::string planId = in::reqUuid(b, "plan_id");
    auto r = tx.opt("SELECT pp.id FROM plans p JOIN plan_prices pp ON pp.plan_id = p.id AND pp.currency = p.currency "
                    "AND pp.duration_months = CASE p.billing_period WHEN 'annual' THEN 12 ELSE 1 END "
                    "WHERE p.id = $1 AND p.active",
                    planId);
    if (!r) throw notFound("plan");
    return db::str((*r)[0]);
}

std::optional<std::string> promoParam(Env& env, const Ctx& ctx, const json& b) {
    auto code = in::optStr(b, "promo_code", 32);
    if (code && !code->empty()) {
        // Guessing codes is throttled per establishment and per IP.
        throttle(env.pool, "promo:est:" + ctx.actor.establishmentId, 15);
        throttle(env.pool, "promo:ip:" + ctx.actor.ip, 30);
    }
    return code;
}

// Starts a hosted checkout for a pending order with the chosen provider.
json startCheckout(Env& env, db::Tx& tx, const std::string& orderId, const std::string& providerCode) {
    auto p = providers::byCode(env, tx, providerCode, true);
    if (!p || !p->enabled) throw notFound("provider");
    auto o = tx.one("SELECT o.amount_minor, o.currency, o.status, p.name_fr, e.trade_name, e.email FROM subscription_orders o "
                    "JOIN plans p ON p.id = o.plan_id JOIN establishments e ON e.id = o.establishment_id "
                    "WHERE o.id = $1 FOR UPDATE OF o",
                    orderId);
    if (db::str(o[2]) != "pending") throw conflict("order_not_payable");
    providers::OrderInfo info{orderId, o[0].as<int64_t>(), db::str(o[1]), "Nhill — " + db::str(o[3]), db::str(o[4]),
                              db::str(o[5])};
    auto c = providers::createCheckout(env, *p, info);
    tx.q("UPDATE subscription_orders SET provider_id = $2, provider = $3, provider_session = $4, checkout_url = $5 "
         "WHERE id = $1",
         orderId, p->id, p->code, c.session, c.url);
    return {{"provider", p->code}, {"checkout_url", c.url}};
}

json enabledProviders(Env& env, db::Tx& tx, const std::string& currency) {
    json out = json::array();
    for (const auto& r : tx.q("SELECT code FROM payment_providers WHERE enabled AND kind <> 'payoneer' "
                              "AND (cardinality(currencies) = 0 OR $1::char(3) = ANY(currencies)) ORDER BY sort_order, code",
                              currency)) {
        auto p = providers::byCode(env, tx, db::str(r[0]), false);
        if (p) out.push_back({{"code", p->code}, {"name", p->displayName}, {"kind", p->kind}, {"environment", p->environment}});
    }
    return out;
}

}  // namespace

void registerSubscriptionRoutes(App& app, Env& env) {
    app.get("/api/v1/plans", {Access::Public}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(std::string("SELECT ") + subscriptions::kPlanCols +
                                      " FROM plans WHERE active ORDER BY tier, sort_order, price_minor"))
                items.push_back(subscriptions::planJson(tx, r, true));
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    // Plan, prices, payments and history: owners and administrators only.
    app.get("/api/v1/subscription", {Access::User, "subscription.manage"}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json orders = json::array();
            for (const auto& r : tx.q("SELECT id FROM subscription_orders WHERE establishment_id = $1 "
                                      "ORDER BY created_at DESC LIMIT 30",
                                      ctx.actor.establishmentId))
                orders.push_back(subscriptions::orderJson(tx, db::str(r[0])));
            json history = json::array();
            for (const auto& r : tx.q(
                     "SELECT kind, from_status, to_status, nh_iso(period_start), nh_iso(period_end), nh_iso(created_at) "
                     "FROM subscription_events WHERE establishment_id = $1 ORDER BY id DESC LIMIT 30",
                     ctx.actor.establishmentId))
                history.push_back({{"kind", db::str(r[0])}, {"from", db::str(r[1])}, {"to", db::str(r[2])},
                                   {"period_start", db::str(r[3])}, {"period_end", db::str(r[4])},
                                   {"at", db::str(r[5])}});
            auto inst = tx.opt("SELECT value FROM platform_settings WHERE key = 'payment_instructions'");
            return {{"subscription", subscriptions::view(tx, ctx.actor.establishmentId)},
                    {"orders", orders},
                    {"history", history},
                    {"grace_days", setting(tx, "grace_days", 7)},
                    {"payment_instructions", inst ? json::parse(db::str((*inst)[0])) : json(nullptr)}};
        });
        ctx.json(200, out);
    });

    // Price simulation (nothing is reserved): kind, pro-rata credit, discount, tax, total.
    app.post("/api/v1/subscription/quote", {Access::User, "subscription.manage"}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        auto promo = promoParam(env, ctx, b);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            std::string priceId = resolvePriceId(tx, b);
            auto q = subscriptions::quote(tx, ctx.actor.establishmentId, ctx.actor.establishmentKind, priceId, promo, false);
            json j = subscriptions::quoteJson(q);
            j["payment_providers"] = enabledProviders(env, tx, q.currency);
            return j;
        });
        ctx.json(200, out);
    });

    // Creates a payment request (and, with "provider", the hosted checkout). Nothing is
    // charged here: the order becomes paid only through a verified provider event or a
    // manual, audited confirmation by a billing administrator.
    app.post("/api/v1/subscription/orders", {Access::User, "subscription.manage"}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        auto promo = promoParam(env, ctx, b);
        auto providerCode = in::optStr(b, "provider", 40);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            std::string priceId = resolvePriceId(tx, b);
            json created = subscriptions::createOrder(env, tx, ctx.actor, priceId, promo, "user");
            std::string id = created["id"];
            json checkout = nullptr;
            if (created["total_minor"].get<int64_t>() == 0) {
                // Fully covered by a promo code / credit: nothing to collect.
                tx.q("UPDATE subscription_orders SET provider = 'none' WHERE id = $1", id);
                subscriptions::applyPaidOrder(env, tx, id, "user",
                                              std::optional<std::string>(ctx.actor.userId), "zero-amount");
            } else if (providerCode && !providerCode->empty()) {
                // Provider failures do not cancel the order: the client may retry the checkout.
                // (No SQL error can occur before the provider call, so the transaction stays usable.)
                try {
                    checkout = startCheckout(env, tx, id, *providerCode);
                } catch (const ApiError& e) {
                    checkout = {{"error", e.code}};
                }
            }
            auto inst = tx.opt("SELECT value FROM platform_settings WHERE key = 'payment_instructions'");
            json o = subscriptions::orderJson(tx, id);
            return {{"order", o},
                    {"checkout", checkout},
                    {"payment_providers", enabledProviders(env, tx, o["currency"])},
                    {"payment_instructions", inst ? json::parse(db::str((*inst)[0])) : json(nullptr)}};
        });
        ctx.json(201, out);
    });

    app.post("/api/v1/subscription/orders/:id/checkout", {Access::User, "subscription.manage"}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "order");
        std::string provider = in::reqStr(ctx.body(), "provider", 40);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            tx.one("SELECT 1 FROM subscription_orders WHERE id = $1 AND establishment_id = $2", id,
                   ctx.actor.establishmentId);
            return startCheckout(env, tx, id, provider);
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/subscription/orders/:id/cancel", {Access::User, "subscription.manage"}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "order");
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.q("UPDATE subscription_orders SET status = 'canceled', failure_reason = 'canceled_by_customer' "
                          "WHERE id = $1 AND establishment_id = $2 AND status = 'pending' RETURNING id",
                          id, ctx.actor.establishmentId);
            if (r.empty()) throw conflict("order_not_cancelable");
            subscriptions::releasePromo(tx, id);
            audit(tx, ctx.actor, "subscription.order_canceled", "subscription_order", id);
        });
        ctx.noContent();
    });

    app.post("/api/v1/subscription/auto-renew", {Access::User, "subscription.manage"}, [&env](Ctx& ctx) {
        bool consent = in::optBool(ctx.body(), "consent", false);
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.q("UPDATE subscriptions SET auto_renew_consent_at = CASE WHEN $2 THEN now() ELSE NULL END "
                 "WHERE establishment_id = $1",
                 ctx.actor.establishmentId, consent);
            audit(tx, ctx.actor, consent ? "subscription.auto_renew_consented" : "subscription.auto_renew_withdrawn",
                  "subscription", ctx.actor.establishmentId);
        });
        ctx.noContent();
    });

    // Landing page after a hosted checkout. Informational only: rights change only
    // through the verified webhook.
    app.get("/payment/return", {Access::Public}, [](Ctx& ctx) {
        bool ok = ctx.query("outcome").value_or("") == "success";
        std::string msg = ok ? "Paiement transmis. Votre abonnement sera mis à jour dès la confirmation du prestataire "
                               "de paiement. Vous pouvez revenir dans Nhill Business."
                             : "Paiement annulé. Aucun montant n'a été prélevé par Nhill. Vous pouvez revenir dans Nhill "
                               "Business pour réessayer.";
        ctx.res.set_header("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'");
        ctx.file("<!doctype html><html lang=\"fr\"><meta charset=\"utf-8\"><meta name=\"viewport\" "
                            "content=\"width=device-width\"><title>Nhill</title><body style=\"font-family:sans-serif;"
                            "background:#F7F5F0;color:#14181F;display:flex;align-items:center;justify-content:center;"
                            "min-height:90vh\"><div style=\"max-width:460px;padding:24px;background:#fff;border-radius:12px;"
                            "border:1px solid #e4e0d8\"><h2 style=\"color:#173F5F;margin-top:0\">Nhill</h2><p>" +
                                msg + "</p></div></body></html>",
                            "text/html; charset=utf-8", "nhill-payment.html", true);
    });

    // ---------------------------------------------------------------- provider webhooks
    // Provider configured in the console (payment_providers.code), or a legacy
    // generic HMAC secret from the server configuration (NH_WEBHOOK_SECRET_<CODE>).
    app.post("/api/v1/webhooks/payments/:provider", {Access::Webhook}, [&env](Ctx& ctx) {
        std::string code = ctx.param("provider");
        if (code.size() > 40) throw notFound("provider");
        std::optional<providers::Provider> p = db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            return providers::byCode(env, tx, code, true);
        });
        if (!p) {
            auto it = env.cfg.webhookSecrets.find(code);
            if (it == env.cfg.webhookSecrets.end()) throw notFound("provider");
            p = providers::Provider{};
            p->code = code;
            p->kind = "generic_hmac";
            p->enabled = true;
            p->secrets = {{"webhook_secret", it->second}};
        }
        if (!p->enabled) throw notFound("provider");
        providers::Event e;
        try {
            e = providers::parseWebhook(env, *p, ctx.req);
        } catch (const ApiError& err) {
            if (err.status == 401) log::warn("webhook verification failed for provider " + code);
            throw;
        }
        if (e.eventId.empty()) throw badRequest("invalid_event");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto ins = tx.q("INSERT INTO payment_webhook_events (provider, event_id, payload) VALUES ($1, $2, $3::jsonb) "
                            "ON CONFLICT (provider, event_id) DO NOTHING RETURNING id",
                            code, e.eventId, ctx.req.body);
            if (ins.empty()) return {{"status", "duplicate"}};
            std::string evId = db::str(ins[0][0]);
            std::string outcome;
            std::optional<pqxx::row> o;
            if (e.type != "ignored" && in::isUuid(e.orderId))
                o = tx.opt("SELECT status, amount_minor, currency, establishment_id FROM subscription_orders "
                           "WHERE id = $1 FOR UPDATE",
                           e.orderId);
            if (e.type == "ignored") {
                outcome = "ignored" + (e.detail.empty() ? std::string() : ":" + e.detail);
            } else if (!o) {
                outcome = "unknown_order";
            } else if ((*o)[1].as<int64_t>() != e.amountMinor || db::str((*o)[2]) != e.currency) {
                outcome = "amount_mismatch";
                auditSystem(tx, db::str((*o)[3]), "subscription.payment_amount_mismatch", "subscription_order", e.orderId,
                            {{"received", money(e.amountMinor, e.currency.empty() ? "USD" : e.currency)},
                             {"currency", e.currency}, {"provider", code}},
                            "provider");
                tx.q("INSERT INTO admin_notifications (kind, params, establishment_id) VALUES "
                     "('payment_amount_mismatch', $1::jsonb, $2)",
                     json{{"order_id", e.orderId}, {"provider", code}}.dump(), db::str((*o)[3]));
            } else if (e.type == "succeeded") {
                tx.q("UPDATE subscription_orders SET provider = COALESCE(provider, $2) WHERE id = $1", e.orderId, code);
                outcome = subscriptions::applyPaidOrder(env, tx, e.orderId, "provider", std::nullopt, e.reference)
                              ? "applied"
                              : "already_paid";
            } else if (e.type == "failed") {
                outcome = subscriptions::failOrder(tx, e.orderId, "provider:" + code, "provider") ? "marked_failed"
                                                                                                 : "not_pending";
            } else {
                tx.q("UPDATE subscription_orders SET status = 'refunded' WHERE id = $1 AND status = 'paid'", e.orderId);
                auditSystem(tx, db::str((*o)[3]), "subscription.refunded", "subscription_order", e.orderId, {}, "provider");
                tx.q("INSERT INTO admin_notifications (kind, params, establishment_id) VALUES ('order_refunded', $1::jsonb, $2)",
                     json{{"order_id", e.orderId}}.dump(), db::str((*o)[3]));
                outcome = "refund_recorded_manual_review";
            }
            tx.q("UPDATE payment_webhook_events SET processed_at = now(), outcome = $2 WHERE id = $1", evId, outcome);
            return {{"status", "processed"}, {"outcome", outcome}};
        });
        ctx.json(200, out);
    });
}

}  // namespace nh
