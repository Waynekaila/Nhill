// First installation (setup mode): role bootstrap, migrations, verification and the
// setup-mode HTTP endpoints, against throwaway clusters using password authentication.
#include <gtest/gtest.h>
#include <httplib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <filesystem>
#include <fstream>
#include <pqxx/pqxx>
#include <thread>

#include "config.hpp"
#include "setup.hpp"
#include "util/crypto.hpp"

namespace fs = std::filesystem;

// Same rule as tests/server/harness.cpp (this test is a separate executable):
// $NH_PG_BIN, else the newest /usr/lib/postgresql/<version>/bin, else version 16.
static std::string pgBinDir() {
    if (const char* b = std::getenv("NH_PG_BIN")) return b;
    int best = -1;
    std::string found;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator("/usr/lib/postgresql", ec)) {
        const std::string name = e.path().filename().string();
        if (name.empty() || name.find_first_not_of("0123456789") != std::string::npos) continue;
        if (!fs::exists(e.path() / "bin" / "pg_ctl", ec)) continue;
        int v = std::atoi(name.c_str());
        if (v > best) { best = v; found = (e.path() / "bin").string(); }
    }
    return found.empty() ? std::string("/usr/lib/postgresql/16/bin") : found;
}
using nlohmann::json;

namespace {

void sh(const std::string& cmd) {
    if (std::system(cmd.c_str()) != 0) throw std::runtime_error("command failed: " + cmd);
}

int freePort() {
    // A closed socket (httplib::Server::stop() does not close a socket that never listened).
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof a);
    socklen_t len = sizeof a;
    getsockname(fd, reinterpret_cast<sockaddr*>(&a), &len);
    close(fd);
    return ntohs(a.sin_port);
}

// A PostgreSQL cluster reachable over TCP with SCRAM passwords (like a hosted database).
struct Cluster {
    std::string dir, bin = pgBinDir(), as;
    int port = 0;
    Cluster() {
        char tmpl[] = "/tmp/nhsetup-XXXXXX";
        dir = mkdtemp(tmpl);
        as = geteuid() == 0 ? "runuser -u postgres -- " : "";
        if (!as.empty()) sh("chown postgres:postgres " + dir);
        std::ofstream(dir + "/pw") << "super-secret-pw\n";
        if (!as.empty()) sh("chown postgres:postgres " + dir + "/pw");
        port = freePort();
        sh(as + bin + "/initdb -D " + dir + "/data -U postgres --pwfile=" + dir +
           "/pw --auth-local=trust --auth-host=scram-sha-256 -E UTF8 --locale=C.UTF-8 >/dev/null");
        sh(as + bin + "/pg_ctl -D " + dir + "/data -o \"-p " + std::to_string(port) + " -k " + dir +
           " -c listen_addresses=127.0.0.1 -c fsync=off\" -l " + dir + "/log -w start >/dev/null");
    }
    ~Cluster() {
        if (std::getenv("NH_TEST_KEEP_DB")) return;
        if (std::system((as + bin + "/pg_ctl -D " + dir + "/data -m immediate stop >/dev/null 2>&1").c_str()) != 0) {
        }
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
    void super(const std::string& sql, const std::string& db = "postgres") {
        pqxx::connection c("host=" + dir + " port=" + std::to_string(port) + " user=postgres dbname=" + db);
        pqxx::nontransaction n(c);
        n.exec(sql);
    }
    nh::setup::Target target(const std::string& user, const std::string& pw, const std::string& db) {
        return nh::setup::Target::fromJson({{"provider", "postgres"}, {"host", "127.0.0.1"}, {"port", port},
                                            {"database", db}, {"user", user}, {"password", pw}, {"sslmode", "disable"}});
    }
};

nh::Config baseConfig() {
    nh::Config c;
    c.migrationsDir = std::string(NHILL_SOURCE_DIR) + "/db/migrations";
    c.masterKey = nh::crypto::randomBytes(32);
    c.deriveKeys();
    return c;
}

std::string tmpConfig() {
    char tmpl[] = "/tmp/nhsetup-cfg-XXXXXX";
    std::string d = mkdtemp(tmpl);
    return d + "/server.conf";
}

std::string fileText(const std::string& p) {
    std::ifstream in(p);
    return {std::istreambuf_iterator<char>(in), {}};
}

bool canConnect(const std::string& url) {
    try {
        pqxx::connection c(url);
        return c.is_open();
    } catch (...) {
        return false;
    }
}

void expectAllChecks(const json& report) {
    for (const auto& c : report["verification"]["checks"])
        EXPECT_TRUE(c["ok"].get<bool>()) << c["id"] << ": " << c["detail"];
}

}  // namespace

