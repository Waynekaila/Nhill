#include "setup.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <httplib.h>
#include <pqxx/pqxx>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>
#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "migrate.hpp"
#include "nhill/version.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh::setup {

namespace {

std::string quoteConninfo(const std::string& v) {
    std::string out = "'";
    for (char c : v) {
        if (c == '\'' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    return out + "'";
}

std::string str(const json& j, const char* k, const std::string& def = "") {
    if (!j.contains(k) || j[k].is_null()) return def;
    if (!j[k].is_string()) throw std::invalid_argument(k);
    return j[k].get<std::string>();
}

bool safeIdentifierChars(const std::string& s) {
    if (s.empty() || s.size() > 253) return false;
    for (char c : s)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_' || c == ':')) return false;
    return true;
}

std::string newPassword() { return crypto::toHex(crypto::randomBytes(24)); }  // 192 bits, conninfo-safe

const std::vector<std::string> kRoles = {"nhill_owner", "nhill_app", "nhill_backup"};

}  // namespace

// ------------------------------------------------------------------ target
Target Target::fromJson(const json& j) {
    Target t;
    t.provider = str(j, "provider", "postgres");
    if (t.provider != "supabase" && t.provider != "postgres") throw std::invalid_argument("provider");
    t.host = str(j, "host");
    if (!safeIdentifierChars(t.host)) throw std::invalid_argument("host");
    if (j.contains("port")) {
        if (!j["port"].is_number_integer()) throw std::invalid_argument("port");
        t.port = j["port"].get<int>();
    }
    if (t.port < 1 || t.port > 65535) throw std::invalid_argument("port");
    t.database = str(j, "database", "postgres");
    if (!safeIdentifierChars(t.database)) throw std::invalid_argument("database");
    t.user = str(j, "user", "postgres");
    if (!safeIdentifierChars(t.user)) throw std::invalid_argument("user");
    t.password = str(j, "password");
    if (t.password.empty() || t.password.size() > 512) throw std::invalid_argument("password");
    t.sslmode = str(j, "sslmode", t.provider == "supabase" ? "require" : "prefer");
    static const std::set<std::string> modes = {"disable", "allow", "prefer", "require", "verify-ca", "verify-full"};
    if (!modes.count(t.sslmode)) throw std::invalid_argument("sslmode");
    if (t.provider == "supabase" && (t.sslmode == "disable" || t.sslmode == "allow" || t.sslmode == "prefer"))
        throw std::invalid_argument("sslmode");  // Supabase is always reached over TLS
    t.sslrootcert = str(j, "sslrootcert");
    if (!t.sslrootcert.empty() && !std::filesystem::is_regular_file(t.sslrootcert)) throw std::invalid_argument("sslrootcert");
    return t;
}

std::string Target::loginFor(const std::string& role) const {
    // Supavisor (Supabase pooler) identifies the project with "<role>.<project-ref>".
    auto dot = user.find('.');
    return dot == std::string::npos ? role : role + user.substr(dot);
}

std::string Target::conninfo(const std::string& u, const std::string& pw) const {
    std::string c = "host=" + quoteConninfo(host) + " port=" + std::to_string(port) + " dbname=" + quoteConninfo(database) +
                    " user=" + quoteConninfo(u) + " password=" + quoteConninfo(pw) + " sslmode=" + sslmode +
                    " connect_timeout=15 application_name=nhill-server";
    if (!sslrootcert.empty()) c += " sslrootcert=" + quoteConninfo(sslrootcert);
    return c;
}

// ------------------------------------------------------------------ probe
json probe(const Target& t) {
    pqxx::connection c(t.adminConninfo());
    pqxx::work w(c);
    auto r = w.exec(
        "SELECT current_setting('server_version'), current_user, r.rolsuper, r.rolcreaterole, "
        "has_database_privilege(current_database(), 'CREATE'), "
        "EXISTS (SELECT FROM pg_namespace WHERE nspname = 'nhill'), "
        "to_regclass('nhill.schema_migrations') IS NOT NULL, "
        "EXISTS (SELECT FROM pg_roles WHERE rolname = 'anon') "
        "FROM pg_roles r WHERE r.rolname = current_user");
    int major = std::stoi(r[0][0].c_str());
    json out = {{"server_version", r[0][0].c_str()},
                {"user", r[0][1].c_str()},
                {"superuser", r[0][2].as<bool>()},
                {"can_create_roles", r[0][2].as<bool>() || r[0][3].as<bool>()},
                {"can_create_in_database", r[0][4].as<bool>()},
                {"schema_exists", r[0][5].as<bool>()},
                {"supabase_roles_present", r[0][7].as<bool>()},
                {"version_supported", major >= 15}};
    if (r[0][6].as<bool>())
        out["applied_migrations"] = w.exec("SELECT count(*) FROM nhill.schema_migrations")[0][0].as<int>();
    else
        out["applied_migrations"] = 0;
    return out;
}

// ------------------------------------------------------------------ bootstrap
Bootstrap bootstrap(const Target& t) {
    Bootstrap b;
    pqxx::connection c(t.adminConninfo());
    const std::string ownerPw = newPassword(), appPw = newPassword(), backupPw = newPassword();
    {
        pqxx::work w(c);
        auto me = w.exec("SELECT rolsuper OR rolcreaterole FROM pg_roles WHERE rolname = current_user");
        if (me.empty() || !me[0][0].as<bool>())
            throw std::runtime_error("the administrator account cannot create roles (CREATEROLE required)");
        auto mk = [&](const std::string& role, const std::string& attrs) {
            w.exec("DO $$ BEGIN IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = " + w.quote(role) + ") THEN CREATE ROLE " +
                   w.quote_name(role) + " LOGIN " + attrs + "; END IF; END $$");
        };
        mk("nhill_owner", "NOSUPERUSER NOCREATEDB NOCREATEROLE NOBYPASSRLS");
        mk("nhill_app", "NOSUPERUSER NOCREATEDB NOCREATEROLE NOBYPASSRLS");
        w.exec("ALTER ROLE nhill_owner WITH LOGIN PASSWORD " + w.quote(ownerPw));
        w.exec("ALTER ROLE nhill_app WITH LOGIN PASSWORD " + w.quote(appPw));
        // Changing BYPASSRLS needs that attribute itself: only touched when it must be removed.
        if (w.exec("SELECT rolbypassrls OR rolsuper FROM pg_roles WHERE rolname = 'nhill_app'")[0][0].as<bool>())
            w.exec("ALTER ROLE nhill_app WITH NOSUPERUSER NOBYPASSRLS");
        w.exec("ALTER ROLE nhill_owner SET search_path = nhill");
        w.exec("ALTER ROLE nhill_app SET search_path = nhill");
        w.commit();
    }
    {
        // The owner role needs to create the schema "nhill". When the administrator owns the
        // database it grants CREATE; otherwise (hosted platforms where the database belongs
        // to the platform) the administrator creates the schema on behalf of the owner role.
        bool granted = false;
        try {
            pqxx::work w(c);
            w.exec("GRANT CONNECT, CREATE ON DATABASE " + w.quote_name(t.database) + " TO nhill_owner");
            // Without the grant option PostgreSQL only emits a warning: check the outcome.
            granted = w.exec("SELECT has_database_privilege('nhill_owner', current_database(), 'CREATE')")[0][0].as<bool>();
            w.commit();
        } catch (const std::exception&) {
        }
        pqxx::work w(c);
        bool exists = w.exec("SELECT to_regnamespace('nhill') IS NOT NULL")[0][0].as<bool>();
        bool ownerOwns = exists && w.exec("SELECT n.nspowner = (SELECT oid FROM pg_roles WHERE rolname = 'nhill_owner') "
                                          "FROM pg_namespace n WHERE n.nspname = 'nhill'")[0][0].as<bool>();
        if (!ownerOwns && (!granted || exists)) {
            // Membership is required to create a schema for (or hand it over to) another role.
            // (PostgreSQL 16+: the creator holds ADMIN on the role but not SET; grant SET only.)
            w.exec("DO $$ BEGIN "
                   "IF current_setting('server_version_num')::int >= 160000 THEN "
                   "  IF NOT pg_has_role(current_user, 'nhill_owner', 'SET') THEN "
                   "    EXECUTE format('GRANT nhill_owner TO %I WITH INHERIT FALSE, SET TRUE', current_user); END IF; "
                   "ELSIF NOT pg_has_role(current_user, 'nhill_owner', 'MEMBER') THEN "
                   "  EXECUTE format('GRANT nhill_owner TO %I', current_user); END IF; END $$");
            if (exists) w.exec("ALTER SCHEMA nhill OWNER TO nhill_owner");
            else w.exec("CREATE SCHEMA nhill AUTHORIZATION nhill_owner");
        }
        w.commit();
    }
    // Backup role: read-only, the only one allowed to bypass row-level security. Optional:
    // some hosted platforms refuse BYPASSRLS / pg_read_all_data to non-superusers.
    try {
        pqxx::work w(c);
        w.exec("DO $$ BEGIN IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'nhill_backup') THEN "
               "CREATE ROLE nhill_backup LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE BYPASSRLS; END IF; END $$");
        w.exec("ALTER ROLE nhill_backup WITH LOGIN BYPASSRLS PASSWORD " + w.quote(backupPw));
        w.exec("GRANT pg_read_all_data TO nhill_backup");
        w.exec("ALTER ROLE nhill_backup SET default_transaction_read_only = on");
        w.exec("ALTER ROLE nhill_backup SET search_path = nhill");
        w.commit();
        b.backupLogin = t.loginFor("nhill_backup");
        b.backupPassword = backupPw;
    } catch (const std::exception& e) {
        b.warnings.push_back(std::string("backup role not created (") + e.what() +
                             "): use the platform's own backups (Supabase) or create it as superuser");
    }
    b.ownerUrl = t.conninfo(t.loginFor("nhill_owner"), ownerPw);
    b.appUrl = t.conninfo(t.loginFor("nhill_app"), appPw);
    return b;
}

// ------------------------------------------------------------------ verification
json verify(const std::string& ownerUrl, const std::string& appUrl, const std::string& migrationsDir) {
    json checks = json::array();
    auto add = [&](const std::string& id, bool ok, const std::string& detail) {
        checks.push_back({{"id", id}, {"ok", ok}, {"detail", detail}});
    };
    try {
        auto pending = pendingMigrations(ownerUrl, migrationsDir);
        add("migrations", pending.empty(), pending.empty() ? "all migrations applied" : "pending: " + pending.front());
    } catch (const std::exception& e) {
        add("migrations", false, e.what());
    }
    try {
        pqxx::connection c(ownerUrl);
        pqxx::work w(c);
        auto app = w.exec("SELECT rolsuper, rolbypassrls FROM pg_roles WHERE rolname = 'nhill_app'");
        bool appOk = !app.empty() && !app[0][0].as<bool>() && !app[0][1].as<bool>();
        add("app_role", appOk, appOk ? "nhill_app: no superuser, no RLS bypass" : "nhill_app has elevated attributes");
        int owned = w.exec("SELECT count(*) FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace "
                           "WHERE n.nspname = 'nhill' AND c.relowner = (SELECT oid FROM pg_roles WHERE rolname = 'nhill_app')")[0][0]
                        .as<int>();
        add("app_owns_nothing", owned == 0, std::to_string(owned) + " object(s) owned by nhill_app");
        auto rls = w.exec("SELECT count(*), count(*) FILTER (WHERE c.relrowsecurity AND c.relforcerowsecurity) "
                          "FROM pg_policy p JOIN pg_class c ON c.oid = p.polrelid JOIN pg_namespace n ON n.oid = c.relnamespace "
                          "WHERE n.nspname = 'nhill' AND p.polname LIKE '%\\_tenant'");
        int pol = rls[0][0].as<int>(), forced = rls[0][1].as<int>();
        add("row_level_security", pol >= 20 && pol == forced,
            std::to_string(forced) + "/" + std::to_string(pol) + " tenant tables with forced RLS");
        int publicPublic = w.exec("SELECT count(*) FROM pg_tables WHERE schemaname = 'public' AND tablename IN "
                                  "('establishments','sales_documents','users')")[0][0].as<int>();
        add("not_in_public_schema", publicPublic == 0, "Nhill tables live in schema nhill only");
        bool openToApiRoles = false;
        for (const auto& r : w.exec("SELECT rolname FROM pg_roles WHERE rolname IN ('anon','authenticated')"))
            if (w.exec_params("SELECT has_schema_privilege($1, 'nhill', 'USAGE')", r[0].c_str())[0][0].as<bool>())
                openToApiRoles = true;
        bool publicUsage = w.exec("SELECT has_schema_privilege('public', 'nhill', 'USAGE')")[0][0].as<bool>();
        add("closed_to_api_roles", !openToApiRoles && !publicUsage,
            "schema nhill not usable by PUBLIC / anon / authenticated");
        int noPath = w.exec("SELECT count(*) FROM pg_proc p JOIN pg_namespace n ON n.oid = p.pronamespace WHERE "
                            "n.nspname = 'nhill' AND NOT EXISTS (SELECT FROM unnest(p.proconfig) x "
                            "WHERE x LIKE 'search_path=%')")[0][0].as<int>();
        add("functions_search_path", noPath == 0, std::to_string(noPath) + " function(s) without a fixed search_path");
    } catch (const std::exception& e) {
        add("owner_connection", false, e.what());
    }
    try {
        pqxx::connection c(appUrl);
        pqxx::work w(c);
        w.exec("SET LOCAL search_path TO nhill");
        int visible = w.exec("SELECT count(*) FROM parties")[0][0].as<int>();
        add("isolation_without_tenant", visible == 0, std::to_string(visible) + " business row(s) visible without tenant");
        bool insertRefused = false;
        try {
            pqxx::subtransaction s(w);
            s.exec("INSERT INTO parties (establishment_id, name, is_customer) VALUES (gen_random_uuid(), 'x', true)");
        } catch (const std::exception&) {
            insertRefused = true;
        }
        add("isolation_write", insertRefused, insertRefused ? "write outside a tenant refused" : "write outside a tenant accepted");
        bool noDdl = false;
        try {
            pqxx::subtransaction s(w);
            s.exec("CREATE TABLE nhill.nhill_setup_probe (x int)");
        } catch (const std::exception&) {
            noDdl = true;
        }
        add("app_no_ddl", noDdl, noDdl ? "application role cannot create tables" : "application role can create tables");
        w.abort();
    } catch (const std::exception& e) {
        add("app_connection", false, e.what());
    }
    bool all = true;
    for (const auto& c : checks) all = all && c["ok"].get<bool>();
    return {{"ok", all}, {"checks", checks}};
}

// ------------------------------------------------------------------ configuration file
void writeConfigValues(const std::string& path, const std::map<std::string, std::string>& values) {
    std::vector<std::string> lines;
    std::set<std::string> done;
    {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            auto start = line.find_first_not_of(" \t");
            if (eq != std::string::npos && start != std::string::npos && line[start] != '#') {
                std::string k = line.substr(start, eq - start);
                while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
                if (auto it = values.find(k); it != values.end()) {
                    line = k + "=" + it->second;
                    done.insert(k);
                }
            }
            lines.push_back(line);
        }
    }
    for (const auto& [k, v] : values)
        if (!done.count(k)) lines.push_back(k + "=" + v);
    const std::string tmp = path + ".tmp";
    {
        umask(077);
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + tmp);
        for (const auto& l : lines) out << l << "\n";
        out.flush();
        if (!out) throw std::runtime_error("cannot write " + tmp);
    }
    chmod(tmp.c_str(), 0600);
    if (std::rename(tmp.c_str(), path.c_str()) != 0) throw std::runtime_error("cannot replace " + path);
}

