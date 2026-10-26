#pragma once

#include <pqxx/pqxx>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "errors.hpp"

namespace nh::db {

class Pool {
public:
    Pool(std::string url, int size);
    ~Pool();

    class Lease {
    public:
        Lease(Pool* p, std::unique_ptr<pqxx::connection> c) : pool_(p), conn_(std::move(c)) {}
        Lease(Lease&& o) noexcept : pool_(o.pool_), conn_(std::move(o.conn_)) { o.pool_ = nullptr; }
        ~Lease();
        pqxx::connection& conn() { return *conn_; }
        void discard() { conn_.reset(); }  // broken connection: do not return it

    private:
        Pool* pool_;
        std::unique_ptr<pqxx::connection> conn_;
    };

    Lease acquire();
    const std::string& url() const { return url_; }

private:
    friend class Lease;
    std::unique_ptr<pqxx::connection> connect();
    void release(std::unique_ptr<pqxx::connection> c);

    std::string url_;
    int size_;
    int created_ = 0;
    std::vector<std::unique_ptr<pqxx::connection>> idle_;
    std::mutex m_;
    std::condition_variable cv_;
};

// Thin helper around a pqxx transaction. All statements are parameterised.
class Tx {
public:
    explicit Tx(pqxx::work& w) : w_(w) {}

    template <typename... Args>
    pqxx::result q(std::string_view sql, Args&&... args) {
        return w_.exec_params(pqxx::zview(sql.data(), sql.size()), std::forward<Args>(args)...);
    }
    // Exactly one row, or ApiError 404.
    template <typename... Args>
    pqxx::row one(std::string_view sql, Args&&... args) {
        auto r = q(sql, std::forward<Args>(args)...);
        if (r.size() != 1) throw notFound();
        return r[0];
    }
    template <typename... Args>
    std::optional<pqxx::row> opt(std::string_view sql, Args&&... args) {
        auto r = q(sql, std::forward<Args>(args)...);
        if (r.empty()) return std::nullopt;
        return r[0];
    }

    void setTenant(const std::string& establishmentId);
    std::optional<std::string> tenant() const { return tenant_; }
    pqxx::work& work() { return w_; }

private:
    pqxx::work& w_;
    std::optional<std::string> tenant_;
};

// Runs `f(Tx&)` in a transaction, committing on success. Retries on
// serialization failures / deadlocks (the whole closure is re-executed).
template <typename F>
auto run(Pool& pool, const std::optional<std::string>& tenant, F&& f) -> decltype(f(std::declval<Tx&>())) {
    using R = decltype(f(std::declval<Tx&>()));
    for (int attempt = 0;; ++attempt) {
        auto lease = pool.acquire();
        try {
            pqxx::work w(lease.conn());
            Tx tx(w);
            if (tenant) tx.setTenant(*tenant);
            if constexpr (std::is_void_v<R>) {
                f(tx);
                w.commit();
                return;
            } else {
                R r = f(tx);
                w.commit();
                return r;
            }
        } catch (const pqxx::serialization_failure&) {
            if (attempt < 3) continue;
            throw;
        } catch (const pqxx::deadlock_detected&) {
            if (attempt < 3) continue;
            throw;
        } catch (const pqxx::broken_connection&) {
            lease.discard();
            if (attempt < 1) continue;
            throw;
        }
    }
}

// Helpers to read columns.
inline std::string str(const pqxx::field& f) { return f.is_null() ? std::string() : f.c_str(); }
inline std::optional<std::string> optStr(const pqxx::field& f) {
    if (f.is_null()) return std::nullopt;
    return std::string(f.c_str());
}
inline int64_t i64(const pqxx::field& f) { return f.is_null() ? 0 : f.as<int64_t>(); }
inline bool boolean(const pqxx::field& f) { return !f.is_null() && f.as<bool>(); }

}  // namespace nh::db
