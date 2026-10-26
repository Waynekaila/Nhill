#include <gtest/gtest.h>

#include <random>
#include <set>

#include "nhill/establishment_code.hpp"
#include "nhill/money.hpp"
#include "nhill/validation.hpp"

using namespace nhill;

TEST(Money, ParsesWithBothSeparators) {
    EXPECT_EQ(parseMoney("1234.5", 2), 123450);
    EXPECT_EQ(parseMoney("1 234,50", 2), 123450);
    EXPECT_EQ(parseMoney("1\xE2\x80\xAF" "234,50", 2), 123450);  // narrow nbsp
    EXPECT_EQ(parseMoney("-0.01", 2), -1);
    EXPECT_EQ(parseMoney("12", 0), 12);
    EXPECT_EQ(parseMoney("12.00", 0), 12);  // trailing zeros tolerated
}

TEST(Money, RejectsMalformedOrTooPrecise) {
    EXPECT_FALSE(parseMoney("", 2));
    EXPECT_FALSE(parseMoney("abc", 2));
    EXPECT_FALSE(parseMoney("1.2.3", 2));
    EXPECT_FALSE(parseMoney("1.234", 2));       // silently rounding money is forbidden
    EXPECT_FALSE(parseMoney("1e5", 2));
    EXPECT_FALSE(parseMoney("99999999999999999999", 2));  // overflow
    EXPECT_FALSE(parseMoney("-", 2));
}

TEST(Money, NoFloatingPointDrift) {
    // 0.1 + 0.2 == 0.3 exactly in minor units
    EXPECT_EQ(*parseMoney("0.1", 2) + *parseMoney("0.2", 2), *parseMoney("0.3", 2));
    // Summing 1 000 000 times 0.01 stays exact
    int64_t sum = 0;
    for (int i = 0; i < 1000000; ++i) sum += *parseMoney("0.01", 2);
    EXPECT_EQ(sum, 1000000);
}

TEST(Money, Formatting) {
    EXPECT_EQ(formatDecimal(123450, 2, "en"), "1,234.50");
    EXPECT_EQ(formatDecimal(123450, 2, "fr"), "1\xE2\x80\xAF" "234,50");
    EXPECT_EQ(formatDecimal(-5, 2, "en"), "-0.05");
    EXPECT_EQ(formatDecimal(2500, 3, "en", true), "2.5");
    EXPECT_EQ(formatDecimal(2000, 3, "fr", true), "2");
    EXPECT_EQ(toPlainDecimal(-123450, 2), "-1234.50");
    EXPECT_EQ(toPlainDecimal(std::numeric_limits<int64_t>::min(), 2), "-92233720368547758.08");
    EXPECT_EQ(formatMoney(1000000, "CDF", "fr"), "10\xE2\x80\xAF" "000,00\xC2\xA0" "CDF");
    EXPECT_EQ(formatMoney(1999, "USD", "en"), "USD\xC2\xA0" "19.99");
}

TEST(Money, MulDivRounding) {
    EXPECT_EQ(mulDiv(5, 1, 10, Rounding::HalfUp), 1);     // 0.5 -> 1
    EXPECT_EQ(mulDiv(-5, 1, 10, Rounding::HalfUp), -1);   // symmetric
    EXPECT_EQ(mulDiv(5, 1, 10, Rounding::HalfEven), 0);   // 0.5 -> 0
    EXPECT_EQ(mulDiv(15, 1, 10, Rounding::HalfEven), 2);  // 1.5 -> 2
    EXPECT_FALSE(mulDiv(std::numeric_limits<int64_t>::max(), 2, 1));
    EXPECT_FALSE(mulDiv(1, 1, 0));
}

