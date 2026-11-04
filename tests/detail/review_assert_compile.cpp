// 03_review compile-parity tests for DSA_ASSERT (Claude, p0-infra round 1).
// Rule under test: a condition that compiles with asserts on must compile
// with asserts off, and one that is rejected with asserts on must be rejected
// with asserts off. Otherwise debug and release are different programs, and
// a release-only build break shows up first in CI or at a customer.
// Exactly one REVIEW_CASE_* is defined per target (see CMakeLists.txt).
#include <dsa/detail/assert.hpp>

#if defined(REVIEW_CASE_BITFIELD)
// Must compile in both modes: `if (s.ready)` is fine, so the assert must be too.
struct Status {
    unsigned ready : 1;
    unsigned error : 1;
};

int main()
{
    const Status s{1, 0};
    DSA_ASSERT(s.ready, "bit-field condition");
    DSA_ASSERT(!s.error, "negated bit-field condition");
    return 0;
}

#elif defined(REVIEW_CASE_NOT_TESTABLE)
// Must be REJECTED in both modes: `if (n)` doesn't compile for this type.
struct NotTestable {};

int main()
{
    const NotTestable n{};
    DSA_ASSERT(n, "this condition has no bool meaning");
    (void)n;
    return 0;
}

#else
#  error "define one REVIEW_CASE_*"
#endif
