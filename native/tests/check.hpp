#pragma once

#include <cmath>
#include <cstdio>

/// Minimal test harness: no framework, just a counter and a printed line per check.
namespace dl::test {

inline int failures = 0;

inline void check(bool ok, const char* what) {
    std::printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

inline bool close(float a, float b, float epsilon = 1e-5f) { return std::fabs(a - b) < epsilon; }

inline int report() {
    std::printf("\n%s\n", failures == 0 ? "ALL PASSED" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}

}  // namespace dl::test