TEST(Tax, LineComputationDrcVat) {
    // 3 units at 1 999.99 CDF, 10 % discount, 16 % VAT
    LineInput in{3000, 199999, 1000, 1600};
    auto la = computeLine(in);
    ASSERT_TRUE(la);
    EXPECT_EQ(la->grossMinor, 599997);
    EXPECT_EQ(la->discountMinor, 60000);   // 59 999.7 -> 60 000
    EXPECT_EQ(la->netMinor, 539997);
    EXPECT_EQ(la->taxMinor, 86400);        // 86 399.52 -> 86 400
    EXPECT_EQ(la->totalMinor, 626397);
}

TEST(Tax, FractionalQuantities) {
    // 2.5 kg at 3.33 USD
    auto la = computeLine({2500, 333, 0, 0});
    ASSERT_TRUE(la);
    EXPECT_EQ(la->grossMinor, 833);  // 8.325 -> 8.33
}

TEST(Tax, RoundingModesDiffer) {
    // Three lines of 0.05 at 16 % : per-line tax 0.01 each (0.008 -> 0.01) = 0.03 ;
    // per-rate tax on 0.15 = 0.024 -> 0.02. Both modes must be exact and documented.
    std::vector<LineInput> lines(3, LineInput{1000, 5, 0, 1600});
    auto perLine = computeDocument(lines, TaxRounding::PerLine);
    auto perRate = computeDocument(lines, TaxRounding::PerRate);
    ASSERT_TRUE(perLine && perRate);
    EXPECT_EQ(perLine->taxMinor, 3);
    EXPECT_EQ(perRate->taxMinor, 2);
    EXPECT_EQ(perLine->netMinor, 15);
    EXPECT_EQ(perLine->totalMinor, 18);
    EXPECT_EQ(perRate->totalMinor, 17);
}

TEST(Tax, MixedRatesBuckets) {
    std::vector<LineInput> lines{{1000, 10000, 0, 1600}, {2000, 500, 0, 0}, {1000, 2000, 0, 1600}};
    auto doc = computeDocument(lines, TaxRounding::PerLine);
    ASSERT_TRUE(doc);
    ASSERT_EQ(doc->taxes.size(), 2u);
    EXPECT_EQ(doc->taxes[0].rateBp, 0);
    EXPECT_EQ(doc->taxes[0].baseMinor, 1000);
    EXPECT_EQ(doc->taxes[1].rateBp, 1600);
    EXPECT_EQ(doc->taxes[1].baseMinor, 12000);
    EXPECT_EQ(doc->taxes[1].taxMinor, 1920);
    EXPECT_EQ(doc->totalMinor, 14920);
}

TEST(Tax, RejectsInvalidLines) {
    EXPECT_FALSE(computeLine({0, 100, 0, 0}));
    EXPECT_FALSE(computeLine({1000, -1, 0, 0}));
    EXPECT_FALSE(computeLine({1000, 100, 10001, 0}));
    EXPECT_FALSE(computeLine({1000, 100, 0, -1}));
}

TEST(Fx, ExactConversion) {
    auto r = ExchangeRate::parse("2850.5");  // 1 USD = 2 850,5 CDF
    ASSERT_TRUE(r);
    EXPECT_EQ(r->toString(), "2850.5");
    EXPECT_EQ(convert(10000, 2, 2, *r), 28505000);  // 100.00 USD -> 285 050.00 CDF
    auto inv = ExchangeRate::parse("0.000350815");
    ASSERT_TRUE(inv);
    EXPECT_EQ(convert(28505000, 2, 2, *inv), 10000);  // 285 050 * 0.000350815 = 99.9998 -> 100.00
    EXPECT_FALSE(ExchangeRate::parse("0"));
    EXPECT_FALSE(ExchangeRate::parse("-1"));
    EXPECT_FALSE(convert(std::numeric_limits<int64_t>::max(), 0, 6, *r));
}

TEST(Code, GenerateIsCanonicalAndUnique) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    auto rng = [&](uint8_t* out, size_t n) {
        for (size_t i = 0; i < n; ++i) out[i] = static_cast<uint8_t>(gen());
    };
    std::set<std::string> seen;
    for (int i = 0; i < 20000; ++i) {
        auto c = code::generate(rng);
        ASSERT_EQ(c.size(), 12u);
        ASSERT_EQ(code::normalize(c), c);
        seen.insert(c);
    }
    EXPECT_EQ(seen.size(), 20000u);
}

