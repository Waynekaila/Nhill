#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "config.hpp"
#include "db.hpp"
#include "errors.hpp"
#include "http/app.hpp"

namespace nh {

using json = nlohmann::json;

// Country invoicing profiles (server/profiles/*.json), versioned.
class Profiles {
public:
    void loadDir(const std::string& dir);
    // Upserts every loaded profile into compliance_profiles. Refuses to start
    // if a (code, version) pair already exists with different content.
    void syncToDb(db::Pool& pool);
    const json& get(const std::string& code) const;   // latest version
    bool has(const std::string& code) const { return latest_.count(code) > 0; }
    std::string codeForCountry(const std::string& country) const;
    const std::map<std::string, json>& all() const { return latest_; }

private:
    std::map<std::string, json> latest_;
    std::map<std::string, std::string> sha_;
};

// Server-side strings for generated documents (PDF, e-mails).
class I18n {
public:
    void loadDir(const std::string& dir);
    std::string t(const std::string& lang, const std::string& key) const;

private:
    std::map<std::string, json> dict_;
};

struct Env {
    Config& cfg;
    db::Pool& pool;
    Profiles& profiles;
    I18n& i18n;
};

// --------------------------------------------------------------------- input parsing
namespace in {
std::string reqStr(const json& j, const std::string& key, size_t maxLen = 200);
std::optional<std::string> optStr(const json& j, const std::string& key, size_t maxLen = 200);
std::string reqEmail(const json& j, const std::string& key);
std::optional<std::string> optEmail(const json& j, const std::string& key);
std::optional<std::string> optPhone(const json& j, const std::string& key);
std::string reqUuid(const json& j, const std::string& key);
std::optional<std::string> optUuid(const json& j, const std::string& key);
bool isUuid(const std::string& s);
std::string uuidParam(const std::string& value, const std::string& what = "resource");
int64_t reqMoney(const json& j, const std::string& key, const std::string& currency, bool allowZero = true);
std::optional<int64_t> optMoney(const json& j, const std::string& key, const std::string& currency);
int64_t reqQty(const json& j, const std::string& key);
std::optional<std::string> optDate(const json& j, const std::string& key);
bool optBool(const json& j, const std::string& key, bool def);
int optInt(const json& j, const std::string& key, int def, int min, int max);
std::string reqEnum(const json& j, const std::string& key, std::initializer_list<const char*> allowed);
std::optional<std::string> optEnum(const json& j, const std::string& key, std::initializer_list<const char*> allowed);
std::string reqCurrency(const json& j, const std::string& key);
bool isDate(const std::string& s);
}  // namespace in

// --------------------------------------------------------------------- output helpers
std::string money(int64_t minor, const std::string& currency);   // "1234.50"
std::string qty(int64_t milli);                                  // "2.5"
int minorUnits(const std::string& currency);

struct Page {
    int limit = 50;
    int offset = 0;
};
Page page(const Ctx& ctx);

// --------------------------------------------------------------------- side effects
void audit(db::Tx& tx, const Actor& a, const std::string& action, const std::string& entity,
           const std::string& entityId, const json& details = json::object());
void auditSystem(db::Tx& tx, const std::optional<std::string>& establishmentId, const std::string& action,
                 const std::string& entity, const std::string& entityId, const json& details = json::object(),
                 const std::string& actorType = "system", const std::optional<std::string>& actorId = std::nullopt);
void notify(db::Tx& tx, const std::string& establishmentId, const std::optional<std::string>& userId,
            const std::string& kind, const json& params = json::object());
// Queues an e-mail to every active owner/admin of the establishment.
void emailManagers(db::Tx& tx, const std::string& establishmentId, const std::string& tmpl, const json& params);

// Idempotent execution of a write. The key and the response are stored in the
// same transaction as the operation itself. `fn` returns {status, body}.
using Result = std::pair<int, json>;
Result idempotent(db::Tx& tx, Ctx& ctx, const std::function<Result()>& fn);

// Throttling in a dedicated, always-committed transaction.
// Throws 429 when `bucket` has more than `limit` hits in the current window.
void throttle(db::Pool& pool, const std::string& bucket, int limit, int windowMinutes = 15);

// Reads an integer platform setting.
int setting(db::Tx& tx, const std::string& key, int def);

}  // namespace nh
