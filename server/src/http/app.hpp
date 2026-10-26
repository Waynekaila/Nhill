#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <functional>
#include <optional>
#include <set>
#include <string>

#include "config.hpp"
#include "db.hpp"
#include "errors.hpp"

namespace nh {

using json = nlohmann::json;

enum class Access { Public, User, Admin, Webhook };

struct Actor {
    // tenant user
    std::string userId;
    std::string email;
    std::string fullName;
    std::string establishmentId;
    std::string establishmentKind;     // individual | company
    std::string establishmentStatus;   // active | suspended | closed
    std::string subscriptionStatus;    // trialing | active | past_due | suspended | canceled
    std::string role;
    std::string sessionId;
    // platform administrator
    std::string adminId;
    std::string adminRole;
    // request
    std::string ip;
    std::string lang = "fr";
    std::string userAgent;
    std::string requestId;
};

class Ctx {
public:
    Ctx(const httplib::Request& rq, httplib::Response& rs) : req(rq), res(rs) {}

    const httplib::Request& req;
    httplib::Response& res;
    Actor actor;

    const nlohmann::json& body();                       // 400 on malformed JSON
    std::string param(const std::string& name) const;   // path parameter
    std::optional<std::string> query(const std::string& name) const;
    std::string header(const std::string& name) const;

    void json(int status, const nlohmann::json& j);
    void file(const std::string& content, const std::string& mime, const std::string& filename, bool inlineDisp = false);
    void noContent();
    bool responded() const { return responded_; }

private:
    std::optional<nlohmann::json> body_;
    bool responded_ = false;
};

using Handler = std::function<void(Ctx&)>;

struct RouteOpts {
    Access access = Access::User;
    std::string permission;            // required tenant permission, empty = any member
    bool write = false;                // requires an entitled subscription
    std::set<std::string> adminRoles;  // for Access::Admin; empty = any admin
};

// Authentication hooks are supplied by the services layer (see routes.cpp).
struct AuthHooks {
    std::function<void(Ctx&)> authenticateUser;   // fills ctx.actor or throws 401
    std::function<void(Ctx&)> authenticateAdmin;  // fills ctx.actor or throws 401
    std::function<void(Ctx&, const std::string&)> onServerError;  // 5xx answered (best effort)
};

class App {
public:
    App(Config& cfg, db::Pool& pool);

    void setAuthHooks(AuthHooks h) { hooks_ = std::move(h); }

    void get(const std::string& pattern, RouteOpts o, Handler h);
    void post(const std::string& pattern, RouteOpts o, Handler h);
    void put(const std::string& pattern, RouteOpts o, Handler h);
    void patch(const std::string& pattern, RouteOpts o, Handler h);
    void del(const std::string& pattern, RouteOpts o, Handler h);

    httplib::Server& server() { return server_; }
    bool listen(const std::string& host, int port);
    int bindToAnyPort(const std::string& host);
    void listenAfterBind();
    void stop() { server_.stop(); }

    static bool roleHas(const std::string& role, const std::string& permission);

private:
    httplib::Server::Handler wrap(RouteOpts o, Handler h);
    Config& cfg_;
    db::Pool& pool_;
    AuthHooks hooks_;
    httplib::Server server_;
};

// Converts any exception into a JSON error response.
void writeError(Ctx& ctx, const std::exception& e);

}  // namespace nh
