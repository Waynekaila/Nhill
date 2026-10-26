// Establishment access codes: 12 Crockford Base32 symbols (60 bits),
// displayed as "NH-XXXX-XXXX-XXXX". A code is a renewable *rattachement*
// identifier; it is never sufficient to authenticate anybody.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace nhill::code {

constexpr int kSymbols = 12;
constexpr std::string_view kPrefix = "NH";

// Crockford alphabet without I, L, O, U.
constexpr std::string_view kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

// Fills `out` with `n` cryptographically secure random bytes.
using RandomSource = std::function<void(uint8_t* out, size_t n)>;

// Generates the 12 canonical symbols (no prefix, no dashes).
std::string generate(const RandomSource& rng);

// Normalises user input: uppercase, strips spaces/dashes and the optional
// "NH" prefix, maps O->0 and I/L->1. Returns nullopt if the result is not
// exactly 12 valid symbols.
std::optional<std::string> normalize(std::string_view input);

// "NH-XXXX-XXXX-XXXX"
std::string display(std::string_view canonical);

}  // namespace nhill::code
