#include "services/common.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include "nhill/money.hpp"
#include "nhill/validation.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh {

// --------------------------------------------------------------------- Profiles

void Profiles::loadDir(const std::string& dir) {
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() != ".json") continue;
        std::ifstream in(e.path());
        std::stringstream ss;
        ss << in.rdbuf();
        json j = json::parse(ss.str());
        std::string code = j.at("code");
        int version = j.at("version");
        auto it = latest_.find(code);
        if (it == latest_.end() || it->second.at("version").get<int>() < version) {
            latest_[code] = j;
            sha_[code] = crypto::sha256Hex(j.dump());
        }
    }
    if (latest_.empty()) throw std::runtime_error("no invoicing profile found in " + dir);
}

void Profiles::syncToDb(db::Pool& pool) {
    db::run(pool, std::nullopt, [&](db::Tx& tx) {
        for (const auto& [code, j] : latest_) {
            int version = j.at("version");
            auto existing = tx.opt("SELECT content_sha256 FROM compliance_profiles WHERE code = $1 AND version = $2",
                                   code, version);
            if (existing) {
                if (db::str((*existing)[0]) != sha_[code])
                    throw std::runtime_error("profile " + code + " v" + std::to_string(version) +
                                             " changed without a version bump");
                continue;
            }
            std::optional<std::string> country;
            if (j.contains("country") && j["country"].is_string()) country = j["country"].get<std::string>();
            tx.q("INSERT INTO compliance_profiles (code, version, country_code, definition, content_sha256, "
                 "validation_note) VALUES ($1, $2, $3, $4::jsonb, $5, $6)",
                 code, version, country, j.dump(), sha_[code], j.value("validation_status", std::string()));
            log::info("compliance profile registered: " + code + " v" + std::to_string(version));
        }
    });
}

const json& Profiles::get(const std::string& code) const {
    auto it = latest_.find(code);
    if (it == latest_.end()) throw std::runtime_error("unknown profile " + code);
    return it->second;
}

std::string Profiles::codeForCountry(const std::string& country) const {
    for (const auto& [code, j] : latest_)
        if (j.contains("country") && j["country"].is_string() && j["country"] == country) return code;
    return "INTL";
}

// --------------------------------------------------------------------- I18n

void I18n::loadDir(const std::string& dir) {
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() != ".json") continue;
        std::ifstream in(e.path());
        std::stringstream ss;
        ss << in.rdbuf();
        dict_[e.path().stem().string()] = json::parse(ss.str());
    }
}

std::string I18n::t(const std::string& lang, const std::string& key) const {
    auto it = dict_.find(lang);
    if (it != dict_.end() && it->second.contains(key)) return it->second[key];
    it = dict_.find("en");
    if (it != dict_.end() && it->second.contains(key)) return it->second[key];
    return key;
}

// --------------------------------------------------------------------- input