TEST(Setup, TargetValidation) {
    nh::crypto::init();
    json ok = {{"provider", "supabase"}, {"host", "aws-0-eu-west-1.pooler.supabase.com"}, {"port", 5432},
               {"user", "postgres.abcdefghijklmnop"}, {"password", "pw'with\\quote"}};
    auto t = nh::setup::Target::fromJson(ok);
    EXPECT_EQ(t.sslmode, "require");  // TLS by default on Supabase
    EXPECT_EQ(t.loginFor("nhill_app"), "nhill_app.abcdefghijklmnop");
    EXPECT_NE(t.adminConninfo().find("password='pw\\'with\\\\quote'"), std::string::npos);
    auto direct = nh::setup::Target::fromJson({{"host", "db.example.org"}, {"user", "postgres"}, {"password", "x"}});
    EXPECT_EQ(direct.loginFor("nhill_app"), "nhill_app");
    for (auto [k, v] : std::vector<std::pair<std::string, json>>{{"host", "evil host"},
                                                                {"host", "h' user=x"},
                                                                {"port", 0},
                                                                {"provider", "mysql"},
                                                                {"sslmode", "disable"},
                                                                {"user", "a b"},
                                                                {"password", ""}}) {
        json bad = ok;
        bad[k] = v;
        EXPECT_THROW(nh::setup::Target::fromJson(bad), std::invalid_argument) << k;
    }
}

TEST(Setup, ConfigFileIsWrittenAtomicallyWithPrivateMode) {
    auto p = tmpConfig();
    std::ofstream(p) << "# comment\nNH_LISTEN_PORT=9000\nNH_DATABASE_URL=old\n";
    chmod(p.c_str(), 0600);
    EXPECT_TRUE(nh::setup::ensureSecretKey(p));
    EXPECT_FALSE(nh::setup::ensureSecretKey(p));
    nh::setup::writeConfigValues(p, {{"NH_DATABASE_URL", "new"}, {"NH_DB_PROVIDER", "supabase"}});
    auto kv = nh::readConfigFile(p);
    EXPECT_EQ(kv["NH_LISTEN_PORT"], "9000");
    EXPECT_EQ(kv["NH_DATABASE_URL"], "new");
    EXPECT_EQ(kv["NH_SECRET_KEY"].size(), 64u);
    EXPECT_NE(fileText(p).find("# comment"), std::string::npos);
    struct stat st{};
    stat(p.c_str(), &st);
    EXPECT_EQ(st.st_mode & 0777, 0600u);
}

