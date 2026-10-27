// Scheduled jobs. Safe to run concurrently on several servers: every unit of
// work is claimed with FOR UPDATE SKIP LOCKED and processed in its own
// transaction, so one failure never blocks the others.
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "services/services.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"
#include "util/mailer.hpp"

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

// --------------------------------------------------------------------- e-mails

namespace {

// Template text from server/i18n/<lang>.json: "{name}" is replaced by the parameter of
// that name (dates are shown as YYYY-MM-DD). Unknown placeholders stay as they are.
std::string fill(std::string text, const json& params) {
    for (auto it = params.begin(); it != params.end(); ++it) {
        std::string value;
        if (it.value().is_string()) {
            value = it.value().get<std::string>();
            const std::string& k = it.key();
            bool isDate = (k.size() > 4 && (k.compare(k.size() - 4, 4, "_end") == 0 || k.compare(k.size() - 3, 3, "_at") == 0)) ||
                          (k.size() > 6 && k.compare(k.size() - 6, 6, "_until") == 0);
            if (isDate && value.size() >= 10) value = value.substr(0, 10);
        } else if (it.value().is_number()) {
            value = it.value().dump();
        } else {
            continue;
        }
        const std::string key = "{" + it.key() + "}";
        for (size_t pos = text.find(key); pos != std::string::npos; pos = text.find(key, pos + value.size()))
            text.replace(pos, key.size(), value);
    }
    return text;
}

// Secret parameters (the one-time code of a password reset) are kept sealed in the
// outbox and opened only to build the message.
json withSecrets(const Env& env, json params) {
    if (params.contains("code_box") && params.contains("code_nonce")) {
        auto box = crypto::fromHex(params["code_box"].get<std::string>());
        auto nonce = crypto::fromHex(params["code_nonce"].get<std::string>());
        std::optional<std::string> code;
        if (box && nonce) code = crypto::open(env.cfg.dataKey, *box, *nonce);
        if (!code) throw std::runtime_error("sealed parameter cannot be opened");
        params["code"] = code->size() == 8 ? code->substr(0, 4) + "-" + code->substr(4) : *code;
        params.erase("code_box");
        params.erase("code_nonce");
    }
    return params;
}

std::mutex gMailMutex;
std::condition_variable gMailCv;
bool gMailKick = false;
bool gMailStop = false;

}  // namespace

mail::Message renderEmail(const Env& env, const std::string& to, const std::string& lang, const std::string& tmpl,
                          const json& params) {
    json p = withSecrets(env, params);
    mail::Message m;
    m.to = to;
    m.subject = fill(env.i18n.t(lang, "email." + tmpl + ".subject"), p);
    m.body = fill(env.i18n.t(lang, "email." + tmpl + ".body"), p) + env.i18n.t(lang, "email.signature");
    return m;
}

// One message per transaction (claimed with FOR UPDATE SKIP LOCKED, so several workers
// never send the same message). A failed delivery is retried at the next run, up to
// 5 attempts. Secret parameters are erased once the message is sent, failed or expired.
// The "log" transport writes a masked line instead of sending (development only:
// recovery codes are then never delivered, nor written to the log).
Stats dispatchEmails(Env& env) {
    Stats st;
    if (env.cfg.emailTransport == "disabled") return st;
    std::vector<std::string> skipped;
    for (int guard = 0; guard < 1000; ++guard) {
        bool more = db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.opt(
                "SELECT id, to_address, template, language, params::text, "
                "(params ? 'expires_at' AND (params->>'expires_at')::timestamptz <= now()), attempts "
                "FROM email_outbox WHERE status = 'pending' AND NOT (id = ANY($1::uuid[])) "
                "ORDER BY created_at LIMIT 1 FOR UPDATE SKIP LOCKED",
                uuidArray(skipped));
            if (!r) return false;
            const std::string id = db::str((*r)[0]), addr = db::str((*r)[1]), tmpl = db::str((*r)[2]);
            const std::string scrub = "CASE WHEN params ? 'code_box' THEN '{}'::jsonb ELSE params END";
            if (db::boolean((*r)[5])) {
                tx.q("UPDATE email_outbox SET status = 'failed', last_error = 'expired', params = " + scrub +
                     " WHERE id = $1", id);
                return true;
            }
            if (env.cfg.emailTransport == "log") {
                auto at = addr.find('@');
                std::string masked = addr.substr(0, std::min<size_t>(2, at)) + "***" +
                                     (at == std::string::npos ? "" : addr.substr(at));
                log::info("email (log transport, not sent) template=" + tmpl + " to=" + masked);
                tx.q("UPDATE email_outbox SET status = 'sent', sent_at = now(), attempts = attempts + 1, params = " +
                     scrub + " WHERE id = $1", id);
                ++st.emailsSent;
                return true;
            }
            try {
                mail::send(env.cfg, renderEmail(env, addr, db::str((*r)[3]), tmpl, json::parse(db::str((*r)[4]))));
                tx.q("UPDATE email_outbox SET status = 'sent', sent_at = now(), attempts = attempts + 1, "
                     "last_error = NULL, params = " + scrub + " WHERE id = $1", id);
                ++st.emailsSent;
            } catch (const std::exception& e) {
                std::string err = e.what();
                if (err.size() > 300) err.resize(300);
                tx.q("UPDATE email_outbox SET attempts = attempts + 1, last_error = $2, "
                     "status = CASE WHEN attempts + 1 >= 5 THEN 'failed' ELSE 'pending' END, "
                     "params = CASE WHEN attempts + 1 >= 5 THEN " + scrub + " ELSE params END WHERE id = $1",
                     id, err);
                log::error("email " + tmpl + " not delivered: " + err);
                skipped.push_back(id);
                ++st.emailFailures;
            }
            return true;
        });
        if (!more) break;
    }
    return st;
}

void kickEmails() {
    {
        std::lock_guard<std::mutex> l(gMailMutex);
        gMailKick = true;
    }
    gMailCv.notify_one();
}

void runEmailWorker(Env& env) {
    std::unique_lock<std::mutex> l(gMailMutex);
    while (!gMailStop) {
        // Woken right after a time-sensitive message is queued (recovery code), otherwise
        // every minute for retries.
        gMailCv.wait_for(l, std::chrono::seconds(60), [] { return gMailKick || gMailStop; });
        if (gMailStop) break;
        gMailKick = false;
        l.unlock();
        try {
            dispatchEmails(env);
        } catch (const std::exception& e) {
            log::error(std::string("email worker: ") + e.what());
        }
        l.lock();
    }
}

void stopEmailWorker() {
    {
        std::lock_guard<std::mutex> l(gMailMutex);
        gMailStop = true;
    }
    gMailCv.notify_all();
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
        total.emailFailures += s.emailFailures;
        total.planChanges += s.planChanges;
        total.ordersExpired += s.ordersExpired;
    };
    add(rotateCodes(env));
    add(subscriptionLifecycle(env));
    add(dispatchEmails(env));
    db::run(env.pool, std::nullopt, [](db::Tx& tx) {
        tx.q("DELETE FROM join_tickets WHERE expires_at < now() - interval '1 hour'");
        tx.q("DELETE FROM auth_throttle WHERE window_start < now() - interval '1 day'");
        // Recovery secrets are only hashes; finished ones are kept 30 days for investigations.
        tx.q("DELETE FROM password_resets WHERE expires_at < now() - interval '30 days'");
    });
    return total;
}

}  // namespace nh::jobs
