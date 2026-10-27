// Tests of nhill::PortableInt128, the 128-bit integer used by money.cpp on compilers
// without __int128 (MSVC). Where __int128 exists, results are compared with it on
// many pseudo-random operands; fixed cases run everywhere.
#define MINIGTEST_NO_MAIN  // only for the standalone runner; ignored by GoogleTest
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "nhill/int128.hpp"

using nhill::PortableInt128;

namespace {

uint64_t splitmix(uint64_t& s) {
    uint64_t z = (s += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

PortableInt128 P(long long v) { return PortableInt128(v); }

}  // namespace

TEST(Int128, FixedArithmetic) {
    const long long big = std::numeric_limits<int64_t>::max();
    // (2^63 - 1)^2 = 85070591730234615847396907784232501249
    PortableInt128 sq = P(big) * P(big);
    PortableInt128 back = sq / P(big);
    EXPECT_TRUE(back == P(big));
    EXPECT_TRUE(sq % P(big) == P(0));
    EXPECT_TRUE(sq > P(big));
    // Truncating division and sign of the remainder, as for built-in integers.
    EXPECT_EQ(static_cast<int64_t>(P(-7) / P(2)), -3);
    EXPECT_EQ(static_cast<int64_t>(P(-7) % P(2)), -1);
    EXPECT_EQ(static_cast<int64_t>(P(7) / P(-2)), -3);
    EXPECT_EQ(static_cast<int64_t>(P(7) % P(-2)), 1);
    EXPECT_EQ(static_cast<int64_t>(P(-8) / P(-2)), 4);
    // |INT64_MIN| fits.
    PortableInt128 m = -P(std::numeric_limits<int64_t>::min());
    EXPECT_TRUE(m > P(big));
    EXPECT_TRUE(m - P(1) == P(big));
    // Limits.
    EXPECT_TRUE(PortableInt128::max() > P(0));
    EXPECT_TRUE(PortableInt128::min() < P(0));
    EXPECT_TRUE(PortableInt128::max() + P(1) == PortableInt128::min());  // wraps like __int128
    // Powers of ten up to 10^38.
    PortableInt128 p = 1;
    for (int i = 0; i < 38; ++i) p *= 10;
    PortableInt128 q = p;
    for (int i = 0; i < 38; ++i) q /= 10;
    EXPECT_TRUE(q == P(1));
}

#if defined(__SIZEOF_INT128__)
namespace {
__int128 toNative(PortableInt128 v) {
    return static_cast<__int128>((static_cast<unsigned __int128>(v.high()) << 64) | v.low());
}
PortableInt128 fromNative(__int128 v) {
    const auto u = static_cast<unsigned __int128>(v);
    return PortableInt128::fromParts(static_cast<uint64_t>(u >> 64), static_cast<uint64_t>(u));
}
}  // namespace

TEST(Int128, MatchesNativeInt128) {
    uint64_t seed = 20261027;
    for (int i = 0; i < 20000; ++i) {
        // Mix of small, 64-bit and full 128-bit operands, both signs.
        auto pick = [&](int kind) -> __int128 {
            const uint64_t a = splitmix(seed), b = splitmix(seed);
            switch (kind % 4) {
            case 0: return static_cast<__int128>(static_cast<int64_t>(a % 2001) - 1000);
            case 1: return static_cast<__int128>(static_cast<int64_t>(a));
            case 2: return static_cast<__int128>((static_cast<unsigned __int128>(a) << 64) | b);
            default: return static_cast<__int128>(static_cast<int64_t>(a)) * static_cast<int64_t>(b >> 1);
            }
        };
        const __int128 x = pick(i), y = pick(i / 4 + 1);
        const PortableInt128 px = fromNative(x), py = fromNative(y);
        ASSERT_TRUE(toNative(px) == x);
        EXPECT_TRUE(toNative(px + py) == static_cast<__int128>(static_cast<unsigned __int128>(x) + static_cast<unsigned __int128>(y)));
        EXPECT_TRUE(toNative(px - py) == static_cast<__int128>(static_cast<unsigned __int128>(x) - static_cast<unsigned __int128>(y)));
        EXPECT_TRUE(toNative(px * py) == static_cast<__int128>(static_cast<unsigned __int128>(x) * static_cast<unsigned __int128>(y)));
        EXPECT_EQ(px < py, x < y);
        EXPECT_EQ(px == py, x == y);
        const __int128 kMin = static_cast<__int128>(static_cast<unsigned __int128>(1) << 127);
        if (y != 0 && !(x == kMin && y == -1)) {
            EXPECT_TRUE(toNative(px / py) == x / y);
            EXPECT_TRUE(toNative(px % py) == x % y);
        }
    }
}
#endif
