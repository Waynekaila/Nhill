// Integration test harness: throwaway PostgreSQL cluster + in-process API server.
#pragma once

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "config.hpp"
#include "db.hpp"
#include "http/app.hpp"
#include "services/services.hpp"

namespace nht {

// Directory of the PostgreSQL server binaries used for the throwaway test clusters:
// $NH_PG_BIN, otherwise the newest /usr/lib/postgresql/<version>/bin (16 on Ubuntu 24.04,
// 17 on Debian 13), otherwise /usr/lib/postgresql/16/bin.
std::string pgBinDir();

using json = nlohmann::json;

struct Response {
    int status = 0;
    json body;
    httplib::Headers headers;
    std::string raw;
    std::string header(const std::string& k) const {
        auto it = headers.find(k);
        return it == headers.end() ? "" : it->second;
    }
};

class Harness {
public:
    static Harness& get();   // lazily started, shared by all tests

    // Superuser SQL (bypasses RLS) — for time travel and white-box checks.
    pqxx::result su(const std::string& sql);
    // Application-role SQL with an explicit tenant (RLS applies).
    pqxx::result asApp(const std::optional<std::string>& tenant, const std::string& sql);

    Response request(const std::string& method, const std::string& path, const json& body = nullptr,
                     const std::string& token = "", const std::string& idemKey = "",
                     const std::string& contentType = "application/json", const std::string& rawBody = "");
    Response get(const std::string& path, const std::string& token = "") { return request("GET", path, nullptr, token); }
    Response post(const std::string& path, const json& body, const std::string& token = "", const std::string& idem = "") {
        return request("POST", path, body, token, idem);
    }

    nh::Env& env() { return *env_; }
    nh::Config& cfg() { return cfg_; }
    std::string appUrl, ownerUrl, superUrl;
    std::string migrationsDir;
    int port = 0;

    ~Harness();

private:
    Harness();
    void startCluster();
    std::string dir_;
    int pgPort_ = 0;
    nh::Config cfg_;
    std::unique_ptr<nh::db::Pool> pool_;
    std::unique_ptr<nh::Profiles> profiles_;
    std::unique_ptr<nh::I18n> i18n_;
    std::unique_ptr<nh::Env> env_;
    std::unique_ptr<nh::App> app_;
    std::thread serverThread_;
};

// ---- helpers shared by test files

std::string uniq(const std::string& prefix = "");   // unique suffix
std::string newKey();                               // idempotency key
void resetIpThrottles();

struct Tenant {
    std::string id;
    std::string token;
    std::string refresh;
    std::string code;
    std::string ownerEmail;
    std::string password = "Kinshasa-2026-tests";
};

// Creates an establishment through the public API.
Tenant createTenant(const std::string& kind = "company", json legalIds = json::object(), bool vat = false,
                    const std::string& email = "");
// Logs in an extra user with a given role in the tenant (invitation + activation).
std::string addMember(const Tenant& t, const std::string& role, std::string* emailOut = nullptr);
std::string createCustomer(const Tenant& t, const std::string& name = "Client test");
std::string createProduct(const Tenant& t, const std::string& sku, const std::string& price, bool stock = true);
void adjustStock(const Tenant& t, const std::string& product, const std::string& qty);
std::string draftInvoice(const Tenant& t, const std::string& customer, const json& lines,
                         const std::string& type = "invoice", const std::string& currency = "");
Response issue(const Tenant& t, const std::string& docId, const std::string& idem = "");
// Platform administrator created through the real setup steps (password + TOTP); returns a session token.
std::string adminLogin(const std::string& role = "superadmin");

}  // namespace nht