bool ensureSecretKey(const std::string& path) {
    std::map<std::string, std::string> kv;
    if (std::filesystem::exists(path)) kv = readConfigFile(path);
    if (kv.count("NH_SECRET_KEY") && kv["NH_SECRET_KEY"].size() == 64) return false;
    writeConfigValues(path, {{"NH_SECRET_KEY", crypto::toHex(crypto::randomBytes(32))}});
    return true;
}

// ------------------------------------------------------------------ first administrator
std::pair<std::string, std::string> createFirstAdmin(const std::string& appUrl, const std::string& email,
                                                     const std::string& name) {
    std::string e = email;
    for (auto& ch : e) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const std::string token = crypto::randomToken(24);
    pqxx::connection c(appUrl);
    pqxx::work w(c);
    w.exec("SET LOCAL search_path TO nhill");
    w.exec("LOCK TABLE admin_users IN EXCLUSIVE MODE");
    if (w.exec("SELECT count(*) FROM admin_users")[0][0].as<int>() > 0)
        throw std::runtime_error("an administrator already exists");
    auto r = w.exec_params("INSERT INTO admin_users (email, full_name, role, setup_token_hash, setup_expires_at) VALUES "
                           "($1, $2, 'superadmin', decode($3,'hex'), now() + interval '30 minutes') "
                           "RETURNING id, nh_iso(setup_expires_at)",
                           e, name, crypto::sha256Hex(token));
    w.exec_params("INSERT INTO audit_log (actor_type, action, entity, entity_id, details) VALUES "
                  "('system', 'admin.bootstrapped', 'admin_user', $1, jsonb_build_object('email', $2::text, 'via', 'setup'))",
                  r[0][0].c_str(), e);
    w.commit();
    return {token, r[0][1].c_str()};
}

