#pragma once

#include "http/app.hpp"
#include "services/common.hpp"

namespace nh {

// Route registration, one function per domain.
void registerMetaRoutes(App& app, Env& env);
void registerAuthRoutes(App& app, Env& env);
void registerEstablishmentRoutes(App& app, Env& env);
void registerMemberRoutes(App& app, Env& env);
void registerSubscriptionRoutes(App& app, Env& env);
void registerCatalogRoutes(App& app, Env& env);
void registerPartyRoutes(App& app, Env& env);
void registerStockRoutes(App& app, Env& env);
void registerSalesRoutes(App& app, Env& env);
void registerPurchaseRoutes(App& app, Env& env);
void registerPaymentRoutes(App& app, Env& env);
void registerExpenseRoutes(App& app, Env& env);
void registerReportRoutes(App& app, Env& env);
void registerActivityRoutes(App& app, Env& env);
void registerAdminRoutes(App& app, Env& env);
void registerConsoleRoutes(App& app, Env& env);
void registerSupportRoutes(App& app, Env& env);
void registerPosRoutes(App& app, Env& env);

// Technical error journal (support.cpp). Best effort: never throws.
void recordErrorEvent(db::Pool& pool, const std::string& source, const std::string& level, const std::string& code,
                      const std::string& message, const json& context, const std::optional<std::string>& establishmentId,
                      const std::optional<std::string>& userId, const std::string& appVersion,
                      const std::string& platform);

// Installs every route and the authentication hooks.
void registerAll(App& app, Env& env);

// Authentication hooks (auth.cpp / admin.cpp).
void authenticateUser(Env& env, Ctx& ctx);
void authenticateAdmin(Env& env, Ctx& ctx);

// Session issuing (auth.cpp) — shared by login, join and activation flows.
json createSession(Env& env, db::Tx& tx, const std::string& userId, const std::string& establishmentId,
                   const Actor& a);
// Password login restricted to one establishment (used by the join flow).
json loginForEstablishment(Env& env, Ctx& ctx, const std::string& email, const std::string& password,
                           const std::string& establishmentId);

// Establishment codes (codes.cpp).
namespace codes {
struct Current {
    std::string display;
    std::string createdAt;
    std::string expiresAt;
};
// Issues a fresh active code for the establishment. The caller must have
// retired the previous active code in the same transaction.
std::string issue(Env& env, db::Tx& tx, const std::string& establishmentId, const std::string& byType,
                  const std::optional<std::string>& byId);
// Retires the active code (superseded/revoked) and issues a new one, atomically.
std::string rotate(Env& env, db::Tx& tx, const std::string& establishmentId, const std::string& endStatus,
                   const std::string& reason, const std::string& byType, const std::optional<std::string>& byId);
std::optional<Current> current(Env& env, db::Tx& tx, const std::string& establishmentId);
// Looks up an active, unexpired code of an active establishment.
std::optional<std::string> resolve(Env& env, db::Tx& tx, const std::string& canonicalCode);
// Test hook: replaces the random generator (nullptr restores it).
void setGeneratorForTests(std::function<std::string()> gen);
}  // namespace codes

// Subscriptions (subscriptions.cpp).
namespace subscriptions {
constexpr const char* kPlanCols =
    "id, code, name_fr, name_en, description_fr, description_en, billing_period, price_minor, currency, tax_rate_bp, "
    "max_users, audience, features, active, sort_order, tier, max_registers, max_warehouses";
json planJson(db::Tx& tx, const pqxx::row& r, bool activePricesOnly);

struct Quote {
    std::string kind;   // new | renewal | upgrade | downgrade
    std::string priceId, planId, planCode, planName, currency, startsAt;
    std::string promoId, promoCode, promoDescription;
    int durationMonths = 0;
    int64_t subtotal = 0, discount = 0, credit = 0, tax = 0, total = 0;
    std::vector<std::string> notes;
};
// Computes an order for `priceId` (forOrder = lock the rows involved).
Quote quote(db::Tx& tx, const std::string& establishmentId, const std::string& establishmentKind,
            const std::string& priceId, const std::optional<std::string>& promoCode, bool forOrder);
json quoteJson(const Quote& q);
// Creates a pending order (replacing any open one) and reserves the promo code.
json createOrder(Env& env, db::Tx& tx, const Actor& actor, const std::string& priceId,
                 const std::optional<std::string>& promoCode, const std::string& createdByType);
json orderJson(db::Tx& tx, const std::string& orderId);
void releasePromo(db::Tx& tx, const std::string& orderId);
// Marks an order paid and extends the subscription accordingly. Idempotent:
// an order already paid is left untouched (returns false).
bool applyPaidOrder(Env& env, db::Tx& tx, const std::string& orderId, const std::string& actorType,
                    const std::optional<std::string>& actorId, const std::string& providerReference);
bool failOrder(db::Tx& tx, const std::string& orderId, const std::string& reason, const std::string& actorType);
json view(db::Tx& tx, const std::string& establishmentId);
}  // namespace subscriptions

// Scheduled jobs (jobs.cpp).
namespace jobs {
struct Stats {
    int codesRotated = 0;
    int codeRotationFailures = 0;
    int remindersSent = 0;
    int enteredGrace = 0;
    int suspended = 0;
    int emailsSent = 0;
    int planChanges = 0;
    int ordersExpired = 0;
};
Stats rotateCodes(Env& env);
Stats subscriptionLifecycle(Env& env);
Stats dispatchEmails(Env& env);
Stats runAll(Env& env);
}  // namespace jobs

// Logo validation (establishments.cpp): returns {mime, width, height} or throws 422.
struct ImageInfo {
    std::string mime;
    int width = 0;
    int height = 0;
};
ImageInfo validateImage(const std::string& bytes, const std::string& declaredMime);

}  // namespace nh
