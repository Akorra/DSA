/*
 * TEST MATRIX SPECIFICATION
 * 
 * PORTABLE GUARANTEES (Standard C++):
 * - The copy-vs-move choice governed by std::move_if_noexcept.
 * - The Strong Exception Guarantee (vector remains entirely unchanged after a throw).
 * - Logical lifecycle side-effects (e.g., clear() resetting size while keeping capacity).
 * - Resource safety: every successfully constructed element is destroyed exactly once (no leaks).
 * 
 * IMPLEMENTATION-SPECIFIC OBSERVATIONS (Tagged with [libstdc++]):
 * - Exact internal operation counts (value/copy/move constructors and destructors).
 * - The precise order of object construction and argument evaluation during buffer reallocation.
 */


#include "support/tracked.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using dsa::test::injected_error;
using dsa::test::tracked;
using dsa::test::tracked_throwing_move;


TEST_CASE("std::vector: push_back lvalue copies once", "[vector][libstdc++]")
{
    // Implementation-specific: Asserts exact structural constructor/destructor operations.
    tracked::reset();
    {
        std::vector<tracked> v; // empty vector: no existing elements to relocate
        
        tracked x{5};

        v.push_back(x);

        const auto& s = tracked::stats();
        CHECK(s.value_ctor == 1);
        CHECK(s.copy_ctor == 1);
        CHECK(s.move_ctor == 0);
        CHECK(s.copy_assign == 0);
        CHECK(s.move_assign == 0);
        CHECK(s.dtor == 0);
        CHECK(s.live() == 2);
    }
    CHECK(tracked::stats().dtor == 2);
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("std::vector: push_back rvalue moves once", "[vector][libstdc++]")
{
    // Implementation-specific: Asserts exact operations and temporary destruction lifetimes.
    tracked::reset();
    {
        std::vector<tracked> v; // empty vector: no existing elements to relocate

        v.push_back(tracked{5});

        const auto& s = tracked::stats();
        CHECK(s.value_ctor == 1);
        CHECK(s.copy_ctor == 0);
        CHECK(s.move_ctor == 1);
        CHECK(s.copy_assign == 0);
        CHECK(s.move_assign == 0);
        CHECK(s.dtor == 1); // the temporary is destroyed after move
        CHECK(s.live() == 1);
    }
    CHECK(tracked::stats().dtor == 2);
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("std::vector: emplace_back constructs in place", "[vector][libstdc++]")
{
    // Implementation-specific: Asserts exact count (0 copies/moves), though conceptually portable.
    tracked::reset();
    {
        std::vector<tracked> v; // empty vector: no existing elements to relocate

        v.emplace_back(5);

        const auto& s = tracked::stats();
        CHECK(s.value_ctor == 1);
        CHECK(s.copy_ctor == 0);
        CHECK(s.move_ctor == 0);
        CHECK(s.copy_assign == 0);
        CHECK(s.move_assign == 0);
        CHECK(s.dtor == 0); 
        CHECK(s.live() == 1);
    }
    CHECK(tracked::stats().dtor == 1);
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("std::vector: reallocation moves when move is noexcept", "[vector][libstdc++]")
{
    // Implementation-specific: Asserts exact operation counts during capacity scaling.
    long n = 0;

    tracked::reset();
    {
        std::vector<tracked> v;
        v.reserve(4);

        // fill to capacity to trigger reallocation on next insert
        n = static_cast<long>(v.capacity());
        REQUIRE(n > 0); 

        for(std::size_t i = 0; i < static_cast<std::size_t>(n); ++i)
            v.emplace_back(static_cast<int>(i));
        
        // next insert will trigger reallocation, which will move existing elements to new storage
        v.emplace_back(static_cast<int>(n));

        REQUIRE(v.capacity() > static_cast<std::size_t>(n)); // reallocation occurred

        const auto& s = tracked::stats();
        CHECK(s.value_ctor == n + 1);
        CHECK(s.copy_ctor == 0);
        CHECK(s.move_ctor == n); // all existing elements moved to new storage
        CHECK(s.copy_assign == 0);
        CHECK(s.move_assign == 0);
        CHECK(s.dtor == n); // free old storage after moving
        CHECK(s.live() == n + 1);
    }
    CHECK(tracked::stats().dtor == n*2 + 1);
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("std::vector: reallocation copies when move may throw", "[vector][libstdc++]")
{
    // Implementation-specific: Asserts exact operation counts under throwing-move conditions.
    long n = 0;

    tracked_throwing_move::reset();
    {
        std::vector<tracked_throwing_move> v;
        v.reserve(4);

        // fill to capacity to trigger reallocation on next insert
        n = static_cast<long>(v.capacity());
        REQUIRE(n > 0);

        for(std::size_t i = 0; i < static_cast<std::size_t>(n); ++i)
            v.emplace_back(static_cast<int>(i));
        
        // next insert will trigger reallocation, which will copy existing elements to new storage
        v.emplace_back(static_cast<int>(n));

        REQUIRE(v.capacity() > static_cast<std::size_t>(n)); // reallocation occurred

        const auto& s = tracked_throwing_move::stats();
        CHECK(s.value_ctor == n + 1);
        CHECK(s.copy_ctor == n);
        CHECK(s.move_ctor == 0);
        CHECK(s.copy_assign == 0);
        CHECK(s.move_assign == 0);
        CHECK(s.dtor == n); // free old storage after copying
        CHECK(s.live() == n + 1);
    }
    CHECK(tracked_throwing_move::stats().dtor == n * 2 + 1);
    CHECK(tracked_throwing_move::stats().live() == 0);
    CHECK(tracked_throwing_move::stats().violations == 0);
}

TEST_CASE("std::vector: throw during reallocation leaves vector unchanged", "[vector][libstdc++]")
{
    // Mixed: Asserts the portable Strong Guarantee, but relies on libstdc++ order of construction for the final dtor count.
    long n = 0;

    tracked_throwing_move::reset();
    {
        std::vector<tracked_throwing_move> v;
        v.reserve(4);

        // fill to capacity to trigger reallocation on next insert
        n = static_cast<long>(v.capacity());
        REQUIRE(n > 0);

        for(std::size_t i = 0; i < static_cast<std::size_t>(n); ++i)
            v.emplace_back(static_cast<int>(i));

        tracked_throwing_move::throw_on_copy(3);
        
        // next insert will trigger reallocation, which will copy existing elements to new storage
        // but we will throw during the copy of the 3rd element
        REQUIRE_THROWS_AS(v.emplace_back(static_cast<int>(n)), injected_error);
        REQUIRE(v.capacity() == static_cast<std::size_t>(n));

        // vector should be unchanged: size and capacity should be the same as before the failed emplace_back

        // --- Portable Strong Guarantee Assertions ---
        CHECK(v.size() == static_cast<std::size_t>(n));
        CHECK(v.capacity() == static_cast<std::size_t>(n));
        CHECK(tracked_throwing_move::stats().live() == n);

        for(std::size_t i = 0; i < static_cast<std::size_t>(n); ++i)
        {
            CHECK(v[i].value() == static_cast<int>(i));
            CHECK(v[i].moved_from() == false);
        }
    }
    // n at scope end + 3 from the failed reallocation's rollback: the 2 copies
    // that succeeded, plus the new element. The copy that threw constructed
    // nothing. libstdc++-specific: it constructs the new element in the new
    // storage *before* relocating (so push_back(v[0]) works); the standard
    // only requires that aliasing works, not this order.

    // --- Implementation-Specific Count (libstdc++ dependent) ---
    CHECK(tracked_throwing_move::stats().dtor == n + 3);
    CHECK(tracked_throwing_move::stats().live() == 0);
    CHECK(tracked_throwing_move::stats().violations == 0);
}

TEST_CASE("std::vector: clear destroys all and keeps capacity", "[vector]")
{
    // Fully Portable: Validates standard-mandated side effects of clear() and total resource cleanup.
    tracked::reset();
    {
        std::vector<tracked> v;
        v.reserve(4);

        for(int i = 0; i < 4; ++i)
            v.emplace_back(i);

        std::size_t old_capacity = v.capacity();

        REQUIRE(v.size() == 4);

        v.clear();

        CHECK(v.size() == 0);
        CHECK(v.capacity() == old_capacity); // capacity remains unchanged after clear

        const auto& s = tracked::stats();
        CHECK(s.value_ctor == 4);
        CHECK(s.copy_ctor == 0);
        CHECK(s.move_ctor == 0);
        CHECK(s.copy_assign == 0);
        CHECK(s.move_assign == 0);
        CHECK(s.dtor == 4); // all elements destroyed
        CHECK(s.live() == 0);
    }
    CHECK(tracked::stats().dtor == 4);
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("std::vector: vector destructor destroys every element", "[vector]")
{
    // Fully Portable: Validates strict RAII container destruction guarantees.
    tracked::reset();
    {
        std::vector<tracked> v;
        v.reserve(4);

        for(int i = 0; i < 4; ++i)
            v.emplace_back(i);

        REQUIRE(v.size() == 4);
    }
    CHECK(tracked::stats().dtor == 4);
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