// ------------------------------------------------------------------ installation
json install(const std::string& configPath, const Config& cfg, const Target& t,
             const std::optional<std::pair<std::string, std::string>>& firstAdmin) {
    json report;
    report["probe"] = probe(t);
    if (!report["probe"]["version_supported"].get<bool>()) throw std::runtime_error("PostgreSQL 15 or later is required");
    Bootstrap b = bootstrap(t);
    auto mig = runMigrations(b.ownerUrl, cfg.migrationsDir);
    report["migrations"] = {{"applied", mig.applied}, {"already_applied", mig.alreadyApplied.size()}};
    report["verification"] = verify(b.ownerUrl, b.appUrl, cfg.migrationsDir);
    report["warnings"] = b.warnings;
    if (!report["verification"]["ok"].get<bool>()) {
        report["ok"] = false;
        return report;  // configuration is NOT written: the server stays in setup mode
    }
    writeConfigValues(configPath, {{"NH_DATABASE_URL", b.appUrl},
                                   {"NH_MIGRATION_DATABASE_URL", b.ownerUrl},
                                   {"NH_DB_PROVIDER", t.provider}});
    if (b.backupLogin) report["backup_account"] = {{"user", *b.backupLogin}, {"password", *b.backupPassword}};
    if (firstAdmin) {
        try {
            auto [token, expires] = createFirstAdmin(b.appUrl, firstAdmin->first, firstAdmin->second);
            report["first_admin"] = {{"email", firstAdmin->first}, {"setup_token", token}, {"expires_at", expires}};
        } catch (const std::exception& e) {
            report["first_admin"] = {{"error", e.what()}};
        }
    }
    report["ok"] = true;
    log::info("setup: installation completed on " + t.provider + " (" + t.host + ")");
    return report;
}

