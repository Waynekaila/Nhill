#include "nhill/money.hpp"
#include "nhill/int128.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <map>

namespace nhill {

namespace {

struct CurrencyInfo {
    const char* code;
    int minor;
};

// Subset of ISO 4217 relevant to the target markets. Extend through the
// server `currencies` table; this list only serves validation/formatting.
// ISO 4217 minor units of the currencies the platform offers (migrations 0004 and 0013).
constexpr std::array<CurrencyInfo, 36> kCurrencies{{
    {"CDF", 2}, {"USD", 2}, {"EUR", 2}, {"XAF", 0}, {"XOF", 0}, {"RWF", 0},
    {"BIF", 0}, {"UGX", 0}, {"KES", 2}, {"TZS", 2}, {"ZAR", 2}, {"ZMW", 2},
    {"AOA", 2}, {"GBP", 2}, {"CHF", 2}, {"CAD", 2}, {"CNY", 2}, {"JPY", 0},
    {"AED", 2}, {"NGN", 2}, {"GHS", 2}, {"MAD", 2}, {"AUD", 2}, {"BRL", 2},
    {"DKK", 2}, {"DZD", 2}, {"EGP", 2}, {"ETB", 2}, {"GNF", 0}, {"INR", 2},
    {"MZN", 2}, {"NOK", 2}, {"PLN", 2}, {"SAR", 2}, {"SEK", 2}, {"TND", 3},
}};

constexpr i128 kI64Max = std::numeric_limits<int64_t>::max();
constexpr i128 kI64Min = std::numeric_limits<int64_t>::min();

i128 pow10i(int n) {
    i128 v = 1;
    for (int i = 0; i < n; ++i) v *= 10;
    return v;
}

// Rounded division of a signed 128-bit numerator by a positive divisor.
i128 roundDiv(i128 num, i128 den, Rounding r) {
    bool neg = num < 0;
    i128 a = neg ? -num : num;
    i128 q = a / den;
    i128 rem = a % den;
    i128 twice = rem * 2;
    if (twice > den) {
        ++q;
    } else if (twice == den) {
        if (r == Rounding::HalfUp || (q % 2) == 1) ++q;
    }
    return neg ? -q : q;
}

std::optional<int64_t> narrow(i128 v) {
    if (v > kI64Max || v < kI64Min) return std::nullopt;
    return static_cast<int64_t>(v);
}

std::string groupDigits(const std::string& digits, const std::string& sep) {
    std::string out;
    int n = static_cast<int>(digits.size());
    for (int i = 0; i < n; ++i) {
        out.push_back(digits[i]);
        int remaining = n - i - 1;
        if (remaining > 0 && remaining % 3 == 0) out += sep;
    }
    return out;
}

}  // namespace

int currencyMinorUnits(std::string_view code) {
    for (const auto& c : kCurrencies)
        if (code == c.code) return c.minor;
    return -1;
}

bool isValidCurrencyCode(std::string_view code) {
    if (code.size() != 3) return false;
    return std::all_of(code.begin(), code.end(), [](char ch) { return ch >= 'A' && ch <= 'Z'; });
}

std::optional<int64_t> mulDiv(int64_t a, int64_t b, int64_t c, Rounding r) {
    if (c <= 0) return std::nullopt;
    i128 num = i128(a) * i128(b);
    return narrow(roundDiv(num, c, r));
}

std::optional<int64_t> parseDecimal(std::string_view text, int scale) {
    if (scale < 0 || scale > 10) return std::nullopt;
    std::string s;
    s.reserve(text.size());
    // Strip grouping characters: ASCII space, apostrophe, NBSP and NNBSP (UTF-8).
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == ' ' || ch == '\'' || ch == '\t') continue;
        if (ch == 0xC2 && i + 1 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0xA0) {
            ++i;
            continue;
        }
        if (ch == 0xE2 && i + 2 < text.size() && static_cast<unsigned char>(text[i + 1]) == 0x80 &&
            static_cast<unsigned char>(text[i + 2]) == 0xAF) {
            i += 2;
            continue;
        }
        s.push_back(static_cast<char>(ch));
    }
    if (s.empty()) return std::nullopt;
    bool neg = false;
    size_t pos = 0;
    if (s[0] == '-' || s[0] == '+') {
        neg = s[0] == '-';
        pos = 1;
    }
    if (pos >= s.size()) return std::nullopt;

    i128 intPart = 0;
    i128 frac = 0;
    int fracDigits = 0;
    bool seenSep = false;
    bool anyDigit = false;
    for (; pos < s.size(); ++pos) {
        char ch = s[pos];
        if (ch == '.' || ch == ',') {
            if (seenSep) return std::nullopt;
            seenSep = true;
            continue;
        }
        if (ch < '0' || ch > '9') return std::nullopt;
        anyDigit = true;
        int d = ch - '0';
        if (!seenSep) {
            intPart = intPart * 10 + d;
            if (intPart > kI64Max) return std::nullopt;
        } else {
            if (fracDigits >= scale) {
                // Extra digits are only accepted when they are zeros.
                if (d != 0) return std::nullopt;
                continue;
            }
            frac = frac * 10 + d;
            ++fracDigits;
        }
    }
    if (!anyDigit) return std::nullopt;
    i128 value = intPart * pow10i(scale) + frac * pow10i(scale - fracDigits);
    if (neg) value = -value;
    return narrow(value);
}

