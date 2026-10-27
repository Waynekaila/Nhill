// Rights of an establishment: computed access, plan features and limits (see entitlements.hpp).
#include "services/entitlements.hpp"

#include <algorithm>
#include <cctype>

#include "services/services.hpp"
#include "util/crypto.hpp"

namespace nh::entitlements {

namespace {

std::optional<int64_t> optInt64(const pqxx::field& f) {
    if (f.is_null()) return std::nullopt;
    return f.as<int64_t>();
}

json limitValue(const std::optional<int64_t>& v) { return v ? json(*v) : json(nullptr); }

constexpr int64_t kMiB = 1024 * 1024;

}  // namespace

std::set<std::string> Snapshot::enabled() const {
    std::set<std::string> out;
    for (const auto& [k, on] : features)
        if (on) out.insert(k);
    return out;
}

Access access(db::Tx& tx, const std::string& est) {
    Access a;
    auto r = tx.opt("SELECT access, reason, nh_iso(access_until), effective_plan_id, is_trial, COALESCE(status, '') "
                    "FROM nh_subscription_access($1)",
                    est);
    if (!r) {
        a.state = "none";
        a.reason = "establishment_closed";
        return a;
    }
    a.state = db::str((*r)[0]);
    a.reason = db::str((*r)[1]);
    if (!(*r)[2].is_null()) a.until = db::str((*r)[2]);
    if (!(*r)[3].is_null()) a.planId = db::str((*r)[3]);
    a.trial = db::boolean((*r)[4]);
    a.status = db::str((*r)[5]);
    return a;
}

Snapshot load(db::Tx& tx, const std::string& est, const std::string& kind) {
    Snapshot s;
    s.access = access(tx, est);
    s.planId = s.access.planId;
    if (s.planId) {
        if (auto p = tx.opt("SELECT code, name_fr, name_en, kind FROM plans WHERE id = $1", *s.planId)) {
            s.planCode = db::str((*p)[0]);
            s.planNameFr = db::str((*p)[1]);
            s.planNameEn = db::str((*p)[2]);
            s.planKind = db::str((*p)[3]);
        }
    }
    // A trial started without a plan keeps the 2.x rules (platform settings).
    const bool trialWithoutPlan = !s.planId;
    for (const auto& r : tx.q("SELECT c.key, c.kind, c.default_enabled, c.default_limit, c.trial_enabled, c.trial_limit, "
                              "e.key IS NOT NULL, e.enabled, e.limit_value FROM plan_capabilities c "
                              "LEFT JOIN plan_entitlements e ON e.key = c.key AND e.plan_id = $1::uuid "
                              "ORDER BY c.sort_order, c.key",
                              s.planId)) {
        std::string k = db::str(r[0]);
        bool hasEntry = db::boolean(r[6]);
        if (db::str(r[1]) == "feature") {
            bool on = hasEntry ? db::boolean(r[7]) : (trialWithoutPlan ? db::boolean(r[4]) : db::boolean(r[2]));
            s.features[k] = on;
        } else {
            std::optional<int64_t> v = hasEntry ? optInt64(r[8]) : (trialWithoutPlan ? optInt64(r[5]) : optInt64(r[3]));
            if (trialWithoutPlan && !hasEntry) {
                if (k == key::kUsers) v = kind == "individual" ? 1 : setting(tx, "trial_max_users", 5);
                if (k == key::kRegisters) v = setting(tx, "trial_max_registers", 1);
            }
            s.limits[k] = v;
        }
    }
    return s;
}

Usage usage(db::Tx& tx, const std::string& est) {
    Usage u;
    auto r = tx.one("SELECT users, registers, warehouses, products, documents_month, storage_bytes "
                    "FROM nh_establishment_usage($1)",
                    est);
    u.users = r[0].as<int64_t>();
    u.registers = r[1].as<int64_t>();
    u.warehouses = r[2].as<int64_t>();
    u.products = r[3].as<int64_t>();
    u.documentsMonth = r[4].as<int64_t>();
    u.storageBytes = r[5].as<int64_t>();
    return u;
}

int64_t used(const Usage& u, const std::string& k) {
    if (k == key::kUsers) return u.users;
    if (k == key::kRegisters) return u.registers;
    if (k == key::kWarehouses) return u.warehouses;
    if (k == key::kProducts) return u.products;
    if (k == key::kDocumentsMonth) return u.documentsMonth;
    if (k == key::kStorageMb) return u.storageBytes;
    return 0;
}

json toJson(const Snapshot& s, const std::optional<Usage>& u) {
    json features = json::array();
    for (const auto& [k, on] : s.features)
        if (on) features.push_back(k);
    json limits = json::object();
    for (const auto& [k, v] : s.limits) {
        json l = {{"max", limitValue(v)}};
        if (u) {
            int64_t n = used(*u, k);
            if (k == key::kStorageMb) {
                l["used_bytes"] = n;
                l["used"] = (n + kMiB - 1) / kMiB;
            } else {
                l["used"] = n;
            }
        }
        limits[k] = l;
    }
    json plan = nullptr;
    if (s.planId)
        plan = {{"id", *s.planId},
                {"code", s.planCode.value_or("")},
                {"name_fr", s.planNameFr.value_or("")},
                {"name_en", s.planNameEn.value_or("")},
                {"kind", s.planKind.value_or("paid")}};
    return {{"access",
             {{"state", s.access.state},
              {"reason", s.access.reason},
              {"until", s.access.until ? json(*s.access.until) : json(nullptr)}}},
            {"subscription_status", s.access.status},
            {"trial", s.access.trial},
            {"plan", plan},
            {"features", features},
            {"limits", limits}};
}

std::string version(const std::string& role, const Snapshot& s) {
    std::string material = role + "|" + s.access.state + "|" + s.planId.value_or("-");
    for (const auto& k : s.enabled()) material += "|" + k;
    return crypto::sha256Hex(material).substr(0, 16);
}

void fillActor(db::Tx& tx, Actor& a) {
    Snapshot s = load(tx, a.establishmentId, a.establishmentKind);
    a.access = s.access.state;
    a.accessReason = s.access.reason;
    a.features = s.enabled();
    a.rightsVersion = version(a.role, s);
}

void requireCapacity(db::Tx& tx, const std::string& est, const std::string& kind, const std::string& limitKey,
                     int64_t adding) {
    Snapshot s = load(tx, est, kind);
    auto it = s.limits.find(limitKey);
    if (it == s.limits.end() || !it->second) return;
    // Serialises concurrent additions on the same limit of the same establishment until the
    // end of the transaction, so two requests cannot both take the last unit.
    tx.q("SELECT pg_advisory_xact_lock(hashtext($1 || ':' || $2))", est, limitKey);
    Usage u = usage(tx, est);
    int64_t current = used(u, limitKey);
    int64_t max = *it->second;
    int64_t cap = limitKey == key::kStorageMb ? max * kMiB : max;
    if (current + adding <= cap) return;
    json d = {{"limit_key", limitKey}, {"limit", max},
              {"used", limitKey == key::kStorageMb ? (current + kMiB - 1) / kMiB : current}};
    // Codes of 2.x kept for the users and registers limits (clients already translate them).
    if (limitKey == key::kUsers) throw conflict("user_limit_reached", "the current plan does not allow more users", d);
    if (limitKey == key::kRegisters) throw conflict("plan_register_limit", "the plan does not allow more registers", d);
    throw conflict("plan_limit_reached", "the current plan does not allow more", d);
}

void requireFeature(const Actor& a, const std::string& featureKey) {
    if (!a.features.count(featureKey))
        throw ApiError(403, "feature_not_included", "feature not included in the plan", {{"feature", featureKey}});
}

void requireCurrency(db::Tx& tx, const Actor& a, const std::string& currency) {
    if (a.features.count(key::kMultiCurrency)) return;
    auto r = tx.one("SELECT main_currency FROM establishments WHERE id = $1", a.establishmentId);
    if (db::str(r[0]) != currency) requireFeature(a, key::kMultiCurrency);
}

std::string docClass(const std::string& docType, bool posSession) {
    return docType == "credit_note" && posSession ? "pos_credit_note" : docType;
}

std::string featureOfDocClass(const std::string& c) {
    if (c == "quote" || c == "order") return key::kQuotes;
    if (c == "invoice" || c == "credit_note") return key::kInvoices;
    if (c == "receipt" || c == "pos_credit_note") return key::kPos;
    return "";
}

bool docVisible(const Actor& a, const std::string& docType, bool posSession) {
    return a.features.count(featureOfDocClass(docClass(docType, posSession))) > 0;
}

std::string docVisibleSql(const std::string& d, int param) {
    return "((CASE WHEN " + d + ".doc_type = 'credit_note' AND " + d + ".pos_session_id IS NOT NULL THEN 'pos_credit_note' "
           "ELSE " + d + ".doc_type END) = ANY($" + std::to_string(param) + "::text[]))";
}

std::string docTypesArray(const Actor& a) {
    std::string out = "{";
    bool first = true;
    for (const char* c : {"quote", "order", "invoice", "credit_note", "receipt", "pos_credit_note"})
        if (a.features.count(featureOfDocClass(c))) {
            out += std::string(first ? "" : ",") + c;
            first = false;
        }
    return out + "}";
}

std::string movementFilterSql(const std::string& m, int multiParam, int purchasesParam, int typesParam) {
    const std::string pm = "$" + std::to_string(multiParam), pp = "$" + std::to_string(purchasesParam);
    return "((" + pm + "::boolean OR " + m + ".kind NOT IN ('transfer_in','transfer_out')) AND (" + pp +
           "::boolean OR " + m + ".kind NOT IN ('purchase','return_out')) AND (" + m +
           ".source_type IS DISTINCT FROM 'sales_document' OR EXISTS (SELECT 1 FROM sales_documents sd WHERE sd.id = " + m +
           ".source_id AND " + docVisibleSql("sd", typesParam) + ")))";
}

std::string paymentFilterSql(const std::string& y, int purchasesParam, int expensesParam, int typesParam) {
    const std::string pp = "$" + std::to_string(purchasesParam), pe = "$" + std::to_string(expensesParam);
    return "((" + y + ".purchase_id IS NULL OR " + pp + "::boolean) AND (" + y + ".expense_id IS NULL OR " + pe +
           "::boolean) AND (" + y + ".document_id IS NULL OR EXISTS (SELECT 1 FROM sales_documents pd WHERE pd.id = " + y +
           ".document_id AND " + docVisibleSql("pd", typesParam) + ")))";
}

std::optional<Assignment> matchRule(db::Tx& tx, const std::string& kind, const std::string& country, bool trialAvailable) {
    auto r = tx.opt(
        "SELECT r.id, r.plan_id, p.code, r.mode, COALESCE(r.duration_days, CASE WHEN r.mode = 'trial' THEN "
        "COALESCE(p.trial_days, nh_setting_int('trial_days', 30)) END) "
        "FROM plan_assignment_rules r JOIN plans p ON p.id = r.plan_id "
        "WHERE r.active AND p.active AND p.archived_at IS NULL "
        "AND (r.audience = 'any' OR r.audience = $1) AND (p.audience = 'any' OR p.audience = $1) "
        "AND (r.country_code IS NULL OR r.country_code = $2) AND (r.mode = 'grant' OR $3) "
        "ORDER BY r.priority, r.created_at LIMIT 1",
        kind, country, trialAvailable);
    if (!r) return std::nullopt;
    Assignment a;
    a.ruleId = db::str((*r)[0]);
    a.planId = db::str((*r)[1]);
    a.planCode = db::str((*r)[2]);
    a.mode = db::str((*r)[3]);
    if (!(*r)[4].is_null()) a.days = (*r)[4].as<int>();
    return a;
}

json catalogJson(db::Tx& tx) {
    json out = json::array();
    for (const auto& r : tx.q("SELECT key, kind, label_fr, label_en, description_fr, description_en, unit, "
                              "default_enabled, default_limit, trial_enabled, trial_limit, sort_order "
                              "FROM plan_capabilities ORDER BY sort_order, key")) {
        bool feature = db::str(r[1]) == "feature";
        json c = {{"key", db::str(r[0])},           {"kind", db::str(r[1])},
                  {"label_fr", db::str(r[2])},      {"label_en", db::str(r[3])},
                  {"description_fr", db::str(r[4])}, {"description_en", db::str(r[5])},
                  {"sort_order", r[11].as<int>()}};
        if (feature) {
            c["default"] = db::boolean(r[7]);
            c["trial_default"] = db::boolean(r[9]);
        } else {
            c["unit"] = db::str(r[6]);
            c["default"] = limitValue(optInt64(r[8]));
            c["trial_default"] = limitValue(optInt64(r[10]));
        }
        out.push_back(c);
    }
    return out;
}

std::vector<Entry> parsePlanEntitlements(db::Tx& tx, const json& values) {
    if (!values.is_object()) throw invalid("entitlements", "object_required");
    std::map<std::string, std::string> kinds;
    for (const auto& r : tx.q("SELECT key, kind FROM plan_capabilities")) kinds[db::str(r[0])] = db::str(r[1]);
    std::vector<Entry> out;
    for (auto it = values.begin(); it != values.end(); ++it) {
        auto k = kinds.find(it.key());
        if (k == kinds.end()) throw invalid("entitlements." + it.key(), "unknown_capability");
        Entry e;
        e.key = it.key();
        if (k->second == "feature") {
            if (!it.value().is_boolean()) throw invalid("entitlements." + it.key(), "boolean_required");
            e.enabled = it.value().get<bool>();
        } else if (!it.value().is_null()) {
            if (!it.value().is_number_integer()) throw invalid("entitlements." + it.key(), "integer_or_null_required");
            int64_t v = it.value().get<int64_t>();
            int64_t min = (e.key == key::kUsers || e.key == key::kWarehouses) ? 1 : 0;
            if (v < min || v > 1000000) throw invalid("entitlements." + it.key(), "out_of_range");
            e.limit = v;
        }
        out.push_back(e);
    }
    return out;
}

void savePlanEntitlements(db::Tx& tx, const std::string& planId, const std::vector<Entry>& entries, bool replaceAll) {
    if (replaceAll) {
        std::string keys = "{";
        for (size_t i = 0; i < entries.size(); ++i) keys += (i ? "," : "") + entries[i].key;
        keys += "}";
        tx.q("DELETE FROM plan_entitlements WHERE plan_id = $1 AND NOT (key = ANY($2::text[]))", planId, keys);
    }
    for (const auto& e : entries)
        tx.q("INSERT INTO plan_entitlements (plan_id, key, enabled, limit_value) VALUES ($1, $2, $3, $4) "
             "ON CONFLICT (plan_id, key) DO UPDATE SET enabled = EXCLUDED.enabled, limit_value = EXCLUDED.limit_value",
             planId, e.key, e.enabled, e.limit);
}

json planEntitlementsJson(db::Tx& tx, const std::string& planId) {
    json out = json::object();
    for (const auto& r : tx.q("SELECT e.key, c.kind, e.enabled, e.limit_value FROM plan_entitlements e "
                              "JOIN plan_capabilities c ON c.key = e.key WHERE e.plan_id = $1 ORDER BY c.sort_order",
                              planId))
        out[db::str(r[0])] = db::str(r[1]) == "feature" ? json(db::boolean(r[2])) : limitValue(optInt64(r[3]));
    return out;
}

namespace {

json ruleJson(const pqxx::row& r) {
    return {{"id", db::str(r[0])},
            {"priority", r[1].as<int>()},
            {"active", db::boolean(r[2])},
            {"audience", db::str(r[3])},
            {"country_code", r[4].is_null() ? json(nullptr) : json(db::str(r[4]))},
            {"plan_id", db::str(r[5])},
            {"plan_code", db::str(r[6])},
            {"mode", db::str(r[7])},
            {"duration_days", r[8].is_null() ? json(nullptr) : json(r[8].as<int>())},
            {"description", db::str(r[9])},
            {"created_at", db::str(r[10])},
            {"updated_at", db::str(r[11])}};
}
constexpr const char* kRuleSelect =
    "SELECT r.id, r.priority, r.active, r.audience, r.country_code, r.plan_id, p.code, r.mode, r.duration_days, "
    "r.description, nh_iso(r.created_at), nh_iso(r.updated_at) FROM plan_assignment_rules r JOIN plans p ON p.id = r.plan_id ";

std::optional<std::string> countryParam(const json& b) {
    if (!b.contains("country_code") || b["country_code"].is_null()) return std::nullopt;
    std::string c = in::reqStr(b, "country_code", 2);
    for (auto& ch : c) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (c.size() != 2 || !std::isupper(static_cast<unsigned char>(c[0])) || !std::isupper(static_cast<unsigned char>(c[1])))
        throw invalid("country_code", "invalid_country");
    return c;
}

}  // namespace

void registerRoutes(App& app, Env& env) {
    // Rights of the current establishment: only what the plan includes (rule: a feature
    // outside the plan is never shown, not even as "not included").
    app.get("/api/v1/entitlements", {nh::Access::User}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            Snapshot s = load(tx, ctx.actor.establishmentId, ctx.actor.establishmentKind);
            json j = toJson(s, usage(tx, ctx.actor.establishmentId));
            json labels = json::object();
            auto enabled = s.enabled();
            for (const auto& c : catalogJson(tx)) {
                std::string k = c["key"];
                if (c["kind"] == "limit" || enabled.count(k))
                    labels[k] = {{"kind", c["kind"]}, {"label_fr", c["label_fr"]}, {"label_en", c["label_en"]},
                                 {"description_fr", c["description_fr"]}, {"description_en", c["description_en"]}};
            }
            j["labels"] = labels;
            j["version"] = version(ctx.actor.role, s);
            return j;
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- console: catalogue
    app.get("/api/v1/admin/capabilities", {nh::Access::Admin}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json { return {{"items", catalogJson(tx)}}; });
        ctx.json(200, out);
    });

