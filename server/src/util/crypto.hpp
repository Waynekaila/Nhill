#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nh::crypto {

// Must be called once before any other function (sodium_init).
void init();

std::string randomBytes(size_t n);
void randomFill(uint8_t* out, size_t n);
// URL-safe base64 (no padding) of n random bytes — used for opaque tokens.
std::string randomToken(size_t nBytes = 32);
// Decimal activation code of `digits` digits (uniform).
std::string randomDigits(int digits);

std::string toHex(std::string_view bytes);
std::optional<std::string> fromHex(std::string_view hex);
std::string base64UrlEncode(std::string_view bytes);
std::string base32Encode(std::string_view bytes);                 // RFC 4648, no padding
std::optional<std::string> base32Decode(std::string_view text);

std::string sha256(std::string_view data);                        // raw 32 bytes
std::string sha256Hex(std::string_view data);
std::string hmacSha256(std::string_view key, std::string_view data);  // raw 32 bytes
std::string hmacSha256Hex(std::string_view key, std::string_view data);
std::string hmacSha1(std::string_view key, std::string_view data);    // raw 20 bytes (TOTP)
bool constantTimeEquals(std::string_view a, std::string_view b);

struct Argon2Params {
    unsigned long long opsLimit = 3;
    size_t memLimitBytes = 64ull * 1024 * 1024;
};
std::string hashPassword(std::string_view password, const Argon2Params& p);
bool verifyPassword(std::string_view hash, std::string_view password);
bool passwordNeedsRehash(std::string_view hash, const Argon2Params& p);

// Authenticated encryption (XSalsa20-Poly1305). key must be 32 bytes.
struct Sealed {
    std::string cipher;
    std::string nonce;
};
Sealed seal(std::string_view key, std::string_view plaintext);
std::optional<std::string> open(std::string_view key, std::string_view cipher, std::string_view nonce);

// Derives a 32-byte sub-key from the 32-byte master key (libsodium KDF).
std::string deriveKey(std::string_view master, uint64_t id, const char context[8]);

// RFC 6238 TOTP (HMAC-SHA1, 6 digits, 30 s). Returns the matching time step
// within +/- `window` steps, or nullopt.
std::string totpCode(std::string_view secret, int64_t step);
std::optional<int64_t> totpVerify(std::string_view secret, std::string_view code, int64_t unixTime, int window = 1);

}  // namespace nh::crypto
