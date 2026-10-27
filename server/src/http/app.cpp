#include "http/app.hpp"

#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <cstdlib>
#include <map>

#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh {

// --------------------------------------------------------------------- Ctx

const json& Ctx::body() {
    if (!body_) {
        if (req.body.empty()) {
            body_ = nlohmann::json::object();
        } else {
            try {
                body_ = nlohmann::json::parse(req.body);
            } catch (const std::exception&) {
                throw badRequest("malformed_json", "request body is not valid JSON");
            }
            if (!body_->is_object()) throw badRequest("malformed_json", "request body must be a JSON object");
        }
    }
    return *body_;
}

std::string Ctx::param(const std::string& name) const {
    auto it = req.path_params.find(name);
    return it == req.path_params.end() ? std::string() : it->second;
}

std::optional<std::string> Ctx::query(const std::string& name) const {
    if (!req.has_param(name)) return std::nullopt;
    return req.get_param_value(name);
}

std::string Ctx::header(const std::string& name) const { return req.get_header_value(name); }

void Ctx::json(int status, const nlohmann::json& j) {
    res.status = status;
    res.set_content(j.dump(), "application/json; charset=utf-8");
    responded_ = true;
}

void Ctx::file(const std::string& content, const std::string& mime, const std::string& filename, bool inlineDisp) {
    res.status = 200;
    res.set_header("Content-Disposition",
                   std::string(inlineDisp ? "inline" : "attachment") + "; filename=\"" + filename + "\"");
    res.set_content(content, mime);
    responded_ = true;
}

void Ctx::noContent() {
    res.status = 204;
    responded_ = true;
}

// --------------------------------------------------------------------- errors

namespace {

// Maps constraint names to stable API error codes.
std::string constraintCode(const std::string& msg) {
    static const std::map<std::string, std::string> known = {
        {"establishment_identifiers_country_code_kind_normalized_value_key", "establishment_exists"},
        {"users_email_key", "email_taken"},
        {"products_establishment_id_sku_key", "sku_taken"},
        {"products_barcode", "barcode_taken"},
        {"tax_rates_establishment_id_code_key", "tax_code_taken"},
        {"product_categories_name", "category_exists"},
        {"expense_categories_name", "category_exists"},
        {"memberships_establishment_id_user_id_key", "already_member"},
        {"invitations_pending", "invitation_pending"},
        {"plans_code_key", "plan_code_taken"},
        {"exchange_rates_establishment_id_from_currency_to_currency_rate_date_key", "rate_exists"},
        {"admin_users_email_key", "email_taken"},
    };
    for (const auto& [name, code] : known)
        if (msg.find(name) != std::string::npos) return code;
    return "conflict";
}

}  // namespace

void writeError(Ctx& ctx, const std::exception& e) {
    nlohmann::json err;
    int status = 500;
    if (auto* api = dynamic_cast<const ApiError*>(&e)) {
        status = api->status;
        err = {{"code", api->code}, {"message", api->what()}};
        if (!api->details.is_null()) err["details"] = api->details;
    } else if (auto* sql = dynamic_cast<const pqxx::sql_error*>(&e)) {
        const std::string state = sql->sqlstate();
        const std::string msg = sql->what();
        if (state == "NH001") {
            status = 409;
            err = {{"code", "insufficient_stock"}, {"message", "insufficient stock"}};
        } else if (state == "NH002") {
            status = 409;
            err = {{"code", "document_immutable"}, {"message", "issued documents cannot be modified"}};
        } else if (state == "NH003" || state == "NH004") {
            status = 422;
            err = {{"code", state == "NH003" ? "invalid_reference" : "product_not_stocked"},
                   {"message", "invalid stock operation"}};
        } else if (state == "23505") {
            status = 409;
            err = {{"code", constraintCode(msg)}, {"message", "duplicate value"}};
        } else if (state == "23503") {
            status = 422;
            err = {{"code", "invalid_reference"}, {"message", "referenced record does not exist"}};
        } else if (state == "23514" || state == "22P02" || state == "22003" || state == "22007" ||
                   state == "22008") {
            status = 422;
            err = {{"code", "invalid_value"}, {"message", "value rejected by validation rules"}};
        } else if (state == "42501") {
            // Includes row-level security violations: never reveal more.
            status = 404;
            err = {{"code", "not_found"}, {"message", "resource not found"}};
        } else if (state == "55P03" || state == "57014") {
            status = 503;
            err = {{"code", "busy"}, {"message", "the server is busy, retry"}};
        } else {
            log::error("[" + ctx.actor.requestId + "] sql error " + state + ": " + msg);
            err = {{"code", "internal_error"}, {"message", "internal error"}};
        }
    } else {
        log::error("[" + ctx.actor.requestId + "] unhandled: " + std::string(e.what()));
        err = {{"code", "internal_error"}, {"message", "internal error"}};
    }
    err["request_id"] = ctx.actor.requestId;
    ctx.json(status, {{"error", err}});
}