namespace in {

namespace v = nhill::validation;

std::optional<std::string> optStr(const json& j, const std::string& key, size_t maxLen) {
    if (!j.contains(key) || j[key].is_null()) return std::nullopt;
    if (!j[key].is_string()) throw invalid(key, "must_be_string");
    std::string s = v::trim(j[key].get<std::string>());
    if (s.empty()) return std::nullopt;
    if (!v::isValidText(s, maxLen)) throw invalid(key, "invalid_text");
    return s;
}

std::string reqStr(const json& j, const std::string& key, size_t maxLen) {
    auto s = optStr(j, key, maxLen);
    if (!s) throw invalid(key, "required");
    return *s;
}

std::optional<std::string> optEmail(const json& j, const std::string& key) {
    auto s = optStr(j, key, 254);
    if (!s) return std::nullopt;
    if (!v::isValidEmail(*s)) throw invalid(key, "invalid_email");
    return v::normalizeEmail(*s);
}

std::string reqEmail(const json& j, const std::string& key) {
    auto s = optEmail(j, key);
    if (!s) throw invalid(key, "required");
    return *s;
}

std::optional<std::string> optPhone(const json& j, const std::string& key) {
    auto s = optStr(j, key, 32);
    if (!s) return std::nullopt;
    auto p = v::normalizePhone(*s);
    if (!p) throw invalid(key, "invalid_phone");
    return p;
}

bool isUuid(const std::string& s) {
    static const std::regex re("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    return std::regex_match(s, re);
}

std::string uuidParam(const std::string& value, const std::string& what) {
    if (!isUuid(value)) throw notFound(what);
    return value;
}

std::optional<std::string> optUuid(const json& j, const std::string& key) {
    if (!j.contains(key) || j[key].is_null()) return std::nullopt;
    if (!j[key].is_string() || !isUuid(j[key].get<std::string>())) throw invalid(key, "invalid_id");
    return j[key].get<std::string>();
}

std::string reqUuid(const json& j, const std::string& key) {
    auto s = optUuid(j, key);
    if (!s) throw invalid(key, "required");
    return *s;
}

std::optional<int64_t> optMoney(const json& j, const std::string& key, const std::string& currency) {
    if (!j.contains(key) || j[key].is_null()) return std::nullopt;
    // Amounts travel as decimal strings to avoid any floating point on the wire.
    if (!j[key].is_string()) throw invalid(key, "amount_must_be_decimal_string");
    auto v = nhill::parseMoney(j[key].get<std::string>(), minorUnits(currency));
    if (!v) throw invalid(key, "invalid_amount");
    if (*v < 0) throw invalid(key, "negative_amount");
    return v;
}

int64_t reqMoney(const json& j, const std::string& key, const std::string& currency, bool allowZero) {
    auto v = optMoney(j, key, currency);
    if (!v) throw invalid(key, "required");
    if (!allowZero && *v == 0) throw invalid(key, "must_be_positive");
    return *v;
}

int64_t reqQty(const json& j, const std::string& key) {
    if (!j.contains(key) || !j[key].is_string()) throw invalid(key, "quantity_must_be_decimal_string");
    auto v = nhill::parseQuantity(j[key].get<std::string>());
    if (!v) throw invalid(key, "invalid_quantity");
    return *v;
}

bool isDate(const std::string& s) {
    static const std::regex re("^(19|20|21)[0-9]{2}-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])$");
    return std::regex_match(s, re);
}

std::optional<std::string> optDate(const json& j, const std::string& key) {
    auto s = optStr(j, key, 10);
    if (!s) return std::nullopt;
    if (!isDate(*s)) throw invalid(key, "invalid_date");
    return s;
}

bool optBool(const json& j, const std::string& key, bool def) {
    if (!j.contains(key) || j[key].is_null()) return def;
    if (!j[key].is_boolean()) throw invalid(key, "must_be_boolean");
    return j[key].get<bool>();
}

int optInt(const json& j, const std::string& key, int def, int min, int max) {
    if (!j.contains(key) || j[key].is_null()) return def;
    if (!j[key].is_number_integer()) throw invalid(key, "must_be_integer");
    auto v = j[key].get<int64_t>();
    if (v < min || v > max) throw invalid(key, "out_of_range");
    return static_cast<int>(v);
}

std::optional<std::string> optEnum(const json& j, const std::string& key, std::initializer_list<const char*> allowed) {
    auto s = optStr(j, key, 40);
    if (!s) return std::nullopt;
    for (const char* a : allowed)
        if (*s == a) return s;
    throw invalid(key, "not_allowed");
}

std::string reqEnum(const json& j, const std::string& key, std::initializer_list<const char*> allowed) {
    auto s = optEnum(j, key, allowed);
    if (!s) throw invalid(key, "required");
    return *s;
}

std::string reqCurrency(const json& j, const std::string& key) {
    auto s = reqStr(j, key, 3);
    if (!nhill::isValidCurrencyCode(s)) throw invalid(key, "invalid_currency");
    return s;
}

}  // namespace in

// --------------------------------------------------------------------- output

int minorUnits(const std::string& currency) {
    int m = nhill::currencyMinorUnits(currency);
    return m < 0 ? 2 : m;
}

std::string money(int64_t minor, const std::string& currency) {
    return nhill::toPlainDecimal(minor, minorUnits(currency));
}

std::string qty(int64_t milli) {
    std::string s = nhill::toPlainDecimal(milli, 3);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

Page page(const Ctx& ctx) {
    Page p;
    if (auto l = ctx.query("limit")) {
        try {
            p.limit = std::clamp(std::stoi(*l), 1, 200);
        } catch (...) {
        }
    }
    if (auto o = ctx.query("offset")) {
        try {
            p.offset = std::max(0, std::stoi(*o));
        } catch (...) {
        }
    }
    return p;
}

// --------------------------------------------------------------------- side effects

void audit(db::Tx& tx, const Actor& a, const std::string& action, const std::string& entity,
           const std::string& entityId, const json& details) {
    std::optional<std::string> est = a.establishmentId.empty() ? std::nullopt : std::optional(a.establishmentId);
    std::string type = !a.adminId.empty() ? "admin" : !a.userId.empty() ? "user" : "anonymous";
    std::optional<std::string> actorId = !a.adminId.empty() ? std::optional(a.adminId)
                                         : !a.userId.empty() ? std::optional(a.userId)
                                                             : std::nullopt;
    tx.q("INSERT INTO audit_log (establishment_id, actor_type, actor_id, action, entity, entity_id, details, ip) "
         "VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb, $8)",
         est, type, actorId, action, entity, entityId, details.dump(), a.ip);
}

void auditSystem(db::Tx& tx, const std::optional<std::string>& establishmentId, const std::string& action,
                 const std::string& entity, const std::string& entityId, const json& details,
                 const std::string& actorType, const std::optional<std::string>& actorId) {
    tx.q("INSERT INTO audit_log (establishment_id, actor_type, actor_id, action, entity, entity_id, details) "
         "VALUES ($1, $2, $3, $4, $5, $6, $7::jsonb)",
         establishmentId, actorType, actorId, action, entity, entityId, details.dump());
}

void notify(db::Tx& tx, const std::string& establishmentId, const std::optional<std::string>& userId,
            const std::string& kind, const json& params) {
    tx.q("INSERT INTO notifications (establishment_id, user_id, kind, params) VALUES ($1, $2, $3, $4::jsonb)",
         establishmentId, userId, kind, params.dump());
}

void emailManagers(db::Tx& tx, const std::string& establishmentId, const std::string& tmpl, const json& extra) {
    json params = extra;
    if (!params.contains("establishment")) {
        auto e = tx.opt("SELECT trade_name FROM establishments WHERE id = $1", establishmentId);
        if (e) params["establishment"] = db::str((*e)[0]);
    }
    auto r = tx.q(
        "SELECT u.email, u.preferred_language FROM memberships m JOIN users u ON u.id = m.user_id "
        "WHERE m.establishment_id = $1 AND m.status = 'active' AND u.status = 'active' AND m.role IN ('owner','admin')",
        establishmentId);
    for (const auto& row : r) {
        tx.q("INSERT INTO email_outbox (to_address, template, language, params) VALUES ($1, $2, $3, $4::jsonb)",
             db::str(row[0]), tmpl, db::str(row[1]), params.dump());
    }
}

void emailUser(db::Tx& tx, const std::string& userId, const std::string& tmpl, const json& params) {
    tx.q("INSERT INTO email_outbox (to_address, template, language, params) "
         "SELECT email, $2, preferred_language, $3::jsonb FROM users WHERE id = $1 AND status = 'active'",
         userId, tmpl, params.dump());
}

Result idempotent(db::Tx& tx, Ctx& ctx, const std::function<Result()>& fn) {
    std::string key = ctx.header("Idempotency-Key");
    if (key.size() < 8 || key.size() > 100)
        throw badRequest("idempotency_key_required", "write operations require an Idempotency-Key header");
    const std::string hash =
        crypto::sha256Hex(ctx.req.method + "\n" + ctx.req.path + "\n" + ctx.req.body);
    const std::string& est = ctx.actor.establishmentId;
    auto inserted = tx.q(
        "INSERT INTO idempotency_keys (establishment_id, idem_key, user_id, request_hash) "
        "VALUES ($1, $2, $3, decode($4, 'hex')) ON CONFLICT DO NOTHING RETURNING 1",
        est, key, ctx.actor.userId, hash);
    if (inserted.empty()) {
        // The key exists. If another request with this key was in flight, the
        // INSERT above waited for it to commit, so the stored response is final.
        auto row = tx.one(
            "SELECT encode(request_hash, 'hex'), status_code, response_body FROM idempotency_keys "
            "WHERE establishment_id = $1 AND idem_key = $2",
            est, key);
        if (db::str(row[0]) != hash) throw conflict("idempotency_key_reused", "key already used for another request");
        if (row[1].is_null()) throw conflict("request_in_progress");
        ctx.res.set_header("Idempotent-Replayed", "true");
        return {row[1].as<int>(), json::parse(db::str(row[2]))};
    }
    Result r = fn();
    tx.q("UPDATE idempotency_keys SET status_code = $3, response_body = $4 WHERE establishment_id = $1 AND idem_key = $2",
         est, key, r.first, r.second.dump());
    return r;
}

void throttle(db::Pool& pool, const std::string& bucket, int limit, int windowMinutes) {
    int hits = db::run(pool, std::nullopt, [&](db::Tx& tx) {
        auto row = tx.one(
            "INSERT INTO auth_throttle (bucket, window_start, hits) VALUES ($1, nh_window($2), 1) "
            "ON CONFLICT (bucket, window_start) DO UPDATE SET hits = auth_throttle.hits + 1 RETURNING hits",
            bucket, windowMinutes);
        return row[0].as<int>();
    });
    if (hits > limit) throw tooMany();
}

int setting(db::Tx& tx, const std::string& key, int def) {
    return tx.one("SELECT nh_setting_int($1, $2)", key, def)[0].as<int>();
}

}  // namespace nh
