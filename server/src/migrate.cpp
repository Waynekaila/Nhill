#include "migrate.hpp"

#include <pqxx/pqxx>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh {

namespace {

struct File {
    std::string version;
    std::string path;
    std::string sql;
    std::string checksum;
};

std::vector<File> listFiles(const std::string& dir) {
    std::vector<File> files;
    if (!std::filesystem::is_directory(dir)) throw std::runtime_error("migrations directory not found: " + dir);
    for (const auto& e : std::filesystem::directory_iterator(dir)) {
        if (e.path().extension() != ".sql") continue;
        std::ifstream in(e.path());
        std::stringstream ss;
        ss << in.rdbuf();
        File f;
        f.version = e.path().stem().string();
        f.path = e.path().string();
        f.sql = ss.str();
        f.checksum = crypto::sha256Hex(f.sql);
        files.push_back(std::move(f));
    }
    std::sort(files.begin(), files.end(), [](const File& a, const File& b) { return a.version < b.version; });
    return files;
}

// Every object lives in the dedicated schema "nhill" (outside the Supabase Data API).
void useSchema(pqxx::connection& c, bool create) {
    pqxx::nontransaction n(c);
    // CREATE SCHEMA checks the database privilege even with IF NOT EXISTS: only create
    // it when absent (the installer may have created it for the owner role).
    if (create && n.exec("SELECT to_regnamespace('nhill') IS NULL")[0][0].as<bool>()) n.exec("CREATE SCHEMA nhill");
    n.exec("SET search_path TO nhill");
}

void ensureTable(pqxx::connection& c) {
    pqxx::work w(c);
    w.exec(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        " version text PRIMARY KEY, checksum text NOT NULL, applied_at timestamptz NOT NULL DEFAULT now())");
    w.commit();
}

// The running server (application role) checks at startup that no migration
// is pending. Granted after every run: a migration may revoke table rights.
void grantReadToApp(pqxx::connection& c) {
    pqxx::work w(c);
    w.exec(
        "DO $$ BEGIN IF EXISTS (SELECT FROM pg_roles WHERE rolname = 'nhill_app') THEN "
        "GRANT USAGE ON SCHEMA nhill TO nhill_app; GRANT SELECT ON nhill.schema_migrations TO nhill_app; END IF; END $$");
    w.commit();
}

}  // namespace

namespace {
std::vector<std::string> unknownIn(pqxx::connection& c, const std::vector<File>& files) {
    std::vector<std::string> known;
    for (const auto& f : files) known.push_back(f.version);
    pqxx::work w(c);
    std::vector<std::string> unknown;
    if (!w.exec("SELECT to_regclass('nhill.schema_migrations') IS NOT NULL")[0][0].as<bool>()) return unknown;
    for (const auto& r : w.exec("SELECT version FROM nhill.schema_migrations ORDER BY version"))
        if (std::find(known.begin(), known.end(), r[0].c_str()) == known.end()) unknown.emplace_back(r[0].c_str());
    return unknown;
}
}  // namespace

std::vector<std::string> unknownMigrations(const std::string& url, const std::string& dir) {
    auto files = listFiles(dir);
    pqxx::connection c(url);
    return unknownIn(c, files);
}

MigrationResult runMigrations(const std::string& ownerUrl, const std::string& dir) {
    MigrationResult result;
    auto files = listFiles(dir);
    pqxx::connection c(ownerUrl);
    useSchema(c, true);
    ensureTable(c);
    if (auto unknown = unknownIn(c, files); !unknown.empty())
        throw std::runtime_error("the database was migrated by a newer version of Nhill (migration " + unknown.back() +
                                 " unknown to this server): install that version, or restore the copy of the "
                                 "database taken before the update");
    for (const auto& f : files) {
        pqxx::work w(c);
        // Serialises concurrent migrators (e.g. two servers starting together).
        w.exec("SELECT pg_advisory_xact_lock(727274)");
        auto r = w.exec_params("SELECT checksum FROM schema_migrations WHERE version = $1", f.version);
        if (!r.empty()) {
            if (r[0][0].c_str() != f.checksum)
                throw std::runtime_error("migration " + f.version +
                                         " was modified after being applied (checksum mismatch)");
            result.alreadyApplied.push_back(f.version);
            w.commit();
            continue;
        }
        w.exec(f.sql);
        w.exec_params("INSERT INTO schema_migrations (version, checksum) VALUES ($1, $2)", f.version, f.checksum);
        w.commit();
        log::info("migration applied: " + f.version);
        result.applied.push_back(f.version);
    }
    grantReadToApp(c);
    return result;
}

std::vector<std::string> pendingMigrations(const std::string& url, const std::string& dir) {
    auto files = listFiles(dir);
    pqxx::connection c(url);
    useSchema(c, false);
    pqxx::work w(c);
    std::vector<std::string> pending;
    auto exists = w.exec("SELECT to_regclass('nhill.schema_migrations') IS NOT NULL")[0][0].as<bool>();
    for (const auto& f : files) {
        if (!exists) {
            pending.push_back(f.version);
            continue;
        }
        auto r = w.exec_params("SELECT checksum FROM schema_migrations WHERE version = $1", f.version);
        if (r.empty()) pending.push_back(f.version);
    }
    return pending;
}

}  // namespace nh
