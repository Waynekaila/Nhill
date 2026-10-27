#include "config.hpp"

#include <sys/stat.h>

#include <cstdlib>
#include <fstream>
#include <stdexcept>

#include "util/log.hpp"

extern char** environ;

namespace nh {

namespace {

std::map<std::string, std::string> readFileImpl(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) throw std::runtime_error("config file not found: " + path);
    if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0 && !std::getenv("NH_ALLOW_INSECURE_CONFIG"))
        throw std::runtime_error("config file " + path + " must not be readable by group/others (chmod 600)");
    std::ifstream in(path);
    std::map<std::string, std::string> kv;
    std::string line;
    while (std::getline(in, line)) {
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(start, eq - start);
        while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
        std::string v = line.substr(eq + 1);
        while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
        if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') || (v.front() == '\'' && v.back() == '\'')))
            v = v.substr(1, v.size() - 2);
        kv[k] = v;
    }
    return kv;
}

}  // namespace

// First line of a secret file (mode 600), without the trailing newline.
std::string readSecretFile(const std::string& path) {
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) throw std::runtime_error("secret file not found: " + path);
    if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0 && !std::getenv("NH_ALLOW_INSECURE_CONFIG"))
        throw std::runtime_error("secret file " + path + " must not be readable by group/others (chmod 600)");
    std::ifstream in(path);
    std::string line;
    std::getline(in, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    return line;
}

// SMTP settings: a scheme the server knows, a host, a sender address, and TLS unless the
// relay is on this machine. Messages never contain the password.
void validateSmtp(const Config& c) {
    std::string url = c.smtpUrl;
    std::string scheme, rest;
    if (url.rfind("smtps://", 0) == 0) {
        scheme = "smtps";
        rest = url.substr(8);
    } else if (url.rfind("smtp://", 0) == 0) {
        scheme = "smtp";
        rest = url.substr(7);
    } else {
        throw std::runtime_error("NH_SMTP_URL must start with smtps:// or smtp:// (NH_EMAIL_TRANSPORT=smtp)");
    }
    std::string host = rest.substr(0, rest.find_first_of("/?"));
    if (host.find('@') != std::string::npos)
        throw std::runtime_error("NH_SMTP_URL must not contain credentials: use NH_SMTP_USERNAME / NH_SMTP_PASSWORD");
    if (!host.empty() && host.front() == '[') {
        host = host.substr(1, host.find(']') == std::string::npos ? std::string::npos : host.find(']') - 1);
    } else {
        host = host.substr(0, host.find(':'));
    }
    if (host.empty()) throw std::runtime_error("NH_SMTP_URL has no host");
    bool loopback = host == "127.0.0.1" || host == "::1" || host == "localhost";
    if (!c.smtpTls && !loopback)
        throw std::runtime_error("NH_SMTP_TLS=off is only accepted for a relay on this machine (127.0.0.1, ::1)");
    if (!c.smtpTls && scheme == "smtps") throw std::runtime_error("NH_SMTP_TLS=off contradicts smtps://");
    auto lt = c.smtpFrom.find('<');
    std::string addr = lt == std::string::npos ? c.smtpFrom : c.smtpFrom.substr(lt + 1, c.smtpFrom.find('>') - lt - 1);
    if (addr.find('@') == std::string::npos || addr.find_first_of(" \r\n") != std::string::npos ||
        c.smtpFrom.find_first_of("\r\n") != std::string::npos)
        throw std::runtime_error("NH_SMTP_FROM must be a sender address, for example: Nhill <no-reply@example.com>");
    if (!c.smtpUsername.empty() && c.smtpPassword.empty())
        throw std::runtime_error("NH_SMTP_USERNAME is set but NH_SMTP_PASSWORD / NH_SMTP_PASSWORD_FILE is empty");
}

std::map<std::string, std::string> readConfigFile(const std::string& path) { return readFileImpl(path); }