// ------------------------------------------------------------------ setup-mode HTTP server
bool runSetupServer(const std::string& configPath, const Config& cfg, const std::string& fixedToken) {
    const std::string token = fixedToken.empty() ? crypto::randomToken(18) : fixedToken;
    const auto expires = std::chrono::steady_clock::now() + std::chrono::minutes(60);
    // Printed once on the server console / journal; only someone with access to the
    // server can read it.
    std::fprintf(stderr,
                 "\n  Nhill server is in SETUP MODE (no database configured).\n"
                 "  Open Nhill Console, choose the server address and enter this setup token\n"
                 "  (valid 60 minutes, new token at each restart):\n\n      %s\n\n",
                 token.c_str());
    httplib::Server srv;
    std::mutex m;
    std::map<std::string, std::vector<std::chrono::steady_clock::time_point>> attempts;
    std::atomic<bool> installed{false};
    auto reply = [](httplib::Response& res, int status, const json& body) {
        res.status = status;
        res.set_header("Cache-Control", "no-store");
        res.set_content(body.dump(), "application/json");
    };
    auto error = [&](httplib::Response& res, int status, const std::string& code, const std::string& message,
                     const json& details = json::object()) {
        reply(res, status, {{"error", {{"code", code}, {"message", message}, {"details", details}}}});
    };
    auto guard = [&](const httplib::Request& req, httplib::Response& res, json& body) -> bool {
        std::string ip = req.remote_addr;
        if (cfg.trustProxy && req.has_header("X-Forwarded-For")) {
            ip = req.get_header_value("X-Forwarded-For");
            ip = ip.substr(0, ip.find(','));
        }
        {
            std::lock_guard<std::mutex> l(m);
            auto& v = attempts[ip];
            auto now = std::chrono::steady_clock::now();
            v.erase(std::remove_if(v.begin(), v.end(), [&](auto t) { return now - t > std::chrono::minutes(15); }), v.end());
            if (v.size() >= 20) {
                error(res, 429, "too_many_attempts", "too many attempts, retry later");
                return false;
            }
            v.push_back(now);
        }
        try {
            body = json::parse(req.body);
        } catch (...) {
            error(res, 400, "invalid_json", "invalid JSON body");
            return false;
        }
        if (std::chrono::steady_clock::now() > expires) {
            error(res, 401, "setup_token_expired", "restart the server to get a new setup token");
            return false;
        }
        if (!body.contains("setup_token") || !body["setup_token"].is_string() ||
            !crypto::constantTimeEquals(body["setup_token"].get<std::string>(), token)) {
            error(res, 401, "invalid_setup_token", "invalid setup token");
            return false;
        }
        return true;
    };
    srv.Get("/health", [&](const httplib::Request&, httplib::Response& res) {
        reply(res, 200, {{"status", "setup"}, {"database", false}});
    });
    // Identity of the server while it waits for its installation: the applications check
    // an address with GET /api/v1/meta before saving it (3.0.0).
    srv.Get("/api/v1/meta", [&](const httplib::Request&, httplib::Response& res) {
        reply(res, 200, {{"product", nhill::kProductName}, {"version", nhill::kProductVersion}, {"api", nhill::kApiVersion},
                         {"min_client_version", nhill::kMinClientVersion}, {"mode", "setup"}});
    });
    srv.Get("/api/v1/setup/status", [&](const httplib::Request&, httplib::Response& res) {
        reply(res, 200, {{"mode", installed ? "restarting" : "setup"}, {"version", nhill::kProductVersion}});
    });
    srv.Post("/api/v1/setup/check", [&](const httplib::Request& req, httplib::Response& res) {
        json b;
        if (!guard(req, res, b)) return;
        try {
            reply(res, 200, probe(Target::fromJson(b.value("database", json::object()))));
        } catch (const std::invalid_argument& e) {
            error(res, 422, "validation_failed", "invalid field", {{"field", e.what()}, {"reason", "invalid"}});
        } catch (const std::exception& e) {
            error(res, 422, "database_unreachable", e.what());
        }
    });
    srv.Post("/api/v1/setup/apply", [&](const httplib::Request& req, httplib::Response& res) {
        json b;
        if (!guard(req, res, b)) return;
        if (installed) {
            error(res, 409, "already_installed", "installation already completed");
            return;
        }
        try {
            Target t = Target::fromJson(b.value("database", json::object()));
            std::optional<std::pair<std::string, std::string>> admin;
            if (b.contains("first_admin") && b["first_admin"].is_object()) {
                std::string email = b["first_admin"].value("email", ""), name = b["first_admin"].value("full_name", "");
                if (email.find('@') == std::string::npos || name.empty()) throw std::invalid_argument("first_admin");
                admin = std::make_pair(email, name);
            }
            json report = install(configPath, cfg, t, admin);
            if (report["ok"].get<bool>()) installed = true;
            reply(res, report["ok"].get<bool>() ? 200 : 422, report);
            if (installed)
                std::thread([&srv] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(800));
                    srv.stop();
                }).detach();
        } catch (const std::invalid_argument& e) {
            error(res, 422, "validation_failed", "invalid field", {{"field", e.what()}, {"reason", "invalid"}});
        } catch (const std::exception& e) {
            log::error(std::string("setup: ") + e.what());
            error(res, 422, "setup_failed", e.what());
        }
    });
    srv.set_payload_max_length(64 * 1024);
    log::info("setup mode listening on " + cfg.listenAddress + ":" + std::to_string(cfg.listenPort));
    srv.listen(cfg.listenAddress, cfg.listenPort);
    return installed;
}

}  // namespace nh::setup
