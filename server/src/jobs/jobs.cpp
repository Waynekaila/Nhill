// Scheduled jobs. Safe to run concurrently on several servers: every unit of
// work is claimed with FOR UPDATE SKIP LOCKED and processed in its own
// transaction, so one failure never blocks the others.
#include "services/services.hpp"
#include "util/log.hpp"

namespace nh::jobs {

namespace {

std::string uuidArray(const std::vector<std::string>& ids) {
    std::string s = "{";
    for (size_t i = 0; i < ids.size(); ++i) s += (i ? "," : "") + ids[i];
    return s + "}";
}

// Repeatedly claims one row with `claimSql` (must return id, establishment_id
// and accept the excluded ids array as $1) and processes it.
template <typename F>
int drain(Env& env, const std::string& claimSql, const std::string& label, F&& process, int* failures = nullptr) {
    std::vector<std::string> failed;
    int done = 0;
    for (int guard = 0; guard < 100000; ++guard) {
        std::string current;
        try {
            bool got = db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
                auto r = tx.opt(claimSql, uuidArray(failed));
                if (!r) return false;
                current = db::str((*r)[0]);
                process(tx, db::str((*r)[0]), db::str((*r)[1]));
                return true;
            });
            if (!got) break;
            ++done;
        } catch (const std::exception& e) {
            if (current.empty()) throw;
            failed.push_back(current);
            if (failures) ++*failures;
            log::error(label + " failed for " + current + ": " + e.what());
            try {
                db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
                    auditSystem(tx, std::nullopt, "job." + label + "_failed", "job", current, {{"error", e.what()}});
                });
            } catch (...) {
            }
        }
    }
    return done;
}

}  // namespace

Stats rotateCodes(Env& env) {
    Stats st;
    st.codesRotated = drain(
        env,
        "SELECT id, establishment_id FROM establishment_codes WHERE status = 'active' AND expires_at <= now() "
        "AND NOT (id = ANY($1::uuid[])) ORDER BY expires_at LIMIT 1 FOR UPDATE SKIP LOCKED",
        "code_rotation",
        [&](db::Tx& tx, const std::string& codeId, const std::string& est) {
            codes::rotate(env, tx, est, "superseded", "scheduled_rotation", "system", std::nullopt);
            notify(tx, est, std::nullopt, "code_rotated", {{"automatic", true}});
            emailManagers(tx, est, "code_rotated", json::object());
            auditSystem(tx, est, "establishment.code_rotated", "establishment_code", codeId, {{"reason", "scheduled"}});
        },
        &st.codeRotationFailures);
    return st;
}

