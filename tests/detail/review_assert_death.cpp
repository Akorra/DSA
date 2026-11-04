// 03_review: a failing DSA_ASSERT inside a noexcept function must still abort
// with our report, not std::terminate's (Claude, p0-infra round 1).
#include <dsa/detail/assert.hpp>

#include <cstdio>

namespace {

int pop_front(int size) noexcept
{
    DSA_ASSERT(size > 0, "pop_front on empty container");
    return size - 1;
}

} // namespace

int main(int argc, char**)
{
    // argc - 1 == 0 when run with no arguments; not a constant, so nothing folds.
    const int r = pop_front(argc - 1);
    std::printf("returned %d\n", r);
    return 0;
}
