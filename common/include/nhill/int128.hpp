#pragma once
// Signed 128-bit integer used for exact intermediate money arithmetic.
//
// GCC and Clang provide the native __int128 type; MSVC (Windows) does not. On compilers
// without __int128 (or when NHILL_PORTABLE_INT128 is defined, to test this code path
// on Linux), nhill::i128 is a small two's-complement class with the operations used
// by money.cpp: + - * / % (C++ truncating division), unary minus, comparisons and
// explicit conversion to int64_t / int. Overflow wraps modulo 2^128 exactly like the
// native type; callers check ranges before multiplying (see money.cpp).
// PortableInt128 is always compiled (and tested against __int128 where it exists).

#include <cstdint>

namespace nhill {

class PortableInt128 {
public:
    constexpr PortableInt128() = default;
    constexpr PortableInt128(int v) : PortableInt128(static_cast<long long>(v)) {}
    constexpr PortableInt128(long v) : PortableInt128(static_cast<long long>(v)) {}
    constexpr PortableInt128(long long v)
        : hi_(v < 0 ? ~uint64_t{0} : 0), lo_(static_cast<uint64_t>(v)) {}
    constexpr PortableInt128(unsigned v) : hi_(0), lo_(v) {}
    constexpr PortableInt128(unsigned long v) : hi_(0), lo_(v) {}
    constexpr PortableInt128(unsigned long long v) : hi_(0), lo_(v) {}

    static constexpr PortableInt128 fromParts(uint64_t hi, uint64_t lo) {
        PortableInt128 r;
        r.hi_ = hi;
        r.lo_ = lo;
        return r;
    }
    static constexpr PortableInt128 max() { return fromParts(~uint64_t{0} >> 1, ~uint64_t{0}); }
    static constexpr PortableInt128 min() { return fromParts(uint64_t{1} << 63, 0); }

    constexpr explicit operator int64_t() const { return static_cast<int64_t>(lo_); }
    constexpr explicit operator int() const { return static_cast<int>(static_cast<int64_t>(lo_)); }
    constexpr bool negative() const { return (hi_ >> 63) != 0; }
    constexpr uint64_t high() const { return hi_; }
    constexpr uint64_t low() const { return lo_; }

    // ------------------------------------------------------------------ arithmetic
    friend constexpr PortableInt128 operator+(PortableInt128 a, PortableInt128 b) {
        uint64_t lo = a.lo_ + b.lo_;
        uint64_t carry = lo < a.lo_ ? 1 : 0;
        return fromParts(a.hi_ + b.hi_ + carry, lo);
    }
    friend constexpr PortableInt128 operator-(PortableInt128 a) { return fromParts(~a.hi_, ~a.lo_) + PortableInt128(1); }
    friend constexpr PortableInt128 operator-(PortableInt128 a, PortableInt128 b) { return a + (-b); }
    friend constexpr PortableInt128 operator*(PortableInt128 a, PortableInt128 b) {
        uint64_t hi = 0, lo = 0;
        mul64(a.lo_, b.lo_, hi, lo);
        hi += a.hi_ * b.lo_ + a.lo_ * b.hi_;
        return fromParts(hi, lo);
    }
    friend PortableInt128 operator/(PortableInt128 a, PortableInt128 b) {
        PortableInt128 q, r;
        divmod(a, b, q, r);
        return q;
    }
    friend PortableInt128 operator%(PortableInt128 a, PortableInt128 b) {
        PortableInt128 q, r;
        divmod(a, b, q, r);
        return r;
    }
    PortableInt128& operator+=(PortableInt128 b) { return *this = *this + b; }
    PortableInt128& operator-=(PortableInt128 b) { return *this = *this - b; }
    PortableInt128& operator*=(PortableInt128 b) { return *this = *this * b; }
    PortableInt128& operator/=(PortableInt128 b) { return *this = *this / b; }
    PortableInt128& operator%=(PortableInt128 b) { return *this = *this % b; }
    PortableInt128& operator++() { return *this = *this + PortableInt128(1); }