Stats subscriptionLifecycle(Env& env) {
    Stats st;
    // 0. Paid downgrades become effective at the end of the higher-tier period.
    st.planChanges = drain(
        env,
        "SELECT id, establishment_id FROM subscriptions WHERE pending_price_id IS NOT NULL AND pending_from <= now() "
        "AND NOT (id = ANY($1::uuid[])) LIMIT 1 FOR UPDATE SKIP LOCKED",
        "plan_change", [&](db::Tx& tx, const std::string& id, const std::string& est) {
            auto r = tx.one("UPDATE subscriptions s SET plan_id = pp.plan_id, price_id = pp.id, pending_price_id = NULL, "
                            "period_start = s.pending_from, pending_from = NULL FROM plan_prices pp "
                            "WHERE s.id = $1 AND pp.id = s.pending_price_id RETURNING (SELECT code FROM plans WHERE id = pp.plan_id), "
                            "s.status",
                            id);
            tx.q("INSERT INTO subscription_events (subscription_id, establishment_id, kind, from_status, to_status, details, "
                 "actor_type) VALUES ($1,$2,'plan_changed',$3,$3,$4::jsonb,'system')",
                 id, est, db::str(r[1]), json{{"plan", db::str(r[0])}}.dump());
            notify(tx, est, std::nullopt, "plan_changed", {{"plan", db::str(r[0])}});
            auditSystem(tx, est, "subscription.plan_changed", "subscription", id, {{"plan", db::str(r[0])}});
        });
    // 1. First reminder N days before the end of the period (trial or paid).
    st.remindersSent = drain(
        env,
        "SELECT id, establishment_id FROM subscriptions WHERE status IN ('trialing','active') AND period_end > now() "
        "AND period_end - make_interval(days => nh_setting_int('reminder_days_before', 7)) <= now() "
        "AND reminder_sent_for IS DISTINCT FROM period_end AND NOT (id = ANY($1::uuid[])) LIMIT 1 FOR UPDATE SKIP LOCKED",
        "subscription_reminder", [&](db::Tx& tx, const std::string& id, const std::string& est) {
            auto r = tx.one("UPDATE subscriptions SET reminder_sent_for = period_end WHERE id = $1 "
                            "RETURNING nh_iso(period_end), is_trial, status",
                            id);
            tx.q("INSERT INTO subscription_events (subscription_id, establishment_id, kind, from_status, to_status, "
                 "actor_type) VALUES ($1,$2,'reminder_sent',$3,$3,'system')",
                 id, est, db::str(r[2]));
            json params = {{"period_end", db::str(r[0])}, {"is_trial", db::boolean(r[1])}};
            notify(tx, est, std::nullopt, "subscription_reminder", params);
            emailManagers(tx, est, "subscription_reminder", params);
        });
    // 2. Due date reached without confirmed payment: grace period, service maintained.
    st.enteredGrace = drain(
        env,
        "SELECT id, establishment_id FROM subscriptions WHERE status IN ('trialing','active') AND period_end <= now() "
        "AND NOT (id = ANY($1::uuid[])) LIMIT 1 FOR UPDATE SKIP LOCKED",
        "subscription_grace", [&](db::Tx& tx, const std::string& id, const std::string& est) {
            std::string from = db::str(tx.one("SELECT status FROM subscriptions WHERE id = $1", id)[0]);
            auto r = tx.one("UPDATE subscriptions s SET status = 'past_due', grace_until = s.period_end + "
                            "make_interval(days => nh_setting_int('grace_days', 7)) WHERE id = $1 "
                            "RETURNING nh_iso(grace_until)",
                            id);
            tx.q("INSERT INTO subscription_events (subscription_id, establishment_id, kind, from_status, to_status, details, "
                 "actor_type) VALUES ($1,$2,'grace_started',$3,'past_due',$4::jsonb,'system')",
                 id, est, from, json{{"grace_until", db::str(r[0])}}.dump());
            json params = {{"grace_until", db::str(r[0])}};
            notify(tx, est, std::nullopt, "subscription_grace", params);
            emailManagers(tx, est, "subscription_grace", params);
        });
    // 3. End of grace: read-only suspension (policy "read_only", announced in the terms).
    st.suspended = drain(
        env,
        "SELECT id, establishment_id FROM subscriptions WHERE status = 'past_due' AND grace_until <= now() "
        "AND NOT (id = ANY($1::uuid[])) LIMIT 1 FOR UPDATE SKIP LOCKED",
        "subscription_suspension", [&](db::Tx& tx, const std::string& id, const std::string& est) {
            tx.q("UPDATE subscriptions SET status = 'suspended' WHERE id = $1", id);
            tx.q("INSERT INTO subscription_events (subscription_id, establishment_id, kind, from_status, to_status, "
                 "actor_type) VALUES ($1,$2,'suspended','past_due','suspended','system')",
                 id, est);
            notify(tx, est, std::nullopt, "subscription_suspended", json::object());
            emailManagers(tx, est, "subscription_suspended", json::object());
            auditSystem(tx, est, "subscription.suspended", "subscription", id, {{"policy", "read_only"}});
        });
    // 4. Payment requests not paid in time expire (their promo code reservation is released).
    st.ordersExpired = drain(
        env,
        "SELECT id, establishment_id FROM subscription_orders WHERE status = 'pending' AND expires_at <= now() "
        "AND NOT (id = ANY($1::uuid[])) LIMIT 1 FOR UPDATE SKIP LOCKED",
        "order_expiry", [&](db::Tx& tx, const std::string& id, const std::string&) {
            tx.q("UPDATE subscription_orders SET status = 'expired', failure_reason = 'expired' WHERE id = $1", id);
            subscriptions::releasePromo(tx, id);
        });
    return st;
}

// SMTP delivery is not implemented in this version: the "log" transport
// records that a message would be sent (without the full address) and marks
// it as handled. See docs/limites.md.
Stats dispatchEmails(Env& env) {
    Stats st;
    if (env.cfg.emailTransport != "log") return st;
    st.emailsSent = db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
        int n = 0;
        for (const auto& r : tx.q("SELECT id, to_address, template FROM email_outbox WHERE status = 'pending' "
                                  "ORDER BY created_at LIMIT 200 FOR UPDATE SKIP LOCKED")) {
            std::string addr = db::str(r[1]);
            auto at = addr.find('@');
            std::string masked = addr.substr(0, std::min<size_t>(2, at)) + "***" + (at == std::string::npos ? "" : addr.substr(at));
            log::info("email (log transport) template=" + db::str(r[2]) + " to=" + masked);
            tx.q("UPDATE email_outbox SET status = 'sent', sent_at = now(), attempts = attempts + 1 WHERE id = $1", db::str(r[0]));
            ++n;
        }
        return n;
    });
    return st;
}

Stats runAll(Env& env) {
    Stats total;
    auto add = [&](const Stats& s) {
        total.codesRotated += s.codesRotated;
        total.codeRotationFailures += s.codeRotationFailures;
        total.remindersSent += s.remindersSent;
        total.enteredGrace += s.enteredGrace;
        total.suspended += s.suspended;
        total.emailsSent += s.emailsSent;
        total.planChanges += s.planChanges;
        total.ordersExpired += s.ordersExpired;
    };
    add(rotateCodes(env));
    add(subscriptionLifecycle(env));
    add(dispatchEmails(env));
    db::run(env.pool, std::nullopt, [](db::Tx& tx) {
        tx.q("DELETE FROM join_tickets WHERE expires_at < now() - interval '1 hour'");
        tx.q("DELETE FROM auth_throttle WHERE window_start < now() - interval '1 day'");
    });
    return total;
}

}  // namespace nh::jobs
