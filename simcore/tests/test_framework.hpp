// Tiny dependency-free test framework (so simcore builds anywhere with no downloads).
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace sctest {

struct TestCase {
    const char* name;
    std::function<void()> fn;
};

inline std::vector<TestCase>& Registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& FailureCount() {
    static int n = 0;
    return n;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { Registry().push_back({name, std::move(fn)}); }
};

inline void ReportFailure(const char* file, int line, const std::string& msg) {
    std::printf("    FAIL %s:%d: %s\n", file, line, msg.c_str());
    ++FailureCount();
}

inline int RunAll(const char* filter = nullptr) {
    int failed_tests = 0;
    // simcore_tests <text> runs only the tests whose name contains it (flushing as it goes, to find a crash).
    for (const TestCase& t : Registry()) {
        if (filter != nullptr && std::strstr(t.name, filter) == nullptr) continue;
        if (filter != nullptr) std::fflush(stdout);
        const int before = FailureCount();
        t.fn();
        const bool ok = FailureCount() == before;
        std::printf("[%s] %s\n", ok ? " OK " : "FAIL", t.name);
        if (!ok) ++failed_tests;
    }
    std::printf("\n%zu tests, %d failed\n", Registry().size(), failed_tests);
    return failed_tests == 0 ? 0 : 1;
}

}  // namespace sctest

#define SC_CONCAT_INNER(a, b) a##b
#define SC_CONCAT(a, b) SC_CONCAT_INNER(a, b)

#define SC_TEST(name)                                                              \
    static void SC_CONCAT(sc_test_fn_, __LINE__)();                                \
    static sctest::Registrar SC_CONCAT(sc_test_reg_, __LINE__)(name, &SC_CONCAT(sc_test_fn_, __LINE__)); \
    static void SC_CONCAT(sc_test_fn_, __LINE__)()

#define SC_EXPECT(cond)                                                            \
    do {                                                                           \
        if (!(cond)) sctest::ReportFailure(__FILE__, __LINE__, "expected: " #cond); \
    } while (0)

#define SC_EXPECT_NEAR(actual, expected, tol)                                      \
    do {                                                                           \
        const double sc_a_ = (actual), sc_e_ = (expected), sc_t_ = (tol);          \
        if (!(std::fabs(sc_a_ - sc_e_) <= sc_t_)) {                                \
            char sc_buf_[256];                                                     \
            std::snprintf(sc_buf_, sizeof(sc_buf_), "%s = %.9g, expected %.9g +/- %.3g", #actual, sc_a_, sc_e_, sc_t_); \
            sctest::ReportFailure(__FILE__, __LINE__, sc_buf_);                    \
        }                                                                          \
    } while (0)
