// 03_review adversarial tests for p0-tracked (Claude, round 1).
// Characterization of std::vector beyond the learner's suite. Every assertion
// here is a std guarantee (strong guarantee, aliasing, no leaks); exact
// operation counts are deliberately NOT asserted, so these hold on any
// conforming standard library.
#include "support/tracked.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

using dsa::test::injected_error;
using dsa::test::tracked;
using dsa::test::tracked_throwing_move;

namespace {

// Fills v with 0, 1, 2, ... until size() == capacity() (capacity >= min_cap).
// Returns the size, i.e. how many elements the next growth must relocate.
template <class T>
long fill_to_capacity(std::vector<T>& v, std::size_t min_cap)
{
    v.reserve(min_cap);
    while (v.size() < v.capacity())
        v.emplace_back(static_cast<int>(v.size()));
    return static_cast<long>(v.size());
}

// Strong guarantee: same size, same capacity, same storage, same values,
// nothing moved-from.
template <class T>
void require_unchanged(const std::vector<T>& v, long n, const T* data_before)
{
    REQUIRE(v.size() == static_cast<std::size_t>(n));
    REQUIRE(v.capacity() == static_cast<std::size_t>(n));
    REQUIRE(v.data() == data_before);
    for (std::size_t i = 0; i < v.size(); ++i) {
        CHECK(v[i].value() == static_cast<int>(i));
        CHECK_FALSE(v[i].moved_from());
    }
}

} // namespace

TEST_CASE("review std::vector: emplace_back throwing on every copy N keeps strong guarantee",
          "[vector][review]")
{
    // The learner's test throws on copy 3 only. Every N exercises a different
    // amount of rollback: N = 1 has nothing to clean up, N = n the most.
    tracked_throwing_move::reset();
    std::vector<tracked_throwing_move> probe;
    const long n = fill_to_capacity(probe, 4);

    for (long k = 1; k <= n; ++k) {
        DYNAMIC_SECTION("throw on copy " << k << " of " << n) {
            tracked_throwing_move::reset();
            {
                std::vector<tracked_throwing_move> v;
                REQUIRE(fill_to_capacity(v, 4) == n);
                const auto* data_before = v.data();

                tracked_throwing_move::throw_on_copy(k);
                REQUIRE_THROWS_AS(v.emplace_back(-7), injected_error);

                require_unchanged(v, n, data_before);
                CHECK(tracked_throwing_move::stats().live() == n);   // rollback leaked nothing
            }
            CHECK(tracked_throwing_move::stats().live() == 0);
            CHECK(tracked_throwing_move::stats().violations == 0);
        }
    }
}

TEST_CASE("review std::vector: a throw plan past the last copy doesn't fire", "[vector][review]")
{
    // Boundary: n copies happen, the plan is for copy n + 1.
    tracked_throwing_move::reset();
    {
        std::vector<tracked_throwing_move> v;
        const long n = fill_to_capacity(v, 4);
        tracked_throwing_move::throw_on_copy(n + 1);
        REQUIRE_NOTHROW(v.emplace_back(-7));
        CHECK(v.size() == static_cast<std::size_t>(n + 1));
        CHECK(v.back().value() == -7);
    }
    CHECK(tracked_throwing_move::stats().live() == 0);
    CHECK(tracked_throwing_move::stats().violations == 0);
}

TEST_CASE("review std::vector: push_back(lvalue) on a full vector, throw on every copy N",
          "[vector][review]")
{
    // n + 1 copies happen: the n relocations and the copy of x. The standard
    // doesn't say in which order, so every N must leave v and x untouched.
    tracked_throwing_move::reset();
    std::vector<tracked_throwing_move> probe;
    const long n = fill_to_capacity(probe, 4);

    for (long k = 1; k <= n + 1; ++k) {
        DYNAMIC_SECTION("throw on copy " << k << " of " << n + 1) {
            tracked_throwing_move::reset();
            {
                std::vector<tracked_throwing_move> v;
                REQUIRE(fill_to_capacity(v, 4) == n);
                const auto* data_before = v.data();
                const tracked_throwing_move x{42};

                tracked_throwing_move::throw_on_copy(k);
                REQUIRE_THROWS_AS(v.push_back(x), injected_error);

                require_unchanged(v, n, data_before);
                CHECK(x.value() == 42);
                CHECK(tracked_throwing_move::stats().live() == n + 1);   // v's elements + x
            }
            CHECK(tracked_throwing_move::stats().live() == 0);
            CHECK(tracked_throwing_move::stats().violations == 0);
        }
    }
}

