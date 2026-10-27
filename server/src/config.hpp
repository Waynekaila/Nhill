#pragma once

#include <map>
#include <optional>
#include <string>

#include "util/crypto.hpp"

namespace nh {

struct Config {
    std::string databaseUrl;            // application role (RLS-bound)
    std::string migrationDatabaseUrl;   // schema owner, used by `migrate` only
    std::string listenAddress = "127.0.0.1";
    int listenPort = 8080;
    int threads = 16;
    int dbPoolSize = 16;
    bool trustProxy = false;            // honour X-Forwarded-For (only behind our proxy)
    int jobsIntervalSeconds = 300;      // 0 disables in-process scheduled jobs
    std::string profileDir;
    std::string i18nDir;
    std::string fontDir;
    std::string migrationsDir;
    std::string emailTransport = "log"; // "log" | "smtp" | "disabled"
    // SMTP relay (NH_EMAIL_TRANSPORT=smtp). TLS is mandatory: smtps:// (implicit TLS) or
    // smtp:// with STARTTLS required; NH_SMTP_TLS=off is accepted for a loopback relay only.
    std::string smtpUrl;                // smtps://smtp.example.com:465 | smtp://smtp.example.com:587
    std::string smtpUsername;
    std::string smtpPassword;           // secret (NH_SMTP_PASSWORD or NH_SMTP_PASSWORD_FILE), never logged
    std::string smtpFrom;               // "Nhill <no-reply@example.com>"
    std::string smtpCaFile;             // optional CA bundle (default: system store)
    bool smtpTls = true;
    int smtpTimeoutSeconds = 20;
    crypto::Argon2Params argon2;
    int accessTokenMinutes = 15;
    int refreshTokenDays = 30;
    int adminSessionHours = 8;

    // Secrets (never logged)
    std::string masterKey;              // 32 bytes
    std::string codeKey;                // derived: HMAC of establishment codes
    std::string dataKey;                // derived: secretbox for codes / TOTP secrets
    std::string fingerprintKey;         // derived: trial registry
    std::string providerKey;            // derived: secretbox for payment provider secrets
    std::string resetKey;               // derived: HMAC of password-recovery codes
    std::string publicBaseUrl;          // public HTTPS address of this API (webhook URLs, payment return pages)
    std::map<std::string, std::string> webhookSecrets;  // provider -> secret

    // Loads KEY=VALUE pairs from an optional file, then the environment
    // (environment wins). Throws std::runtime_error on missing/weak secrets.
    static Config load(const std::optional<std::string>& file);
    void deriveKeys();
};

// Raw KEY=VALUE pairs of a configuration file (same permission checks as load()).
std::map<std::string, std::string> readConfigFile(const std::string& path);

}  // namespace nh
