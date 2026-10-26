// Establishment codes: generation, storage (HMAC + encrypted), rotation.
#include <mutex>

#include "nhill/establishment_code.hpp"
#include "services/services.hpp"
#include "util/crypto.hpp"
#include "util/log.hpp"

namespace nh::codes {

namespace {
std::mutex genMutex;
std::function<std::string()> testGenerator;

std::string generateCanonical() {
    {
        std::lock_guard<std::mutex> lk(genMutex);
        if (testGenerator) return testGenerator();
    }
    return nhill::code::generate([](uint8_t* out, size_t n) { crypto::randomFill(out, n); });
}
}  // namespace

void setGeneratorForTests(std::function<std::string()> gen) {
    std::lock_guard<std::mutex> lk(genMutex);
    testGenerator = std::move(gen);
}

std::string issue(Env& env, db::Tx& tx, const std::string& establishmentId, const std::string& byType,
                  const std::optional<std::string>& byId) {
    const int days = setting(tx, "code_rotation_days", 30);
    // A collision of 60-bit codes is astronomically unlikely, but the unique
    // index on code_hmac is the authority: retry with a new code if it fires.
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::string canonical = generateCanonical();
        std::string mac = crypto::hmacSha256Hex(env.cfg.codeKey, canonical);
        auto sealed = crypto::seal(env.cfg.dataKey, canonical);
        pqxx::subtransaction sub(tx.work(), "code_issue");
        try {
            sub.exec_params(
                "INSERT INTO establishment_codes (establishment_id, code_hmac, code_cipher, code_nonce, status, "
                "expires_at, created_by_type, created_by) VALUES ($1, decode($2,'hex'), decode($3,'hex'), "
                "decode($4,'hex'), 'active', now() + make_interval(days => $5), $6, $7)",
                establishmentId, mac, crypto::toHex(sealed.cipher), crypto::toHex(sealed.nonce), days, byType, byId);
            sub.commit();
            return nhill::code::display(canonical);
        } catch (const pqxx::unique_violation& e) {
            sub.abort();
            if (std::string(e.what()).find("establishment_codes_one_active") != std::string::npos) throw;
            log::warn("establishment code collision, regenerating");
        }
    }
    throw ApiError(500, "code_generation_failed", "could not generate a unique establishment code");
}

std::string rotate(Env& env, db::Tx& tx, const std::string& establishmentId, const std::string& endStatus,
                   const std::string& reason, const std::string& byType, const std::optional<std::string>& byId) {
    tx.q("UPDATE establishment_codes SET status = $2, ended_at = now(), ended_reason = $3 "
         "WHERE establishment_id = $1 AND status = 'active'",
         establishmentId, endStatus, reason);
    return issue(env, tx, establishmentId, byType, byId);
}

std::optional<Current> current(Env& env, db::Tx& tx, const std::string& establishmentId) {
    auto row = tx.opt(
        "SELECT encode(code_cipher,'hex'), encode(code_nonce,'hex'), nh_iso(created_at), nh_iso(expires_at) "
        "FROM establishment_codes WHERE establishment_id = $1 AND status = 'active'",
        establishmentId);
    if (!row) return std::nullopt;
    auto cipher = crypto::fromHex(db::str((*row)[0]));
    auto nonce = crypto::fromHex(db::str((*row)[1]));
    if (!cipher || !nonce) return std::nullopt;
    auto plain = crypto::open(env.cfg.dataKey, *cipher, *nonce);
    if (!plain) {
        log::error("establishment code cannot be decrypted (key mismatch?) for " + establishmentId);
        return std::nullopt;
    }
    return Current{nhill::code::display(*plain), db::str((*row)[2]), db::str((*row)[3])};
}

std::optional<std::string> resolve(Env& env, db::Tx& tx, const std::string& canonicalCode) {
    std::string mac = crypto::hmacSha256Hex(env.cfg.codeKey, canonicalCode);
    auto row = tx.opt(
        "SELECT c.establishment_id FROM establishment_codes c JOIN establishments e ON e.id = c.establishment_id "
        "WHERE c.code_hmac = decode($1,'hex') AND c.status = 'active' AND c.expires_at > now() "
        "AND e.status = 'active'",
        mac);
    if (!row) return std::nullopt;
    return db::str((*row)[0]);
}

}  // namespace nh::codes
