// nhill-server — commands:
//   serve                         start the API (and scheduled jobs unless disabled);
//                                 without a database configured, starts in SETUP MODE
//                                 (first installation driven from Nhill Console)
//   setup --host H [--port P] [--database D] --user U [--provider supabase|postgres]
//         [--sslmode M] [--admin-email E --admin-name N]
//                                 headless installation; the administrator password is read
//                                 from NH_SETUP_DB_PASSWORD or the first line of stdin
//   migrate                       apply database migrations (owner role)
//   run-jobs                      run scheduled jobs once (cron/systemd timer)
//   admin-bootstrap EMAIL NAME    create the first administrator (only if none exists)
//   check                         verify configuration, database and pending migrations
//   version
// Options: --config FILE (KEY=VALUE, mode 600)
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>

#include <unistd.h>

#include "config.hpp"
#include "nhill/version.hpp"
#include "http/app.hpp"
#include "migrate.hpp"
#include "setup.hpp"
#include "pdf/pdf.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"

namespace {

nh::App* gApp = nullptr;
std::atomic<bool> gStop{false};
std::condition_variable gStopCv;
std::mutex gStopMutex;

void onSignal(int) {
    gStop = true;
    gStopCv.notify_all();
    if (gApp) gApp->stop();
}

int usage() {
    std::cerr << "usage: nhill-server [--config FILE] serve|setup ...|migrate|run-jobs|check|version|admin-bootstrap EMAIL NAME\n";
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::optional<std::string> configFile;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--config" && i + 1 < argc) configFile = argv[++i];
        else args.push_back(a);
    }
    if (args.empty()) return usage();
    const std::string cmd = args[0];
    if (cmd == "version") {
        std::cout << nhill::kProductName << " server " << nhill::kProductVersion << " (API " << nhill::kApiVersion
                  << ")\n";
        return 0;
    }
    try {
        nh::crypto::init();
        const bool needsDatabase = !std::getenv("NH_DATABASE_URL") &&
                                   (!configFile || !std::filesystem::exists(*configFile) ||
                                    nh::readConfigFile(*configFile).count("NH_DATABASE_URL") == 0);
        if ((cmd == "serve" || cmd == "setup") && needsDatabase) {
            if (!configFile) throw std::runtime_error("first installation: pass --config FILE (where the server keeps its configuration)");
            if (nh::setup::ensureSecretKey(*configFile)) nh::log::info("generated NH_SECRET_KEY in " + *configFile);
        }
        nh::Config cfg = nh::Config::load(configFile);

        if (cmd == "setup") {
            if (!needsDatabase) throw std::runtime_error("a database is already configured in " + *configFile);
            nh::setup::json t = {{"provider", "postgres"}};
            std::optional<std::pair<std::string, std::string>> admin;
            std::string adminEmail, adminName;
            for (size_t i = 1; i + 1 < args.size(); i += 2) {
                const std::string& k = args[i];
                const std::string& v = args[i + 1];
                if (k == "--port") t["port"] = std::stoi(v);
                else if (k == "--admin-email") adminEmail = v;
                else if (k == "--admin-name") adminName = v;
                else if (k.rfind("--", 0) == 0) t[k.substr(2)] = v;
            }
            if (const char* pw = std::getenv("NH_SETUP_DB_PASSWORD")) {
                t["password"] = pw;
            } else {
                std::string line;
                std::getline(std::cin, line);
                t["password"] = line;
            }
            if (!adminEmail.empty()) admin = std::make_pair(adminEmail, adminName.empty() ? adminEmail : adminName);
            auto report = nh::setup::install(*configFile, cfg, nh::setup::Target::fromJson(t), admin);
            // Shown once (backup account password, first administrator token): never stored by the server.
            std::cout << report.dump(2) << "\n";
            return report["ok"].get<bool>() ? 0 : 1;
        }
        if (cmd == "serve" && cfg.databaseUrl.empty()) {
            if (!nh::setup::runSetupServer(*configFile, cfg)) return 1;
            nh::log::info("setup completed: restarting in normal mode");
            execv("/proc/self/exe", argv);
            throw std::runtime_error("restart failed: start the server again");
        }

        if (cmd == "migrate") {
            if (cfg.migrationDatabaseUrl.empty() && cfg.databaseUrl.empty()) {
                // Not installed yet: `serve` starts in setup mode (Nhill Console installs it).
                std::cout << "no database configured: nothing to migrate (the server starts in setup mode)\n";
                return 0;
            }
            if (cfg.migrationDatabaseUrl.empty()) throw std::runtime_error("NH_MIGRATION_DATABASE_URL is required");
            auto r = nh::runMigrations(cfg.migrationDatabaseUrl, cfg.migrationsDir);
            std::cout << "applied: " << r.applied.size() << ", already applied: " << r.alreadyApplied.size() << "\n";
            return 0;
        }

        if (cfg.databaseUrl.empty()) throw std::runtime_error("NH_DATABASE_URL is required");
        auto pending = nh::pendingMigrations(cfg.databaseUrl, cfg.migrationsDir);
        if (!pending.empty()) throw std::runtime_error("pending migrations (" + pending.front() + "...): run `migrate` first");

        nh::db::Pool pool(cfg.databaseUrl, cfg.dbPoolSize);
        nh::Profiles profiles;
        profiles.loadDir(cfg.profileDir);
        nh::I18n i18n;
        i18n.loadDir(cfg.i18nDir);
        nh::Env env{cfg, pool, profiles, i18n};
        nh::pdf::Fonts::fromDir(cfg.fontDir);  // fails early if fonts are missing

        if (cmd == "check") {
            profiles.syncToDb(pool);
            std::cout << "configuration OK, database reachable, no pending migration\n";
            return 0;
        }
        if (cmd == "run-jobs") {
            profiles.syncToDb(pool);
            auto s = nh::jobs::runAll(env);
            std::cout << "codes rotated: " << s.codesRotated << " (failures " << s.codeRotationFailures << "), reminders: "
                      << s.remindersSent << ", grace: " << s.enteredGrace << ", suspended: " << s.suspended
                      << ", emails: " << s.emailsSent << "\n";
            return s.codeRotationFailures > 0 ? 1 : 0;
        }
        if (cmd == "admin-bootstrap") {
            if (args.size() < 3) return usage();
            std::string email = args[1], name = args[2];
            for (auto& ch : email) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            std::string token = nh::crypto::randomToken(24);
            std::string expires = nh::db::run(pool, std::nullopt, [&](nh::db::Tx& tx) {
                tx.q("LOCK TABLE admin_users IN EXCLUSIVE MODE");
                if (tx.one("SELECT count(*) FROM admin_users")[0].as<int>() > 0)
                    throw std::runtime_error("an administrator already exists: use the console to invite others");
                auto r = tx.one("INSERT INTO admin_users (email, full_name, role, setup_token_hash, setup_expires_at) "
                                "VALUES ($1, $2, 'superadmin', decode($3,'hex'), now() + interval '30 minutes') "
                                "RETURNING id, nh_iso(setup_expires_at)",
                                email, name, nh::crypto::sha256Hex(token));
                nh::auditSystem(tx, std::nullopt, "admin.bootstrapped", "admin_user", nh::db::str(r[0]), {{"email", email}});
                return nh::db::str(r[1]);
            });
            std::cout << "First administrator created (pending).\n"
                      << "One-time setup token (valid until " << expires << "):\n"
                      << token << "\n"
                      << "Open the admin console, choose 'First access', and enter this token.\n";
            return 0;
        }
        if (cmd != "serve") return usage();

        profiles.syncToDb(pool);
        nh::App app(cfg, pool);
        nh::registerAll(app, env);
        gApp = &app;
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);