std::string toPlainDecimal(int64_t value, int scale) {
    bool neg = value < 0;
    // |INT64_MIN| fits in a signed 128-bit integer.
    i128 a = neg ? -i128(value) : i128(value);
    i128 p = pow10i(scale);
    i128 ip = a / p;
    i128 fp = a % p;
    std::string is;
    if (ip == 0) is = "0";
    while (ip > 0) {
        is.insert(is.begin(), static_cast<char>('0' + static_cast<int>(ip % 10)));
        ip /= 10;
    }
    std::string out = neg ? "-" + is : is;
    if (scale > 0) {
        std::string fs(scale, '0');
        for (int i = scale - 1; i >= 0; --i) {
            fs[i] = static_cast<char>('0' + static_cast<int>(fp % 10));
            fp /= 10;
        }
        out += "." + fs;
    }
    return out;
}

std::string formatDecimal(int64_t value, int scale, std::string_view lang, bool trimZeros) {
    std::string plain = toPlainDecimal(value, scale);
    bool neg = !plain.empty() && plain[0] == '-';
    if (neg) plain.erase(0, 1);
    std::string ip = plain, fp;
    auto dot = plain.find('.');
    if (dot != std::string::npos) {
        ip = plain.substr(0, dot);
        fp = plain.substr(dot + 1);
    }
    if (trimZeros) {
        while (!fp.empty() && fp.back() == '0') fp.pop_back();
    }
    const bool fr = lang == "fr";
    // French typography: narrow no-break space (U+202F) for thousands.
    std::string grouped = groupDigits(ip, fr ? "\xE2\x80\xAF" : ",");
    std::string out = (neg ? "-" : "") + grouped;
    if (!fp.empty()) out += (fr ? "," : ".") + fp;
    return out;
}

std::string formatMoney(int64_t minor, std::string_view currency, std::string_view lang) {
    int mu = currencyMinorUnits(currency);
    if (mu < 0) mu = 2;
    std::string n = formatDecimal(minor, mu, lang);
    if (lang == "fr") return n + "\xC2\xA0" + std::string(currency);
    return std::string(currency) + "\xC2\xA0" + n;
}

