// First installation of a Nhill server on Supabase or a self-hosted PostgreSQL.
//
// The administration console never holds database credentials: it sends them
// once to this server (protected by a one-time setup token printed in the server
// log), and the server
//   1. connects with the administrator account (Supabase "postgres" or a
//      PostgreSQL superuser) and creates the Nhill roles with random passwords,
//   2. stores the owner/application connection strings in its own 0600
//      configuration file (the administrator password is never stored),
//   3. applies the versioned migrations (schema "nhill"), and
//   4. verifies the installation (RLS, privileges, isolation, migrations).
#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.hpp"

namespace nh::setup {

using json = nlohmann::json;

struct Target {
    std::string provider = "postgres";  // "supabase" | "postgres"
    std::string host;
    int port = 5432;
    std::string database = "postgres";
    std::string user;                   // administrator (Supabase pooler: "postgres.<project-ref>")
    std::string password;
    std::string sslmode = "require";    // disable | prefer | require | verify-ca | verify-full
    std::string sslrootcert;            // optional CA file on the server (verify-full)

    static Target fromJson(const json& j);   // throws std::invalid_argument with a field name
    // Login name of a Nhill role through the same endpoint ("nhill_app" or "nhill_app.<ref>" behind Supavisor).
    std::string loginFor(const std::string& role) const;
    std::string conninfo(const std::string& user, const std::string& password) const;
    std::string adminConninfo() const { return conninfo(user, password); }
};

struct Bootstrap {
    std::string ownerUrl, appUrl;
    std::optional<std::string> backupLogin, backupPassword;  // shown once, never stored by the server
    std::vector<std::string> warnings;
};

// Connection test with the administrator account: version, rights, existing installation.
json probe(const Target& t);

// Creates/updates the roles nhill_owner, nhill_app (and nhill_backup when allowed) with
// fresh random passwords and the grants they need. Idempotent.
Bootstrap bootstrap(const Target& t);

// Post-installation checks; every entry {id, ok, detail}. ok overall = all entries ok.
json verify(const std::string& ownerUrl, const std::string& appUrl, const std::string& migrationsDir);

// Writes/updates KEY=VALUE pairs atomically (temp file + rename), mode 0600.
void writeConfigValues(const std::string& path, const std::map<std::string, std::string>& values);

// Ensures the file exists with NH_SECRET_KEY (generated when absent). Returns true if created/updated.
bool ensureSecretKey(const std::string& path);

// Creates the first (pending) superadmin; returns {token, expires_at}. Fails if an admin exists.
std::pair<std::string, std::string> createFirstAdmin(const std::string& appUrl, const std::string& email,
                                                     const std::string& name);

// Full headless installation (used by `nhill-server setup` and by the HTTP setup endpoint).
json install(const std::string& configPath, const Config& cfg, const Target& t,
             const std::optional<std::pair<std::string, std::string>>& firstAdmin);

// Setup-mode HTTP server: /health, /api/v1/setup/{status,check,apply}. Returns true when
// the installation succeeded and the process should restart in normal mode.
// fixedToken: tests only (empty = random token printed on stderr).
bool runSetupServer(const std::string& configPath, const Config& cfg, const std::string& fixedToken = "");

}  // namespace nh::setup