        std::thread scheduler;
        if (cfg.jobsIntervalSeconds > 0) {
            scheduler = std::thread([&] {
                while (!gStop) {
                    try {
                        auto s = nh::jobs::runAll(env);
                        if (s.codesRotated || s.enteredGrace || s.suspended || s.remindersSent)
                            nh::log::info("jobs: codes " + std::to_string(s.codesRotated) + ", reminders " +
                                          std::to_string(s.remindersSent) + ", grace " + std::to_string(s.enteredGrace) +
                                          ", suspended " + std::to_string(s.suspended));
                    } catch (const std::exception& e) {
                        nh::log::error(std::string("jobs: ") + e.what());
                    }
                    std::unique_lock<std::mutex> lk(gStopMutex);
                    gStopCv.wait_for(lk, std::chrono::seconds(cfg.jobsIntervalSeconds), [] { return gStop.load(); });
                }
            });
        }
        nh::log::info(std::string("Nhill server ") + nhill::kProductVersion + " listening on " + cfg.listenAddress +
                      ":" + std::to_string(cfg.listenPort));
        bool ok = app.listen(cfg.listenAddress, cfg.listenPort);
        gStop = true;
        gStopCv.notify_all();
        if (scheduler.joinable()) scheduler.join();
        if (!ok && !gStop) throw std::runtime_error("cannot listen on the configured address");
        return 0;
    } catch (const std::exception& e) {
        nh::log::error(e.what());
        return 1;
    }
}