std::optional<LineAmounts> computeLine(const LineInput& in) {
    if (in.quantityMilli <= 0 || in.unitPriceMinor < 0) return std::nullopt;
    if (in.discountBp < 0 || in.discountBp > 10000) return std::nullopt;
    if (in.taxRateBp < 0 || in.taxRateBp > 10000) return std::nullopt;
    LineAmounts out;
    auto gross = mulDiv(in.quantityMilli, in.unitPriceMinor, 1000);
    if (!gross) return std::nullopt;
    out.grossMinor = *gross;
    auto disc = mulDiv(out.grossMinor, in.discountBp, 10000);
    if (!disc) return std::nullopt;
    out.discountMinor = *disc;
    out.netMinor = out.grossMinor - out.discountMinor;
    auto tax = mulDiv(out.netMinor, in.taxRateBp, 10000);
    if (!tax) return std::nullopt;
    out.taxMinor = *tax;
    i128 total = i128(out.netMinor) + out.taxMinor;
    auto t = narrow(total);
    if (!t) return std::nullopt;
    out.totalMinor = *t;
    return out;
}

std::optional<DocumentTotals> computeDocument(const std::vector<LineInput>& lines, TaxRounding mode) {
    DocumentTotals doc;
    std::map<int32_t, std::pair<i128, i128>> buckets;  // rate -> (base, per-line tax sum)
    i128 gross = 0, disc = 0, net = 0;
    for (const auto& li : lines) {
        auto la = computeLine(li);
        if (!la) return std::nullopt;
        doc.lines.push_back(*la);
        gross += la->grossMinor;
        disc += la->discountMinor;
        net += la->netMinor;
        auto& b = buckets[li.taxRateBp];
        b.first += la->netMinor;
        b.second += la->taxMinor;
    }
    i128 taxTotal = 0;
    for (auto& [rate, b] : buckets) {
        TaxBucket tb;
        tb.rateBp = rate;
        auto base = narrow(b.first);
        if (!base) return std::nullopt;
        tb.baseMinor = *base;
        if (mode == TaxRounding::PerLine) {
            auto t = narrow(b.second);
            if (!t) return std::nullopt;
            tb.taxMinor = *t;
        } else {
            auto t = mulDiv(tb.baseMinor, rate, 10000);
            if (!t) return std::nullopt;
            tb.taxMinor = *t;
        }
        taxTotal += tb.taxMinor;
        doc.taxes.push_back(tb);
    }
    auto g = narrow(gross), d = narrow(disc), n = narrow(net), t = narrow(taxTotal), tot = narrow(net + taxTotal);
    if (!g || !d || !n || !t || !tot) return std::nullopt;
    doc.grossMinor = *g;
    doc.discountMinor = *d;
    doc.netMinor = *n;
    doc.taxMinor = *t;
    doc.totalMinor = *tot;
    return doc;
}

std::optional<ExchangeRate> ExchangeRate::parse(std::string_view text) {
    auto v = parseDecimal(text, 10);
    if (!v || *v <= 0) return std::nullopt;
    ExchangeRate r;
    r.scaled = *v;
    return r;
}

std::string ExchangeRate::toString() const {
    std::string s = toPlainDecimal(scaled, 10);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

std::optional<int64_t> convert(int64_t amountMinor, int fromMinorUnits, int toMinorUnits,
                               const ExchangeRate& rate) {
    if (fromMinorUnits < 0 || toMinorUnits < 0 || rate.scaled <= 0) return std::nullopt;
    if (fromMinorUnits > 6 || toMinorUnits > 6) return std::nullopt;
    // result_to_minor = amount / 10^from * rate * 10^to
    const i128 factor = rate.scaled * pow10i(toMinorUnits);  // <= ~9.2e24, fits
    const i128 absAmount = amountMinor < 0 ? -i128(amountMinor) : amountMinor;
    constexpr i128 kI128Max = i128Max();
    if (absAmount != 0 && factor > kI128Max / absAmount) return std::nullopt;
    i128 num = i128(amountMinor) * factor;
    i128 den = pow10i(10) * pow10i(fromMinorUnits);
    return narrow(roundDiv(num, den, Rounding::HalfUp));
}

}  // namespace nhill
