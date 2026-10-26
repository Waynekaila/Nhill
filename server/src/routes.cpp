#include <regex>

#include "nhill/version.hpp"
#include "services/services.hpp"

namespace nh {

void registerAll(App& app, Env& env) {
    app.setAuthHooks({[&env](Ctx& ctx) { authenticateUser(env, ctx); },
                      [&env](Ctx& ctx) { authenticateAdmin(env, ctx); },
                      [&env](Ctx& ctx, const std::string& what) {
                          // Identifiers are removed from the path so that one fault gives one entry.
                          static const std::regex ids("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}|/[0-9]+(?=/|$)");
                          std::string route = std::regex_replace(ctx.req.path, ids, ":id");
                          std::optional<std::string> est, user;
                          if (!ctx.actor.establishmentId.empty()) est = ctx.actor.establishmentId;
                          if (!ctx.actor.userId.empty()) user = ctx.actor.userId;
                          recordErrorEvent(env.pool, "server", "error", ctx.req.method + " " + route, what,
                                           {{"request_id", ctx.actor.requestId}, {"status", ctx.res.status}}, est, user,
                                           nhill::kProductVersion, "server");
                      }});
    registerMetaRoutes(app, env);
    registerAuthRoutes(app, env);
    registerEstablishmentRoutes(app, env);
    registerMemberRoutes(app, env);
    registerSubscriptionRoutes(app, env);
    registerCatalogRoutes(app, env);
    registerPartyRoutes(app, env);
    registerStockRoutes(app, env);
    registerSalesRoutes(app, env);
    registerPurchaseRoutes(app, env);
    registerPaymentRoutes(app, env);
    registerExpenseRoutes(app, env);
    registerReportRoutes(app, env);
    registerActivityRoutes(app, env);
    registerAdminRoutes(app, env);
    registerConsoleRoutes(app, env);
    registerSupportRoutes(app, env);
    registerPosRoutes(app, env);
}

}  // namespace nh
