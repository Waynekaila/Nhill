#include "harness.hpp"

#include <unistd.h>

#include <atomic>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

#include "migrate.hpp"
#include "util/crypto.hpp"

namespace nht {

namespace fs = std::filesystem;

namespace {

std::string sh(const std::string& cmd) {
    int rc = std::system(cmd.c_str());
    if (rc != 0) throw std::runtime_error("command failed (" + std::to_string(rc) + "): " + cmd);
    return {};
}

int freePort() {
    httplib::Server s;
    int p = s.bind_to_any_port("127.0.0.1");
    s.stop();
    return p;
}

}  // namespace

Harness& Harness::get() {
    static Harness h;
    return h;
}

Harness::Harness() {
    nh::crypto::init();
    startCluster();
    migrationsDir = std::string(NHILL_SOURCE_DIR) + "/db/migrations";
    nh::runMigrations(ownerUrl, migrationsDir);

    cfg_.databaseUrl = appUrl;
    cfg_.migrationDatabaseUrl = ownerUrl;
    cfg_.threads = 16;
    cfg_.dbPoolSize = 24;
    cfg_.jobsIntervalSeconds = 0;  // jobs are driven explicitly by the tests
    cfg_.profileDir = std::string(NHILL_SOURCE_DIR) + "/server/profiles";
    cfg_.i18nDir = std::string(NHILL_SOURCE_DIR) + "/server/i18n";
    cfg_.fontDir = std::string(NHILL_SOURCE_DIR) + "/assets/fonts";
    cfg_.migrationsDir = migrationsDir;
    cfg_.argon2.opsLimit = 1;               // fast hashing for tests only
    cfg_.argon2.memLimitBytes = 8 * 1024 * 1024;
    cfg_.masterKey = nh::crypto::randomBytes(32);
    cfg_.webhookSecrets["generic"] = "test-webhook-secret-0123456789abcdef";
    cfg_.deriveKeys();

    pool_ = std::make_unique<nh::db::Pool>(appUrl, cfg_.dbPoolSize);
    profiles_ = std::make_unique<nh::Profiles>();
    profiles_->loadDir(cfg_.profileDir);
    profiles_->syncToDb(*pool_);
    i18n_ = std::make_unique<nh::I18n>();
    i18n_->loadDir(cfg_.i18nDir);
    env_ = std::make_unique<nh::Env>(nh::Env{cfg_, *pool_, *profiles_, *i18n_});
    app_ = std::make_unique<nh::App>(cfg_, *pool_);
    nh::registerAll(*app_, *env_);
    port = app_->bindToAnyPort("127.0.0.1");
    serverThread_ = std::thread([this] { app_->listenAfterBind(); });
    for (int i = 0; i < 100 && !app_->server().is_running(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

Harness::~Harness() {
    if (app_) app_->stop();
    if (serverThread_.joinable()) serverThread_.join();
    app_.reset();
    pool_.reset();
    const char* keep = std::getenv("NH_TEST_KEEP_DB");
    if (!keep) {
        std::string ctl = "/usr/lib/postgresql/16/bin/pg_ctl -D " + dir_ + "/data -m immediate stop >/dev/null 2>&1";
        if (std::system((geteuid() == 0 ? "runuser -u postgres -- " + ctl : ctl).c_str()) != 0) { /* already stopped */ }
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
}

void Harness::startCluster() {
    std::string bin = "/usr/lib/postgresql/16/bin";
    if (const char* b = std::getenv("NH_PG_BIN")) bin = b;
    char tmpl[] = "/tmp/nhtest-XXXXXX";
    dir_ = mkdtemp(tmpl);
    const bool root = geteuid() == 0;
    // PostgreSQL refuses to run as root: delegate to the postgres account.
    const std::string as = root ? "runuser -u postgres -- " : "";
    if (root) sh("chown postgres:postgres " + dir_);
    pgPort_ = freePort();
    sh(as + bin + "/initdb -D " + dir_ + "/data -A trust -U postgres -E UTF8 --locale=C.UTF-8 >/dev/null");
    sh(as + bin + "/pg_ctl -D " + dir_ + "/data -o \"-p " + std::to_string(pgPort_) + " -k " + dir_ +
       " -c listen_addresses='' -c max_connections=200 -c fsync=off\" -l " + dir_ + "/log -w start >/dev/null");
    const std::string base = "host=" + dir_ + " port=" + std::to_string(pgPort_);
    sh("psql \"" + base + " user=postgres dbname=postgres\" -q -v ON_ERROR_STOP=1 -v owner_password=\"'owner-pw'\" "
       "-v app_password=\"'app-pw'\" -f " + std::string(NHILL_SOURCE_DIR) + "/db/bootstrap/00-roles.sql >/dev/null");
    superUrl = base + " user=postgres dbname=nhill";
    ownerUrl = base + " user=nhill_owner password=owner-pw dbname=nhill";
    appUrl = base + " user=nhill_app password=app-pw dbname=nhill";
}

pqxx::result Harness::su(const std::string& sql) {
    pqxx::connection c(superUrl);
    pqxx::work w(c);
    w.exec("SET LOCAL search_path TO nhill, public");
    auto r = w.exec(sql);
    w.commit();
    return r;
}

pqxx::result Harness::asApp(const std::optional<std::string>& tenant, const std::string& sql) {
    pqxx::connection c(appUrl);
    pqxx::work w(c);
    w.exec("SET LOCAL search_path TO nhill");
    if (tenant) w.exec_params("SELECT set_config('app.establishment_id', $1, true)", *tenant);
    auto r = w.exec(sql);
    w.commit();
    return r;
}

Response Harness::request(const std::string& method, const std::string& path, const json& body, const std::string& token,
                          const std::string& idemKey, const std::string& contentType, const std::string& rawBody) {
    httplib::Client cli("127.0.0.1", port);
    cli.set_read_timeout(60, 0);
    httplib::Headers h;
    if (!token.empty()) h.emplace("Authorization", "Bearer " + token);
    if (!idemKey.empty()) h.emplace("Idempotency-Key", idemKey);
    h.emplace("Accept-Language", "fr");
    std::string payload = rawBody.empty() ? (body.is_null() ? std::string() : body.dump()) : rawBody;
    httplib::Result res(nullptr, httplib::Error::Unknown);
    if (method == "GET") res = cli.Get(path, h);
    else if (method == "POST") res = cli.Post(path, h, payload, contentType);
    else if (method == "PUT") res = cli.Put(path, h, payload, contentType);
    else if (method == "PATCH") res = cli.Patch(path, h, payload, contentType);
    else if (method == "DELETE") res = cli.Delete(path, h);
    Response r;
    if (!res) throw std::runtime_error("HTTP request failed: " + httplib::to_string(res.error()));
    r.status = res->status;
    r.headers = res->headers;
    r.raw = res->body;
    auto ct = res->get_header_value("Content-Type");
    if (ct.find("json") != std::string::npos && !res->body.empty()) r.body = json::parse(res->body);
    return r;
}

std::string uniq(const std::string& prefix) {
    static std::atomic<int> counter{0};
    static const std::string run = nh::crypto::toHex(nh::crypto::randomBytes(3));
    return prefix + run + std::to_string(++counter);
}

std::string newKey() { return "idem-" + nh::crypto::toHex(nh::crypto::randomBytes(12)); }

// All test traffic comes from 127.0.0.1: per-IP throttles are reset by the
// helpers (the per-account throttles stay active and are tested separately).
void resetIpThrottles() { Harness::get().su("DELETE FROM auth_throttle WHERE bucket LIKE '%:ip:%'"); }

Tenant createTenant(const std::string& kind, json legalIds, bool vat, const std::string& email) {
    auto& h = Harness::get();
    resetIpThrottles();
    Tenant t;
    t.ownerEmail = email.empty() ? uniq("owner") + "@example.cd" : email;
    json body = {{"kind", kind},
                 {"trade_name", "Boutique " + uniq()},
                 {"country_code", "CD"},
                 {"city", "Kinshasa"},
                 {"address", "Avenue du Commerce 12, Gombe"},
                 {"preferred_language", "fr"},
                 {"main_currency", "CDF"},
                 {"vat_registered", vat},
                 {"legal_ids", legalIds},
                 {"owner", {{"full_name", "Propriétaire Test"}, {"email", t.ownerEmail}, {"password", t.password}}}};
    auto r = h.post("/api/v1/establishments", body);
    if (r.status != 201) throw std::runtime_error("createTenant failed: " + std::to_string(r.status) + " " + r.raw);
    t.id = r.body["establishment"]["id"];
    t.token = r.body["session"]["access_token"];
    t.refresh = r.body["session"]["refresh_token"];
    t.code = r.body["establishment_code"];
    return t;
}

std::string addMember(const Tenant& t, const std::string& role, std::string* emailOut) {
    auto& h = Harness::get();
    std::string email = uniq(role) + "@example.cd";
    resetIpThrottles();
    auto inv = h.post("/api/v1/invitations", {{"email", email}, {"role", role}}, t.token);
    if (inv.status != 201) throw std::runtime_error("invite failed: " + inv.raw);
    auto chk = h.post("/api/v1/join/check-code", {{"code", t.code}});
    if (chk.status != 200) throw std::runtime_error("check-code failed: " + chk.raw);
    auto act = h.post("/api/v1/join/activate", {{"join_ticket", chk.body["join_ticket"]},
                                                {"email", email},
                                                {"activation_code", inv.body["activation_code"]},
                                                {"full_name", "Membre " + role},
                                                {"password", "Membre-password-2026"}});
    if (act.status != 200) throw std::runtime_error("activate failed: " + act.raw);
    if (emailOut) *emailOut = email;
    return act.body["access_token"];
}

std::string createCustomer(const Tenant& t, const std::string& name) {
    auto r = Harness::get().post("/api/v1/parties", {{"is_customer", true}, {"name", name}, {"phone", "+243 81 000 0000"}}, t.token);
    if (r.status != 201) throw std::runtime_error("customer failed: " + r.raw);
    return r.body["id"];
}

std::string createProduct(const Tenant& t, const std::string& sku, const std::string& price, bool stock) {
    auto r = Harness::get().post("/api/v1/products",
                                 {{"sku", sku}, {"name", "Produit " + sku}, {"kind", stock ? "good" : "service"},
                                  {"sale_price", price}, {"purchase_price", "1.00"}, {"reorder_level", "2"}},
                                 t.token);
    if (r.status != 201) throw std::runtime_error("product failed: " + r.raw);
    return r.body["id"];
}

void adjustStock(const Tenant& t, const std::string& product, const std::string& qty) {
    auto r = Harness::get().post("/api/v1/stock/adjustments",
                                 {{"product_id", product}, {"quantity", qty}, {"reason", "Inventaire initial"}, {"kind", "opening"}},
                                 t.token, newKey());
    if (r.status != 201) throw std::runtime_error("adjust failed: " + r.raw);
}

std::string draftInvoice(const Tenant& t, const std::string& customer, const json& lines, const std::string& type,
                         const std::string& currency) {
    json b = {{"doc_type", type}, {"party_id", customer}, {"lines", lines}};
    if (!currency.empty()) b["currency"] = currency;
    auto r = Harness::get().post("/api/v1/sales-documents", b, t.token, newKey());
    if (r.status != 201) throw std::runtime_error("draft failed: " + r.raw);
    return r.body["id"];
}

Response issue(const Tenant& t, const std::string& docId, const std::string& idem) {
    return Harness::get().post("/api/v1/sales-documents/" + docId + "/issue", json::object(), t.token,
                               idem.empty() ? newKey() : idem);
}

// Creates an administrator through the same steps as the bootstrap command + console.
std::string adminLogin(const std::string& role) {
    auto& h = Harness::get();
    std::string email = uniq("admin") + "@nhill.test";
    std::string token = nh::crypto::randomToken(24);
    h.su("INSERT INTO admin_users (email, full_name, role, setup_token_hash, setup_expires_at) VALUES ('" + email +
         "', 'Admin', '" + role + "', decode('" + nh::crypto::sha256Hex(token) + "','hex'), now() + interval '30 minutes')");
    auto setup = h.post("/api/v1/admin/auth/setup", {{"email", email}, {"setup_token", token}, {"password", "Admin-password-2026-long"}});
    EXPECT_EQ(setup.status, 200) << setup.raw;
    std::string secret = *nh::crypto::base32Decode(setup.body["totp_secret"].get<std::string>());
    int64_t now = std::time(nullptr);
    auto confirm = h.post("/api/v1/admin/auth/setup/confirm",
                          {{"email", email}, {"setup_token", token}, {"totp_code", nh::crypto::totpCode(secret, now / 30)}});
    EXPECT_EQ(confirm.status, 204) << confirm.raw;
    // Same TOTP step cannot be replayed: use the next one.
    auto login = h.post("/api/v1/admin/auth/login", {{"email", email}, {"password", "Admin-password-2026-long"},
                                                     {"totp_code", nh::crypto::totpCode(secret, now / 30 + 1)}});
    EXPECT_EQ(login.status, 200) << login.raw;
    return login.body["token"];
}


}  // namespace nht