    // ------------------------------------------------------------------ comparisons
    friend constexpr bool operator==(PortableInt128 a, PortableInt128 b) { return a.hi_ == b.hi_ && a.lo_ == b.lo_; }
    friend constexpr bool operator!=(PortableInt128 a, PortableInt128 b) { return !(a == b); }
    friend constexpr bool operator<(PortableInt128 a, PortableInt128 b) {
        const int64_t ah = static_cast<int64_t>(a.hi_), bh = static_cast<int64_t>(b.hi_);
        return ah != bh ? ah < bh : a.lo_ < b.lo_;
    }
    friend constexpr bool operator>(PortableInt128 a, PortableInt128 b) { return b < a; }
    friend constexpr bool operator<=(PortableInt128 a, PortableInt128 b) { return !(b < a); }
    friend constexpr bool operator>=(PortableInt128 a, PortableInt128 b) { return !(a < b); }

private:
    uint64_t hi_ = 0;
    uint64_t lo_ = 0;

    // 64 x 64 -> 128 bits, unsigned (32-bit halves, portable).
    static constexpr void mul64(uint64_t a, uint64_t b, uint64_t& hi, uint64_t& lo) {
        const uint64_t a0 = a & 0xFFFFFFFFu, a1 = a >> 32, b0 = b & 0xFFFFFFFFu, b1 = b >> 32;
        const uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
        const uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFu) + (p10 & 0xFFFFFFFFu);
        lo = (mid << 32) | (p00 & 0xFFFFFFFFu);
        hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
    }

    // Unsigned long division of magnitudes (shift and subtract, 128 steps).
    static void udivmod(uint64_t nh, uint64_t nl, uint64_t dh, uint64_t dl, uint64_t& qh, uint64_t& ql,
                        uint64_t& rh, uint64_t& rl) {
        qh = ql = rh = rl = 0;
        for (int i = 127; i >= 0; --i) {
            // r = (r << 1) | bit i of n
            rh = (rh << 1) | (rl >> 63);
            rl <<= 1;
            const uint64_t bit = i >= 64 ? (nh >> (i - 64)) & 1u : (nl >> i) & 1u;
            rl |= bit;
            // if r >= d: r -= d, set bit i of q
            if (rh > dh || (rh == dh && rl >= dl)) {
                const uint64_t borrow = rl < dl ? 1 : 0;
                rl -= dl;
                rh -= dh + borrow;
                if (i >= 64) qh |= uint64_t{1} << (i - 64);
                else ql |= uint64_t{1} << i;
            }
        }
    }

    // Signed division truncating toward zero; the remainder has the sign of the dividend
    // (same as C++ built-in integers). Division by zero is a caller error: it returns 0.
    static void divmod(PortableInt128 a, PortableInt128 b, PortableInt128& q, PortableInt128& r) {
        if (b == PortableInt128(0)) {
            q = r = PortableInt128(0);
            return;
        }
        const bool na = a.negative(), nb = b.negative();
        const PortableInt128 ua = na ? -a : a, ub = nb ? -b : b;
        uint64_t qh, ql, rh, rl;
        udivmod(ua.hi_, ua.lo_, ub.hi_, ub.lo_, qh, ql, rh, rl);
        q = fromParts(qh, ql);
        r = fromParts(rh, rl);
        if (na != nb) q = -q;
        if (na) r = -r;
    }
};

#if defined(__SIZEOF_INT128__) && !defined(NHILL_PORTABLE_INT128)
using i128 = __int128;
constexpr i128 i128Max() { return static_cast<__int128>((~static_cast<unsigned __int128>(0)) >> 1); }
#else
using i128 = PortableInt128;
constexpr i128 i128Max() { return PortableInt128::max(); }
#endif

}  // namespace nhill
