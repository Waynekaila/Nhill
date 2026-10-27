// Minimal GoogleTest-compatible test runner (subset) — Nhill project.
//
// Purpose: run tests/common/test_common.cpp (business rules: money, taxes,
// establishment codes, validation) on a machine where GoogleTest is not
// installed, e.g. a sandbox without access to the distribution mirrors.
// The official build (CMake + GoogleTest) remains the reference; this header
// only implements the macros used by tests/common: TEST, EXPECT_/ASSERT_
// EQ, NE, TRUE, FALSE, LT, LE, GT, GE. Used by scripts/test-common-standalone.sh.
#pragma once

#include <cstdio>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace minigtest {

struct Test {
    const char* suite;
    const char* name;
    std::function<void()> fn;
};

inline std::vector<Test>& registry() {
    static std::vector<Test> r;
    return r;
}
inline int& failures() {
    static int f = 0;
    return f;
}
inline bool& currentFailed() {
    static bool f = false;
    return f;
}

struct Registrar {
    Registrar(const char* s, const char* n, std::function<void()> fn) { registry().push_back({s, n, std::move(fn)}); }
};

template <typename T, typename = void>
struct Printable : std::false_type {};
template <typename T>
struct Printable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>> : std::true_type {};

template <typename T>
std::string show(const T& v) {
    if constexpr (std::is_enum_v<T>) {
        return "enum(" + std::to_string(static_cast<long long>(v)) + ")";
    } else if constexpr (Printable<T>::value) {
        std::ostringstream o;
        o << v;
        return o.str();
    } else {
        return "<value>";
    }
}
template <typename T>
std::string show(const std::optional<T>& v) {
    return v ? "optional(" + show(*v) + ")" : "nullopt";
}

inline void report(const char* file, int line, const std::string& msg) {
    std::cerr << file << ":" << line << ": Failure\n" << msg << "\n";
    currentFailed() = true;
}

struct Fatal {};

template <typename A, typename B, typename Op>
bool check(const A& a, const B& b, Op op, const char* sa, const char* sb, const char* sop, const char* file, int line) {
    if (op(a, b)) return true;
    report(file, line, std::string("  Expected: ") + sa + " " + sop + " " + sb + "\n  Actual: " + show(a) + " vs " + show(b));
    return false;
}

}  // namespace minigtest

#define MG_CAT2(a, b) a##b
#define MG_CAT(a, b) MG_CAT2(a, b)

#define TEST(suite, name)                                                                       \
    static void MG_CAT(mg_test_##suite##_, name)();                                             \
    static ::minigtest::Registrar MG_CAT(mg_reg_##suite##_, name)(#suite, #name,                \
                                                                 &MG_CAT(mg_test_##suite##_, name)); \
    static void MG_CAT(mg_test_##suite##_, name)()

// Operands are passed to a function so that temporaries (e.g. *optional{}) stay
// alive for the whole comparison, as in GoogleTest.
#define MG_CMP(a, b, op, fatal)                                                                  \
    do {                                                                                         \
        if (!::minigtest::check((a), (b), [](const auto& mg_x, const auto& mg_y) { return mg_x op mg_y; }, \
                                #a, #b, #op, __FILE__, __LINE__) &&                              \
            (fatal))                                                                             \
            throw ::minigtest::Fatal{};                                                          \
    } while (0)

#define MG_BOOL(c, expected, fatal)                                                              \
    do {                                                                                         \
        if (static_cast<bool>(c) != (expected)) {                                                \
            ::minigtest::report(__FILE__, __LINE__,                                              \
                                std::string("  Value of: ") + #c + "\n  Expected: " + ((expected) ? "true" : "false")); \
            if (fatal) throw ::minigtest::Fatal{};                                               \
        }                                                                                        \
    } while (0)

#define EXPECT_EQ(a, b) MG_CMP(a, b, ==, false)
#define EXPECT_NE(a, b) MG_CMP(a, b, !=, false)
#define EXPECT_LT(a, b) MG_CMP(a, b, <, false)
#define EXPECT_LE(a, b) MG_CMP(a, b, <=, false)
#define EXPECT_GT(a, b) MG_CMP(a, b, >, false)
#define EXPECT_GE(a, b) MG_CMP(a, b, >=, false)
#define ASSERT_EQ(a, b) MG_CMP(a, b, ==, true)
#define ASSERT_NE(a, b) MG_CMP(a, b, !=, true)
#define ASSERT_LT(a, b) MG_CMP(a, b, <, true)
#define ASSERT_GT(a, b) MG_CMP(a, b, >, true)
#define EXPECT_TRUE(c) MG_BOOL(c, true, false)
#define EXPECT_FALSE(c) MG_BOOL(c, false, false)
#define ASSERT_TRUE(c) MG_BOOL(c, true, true)
#define ASSERT_FALSE(c) MG_BOOL(c, false, true)

// Entry point (the CMake build links GTest::gtest_main instead). Define
// MINIGTEST_NO_MAIN in every test file but one when several are linked together.
#ifndef MINIGTEST_NO_MAIN
int main() {
    int passed = 0;
    for (auto& t : ::minigtest::registry()) {
        ::minigtest::currentFailed() = false;
        std::cout << "[ RUN      ] " << t.suite << "." << t.name << "\n";
        try {
            t.fn();
        } catch (const ::minigtest::Fatal&) {
        } catch (const std::exception& e) {
            ::minigtest::report(__FILE__, __LINE__, std::string("  Unexpected exception: ") + e.what());
        }
        if (::minigtest::currentFailed()) {
            ++::minigtest::failures();
            std::cout << "[  FAILED  ] " << t.suite << "." << t.name << "\n";
        } else {
            ++passed;
            std::cout << "[       OK ] " << t.suite << "." << t.name << "\n";
        }
    }
    std::cout << "[==========] " << ::minigtest::registry().size() << " tests, " << passed << " passed, "
              << ::minigtest::failures() << " failed\n";
    return ::minigtest::failures() == 0 ? 0 : 1;
}
#endif  // MINIGTEST_NO_MAIN