    // Labels only: the list of capabilities follows the server's code.
    app.patch("/api/v1/admin/capabilities/:key", {nh::Access::Admin, "", false, {"superadmin"}}, [&env](Ctx& ctx) {
        std::string k = ctx.param("key");
        if (k.size() > 62) throw notFound("capability");
        const auto& b = ctx.body();
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            tx.one("SELECT 1 FROM plan_capabilities WHERE key = $1", k);
            tx.q("UPDATE plan_capabilities SET label_fr = COALESCE($2, label_fr), label_en = COALESCE($3, label_en), "
                 "description_fr = COALESCE($4, description_fr), description_en = COALESCE($5, description_en) WHERE key = $1",
                 k, in::optStr(b, "label_fr", 80), in::optStr(b, "label_en", 80), in::optStr(b, "description_fr", 400),
                 in::optStr(b, "description_en", 400));
            auditSystem(tx, std::nullopt, "admin.capability_updated", "plan_capability", k, b, "admin", ctx.actor.adminId);
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- console: values per plan
    app.get("/api/v1/admin/plans/:id/entitlements", {nh::Access::Admin}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "plan");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            tx.one("SELECT 1 FROM plans WHERE id = $1", id);
            return {{"values", planEntitlementsJson(tx, id)}, {"catalog", catalogJson(tx)}};
        });
        ctx.json(200, out);
    });

    // Replaces every value of the plan. A capability absent from "values" takes its
    // default (catalogue). Rights of subscribed establishments change at their next request.
    app.put("/api/v1/admin/plans/:id/entitlements", {nh::Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "plan");
        const auto& b = ctx.body();
        if (!b.contains("values")) throw invalid("values", "required");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            tx.one("SELECT 1 FROM plans WHERE id = $1 FOR UPDATE", id);
            json before = planEntitlementsJson(tx, id);
            auto entries = parsePlanEntitlements(tx, b["values"]);
            savePlanEntitlements(tx, id, entries, true);
            json after = planEntitlementsJson(tx, id);
            auditSystem(tx, std::nullopt, "admin.plan_entitlements_updated", "plan", id, {{"before", before}, {"after", after}},
                        "admin", ctx.actor.adminId);
            int subscribed = tx.one("SELECT count(*) FROM subscriptions WHERE plan_id = $1", id)[0].as<int>();
            return {{"values", after}, {"subscribed_establishments", subscribed}};
        });
        ctx.json(200, out);
    });

    // ---------------------------------------------------------------- console: assignment rules
    app.get("/api/v1/admin/plan-rules", {nh::Access::Admin}, [&env](Ctx& ctx) {
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            json items = json::array();
            for (const auto& r : tx.q(std::string(kRuleSelect) + "ORDER BY r.priority, r.created_at")) items.push_back(ruleJson(r));
            return {{"items", items}};
        });
        ctx.json(200, out);
    });

    app.post("/api/v1/admin/plan-rules", {nh::Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        const auto& b = ctx.body();
        std::string planId = in::reqUuid(b, "plan_id");
        std::string mode = in::reqEnum(b, "mode", {"trial", "grant"});
        std::string audience = in::optEnum(b, "audience", {"individual", "company", "any"}).value_or("any");
        auto country = countryParam(b);
        std::optional<int> days;
        if (b.contains("duration_days") && !b["duration_days"].is_null()) days = in::optInt(b, "duration_days", 30, 1, 3660);
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto p = tx.opt("SELECT kind, active FROM plans WHERE id = $1", planId);
            if (!p) throw notFound("plan");
            if (!db::boolean((*p)[1])) throw conflict("plan_inactive");
            if (mode == "grant" && !days && db::str((*p)[0]) != "free" && db::str((*p)[0]) != "custom")
                throw conflict("open_ended_grant_requires_free_plan",
                               "an open-ended grant is reserved to free or custom plans");
            auto r = tx.one("INSERT INTO plan_assignment_rules (priority, active, audience, country_code, plan_id, mode, "
                            "duration_days, description, created_by) VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9) RETURNING id",
                            in::optInt(b, "priority", 100, 0, 10000), in::optBool(b, "active", true), audience, country,
                            planId, mode, days, in::optStr(b, "description", 300).value_or(""), ctx.actor.adminId);
            std::string id = db::str(r[0]);
            auditSystem(tx, std::nullopt, "admin.plan_rule_created", "plan_assignment_rule", id, b, "admin", ctx.actor.adminId);
            return ruleJson(tx.one(std::string(kRuleSelect) + "WHERE r.id = $1", id));
        });
        ctx.json(201, out);
    });

    app.patch("/api/v1/admin/plan-rules/:id", {nh::Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "rule");
        const auto& b = ctx.body();
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            tx.one("SELECT 1 FROM plan_assignment_rules WHERE id = $1 FOR UPDATE", id);
            std::optional<int> priority, days;
            std::optional<bool> active;
            if (b.contains("priority")) priority = in::optInt(b, "priority", 100, 0, 10000);
            if (b.contains("active")) active = in::optBool(b, "active", true);
            bool clearDays = b.contains("duration_days") && b["duration_days"].is_null();
            if (b.contains("duration_days") && !clearDays) days = in::optInt(b, "duration_days", 30, 1, 3660);
            bool setCountry = b.contains("country_code");
            auto country = countryParam(b);
            tx.q("UPDATE plan_assignment_rules SET priority = COALESCE($2, priority), active = COALESCE($3, active), "
                 "audience = COALESCE($4, audience), plan_id = COALESCE($5, plan_id), mode = COALESCE($6, mode), "
                 "duration_days = CASE WHEN $7 THEN NULL ELSE COALESCE($8, duration_days) END, "
                 "country_code = CASE WHEN $9 THEN $10 ELSE country_code END, description = COALESCE($11, description) "
                 "WHERE id = $1",
                 id, priority, active, in::optEnum(b, "audience", {"individual", "company", "any"}), in::optUuid(b, "plan_id"),
                 in::optEnum(b, "mode", {"trial", "grant"}), clearDays, days, setCountry, country,
                 in::optStr(b, "description", 300));
            auto chk = tx.one("SELECT r.mode, r.duration_days IS NULL, p.kind, p.active FROM plan_assignment_rules r "
                              "JOIN plans p ON p.id = r.plan_id WHERE r.id = $1",
                              id);
            if (!db::boolean(chk[3])) throw conflict("plan_inactive");
            if (db::str(chk[0]) == "grant" && db::boolean(chk[1]) && db::str(chk[2]) != "free" && db::str(chk[2]) != "custom")
                throw conflict("open_ended_grant_requires_free_plan");
            auditSystem(tx, std::nullopt, "admin.plan_rule_updated", "plan_assignment_rule", id, b, "admin", ctx.actor.adminId);
            return ruleJson(tx.one(std::string(kRuleSelect) + "WHERE r.id = $1", id));
        });
        ctx.json(200, out);
    });

    app.del("/api/v1/admin/plan-rules/:id", {nh::Access::Admin, "", false, {"billing"}}, [&env](Ctx& ctx) {
        std::string id = in::uuidParam(ctx.param("id"), "rule");
        db::run(env.pool, std::nullopt, [&](db::Tx& tx) {
            auto r = tx.q("DELETE FROM plan_assignment_rules WHERE id = $1 RETURNING id", id);
            if (r.empty()) throw notFound("rule");
            auditSystem(tx, std::nullopt, "admin.plan_rule_deleted", "plan_assignment_rule", id, json::object(), "admin",
                        ctx.actor.adminId);
        });
        ctx.noContent();
    });

    // ---------------------------------------------------------------- console: one establishment
    // Rights and usage counts (aggregates, never content).
    app.get("/api/v1/admin/establishments/:id/entitlements", {nh::Access::Admin}, [&env](Ctx& ctx) {
        std::string est = in::uuidParam(ctx.param("id"), "establishment");
        json out = db::run(env.pool, std::nullopt, [&](db::Tx& tx) -> json {
            auto e = tx.one("SELECT kind FROM establishments WHERE id = $1", est);
            Snapshot s = load(tx, est, db::str(e[0]));
            json j = toJson(s, usage(tx, est));
            json all = json::object();
            for (const auto& [k, on] : s.features) all[k] = on;
            j["features_all"] = all;
            return j;
        });
        ctx.json(200, out);
    });
}

}  // namespace nh::entitlements