// Self-hosted PostgreSQL with a superuser: full installation including the backup role.
TEST(Setup, InstallOnSelfHostedPostgresAsSuperuser) {
    nh::crypto::init();
    Cluster cl;
    cl.super("CREATE DATABASE nhill_prod");
    auto t = cl.target("postgres", "super-secret-pw", "nhill_prod");
    auto probe = nh::setup::probe(t);
    EXPECT_TRUE(probe["superuser"].get<bool>());
    EXPECT_TRUE(probe["version_supported"].get<bool>());
    EXPECT_EQ(probe["applied_migrations"], 0);

    auto cfgPath = tmpConfig();
    nh::setup::ensureSecretKey(cfgPath);
    auto report = nh::setup::install(cfgPath, baseConfig(), t, std::make_pair("Boss@Example.org", "Chef"));
    ASSERT_TRUE(report["ok"].get<bool>()) << report.dump(2);
    expectAllChecks(report);
    EXPECT_GT(report["migrations"]["applied"].size(), 5u);
    ASSERT_TRUE(report.contains("backup_account"));
    ASSERT_TRUE(report["first_admin"].contains("setup_token"));
    EXPECT_EQ(report["first_admin"]["email"], "Boss@Example.org");

    // The configuration holds the owner/application roles, never the administrator password.
    auto kv = nh::readConfigFile(cfgPath);
    EXPECT_NE(kv["NH_DATABASE_URL"].find("user='nhill_app'"), std::string::npos);
    EXPECT_NE(kv["NH_MIGRATION_DATABASE_URL"].find("user='nhill_owner'"), std::string::npos);
    EXPECT_EQ(fileText(cfgPath).find("super-secret-pw"), std::string::npos);
    EXPECT_EQ(fileText(cfgPath).find(report["backup_account"]["password"].get<std::string>()), std::string::npos);
    ASSERT_TRUE(canConnect(kv["NH_DATABASE_URL"]));

    // The backup role reads everything (RLS bypass) but cannot write.
    std::string backupUrl = t.conninfo(report["backup_account"]["user"], report["backup_account"]["password"]);
    {
        pqxx::connection c(backupUrl);
        pqxx::work w(c);
        EXPECT_EQ(w.exec("SELECT count(*) FROM nhill.admin_users")[0][0].as<int>(), 1);
        EXPECT_THROW(w.exec("DELETE FROM nhill.admin_users"), pqxx::sql_error);
    }
    {
        pqxx::connection c(kv["NH_DATABASE_URL"]);
        pqxx::work w(c);
        auto a = w.exec("SELECT email, status, role FROM admin_users");
        ASSERT_EQ(a.size(), 1u);
        EXPECT_EQ(a[0][0].as<std::string>(), "boss@example.org");
        EXPECT_EQ(a[0][1].as<std::string>(), "pending");
    }

    // Running the installation again is safe: nothing re-applied, passwords rotated,
    // and a second first administrator is refused.
    auto again = nh::setup::install(cfgPath, baseConfig(), t, std::make_pair("other@example.org", "X"));
    ASSERT_TRUE(again["ok"].get<bool>()) << again.dump(2);
    EXPECT_EQ(again["migrations"]["applied"].size(), 0u);
    EXPECT_TRUE(again["first_admin"].contains("error"));
    EXPECT_FALSE(canConnect(kv["NH_DATABASE_URL"]));  // old password no longer valid
    auto kv2 = nh::readConfigFile(cfgPath);
    EXPECT_TRUE(canConnect(kv2["NH_DATABASE_URL"]));
}

// Hosted platform (Supabase-like): the administrator is not a superuser, does not own
// the database, and API roles anon/authenticated exist. Nhill must stay invisible to them.
TEST(Setup, InstallAsNonSuperuserOnHostedLikeDatabase) {
    nh::crypto::init();
    Cluster cl;
    cl.super("CREATE ROLE anon NOLOGIN; CREATE ROLE authenticated NOLOGIN; CREATE ROLE service_role NOLOGIN BYPASSRLS;"
             "CREATE ROLE platform_admin LOGIN CREATEROLE CREATEDB BYPASSRLS PASSWORD 'hosted-admin-pw'");
    cl.super("CREATE DATABASE hosted");
    cl.super("GRANT CREATE ON DATABASE hosted TO platform_admin;"
             "GRANT USAGE ON SCHEMA public TO anon, authenticated", "hosted");
    auto t = cl.target("platform_admin", "hosted-admin-pw", "hosted");
    auto probe = nh::setup::probe(t);
    EXPECT_FALSE(probe["superuser"].get<bool>());
    EXPECT_TRUE(probe["can_create_roles"].get<bool>());
    EXPECT_TRUE(probe["supabase_roles_present"].get<bool>());

    auto cfgPath = tmpConfig();
    nh::setup::ensureSecretKey(cfgPath);
    auto report = nh::setup::install(cfgPath, baseConfig(), t, std::nullopt);
    ASSERT_TRUE(report["ok"].get<bool>()) << report.dump(2);
    expectAllChecks(report);
    // pg_read_all_data cannot be granted by a non-superuser: no backup role, a warning instead.
    EXPECT_FALSE(report.contains("backup_account"));
    EXPECT_FALSE(report["warnings"].empty());

    pqxx::connection c("host=" + cl.dir + " port=" + std::to_string(cl.port) + " user=postgres dbname=hosted");
    pqxx::work w(c);
    EXPECT_EQ(w.exec("SELECT nspowner::regrole::text FROM pg_namespace WHERE nspname = 'nhill'")[0][0].as<std::string>(),
              "nhill_owner");
    for (const char* role : {"anon", "authenticated"}) {
        EXPECT_FALSE(w.exec_params("SELECT has_schema_privilege($1, 'nhill', 'USAGE')", role)[0][0].as<bool>()) << role;
        EXPECT_EQ(w.exec_params("SELECT count(*) FROM information_schema.role_table_grants WHERE grantee = $1 "
                                "AND table_schema = 'nhill'", role)[0][0].as<int>(), 0) << role;
    }
    EXPECT_GT(w.exec("SELECT count(*) FROM pg_tables WHERE schemaname = 'nhill'")[0][0].as<int>(), 40);
}

