#include "nhill/establishment_code.hpp"

#include <array>
#include <cctype>

namespace nhill::code {

std::string generate(const RandomSource& rng) {
    // 60 bits are needed; draw 8 bytes and use the low 60 bits.
    std::array<uint8_t, 8> buf{};
    rng(buf.data(), buf.size());
    uint64_t v = 0;
    for (uint8_t b : buf) v = (v << 8) | b;
    std::string out(kSymbols, '0');
    for (int i = kSymbols - 1; i >= 0; --i) {
        out[i] = kAlphabet[v & 31u];
        v >>= 5;
    }
    return out;
}

std::optional<std::string> normalize(std::string_view input) {
    std::string s;
    for (char ch : input) {
        if (ch == '-' || ch == ' ' || ch == '_' || ch == '.') continue;
        char u = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (u == 'O') u = '0';
        if (u == 'I' || u == 'L') u = '1';
        s.push_back(u);
    }
    if (s.size() == kSymbols + kPrefix.size() && s.compare(0, kPrefix.size(), kPrefix) == 0) {
        s.erase(0, kPrefix.size());
    }
    if (s.size() != kSymbols) return std::nullopt;
    for (char ch : s)
        if (kAlphabet.find(ch) == std::string_view::npos) return std::nullopt;
    return s;
}

std::string display(std::string_view canonical) {
    std::string out(kPrefix);
    for (size_t i = 0; i < canonical.size(); ++i) {
        if (i % 4 == 0) out.push_back('-');
        out.push_back(canonical[i]);
    }
    return out;
}

}  // namespace nhill::code