TEST_CASE("review std::vector: push_back(lvalue) copy throws with spare capacity", "[vector][review]")
{
    // No reallocation involved: the only copy is the new element's.
    tracked::reset();
    {
        std::vector<tracked> v;
        v.reserve(8);
        v.emplace_back(0);
        v.emplace_back(1);
        const auto* data_before = v.data();
        const auto cap_before = v.capacity();
        const tracked x{42};

        tracked::throw_on_copy(1);
        REQUIRE_THROWS_AS(v.push_back(x), injected_error);

        CHECK(v.size() == 2);
        CHECK(v.capacity() == cap_before);
        CHECK(v.data() == data_before);
        CHECK(v[0].value() == 0);
        CHECK(v[1].value() == 1);
        CHECK(tracked::stats().live() == 3);
    }
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("review std::vector: push_back(v[0]) on a full vector (self-reference)", "[vector][review]")
{
    // The argument refers into the storage that reallocation is about to
    // free. The standard requires this to work for both relocation strategies.
    SECTION("noexcept move: elements are moved, v[0] is moved-from mid-operation") {
        tracked::reset();
        {
            std::vector<tracked> v;
            const long n = fill_to_capacity(v, 4);
            v.push_back(v[0]);
            REQUIRE(v.size() == static_cast<std::size_t>(n + 1));
            CHECK(v.back().value() == 0);
            CHECK(v.front().value() == 0);
        }
        CHECK(tracked::stats().live() == 0);
        CHECK(tracked::stats().violations == 0);
    }
    SECTION("throwing move: elements are copied") {
        tracked_throwing_move::reset();
        {
            std::vector<tracked_throwing_move> v;
            const long n = fill_to_capacity(v, 4);
            v.push_back(v[0]);
            REQUIRE(v.size() == static_cast<std::size_t>(n + 1));
            CHECK(v.back().value() == 0);
            CHECK(v.front().value() == 0);
        }
        CHECK(tracked_throwing_move::stats().live() == 0);
        CHECK(tracked_throwing_move::stats().violations == 0);
    }
    SECTION("emplace_back(v[0]) too") {
        tracked::reset();
        {
            std::vector<tracked> v;
            const long n = fill_to_capacity(v, 4);
            v.emplace_back(v[0]);
            REQUIRE(v.size() == static_cast<std::size_t>(n + 1));
            CHECK(v.back().value() == 0);
        }
        CHECK(tracked::stats().violations == 0);
    }
}

TEST_CASE("review std::vector: erase from the middle shifts by move-assignment, no lifetime bugs",
          "[vector][review]")
{
    // Exercises move-assign into moved-from objects, which tracked allows,
    // and checks that std never reads a moved-from element.
    tracked::reset();
    {
        std::vector<tracked> v;
        for (int i = 0; i < 6; ++i)
            v.emplace_back(i);

        v.erase(v.begin() + 1);

        REQUIRE(v.size() == 5);
        const int expected[] = {0, 2, 3, 4, 5};
        for (std::size_t i = 0; i < v.size(); ++i) {
            CHECK(v[i].value() == expected[i]);
            CHECK_FALSE(v[i].moved_from());
        }
        CHECK(tracked::stats().live() == 5);
        CHECK(tracked::stats().copy_ctor + tracked::stats().copy_assign == 0);   // nothrow move: no copies
    }
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("review std::vector: copy constructor throwing on every copy N leaks nothing",
          "[vector][review]")
{
    // If a container's copy ctor throws, the half-built copy must destroy
    // every element it already copied: the source stays intact, nothing leaks.
    constexpr int n = 5;
    for (int k = 1; k <= n; ++k) {
        DYNAMIC_SECTION("throw on copy " << k << " of " << n) {
            tracked::reset();
            {
                std::vector<tracked> src;
                for (int i = 0; i < n; ++i)
                    src.emplace_back(i);

                tracked::throw_on_copy(k);
                REQUIRE_THROWS_AS(std::vector<tracked>(src), injected_error);

                CHECK(tracked::stats().live() == n);   // only src's elements
                for (int i = 0; i < n; ++i)
                    CHECK(src[static_cast<std::size_t>(i)].value() == i);
            }
            CHECK(tracked::stats().live() == 0);
            CHECK(tracked::stats().violations == 0);
        }
    }
}
