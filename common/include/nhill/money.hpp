// Nhill — exact monetary arithmetic shared by client and server.
//
// Amounts are int64 counts of the currency's minor unit (e.g. cents).
// Quantities are int64 thousandths ("milli-units").
// Tax and discount rates are integer basis points (1600 = 16.00 %).
// No floating point is ever used for money.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nhill {

// ISO 4217 minor units for the currencies we know; -1 when unknown.
int currencyMinorUnits(std::string_view code);
bool isValidCurrencyCode(std::string_view code);

enum class Rounding { HalfUp, HalfEven };

// a * b / c with 128-bit intermediate and the requested rounding (c > 0).
// Returns std::nullopt on overflow of the int64 result.
std::optional<int64_t> mulDiv(int64_t a, int64_t b, int64_t c, Rounding r = Rounding::HalfUp);

// Parses "1234.5", "1 234,50", "-12" into minor units. Accepts '.' or ','
// as decimal separator and spaces / ' ' / apostrophes as grouping.
// Fails (nullopt) if more fractional digits than `minorUnits` are given,
// or on any malformed input or overflow.
std::optional<int64_t> parseDecimal(std::string_view text, int scale);

inline std::optional<int64_t> parseMoney(std::string_view text, int minorUnits) {
    return parseDecimal(text, minorUnits);
}
inline std::optional<int64_t> parseQuantity(std::string_view text) {
    return parseDecimal(text, 3);
}

// Formats a scaled integer. lang "fr" -> "1 234,50" (narrow nbsp grouping),
// "en" -> "1,234.50". `trimZeros` removes trailing fractional zeros (quantities).
std::string formatDecimal(int64_t value, int scale, std::string_view lang, bool trimZeros = false);
// Machine format: "-1234.50" (always '.', no grouping) — used in APIs and CSV.
std::string toPlainDecimal(int64_t value, int scale);

std::string formatMoney(int64_t minor, std::string_view currency, std::string_view lang);

struct LineInput {
    int64_t quantityMilli = 0;     // > 0
    int64_t unitPriceMinor = 0;    // >= 0, tax exclusive
    int32_t discountBp = 0;        // 0..10000
    int32_t taxRateBp = 0;         // 0..10000
};

struct LineAmounts {
    int64_t grossMinor = 0;      // quantity * unit price
    int64_t discountMinor = 0;
    int64_t netMinor = 0;        // taxable base
    int64_t taxMinor = 0;        // per-line tax (used with TaxRounding::PerLine)
    int64_t totalMinor = 0;      // net + tax
};

enum class TaxRounding { PerLine, PerRate };

struct TaxBucket {
    int32_t rateBp = 0;
    int64_t baseMinor = 0;
    int64_t taxMinor = 0;
};

struct DocumentTotals {
    std::vector<LineAmounts> lines;
    std::vector<TaxBucket> taxes;   // ordered by rate
    int64_t grossMinor = 0;
    int64_t discountMinor = 0;
    int64_t netMinor = 0;
    int64_t taxMinor = 0;
    int64_t totalMinor = 0;
};

// Returns std::nullopt if any input is out of range or arithmetic overflows.
std::optional<LineAmounts> computeLine(const LineInput& in);
std::optional<DocumentTotals> computeDocument(const std::vector<LineInput>& lines, TaxRounding mode);

// Exact decimal exchange rate "1 unit of FROM = rate units of TO", up to 10 decimals.
struct ExchangeRate {
    __int128 scaled = 0;  // rate * 10^10
    static std::optional<ExchangeRate> parse(std::string_view text);
    std::string toString() const;
};

// Converts an amount between currencies using an exact rate (half-up).
std::optional<int64_t> convert(int64_t amountMinor, int fromMinorUnits, int toMinorUnits,
                               const ExchangeRate& rate);

}  // namespace nhill
