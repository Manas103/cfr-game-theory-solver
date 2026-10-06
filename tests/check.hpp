#pragma once
#include <cmath>
#include <cstdio>

inline int g_checks = 0;
inline int g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        ++g_checks;                                                         \
        if (!(cond)) {                                                      \
            ++g_failures;                                                   \
            std::fprintf(stderr, "CHECK FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                \
    do {                                                                     \
        ++g_checks;                                                          \
        const double av = (a), bv = (b);                                    \
        if (!(std::fabs(av - bv) <= (tol))) {                                \
            ++g_failures;                                                    \
            std::fprintf(stderr, "CHECK_NEAR FAILED: %s=%.6f vs %s=%.6f (tol %g) (%s:%d)\n", #a, \
                          av, #b, bv, static_cast<double>(tol), __FILE__, __LINE__); \
        }                                                                     \
    } while (0)

inline int check_summary(const char* suite) {
    std::fprintf(stdout, "%s: %d checks, %d failures\n", suite, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