TEST(Code, NormalizationIsTolerant) {
    EXPECT_EQ(code::normalize("nh-abcd-efgh-jk01"), "ABCDEFGHJK01");
    EXPECT_EQ(code::normalize("ABCD EFGH JKO1"), "ABCDEFGHJK01");   // O -> 0
    EXPECT_EQ(code::normalize("abcdefghjkIL"), "ABCDEFGHJK11");     // I/L -> 1
    EXPECT_FALSE(code::normalize("ABCD-EFGH-JK0"));                  // too short
    EXPECT_FALSE(code::normalize("ABCD-EFGH-JK0U"));                 // U not in alphabet
    EXPECT_FALSE(code::normalize("ABCD-EFGH-JK0!"));
    EXPECT_EQ(code::display("ABCDEFGHJK01"), "NH-ABCD-EFGH-JK01");
}

TEST(Code, DeterministicSourceGivesSameCode) {
    auto fixed = [](uint8_t* out, size_t n) {
        for (size_t i = 0; i < n; ++i) out[i] = 0x42;
    };
    EXPECT_EQ(code::generate(fixed), code::generate(fixed));
}

TEST(Validation, Email) {
    using namespace validation;
    EXPECT_TRUE(isValidEmail("wayne@example.cd"));
    EXPECT_TRUE(isValidEmail("  a.b+c@sub.example.com "));
    EXPECT_FALSE(isValidEmail("a@b"));
    EXPECT_FALSE(isValidEmail("a b@example.com"));
    EXPECT_FALSE(isValidEmail("a@@example.com"));
    EXPECT_FALSE(isValidEmail("a@example..com"));
    EXPECT_EQ(normalizeEmail(" Wayne@Example.CD "), "wayne@example.cd");
}

TEST(Validation, PhoneAndLegalIds) {
    using namespace validation;
    EXPECT_EQ(normalizePhone("+243 81 234 5678"), "+243812345678");
    EXPECT_EQ(normalizePhone("(081) 234-5678"), "0812345678");
    EXPECT_FALSE(normalizePhone("12345"));
    EXPECT_FALSE(normalizePhone("+243 81 abc"));
    EXPECT_EQ(normalizeLegalId("cd/kng/rccm/23-b-01234"), "CDKNGRCCM23B01234");
    EXPECT_EQ(normalizeLegalId(" a1.234-5 "), "A12345");
    EXPECT_FALSE(normalizeLegalId(" - / "));
}

TEST(Validation, Passwords) {
    using namespace validation;
    EXPECT_EQ(checkPassword("short", ""), PasswordIssue::TooShort);
    EXPECT_EQ(checkPassword("motdepasse", ""), PasswordIssue::TooCommon);
    EXPECT_EQ(checkPassword("aaaaaaaaaaaa", ""), PasswordIssue::Repetitive);
    EXPECT_EQ(checkPassword("me@example.com", "ME@example.com"), PasswordIssue::SameAsEmail);
    EXPECT_EQ(checkPassword("Kinshasa la belle 2026", ""), PasswordIssue::None);
    // 10 accented characters count as 10 code points, not 20 bytes
    EXPECT_EQ(checkPassword("éééééééééé", ""), PasswordIssue::Repetitive);
    EXPECT_EQ(checkPassword("ééééééééé", ""), PasswordIssue::TooShort);
}

TEST(Validation, Text) {
    using namespace validation;
    EXPECT_TRUE(isValidText("Boutique Malachite", 200));
    EXPECT_FALSE(isValidText("   ", 200));
    EXPECT_TRUE(isValidText("", 200, true));
    EXPECT_FALSE(isValidText("a\x01" "b", 200));
    EXPECT_FALSE(isValidText("\xC3", 200));  // invalid UTF-8
    EXPECT_FALSE(isValidText(std::string(201, 'x'), 200));
}
