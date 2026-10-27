#include "util/crypto.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <sodium.h>

#include <stdexcept>

namespace nh::crypto {

void init() {
    if (sodium_init() < 0) throw std::runtime_error("libsodium initialisation failed");
}

void randomFill(uint8_t* out, size_t n) { randombytes_buf(out, n); }

std::string randomBytes(size_t n) {
    std::string s(n, '\0');
    randombytes_buf(s.data(), n);
    return s;
}

std::string randomToken(size_t nBytes) { return base64UrlEncode(randomBytes(nBytes)); }

std::string randomDigits(int digits) {
    std::string out;
    for (int i = 0; i < digits; ++i) out.push_back(static_cast<char>('0' + randombytes_uniform(10)));
    return out;
}

std::string randomFromAlphabet(std::string_view alphabet, size_t n) {
    std::string out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i)
        out.push_back(alphabet[randombytes_uniform(static_cast<uint32_t>(alphabet.size()))]);
    return out;
}

std::string toHex(std::string_view bytes) {
    std::string out(bytes.size() * 2 + 1, '\0');
    sodium_bin2hex(out.data(), out.size(), reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
    out.pop_back();
    return out;
}

std::optional<std::string> fromHex(std::string_view hex) {
    if (hex.size() % 2 != 0) return std::nullopt;
    std::string out(hex.size() / 2, '\0');
    size_t len = 0;
    if (sodium_hex2bin(reinterpret_cast<unsigned char*>(out.data()), out.size(), hex.data(), hex.size(), nullptr,
                       &len, nullptr) != 0 ||
        len != out.size())
        return std::nullopt;
    return out;
}

std::string base64UrlEncode(std::string_view bytes) {
    const int variant = sodium_base64_VARIANT_URLSAFE_NO_PADDING;
    std::string out(sodium_base64_encoded_len(bytes.size(), variant), '\0');
    sodium_bin2base64(out.data(), out.size(), reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size(),
                      variant);
    out.resize(std::char_traits<char>::length(out.c_str()));
    return out;
}

namespace {
constexpr char kB32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
}

std::string base32Encode(std::string_view bytes) {
    std::string out;
    uint32_t buffer = 0;
    int bits = 0;
    for (unsigned char c : bytes) {
        buffer = (buffer << 8) | c;
        bits += 8;
        while (bits >= 5) {
            out.push_back(kB32[(buffer >> (bits - 5)) & 31]);
            bits -= 5;
        }
    }
    if (bits > 0) out.push_back(kB32[(buffer << (5 - bits)) & 31]);
    return out;
}

std::optional<std::string> base32Decode(std::string_view text) {
    std::string out;
    uint32_t buffer = 0;
    int bits = 0;
    for (char ch : text) {
        if (ch == '=' || ch == ' ') continue;
        char u = static_cast<char>(ch >= 'a' && ch <= 'z' ? ch - 32 : ch);
        const char* p = std::char_traits<char>::find(kB32, 32, u);
        if (!p) return std::nullopt;
        buffer = (buffer << 5) | static_cast<uint32_t>(p - kB32);
        bits += 5;
        if (bits >= 8) {
            out.push_back(static_cast<char>((buffer >> (bits - 8)) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

std::string sha256(std::string_view data) {
    std::string out(crypto_hash_sha256_BYTES, '\0');
    crypto_hash_sha256(reinterpret_cast<unsigned char*>(out.data()),
                       reinterpret_cast<const unsigned char*>(data.data()), data.size());
    return out;
}

std::string sha256Hex(std::string_view data) { return toHex(sha256(data)); }

std::string hmacSha256(std::string_view key, std::string_view data) {
    std::string out(crypto_auth_hmacsha256_BYTES, '\0');
    crypto_auth_hmacsha256_state st;
    crypto_auth_hmacsha256_init(&st, reinterpret_cast<const unsigned char*>(key.data()), key.size());
    crypto_auth_hmacsha256_update(&st, reinterpret_cast<const unsigned char*>(data.data()), data.size());
    crypto_auth_hmacsha256_final(&st, reinterpret_cast<unsigned char*>(out.data()));
    return out;
}

std::string hmacSha256Hex(std::string_view key, std::string_view data) { return toHex(hmacSha256(key, data)); }

std::string hmacSha1(std::string_view key, std::string_view data) {
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    if (!HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(data.data()), data.size(), md, &len))
        throw std::runtime_error("HMAC-SHA1 failed");
    return std::string(reinterpret_cast<char*>(md), len);
}

bool constantTimeEquals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    return sodium_memcmp(a.data(), b.data(), a.size()) == 0;
}

std::string hashPassword(std::string_view password, const Argon2Params& p) {
    char out[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str_alg(out, password.data(), password.size(), p.opsLimit, p.memLimitBytes,
                              crypto_pwhash_ALG_ARGON2ID13) != 0)
        throw std::runtime_error("password hashing failed (out of memory)");
    return std::string(out);
}

bool verifyPassword(std::string_view hash, std::string_view password) {
    std::string h(hash);
    return crypto_pwhash_str_verify(h.c_str(), password.data(), password.size()) == 0;
}

bool passwordNeedsRehash(std::string_view hash, const Argon2Params& p) {
    std::string h(hash);
    return crypto_pwhash_str_needs_rehash(h.c_str(), p.opsLimit, p.memLimitBytes) != 0;
}

Sealed seal(std::string_view key, std::string_view plaintext) {
    if (key.size() != crypto_secretbox_KEYBYTES) throw std::runtime_error("bad secretbox key");
    Sealed s;
    s.nonce = randomBytes(crypto_secretbox_NONCEBYTES);
    s.cipher.resize(plaintext.size() + crypto_secretbox_MACBYTES);
    crypto_secretbox_easy(reinterpret_cast<unsigned char*>(s.cipher.data()),
                          reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size(),
                          reinterpret_cast<const unsigned char*>(s.nonce.data()),
                          reinterpret_cast<const unsigned char*>(key.data()));
    return s;
}

std::optional<std::string> open(std::string_view key, std::string_view cipher, std::string_view nonce) {
    if (key.size() != crypto_secretbox_KEYBYTES || nonce.size() != crypto_secretbox_NONCEBYTES ||
        cipher.size() < crypto_secretbox_MACBYTES)
        return std::nullopt;
    std::string out(cipher.size() - crypto_secretbox_MACBYTES, '\0');
    if (crypto_secretbox_open_easy(reinterpret_cast<unsigned char*>(out.data()),
                                   reinterpret_cast<const unsigned char*>(cipher.data()), cipher.size(),
                                   reinterpret_cast<const unsigned char*>(nonce.data()),
                                   reinterpret_cast<const unsigned char*>(key.data())) != 0)
        return std::nullopt;
    return out;
}

std::string deriveKey(std::string_view master, uint64_t id, const char context[8]) {
    if (master.size() != crypto_kdf_KEYBYTES) throw std::runtime_error("bad master key length");
    std::string out(32, '\0');
    crypto_kdf_derive_from_key(reinterpret_cast<unsigned char*>(out.data()), out.size(), id, context,
                               reinterpret_cast<const unsigned char*>(master.data()));
    return out;
}

std::string totpCode(std::string_view secret, int64_t step) {
    unsigned char msg[8];
    for (int i = 7; i >= 0; --i) {
        msg[i] = static_cast<unsigned char>(step & 0xFF);
        step >>= 8;
    }
    std::string mac = hmacSha1(secret, std::string_view(reinterpret_cast<char*>(msg), 8));
    int offset = static_cast<unsigned char>(mac[19]) & 0x0F;
    uint32_t bin = ((static_cast<unsigned char>(mac[offset]) & 0x7F) << 24) |
                   (static_cast<unsigned char>(mac[offset + 1]) << 16) |
                   (static_cast<unsigned char>(mac[offset + 2]) << 8) | static_cast<unsigned char>(mac[offset + 3]);
    std::string code = std::to_string(bin % 1000000);
    while (code.size() < 6) code.insert(code.begin(), '0');
    return code;
}

std::optional<int64_t> totpVerify(std::string_view secret, std::string_view code, int64_t unixTime, int window) {
    if (code.size() != 6) return std::nullopt;
    int64_t step = unixTime / 30;
    for (int d = -window; d <= window; ++d) {
        if (constantTimeEquals(totpCode(secret, step + d), code)) return step + d;
    }
    return std::nullopt;
}

}  // namespace nh::crypto
