#pragma once

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <string>

namespace nh::log {

enum class Level { Debug, Info, Warn, Error };

inline Level& threshold() {
    static Level l = Level::Info;
    return l;
}

// One JSON object per line on stderr (journald-friendly). Never log secrets,
// tokens, passwords or establishment codes.
inline void write(Level lvl, const std::string& msg) {
    if (lvl < threshold()) return;
    static std::mutex m;
    const char* names[] = {"debug", "info", "warn", "error"};
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);
    char ts[32];
    std::strftime(ts, sizeof ts, "%Y-%m-%dT%H:%M:%SZ", &tm);
    std::string escaped;
    for (char c : msg) {
        if (c == '"' || c == '\\') escaped.push_back('\\');
        if (c == '\n') {
            escaped += "\\n";
            continue;
        }
        escaped.push_back(c);
    }
    std::lock_guard<std::mutex> lk(m);
    std::fprintf(stderr, "{\"ts\":\"%s\",\"level\":\"%s\",\"msg\":\"%s\"}\n", ts, names[static_cast<int>(lvl)],
                 escaped.c_str());
}

inline void debug(const std::string& m) { write(Level::Debug, m); }
inline void info(const std::string& m) { write(Level::Info, m); }
inline void warn(const std::string& m) { write(Level::Warn, m); }
inline void error(const std::string& m) { write(Level::Error, m); }

}  // namespace nh::log