Config Config::load(const std::optional<std::string>& file) {
    std::map<std::string, std::string> kv;
    if (file) kv = readFileImpl(*file);
    for (char** e = environ; *e; ++e) {
        std::string s(*e);
        if (s.rfind("NH_", 0) != 0) continue;
        auto eq = s.find('=');
        kv[s.substr(0, eq)] = s.substr(eq + 1);
    }
    auto get = [&](const std::string& k, const std::string& def = "") {
        auto it = kv.find(k);
        return it == kv.end() ? def : it->second;
    };
    auto getInt = [&](const std::string& k, int def) {
        auto v = get(k);
        return v.empty() ? def : std::stoi(v);
    };

    Config c;
    c.databaseUrl = get("NH_DATABASE_URL");
    c.migrationDatabaseUrl = get("NH_MIGRATION_DATABASE_URL");
    c.listenAddress = get("NH_LISTEN_ADDRESS", c.listenAddress);
    c.listenPort = getInt("NH_LISTEN_PORT", c.listenPort);
    c.threads = getInt("NH_THREADS", c.threads);
    c.dbPoolSize = getInt("NH_DB_POOL_SIZE", c.threads);
    c.trustProxy = get("NH_TRUST_PROXY") == "1" || get("NH_TRUST_PROXY") == "true";
    c.jobsIntervalSeconds = getInt("NH_JOBS_INTERVAL_SECONDS", c.jobsIntervalSeconds);
    c.profileDir = get("NH_PROFILE_DIR", "/usr/share/nhill/profiles");
    c.i18nDir = get("NH_I18N_DIR", "/usr/share/nhill/i18n");
    c.fontDir = get("NH_FONT_DIR", "/usr/share/nhill/fonts");
    c.migrationsDir = get("NH_MIGRATIONS_DIR", "/usr/share/nhill/migrations");
    c.emailTransport = get("NH_EMAIL_TRANSPORT", "log");
    if (c.emailTransport != "log" && c.emailTransport != "smtp" && c.emailTransport != "disabled")
        throw std::runtime_error("NH_EMAIL_TRANSPORT must be log, smtp or disabled");
    c.smtpUrl = get("NH_SMTP_URL");
    c.smtpUsername = get("NH_SMTP_USERNAME");
    c.smtpPassword = get("NH_SMTP_PASSWORD");
    if (c.smtpPassword.empty() && !get("NH_SMTP_PASSWORD_FILE").empty()) {
        // Same permission rule as the configuration file (not readable by group/others).
        auto pw = readSecretFile(get("NH_SMTP_PASSWORD_FILE"));
        c.smtpPassword = pw;
    }
    c.smtpFrom = get("NH_SMTP_FROM");
    c.smtpCaFile = get("NH_SMTP_CA_FILE");
    c.smtpTimeoutSeconds = getInt("NH_SMTP_TIMEOUT_SECONDS", c.smtpTimeoutSeconds);
    {
        std::string tls = get("NH_SMTP_TLS", "required");
        if (tls != "required" && tls != "off") throw std::runtime_error("NH_SMTP_TLS must be required or off");
        c.smtpTls = tls == "required";
    }
    if (c.emailTransport == "smtp") validateSmtp(c);
    c.publicBaseUrl = get("NH_PUBLIC_BASE_URL");
    while (!c.publicBaseUrl.empty() && c.publicBaseUrl.back() == '/') c.publicBaseUrl.pop_back();
    c.argon2.opsLimit = static_cast<unsigned long long>(getInt("NH_ARGON2_OPS", 3));
    c.argon2.memLimitBytes = static_cast<size_t>(getInt("NH_ARGON2_MEM_KIB", 65536)) * 1024;
    c.accessTokenMinutes = getInt("NH_ACCESS_TOKEN_MINUTES", 15);
    c.refreshTokenDays = getInt("NH_REFRESH_TOKEN_DAYS", 30);

    auto master = crypto::fromHex(get("NH_SECRET_KEY"));
    if (!master || master->size() != 32)
        throw std::runtime_error("NH_SECRET_KEY must be 64 hexadecimal characters (32 random bytes)");
    if (master->find_first_not_of(master->front()) == std::string::npos)
        throw std::runtime_error("NH_SECRET_KEY is not random");
    c.masterKey = *master;

    for (const auto& [k, v] : kv) {
        const std::string prefix = "NH_WEBHOOK_SECRET_";
        if (k.rfind(prefix, 0) == 0 && !v.empty()) {
            if (v.size() < 32) throw std::runtime_error(k + " must be at least 32 characters");
            std::string provider = k.substr(prefix.size());
            for (auto& ch : provider) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            c.webhookSecrets[provider] = v;
        }
    }
    c.deriveKeys();
    return c;
}

void Config::deriveKeys() {
    codeKey = crypto::deriveKey(masterKey, 1, "NHCODE__");
    dataKey = crypto::deriveKey(masterKey, 2, "NHDATA__");
    fingerprintKey = crypto::deriveKey(masterKey, 3, "NHTRIAL_");
    providerKey = crypto::deriveKey(masterKey, 4, "NHPAYSEC");
    resetKey = crypto::deriveKey(masterKey, 5, "NHPWRST_");
}

}  // namespace nh
