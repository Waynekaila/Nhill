#pragma once
// Rights of an establishment, computed in one place (3.0.0):
//   access   — full, read_only or none (nh_subscription_access, dates compared with now);
//   features — plan capabilities of kind "feature" that are enabled;
//   limits   — plan capabilities of kind "limit" (nullopt = unlimited) and their usage.
// A user's effective rights are the permissions of their role reduced to the features of
// the plan (App::allowed). The same rights decide what the applications display and what
// the API accepts: a feature outside the plan is hidden and refused, data included.
#include <map>
#include <optional>
#include <set>
#include <string>

#include "services/common.hpp"

namespace nh::entitlements {

namespace key {
inline constexpr const char* kInvoices = "sales.invoices";
inline constexpr const char* kQuotes = "sales.quotes";
inline constexpr const char* kPos = "pos";
inline constexpr const char* kPurchases = "purchases";
inline constexpr const char* kExpenses = "expenses";
inline constexpr const char* kMultiWarehouse = "stock.multi_warehouse";
inline constexpr const char* kMultiCurrency = "multi_currency";
inline constexpr const char* kCatalogImport = "catalog.import";
inline constexpr const char* kAdvancedReports = "reports.advanced";
inline constexpr const char* kUsers = "users.max";
inline constexpr const char* kRegisters = "pos.registers.max";
inline constexpr const char* kWarehouses = "stock.warehouses.max";
inline constexpr const char* kProducts = "catalog.products.max";
inline constexpr const char* kDocumentsMonth = "documents.month.max";
inline constexpr const char* kStorageMb = "storage.mb.max";
}  // namespace key

struct Access {
    std::string state = "read_only";   // full | read_only | none
    std::string reason = "no_subscription";
    std::optional<std::string> until;  // end of the current right (period or grace), ISO 8601
    std::optional<std::string> planId; // effective plan (pending downgrade applied when due)
    bool trial = false;
    std::string status;                // subscription status, empty without subscription
};
Access access(db::Tx& tx, const std::string& establishmentId);

struct Snapshot {
    Access access;
    std::optional<std::string> planId, planCode, planNameFr, planNameEn, planKind;
    std::map<std::string, bool> features;                  // every feature of the catalogue
    std::map<std::string, std::optional<int64_t>> limits;  // every limit of the catalogue
    std::set<std::string> enabled() const;
};
Snapshot load(db::Tx& tx, const std::string& establishmentId, const std::string& establishmentKind);

struct Usage {
    int64_t users = 0, registers = 0, warehouses = 0, products = 0, documentsMonth = 0, storageBytes = 0;
};
Usage usage(db::Tx& tx, const std::string& establishmentId);
// Amount counted against `limitKey` (storage in bytes).
int64_t used(const Usage& u, const std::string& limitKey);

// {access, plan, trial, features, limits[, usage]} for the applications and the console.
json toJson(const Snapshot& s, const std::optional<Usage>& u = std::nullopt);

// Version of the rights a client displays: changes when the role, the access, the plan or
// a feature changes (sent in X-Nhill-Rights; the applications reload /me when it differs).
std::string version(const std::string& role, const Snapshot& s);

// Fills actor.access / accessReason / features / rightsVersion.
void fillActor(db::Tx& tx, Actor& a);

// Throws 409 when `adding` more units would exceed the limit (storage: bytes).
// Codes user_limit_reached (users) and plan_register_limit (registers) as in 2.x,
// plan_limit_reached otherwise; details {limit_key, limit, used}.
void requireCapacity(db::Tx& tx, const std::string& establishmentId, const std::string& establishmentKind,
                     const std::string& limitKey, int64_t adding = 1);
inline void requireCapacity(db::Tx& tx, const Actor& a, const std::string& limitKey, int64_t adding = 1) {
    requireCapacity(tx, a.establishmentId, a.establishmentKind, limitKey, adding);
}
// Throws 403 feature_not_included when the plan lacks `featureKey`.
void requireFeature(const Actor& a, const std::string& featureKey);

// A document in a currency other than the main currency needs multi_currency.
void requireCurrency(db::Tx& tx, const Actor& a, const std::string& currency);

// Sales documents belong to a feature through their class: quote and order (sales.quotes),
// invoice and credit_note (sales.invoices), receipt and pos_credit_note — a credit note made
// at the till — (pos).
std::string docClass(const std::string& docType, bool posSession);
std::string featureOfDocClass(const std::string& docClass);
bool docVisible(const Actor& a, const std::string& docType, bool posSession);
// SQL expression: true when the document `alias` is visible; `param` is the text[] of docTypesArray.
std::string docVisibleSql(const std::string& alias, int param);
// SQL array literal of the visible document classes ("{invoice,credit_note}").
std::string docTypesArray(const Actor& a);
// SQL condition keeping the stock movements of visible features only: transfers need
// several warehouses ($multi), purchase receipts need purchases ($purchases), sales
// movements need the type of their document ($types). Parameters are numbered by the caller.
std::string movementFilterSql(const std::string& alias, int multiParam, int purchasesParam, int typesParam);

// SQL condition keeping the payments of visible features only ($purchases, $expenses,
// document types $types).
std::string paymentFilterSql(const std::string& alias, int purchasesParam, int expensesParam, int typesParam);

// Assignment rule for a new establishment (first active match by priority).
struct Assignment {
    std::string ruleId, planId, planCode, mode;   // mode: trial | grant
    std::optional<int> days;                       // nullopt + grant = open-ended
};
std::optional<Assignment> matchRule(db::Tx& tx, const std::string& establishmentKind, const std::string& country,
                                    bool trialAvailable);

// Capability catalogue (labels, kinds, units) for the applications and the console.
json catalogJson(db::Tx& tx);

// Validated {key: true|false|number|null} map for a plan; throws 422 on unknown keys or
// values of the wrong kind. Returns the rows to upsert (key, enabled, limit).
struct Entry {
    std::string key;
    std::optional<bool> enabled;
    std::optional<int64_t> limit;
};
std::vector<Entry> parsePlanEntitlements(db::Tx& tx, const json& values);
void savePlanEntitlements(db::Tx& tx, const std::string& planId, const std::vector<Entry>& entries, bool replaceAll);
json planEntitlementsJson(db::Tx& tx, const std::string& planId);

void registerRoutes(App& app, Env& env);

}  // namespace nh::entitlements
