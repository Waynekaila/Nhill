#include "nhill/validation.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace nhill::validation {

std::string trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

std::string toLowerAscii(std::string_view s) {
    std::string out(s);
    for (auto& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return out;
}

bool isValidEmail(std::string_view raw) {
    std::string e = trim(raw);
    if (e.empty() || e.size() > 254) return false;
    auto at = e.find('@');
    if (at == std::string::npos || at == 0 || at != e.rfind('@')) return false;
    std::string local = e.substr(0, at), domain = e.substr(at + 1);
    if (local.size() > 64 || domain.size() < 3) return false;
    auto dot = domain.rfind('.');
    if (dot == std::string::npos || dot == 0 || dot == domain.size() - 1) return false;
    if (domain.find("..") != std::string::npos || local.find("..") != std::string::npos) return false;
    for (char ch : e) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (c <= 0x20 || c == 0x7F || ch == ',' || ch == ';' || ch == '<' || ch == '>' || ch == '"' ||
            ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '\\')
            return false;
    }
    return true;
}

std::string normalizeEmail(std::string_view email) { return toLowerAscii(trim(email)); }

std::optional<std::string> normalizePhone(std::string_view phone) {
    std::string out;
    std::string t = trim(phone);
    for (size_t i = 0; i < t.size(); ++i) {
        char ch = t[i];
        if (ch == '+' && out.empty()) {
            out.push_back('+');
        } else if (ch >= '0' && ch <= '9') {
            out.push_back(ch);
        } else if (ch == ' ' || ch == '-' || ch == '.' || ch == '(' || ch == ')') {
            continue;
        } else {
            return std::nullopt;
        }
    }
    size_t digits = static_cast<size_t>(std::count_if(out.begin(), out.end(), [](char c) { return c >= '0' && c <= '9'; }));
    if (digits < 8 || digits > 15) return std::nullopt;
    return out;
}

std::optional<std::string> normalizeLegalId(std::string_view id) {
    std::string out;
    for (char ch : id) {
        unsigned char c = static_cast<unsigned char>(ch);
        if (ch == ' ' || ch == '.' || ch == '-' || ch == '/' || ch == '_' || ch == '\t') continue;
        if (c < 0x20 || c == 0x7F) return std::nullopt;
        out.push_back(static_cast<char>(std::toupper(c)));
    }
    if (out.empty() || out.size() > 64) return std::nullopt;
    return out;
}

PasswordIssue checkPassword(std::string_view password, std::string_view email) {
    auto len = utf8Length(password);
    if (!len || *len < kMinPasswordLength) return PasswordIssue::TooShort;
    if (password.size() > kMaxPasswordBytes) return PasswordIssue::TooLong;
    if (!email.empty() && toLowerAscii(password) == normalizeEmail(email)) return PasswordIssue::SameAsEmail;
    // Very common choices of 8 characters or more (whole-password comparison, lower case).
    // Since 2.0.0 the minimum is 8 characters, so 8- and 9-character favourites are listed too.
    static constexpr std::array<std::string_view, 40> kCommon{
        "1234567890", "0123456789", "motdepasse", "motdepasse1", "password12", "password123",
        "azertyuiop", "qwertyuiop", "1111111111", "abcdefghij", "nhill1", "nhill123",
        "12345678", "123456789", "87654321", "98765432", "password", "password1", "passw0rd",
        "azerty12", "azerty123", "qwerty12", "qwerty123", "azertyui", "qwertyui", "abcd1234",
        "abc12345", "1q2w3e4r", "1qaz2wsx", "iloveyou", "sunshine", "football", "baseball",
        "bienvenue", "welcome1", "jetaime1", "nhill2026", "nhill123456", "kinshasa", "congo123"};
    std::string lower = toLowerAscii(password);
    for (auto c : kCommon)
        if (lower == c) return PasswordIssue::TooCommon;
    // Repetition of one code point (UTF-8 aware): "aaaaaaaaaa", "éééééééééé".
    unsigned char c0 = static_cast<unsigned char>(password[0]);
    size_t n = c0 < 0x80 ? 1 : (c0 >> 5) == 0x6 ? 2 : (c0 >> 4) == 0xE ? 3 : 4;
    bool repetitive = password.size() % n == 0;
    for (size_t i = n; repetitive && i < password.size(); i += n)
        repetitive = password.substr(i, n) == password.substr(0, n);
    if (repetitive) return PasswordIssue::Repetitive;
    return PasswordIssue::None;
}

const char* passwordIssueCode(PasswordIssue p) {
    switch (p) {
        case PasswordIssue::None: return "ok";
        case PasswordIssue::TooShort: return "password_too_short";
        case PasswordIssue::TooLong: return "password_too_long";
        case PasswordIssue::SameAsEmail: return "password_same_as_email";
        case PasswordIssue::TooCommon: return "password_too_common";
        case PasswordIssue::Repetitive: return "password_repetitive";
    }
    return "password_invalid";
}

bool isValidLanguage(std::string_view lang) { return lang == "fr" || lang == "en"; }

bool isValidCountryCode(std::string_view cc) {
    return cc.size() == 2 && std::isupper(static_cast<unsigned char>(cc[0])) &&
           std::isupper(static_cast<unsigned char>(cc[1]));
}

std::optional<size_t> utf8Length(std::string_view s) {
    size_t count = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (n == 0 || i + n > s.size()) return std::nullopt;
        for (size_t k = 1; k < n; ++k)
            if ((static_cast<unsigned char>(s[i + k]) >> 6) != 0x2) return std::nullopt;
        i += n;
        ++count;
    }
    return count;
}

bool isValidText(std::string_view s, size_t maxLen, bool allowEmpty) {
    std::string t = trim(s);
    if (t.empty()) return allowEmpty;
    auto len = utf8Length(t);
    if (!len || *len > maxLen) return false;
    for (char ch : t) {
        unsigned char c = static_cast<unsigned char>(ch);
        if ((c < 0x20 && ch != '\n') || c == 0x7F) return false;
    }
    return true;
}

}  // namespace nhill::validation
