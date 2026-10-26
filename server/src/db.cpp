#include "db.hpp"

#include <chrono>

#include "util/log.hpp"

namespace nh::db {

Pool::Pool(std::string url, int size) : url_(std::move(url)), size_(size > 0 ? size : 1) {}

Pool::~Pool() = default;

std::unique_ptr<pqxx::connection> Pool::connect() {
    auto c = std::make_unique<pqxx::connection>(url_);
    pqxx::nontransaction n(*c);
    n.exec("SET TIME ZONE 'UTC'");
    n.exec("SET search_path TO nhill");
    n.exec("SET statement_timeout = '20s'");
    n.exec("SET lock_timeout = '10s'");
    n.exec("SET idle_in_transaction_session_timeout = '60s'");
    return c;
}

Pool::Lease Pool::acquire() {
    std::unique_lock<std::mutex> lk(m_);
    for (;;) {
        if (!idle_.empty()) {
            auto c = std::move(idle_.back());
            idle_.pop_back();
            if (c->is_open()) return Lease(this, std::move(c));
            --created_;
            continue;
        }
        if (created_ < size_) {
            ++created_;
            lk.unlock();
            try {
                return Lease(this, connect());
            } catch (...) {
                lk.lock();
                --created_;
                cv_.notify_one();
                throw;
            }
        }
        if (!cv_.wait_for(lk, std::chrono::seconds(10), [&] { return !idle_.empty() || created_ < size_; }))
            throw ApiError(503, "server_busy", "database pool exhausted");
    }
}

void Pool::release(std::unique_ptr<pqxx::connection> c) {
    std::lock_guard<std::mutex> lk(m_);
    if (c && c->is_open()) {
        idle_.push_back(std::move(c));
    } else {
        --created_;
    }
    cv_.notify_one();
}

Pool::Lease::~Lease() {
    if (pool_) {
        if (conn_) {
            pool_->release(std::move(conn_));
        } else {
            pool_->release(nullptr);
        }
    }
}

void Tx::setTenant(const std::string& establishmentId) {
    q("SELECT set_config('app.establishment_id', $1, true)", establishmentId);
    tenant_ = establishmentId;
}

}  // namespace nh::db