// --------------------------------------------------------------------- permissions

bool App::roleHas(const std::string& role, const std::string& permission) {
    if (permission.empty()) return true;
    static const std::set<std::string> posAll = {"pos.sell", "pos.refund", "pos.void", "pos.cash", "pos.supervise",
                                                 "pos.manage", "pos.report"};
    static const std::map<std::string, std::set<std::string>> perms = [] {
        std::map<std::string, std::set<std::string>> m = {
            {"owner",
             {"settings.manage", "users.manage", "code.view", "code.rotate", "subscription.manage", "catalog.write",
              "parties.write", "sales.write", "sales.issue", "purchases.write", "stock.adjust", "expenses.write",
              "payments.write", "reports.read", "audit.read"}},
            {"admin",
             {"settings.manage", "users.manage", "code.view", "code.rotate", "subscription.manage", "catalog.write",
              "parties.write", "sales.write", "sales.issue", "purchases.write", "stock.adjust", "expenses.write",
              "payments.write", "reports.read", "audit.read"}},
            {"manager",
             {"code.view", "catalog.write", "parties.write", "sales.write", "sales.issue", "purchases.write",
              "stock.adjust", "expenses.write", "payments.write", "reports.read", "pos.sell", "pos.refund",
              "pos.supervise", "pos.report"}},
            {"sales", {"parties.write", "sales.write", "sales.issue", "payments.write", "pos.sell"}},
            {"accountant", {"expenses.write", "payments.write", "reports.read", "audit.read", "pos.report"}},
            {"viewer", {}},
            // Point of sale roles (Nhill POS).
            {"cash_manager", {"pos.sell", "pos.refund", "pos.void", "pos.cash", "pos.supervise", "pos.report"}},
            {"cashier", {"pos.sell"}},
        };
        for (const auto& r : {"owner", "admin"}) m[r].insert(posAll.begin(), posAll.end());
        return m;
    }();
    auto it = perms.find(role);
    return it != perms.end() && it->second.count(permission) > 0;
}

std::string App::permissionFeature(const std::string& permission) {
    if (permission.rfind("pos.", 0) == 0) return "pos";
    if (permission.rfind("purchases.", 0) == 0) return "purchases";
    if (permission.rfind("expenses.", 0) == 0) return "expenses";
    return "";
}

bool App::hasFeature(const Actor& a, const std::string& spec) {
    size_t start = 0;
    for (;;) {
        size_t bar = spec.find('|', start);
        std::string one = spec.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
        if (!one.empty() && a.features.count(one)) return true;
        if (bar == std::string::npos) return false;
        start = bar + 1;
    }
}

bool App::allowed(const Actor& a, const std::string& permission) {
    if (!roleHas(a.role, permission)) return false;
    std::string f = permissionFeature(permission);
    return f.empty() || a.features.count(f) > 0;
}

// --------------------------------------------------------------------- App

App::App(Config& cfg, db::Pool& pool) : cfg_(cfg), pool_(pool) {
    server_.set_payload_max_length(8 * 1024 * 1024);
    server_.set_read_timeout(15, 0);
    server_.set_write_timeout(30, 0);
    server_.set_keep_alive_max_count(100);
    int threads = cfg_.threads;
    server_.new_task_queue = [threads] { return new httplib::ThreadPool(static_cast<size_t>(threads)); };
    server_.set_default_headers({{"X-Content-Type-Options", "nosniff"},
                                 {"X-Frame-Options", "DENY"},
                                 {"Referrer-Policy", "no-referrer"},
                                 {"Cache-Control", "no-store"}});
    server_.set_error_handler([](const httplib::Request&, httplib::Response& res) {
        if (res.body.empty()) {
            std::string code = res.status == 404 ? "not_found" : res.status == 413 ? "payload_too_large" : "http_error";
            res.set_content(nlohmann::json{{"error", {{"code", code}, {"message", code}}}}.dump(), "application/json");
        }
    });
}