TEST(Setup, RefusesAnAdministratorWithoutRoleCreation) {
    nh::crypto::init();
    Cluster cl;
    cl.super("CREATE ROLE weak LOGIN PASSWORD 'weak-pw'");
    cl.super("CREATE DATABASE w OWNER weak");
    auto cfgPath = tmpConfig();
    nh::setup::ensureSecretKey(cfgPath);
    EXPECT_THROW(nh::setup::install(cfgPath, baseConfig(), cl.target("weak", "weak-pw", "w"), std::nullopt),
                 std::runtime_error);
    EXPECT_EQ(nh::readConfigFile(cfgPath).count("NH_DATABASE_URL"), 0u);
    EXPECT_THROW(nh::setup::probe(cl.target("weak", "wrong-pw", "w")), std::exception);
}

// Setup-mode HTTP server: token-protected, then stops so the process restarts normally.
TEST(Setup, SetupModeEndpoints) {
    nh::crypto::init();
    Cluster cl;
    cl.super("CREATE DATABASE viahttp");
    auto cfgPath = tmpConfig();
    nh::setup::ensureSecretKey(cfgPath);
    nh::Config cfg = baseConfig();
    cfg.listenAddress = "127.0.0.1";
    cfg.listenPort = freePort();
    const std::string token = "test-setup-token-0123456789";
    std::atomic<bool> result{false};
    std::thread srv([&] { result = nh::setup::runSetupServer(cfgPath, cfg, token); });
    httplib::Client cli("127.0.0.1", cfg.listenPort);
    for (int i = 0; i < 100 && !cli.Get("/health"); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto health = cli.Get("/health");
    ASSERT_TRUE(health);
    EXPECT_EQ(json::parse(health->body)["status"], "setup");
    // The applications recognise a Nhill server waiting for its installation.
    auto meta = cli.Get("/api/v1/meta");
    ASSERT_TRUE(meta);
    EXPECT_EQ(json::parse(meta->body)["product"], "Nhill");
    EXPECT_EQ(json::parse(meta->body)["mode"], "setup");
    json db = {{"provider", "postgres"}, {"host", "127.0.0.1"}, {"port", cl.port}, {"database", "viahttp"},
               {"user", "postgres"}, {"password", "super-secret-pw"}, {"sslmode", "disable"}};

    auto bad = cli.Post("/api/v1/setup/check", json{{"setup_token", "nope"}, {"database", db}}.dump(), "application/json");
    ASSERT_TRUE(bad);
    EXPECT_EQ(bad->status, 401);
    auto invalid = cli.Post("/api/v1/setup/check", json{{"setup_token", token}, {"database", {{"host", "x y"}}}}.dump(),
                            "application/json");
    EXPECT_EQ(invalid->status, 422);
    auto wrongPw = db;
    wrongPw["password"] = "wrong";
    auto unreachable =
        cli.Post("/api/v1/setup/check", json{{"setup_token", token}, {"database", wrongPw}}.dump(), "application/json");
    EXPECT_EQ(unreachable->status, 422);
    EXPECT_EQ(json::parse(unreachable->body)["error"]["code"], "database_unreachable");

    auto check = cli.Post("/api/v1/setup/check", json{{"setup_token", token}, {"database", db}}.dump(), "application/json");
    ASSERT_EQ(check->status, 200) << check->body;
    EXPECT_TRUE(json::parse(check->body)["can_create_roles"].get<bool>());

    cli.set_read_timeout(120, 0);
    auto apply = cli.Post("/api/v1/setup/apply",
                          json{{"setup_token", token},
                               {"database", db},
                               {"first_admin", {{"email", "ops@example.org"}, {"full_name", "Ops"}}}}
                              .dump(),
                          "application/json");
    ASSERT_TRUE(apply);
    ASSERT_EQ(apply->status, 200) << apply->body;
    auto r = json::parse(apply->body);
    EXPECT_TRUE(r["ok"].get<bool>());
    EXPECT_FALSE(r["first_admin"]["setup_token"].get<std::string>().empty());
    EXPECT_EQ(apply->get_header_value("Cache-Control"), "no-store");
    srv.join();  // the setup server stops by itself after a successful installation
    EXPECT_TRUE(result.load());
    EXPECT_NE(nh::readConfigFile(cfgPath)["NH_DATABASE_URL"].find("nhill_app"), std::string::npos);
}
