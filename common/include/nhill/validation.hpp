// Validation rules shared by the client (instant feedback) and the server
// (authoritative). The server never trusts the client's result.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace nhill::validation {

std::string trim(std::string_view s);
std::string toLowerAscii(std::string_view s);

// Pragmatic e-mail check (local@domain.tld, no spaces, <= 254 chars).
bool isValidEmail(std::string_view email);
std::string normalizeEmail(std::string_view email);

// Keeps a leading '+' and digits; returns nullopt if fewer than 8 or more
// than 15 digits remain (E.164 bounds).
std::optional<std::string> normalizePhone(std::string_view phone);

// Legal identifiers (RCCM, Id. Nat., NIF...): uppercase, without spaces,
// dots, dashes or slashes. Empty result -> nullopt.
std::optional<std::string> normalizeLegalId(std::string_view id);

enum class PasswordIssue { None, TooShort, TooLong, SameAsEmail, TooCommon, Repetitive };
// NIST SP 800-63B style: length over composition rules.
PasswordIssue checkPassword(std::string_view password, std::string_view email);
const char* passwordIssueCode(PasswordIssue p);

bool isValidLanguage(std::string_view lang);   // "fr" | "en"
bool isValidCountryCode(std::string_view cc);  // ISO 3166-1 alpha-2 shape

// UTF-8 aware length in code points; invalid UTF-8 returns nullopt.
std::optional<size_t> utf8Length(std::string_view s);

// Name-like free text: trimmed, 1..max code points, no control characters.
bool isValidText(std::string_view s, size_t maxLen, bool allowEmpty = false);

}  // namespace nhill::validation