httplib::Server::Handler App::wrap(RouteOpts o, Handler h) {
    return [this, o, h](const httplib::Request& req, httplib::Response& res) {
        Ctx ctx(req, res);
        ctx.actor.requestId = crypto::toHex(crypto::randomBytes(8));
        res.set_header("X-Request-Id", ctx.actor.requestId);
        ctx.actor.ip = req.remote_addr;
        if (cfg_.trustProxy) {
            // Right-most entry: the address added by our own reverse proxy. Entries on the
            // left come from the client and can be forged (per-IP rate limits would be
            // bypassed with a proxy that appends instead of replacing the header).
            auto xff = req.get_header_value("X-Forwarded-For");
            auto comma = xff.rfind(',');
            std::string last = comma == std::string::npos ? xff : xff.substr(comma + 1);
            auto b = last.find_first_not_of(" \t");
            auto e = last.find_last_not_of(" \t");
            if (b != std::string::npos) ctx.actor.ip = last.substr(b, e - b + 1);
        }
        ctx.actor.userAgent = req.get_header_value("User-Agent").substr(0, 200);
        auto lang = req.get_header_value("Accept-Language");
        ctx.actor.lang = lang.rfind("en", 0) == 0 ? "en" : "fr";
        try {
            switch (o.access) {
                case Access::Public:
                case Access::Webhook:
                    break;
                case Access::User: {
                    hooks_.authenticateUser(ctx);
                    if (!ctx.actor.rightsVersion.empty()) res.set_header("X-Nhill-Rights", ctx.actor.rightsVersion);
                    if (!roleHas(ctx.actor.role, o.permission)) throw forbidden("permission_denied");
                    if (ctx.actor.establishmentStatus == "closed" || ctx.actor.access == "none")
                        throw forbidden("establishment_closed");
                    // Plan features: the route itself, then the feature its permission depends on.
                    if (!o.feature.empty() && !hasFeature(ctx.actor, o.feature))
                        throw ApiError(403, "feature_not_included", "feature not included in the plan",
                                       {{"feature", o.feature}});
                    if (!o.permission.empty() && !allowed(ctx.actor, o.permission))
                        throw ApiError(403, "feature_not_included", "feature not included in the plan",
                                       {{"feature", permissionFeature(o.permission)}});
                    if (o.write) {
                        if (ctx.actor.establishmentStatus == "suspended")
                            throw ApiError(403, "establishment_suspended", "establishment suspended by the platform");
                        // Computed access (dates compared with now), not the stored status alone.
                        if (ctx.actor.access != "full")
                            throw ApiError(402, "subscription_suspended", "subscription inactive: read and export only",
                                           {{"reason", ctx.actor.accessReason}});
                    }
                    break;
                }
                case Access::Admin: {
                    hooks_.authenticateAdmin(ctx);
                    if (!o.adminRoles.empty() && !o.adminRoles.count(ctx.actor.adminRole) &&
                        ctx.actor.adminRole != "superadmin")
                        throw forbidden("admin_role_required");
                    break;
                }
            }
            h(ctx);
            if (!ctx.responded()) ctx.noContent();
        } catch (const std::exception& e) {
            writeError(ctx, e);
            if (res.status >= 500 && res.status != 503 && hooks_.onServerError) hooks_.onServerError(ctx, e.what());
        }
    };
}

void App::get(const std::string& p, RouteOpts o, Handler h) { server_.Get(p, wrap(std::move(o), std::move(h))); }
void App::post(const std::string& p, RouteOpts o, Handler h) { server_.Post(p, wrap(std::move(o), std::move(h))); }
void App::put(const std::string& p, RouteOpts o, Handler h) { server_.Put(p, wrap(std::move(o), std::move(h))); }
void App::patch(const std::string& p, RouteOpts o, Handler h) { server_.Patch(p, wrap(std::move(o), std::move(h))); }
void App::del(const std::string& p, RouteOpts o, Handler h) { server_.Delete(p, wrap(std::move(o), std::move(h))); }

namespace {

// cpp-httplib (distribution build) listens with a backlog of 5: bursts of
// simultaneous connections are then dropped. listen() may be called again on a
// listening socket to enlarge its queue; the socket is found by its port.
void enlargeBacklog(int port, int backlog) {
    DIR* d = opendir("/proc/self/fd");
    if (!d) return;
    while (dirent* e = readdir(d)) {
        int fd = std::atoi(e->d_name);
        if (fd <= 2) continue;
        int accepting = 0;
        socklen_t len = sizeof accepting;
        if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &accepting, &len) != 0 || !accepting) continue;
        sockaddr_storage ss{};
        socklen_t sl = sizeof ss;
        if (getsockname(fd, reinterpret_cast<sockaddr*>(&ss), &sl) != 0) continue;
        int p = ss.ss_family == AF_INET ? ntohs(reinterpret_cast<sockaddr_in*>(&ss)->sin_port)
                : ss.ss_family == AF_INET6 ? ntohs(reinterpret_cast<sockaddr_in6*>(&ss)->sin6_port) : -1;
        if (p == port) ::listen(fd, backlog);
    }
    closedir(d);
}

}  // namespace

bool App::listen(const std::string& host, int port) {
    if (!server_.bind_to_port(host, port)) return false;
    enlargeBacklog(port, 1024);
    return server_.listen_after_bind();
}
int App::bindToAnyPort(const std::string& host) {
    int port = server_.bind_to_any_port(host);
    if (port > 0) enlargeBacklog(port, 1024);
    return port;
}
void App::listenAfterBind() { server_.listen_after_bind(); }

}  // namespace nh
