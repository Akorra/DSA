// 03_review adversarial tests for DSA_ASSERT and config.hpp (Claude, p0-infra round 1).
// Built three times (see CMakeLists.txt): asserts forced on, forced off, and
// left to config.hpp's default. Each TEST_CASE must hold in every variant
// that it's compiled into.
#include <dsa/config.hpp>
#include <dsa/detail/assert.hpp>

#include <catch2/catch_test_macros.hpp>

namespace {

// Only usable in a constant expression if the passing assert is constexpr-friendly.
constexpr int checked_index(int i)
{
    DSA_ASSERT(i >= 0 && i < 4, "index out of range");
    return i;
}
static_assert(checked_index(2) == 2, "a passing DSA_ASSERT must be allowed in constexpr");

int noexcept_user(int x) noexcept
{
    DSA_ASSERT(x > 0, "positive");
    return x;
}

// Must compile warning-free (we build with -Werror): a variable whose only
// use is inside an assert must not trigger -Wunused when asserts are off.
int only_used_in_assert()
{
    const int n = checked_index(1);
    DSA_ASSERT(n == 1, "n is only read here");
    int m = 0;
    m = checked_index(3);
    DSA_ASSERT(m == 3, "m is set, then only read here");
    return 0;
}

} // namespace

TEST_CASE("version macros are consistent", "[config]")
{
    STATIC_REQUIRE(DSA_VERSION ==
                   DSA_VERSION_MAJOR * 10000 + DSA_VERSION_MINOR * 100 + DSA_VERSION_PATCH);
    STATIC_REQUIRE(DSA_VERSION_MINOR < 100);
    STATIC_REQUIRE(DSA_VERSION_PATCH < 100);
}

TEST_CASE("DSA_ENABLE_ASSERTS is always 0 or 1", "[config]")
{
    STATIC_REQUIRE((DSA_ENABLE_ASSERTS == 0 || DSA_ENABLE_ASSERTS == 1));
}

#if defined(REVIEW_DEFAULT_CONFIG)
TEST_CASE("default DSA_ENABLE_ASSERTS follows NDEBUG", "[config]")
{
#  ifdef NDEBUG
    STATIC_REQUIRE(DSA_ENABLE_ASSERTS == 0);
#  else
    STATIC_REQUIRE(DSA_ENABLE_ASSERTS == 1);
#  endif
}
#endif

TEST_CASE("condition is evaluated once when on, never when off", "[assert]")
{
    int evaluations = 0;
    DSA_ASSERT(++evaluations > 0, "side effect in the condition");
    CHECK(evaluations == DSA_ENABLE_ASSERTS);
}

TEST_CASE("message is not evaluated when the assert passes", "[assert]")
{
    int evaluations = 0;
    DSA_ASSERT(true, (++evaluations, "message with a side effect"));
    CHECK(evaluations == 0);
}

TEST_CASE("expands to exactly one statement", "[assert]")
{
    // Unbraced if/else: a macro that isn't a single statement either fails
    // to compile here or silently binds the else to the wrong if.
    bool took_else = false;
    const bool flag = false;
    if (flag)
        DSA_ASSERT(true, "then-branch");
    else
        took_else = true;
    CHECK(took_else);

    for (int i = 0; i < 3; ++i)
        DSA_ASSERT(i < 3, "loop body without braces");
}

TEST_CASE("usable in constexpr and noexcept functions", "[assert]")
{
    CHECK(checked_index(3) == 3);
    CHECK(noexcept_user(5) == 5);
    STATIC_REQUIRE(noexcept(noexcept_user(1)));
    CHECK(only_used_in_assert() == 0);
}

TEST_CASE("contextual bool conversion works like if()", "[assert]")
{
    struct ExplicitBool {
        bool value;
        constexpr explicit operator bool() const noexcept { return value; }
    };
    const ExplicitBool ok{true};
    DSA_ASSERT(ok, "explicit operator bool must be enough");

    const int target = 0;
    const int* p = &target;
    DSA_ASSERT(p, "pointer condition");
    SUCCEED();
}
