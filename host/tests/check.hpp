#pragma once
// Minimal test helpers (no external framework needed).
#include <cmath>
#include <cstdio>
#include <cstdlib>

inline int g_failures = 0;

#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
            ++g_failures;                                                                   \
        }                                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                               \
    do {                                                                                    \
        const double va_ = (a), vb_ = (b);                                                  \
        if (!(std::fabs(va_ - vb_) <= (tol))) {                                             \
            std::printf("  FAIL %s:%d  %s = %g, expected %g +/- %g\n", __FILE__, __LINE__,  \
                        #a, va_, vb_, double(tol));                                         \
            ++g_failures;                                                                   \
        }                                                                                   \
    } while (0)

#define TEST_RESULT()                                                                       \
    (std::printf(g_failures ? "%d FAILURE(S)\n" : "ALL PASSED\n", g_failures), g_failures ? 1 : 0)
