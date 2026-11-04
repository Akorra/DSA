// Self-test for tests/support/tracked.hpp (Claude, p0-tracked M1).
// Proves the counters, the throw plans and violation detection work on their
// own, without any container, so a failure in a container test can be
// blamed on the container and not on tracked.
#include "support/tracked.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <new>
#include <type_traits>
#include <utility>

using dsa::test::injected_error;
using dsa::test::tracked;
using dsa::test::tracked_throwing_move;

// The noexcept-ness of the move is the whole point of having two types.
static_assert(std::is_nothrow_move_constructible_v<tracked>);
static_assert(std::is_nothrow_move_assignable_v<tracked>);
static_assert(!std::is_nothrow_move_constructible_v<tracked_throwing_move>);
static_assert(!std::is_nothrow_move_assignable_v<tracked_throwing_move>);
static_assert(!std::is_nothrow_copy_constructible_v<tracked>);

// throw_on_move only exists where a throwing move is legal.
template <class T>
concept has_throw_on_move = requires { T::throw_on_move(1L); };
static_assert(!has_throw_on_move<tracked>);
static_assert(has_throw_on_move<tracked_throwing_move>);

namespace {

// Raw storage for constructing and destroying an object by hand, so the
// self-test can call a destructor twice without the storage going away.
template <class T>
struct raw_slot {
    alignas(T) unsigned char bytes[sizeof(T)];
    // Via void*: GCC's -Wcast-align=strict rejects a direct cast from unsigned
    // char*, even though alignas makes it correct.
    T* ptr() { return std::launder(static_cast<T*>(static_cast<void*>(bytes))); }
};

} // namespace

TEST_CASE("tracked: each special member bumps exactly its counter", "[tracked]")
{
    tracked::reset();
    {
        tracked a;                     // default
        tracked b{7};                  // value
        tracked c{b};                  // copy ctor
        tracked d{std::move(c)};       // move ctor
        a = b;                         // copy assign
        a = std::move(d);              // move assign

        const auto& s = tracked::stats();
        CHECK(s.default_ctor == 1);
        CHECK(s.value_ctor == 1);
        CHECK(s.copy_ctor == 1);
        CHECK(s.move_ctor == 1);
        CHECK(s.copy_assign == 1);
        CHECK(s.move_assign == 1);
        CHECK(s.dtor == 0);
        CHECK(s.live() == 4);
    }
    CHECK(tracked::stats().dtor == 4);
    CHECK(tracked::stats().live() == 0);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("tracked: moved-from reads -1, and assignment revives it", "[tracked]")
{
    tracked::reset();
    tracked a{5};
    tracked b{std::move(a)};
    CHECK(b.value() == 5);
    CHECK(a.moved_from());
    CHECK(a.value() == -1);

    a = tracked{9};                    // assigning to moved-from is allowed
    CHECK_FALSE(a.moved_from());
    CHECK(a.value() == 9);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("tracked: counters are per instantiation", "[tracked]")
{
    tracked::reset();
    tracked_throwing_move::reset();
    tracked a{1};
    CHECK(tracked::stats().value_ctor == 1);
    CHECK(tracked_throwing_move::stats().value_ctor == 0);
}

TEST_CASE("tracked: throw_on_copy fires on the n-th copy only, once", "[tracked]")
{
    tracked::reset();
    const tracked src{3};
    tracked::throw_on_copy(2);

    tracked first{src};                                // copy 1: ok
    CHECK_THROWS_AS(tracked{src}, injected_error);     // copy 2: throws
    tracked third{src};                                // plan spent: ok

    // A ctor that throws never constructed anything.
    CHECK(tracked::stats().copy_ctor == 2);
    CHECK(tracked::stats().live() == 3);               // src, first, third
}

TEST_CASE("tracked: throwing copy assignment leaves target unchanged", "[tracked]")
{
    tracked::reset();
    const tracked src{3};
    tracked dst{8};
    tracked::throw_on_copy(1);
    CHECK_THROWS_AS(dst = src, injected_error);
    CHECK(dst.value() == 8);
    CHECK(tracked::stats().copy_assign == 0);
}

TEST_CASE("tracked: throw_on_move fires for the throwing variant", "[tracked]")
{
    tracked_throwing_move::reset();
    tracked_throwing_move a{4};
    tracked_throwing_move::throw_on_move(1);
    CHECK_THROWS_AS(tracked_throwing_move{std::move(a)}, injected_error);
    // The source wasn't touched: the throw came before the steal.
    CHECK_FALSE(a.moved_from());
    CHECK(a.value() == 4);
    CHECK(tracked_throwing_move::stats().move_ctor == 0);
    CHECK(tracked_throwing_move::stats().live() == 1);
}

TEST_CASE("tracked: reset clears throw plans", "[tracked]")
{
    tracked::reset();
    tracked::throw_on_copy(1);
    tracked::reset();
    const tracked src{1};
    CHECK_NOTHROW(tracked{src});
}

TEST_CASE("tracked: copying from a moved-from object is a violation", "[tracked]")
{
    tracked::reset();
    tracked a{1};
    tracked b{std::move(a)};
    tracked c{a};
    CHECK(tracked::stats().violations == 1);
    tracked d{std::move(a)};
    CHECK(tracked::stats().violations == 2);
}

TEST_CASE("tracked: self-swap is not a violation", "[tracked]")
{
    tracked::reset();
    tracked a{6};
    using std::swap;
    swap(a, a);
    CHECK(a.value() == 6);
    CHECK(tracked::stats().violations == 0);
}

TEST_CASE("tracked: double destroy is a violation", "[tracked]")
{
    tracked::reset();
    raw_slot<tracked> slot;
    std::construct_at(slot.ptr(), 1);
    std::destroy_at(slot.ptr());
    CHECK(tracked::stats().violations == 0);
    std::destroy_at(slot.ptr());       // the bug under test
    CHECK(tracked::stats().violations == 1);
    CHECK(tracked::stats().live() == -1);  // also visible as a negative count
}

TEST_CASE("tracked: use after destroy is a violation", "[tracked]")
{
    tracked::reset();
    raw_slot<tracked> slot;
    std::construct_at(slot.ptr(), 1);
    std::destroy_at(slot.ptr());

    (void)slot.ptr()->value();                 // read
    CHECK(tracked::stats().violations == 1);
    tracked copy{*slot.ptr()};                 // copy from
    CHECK(tracked::stats().violations == 2);
    *slot.ptr() = copy;                        // assign to
    CHECK(tracked::stats().violations == 3);
}

TEST_CASE("tracked: constructing on top of a live object is a violation", "[tracked]")
{
    tracked::reset();
    raw_slot<tracked> slot;
    std::construct_at(slot.ptr(), 1);
    std::construct_at(slot.ptr(), 2);  // the bug under test: first never destroyed
    CHECK(tracked::stats().violations == 1);
    std::destroy_at(slot.ptr());
    // One registration for two constructions, so live() still shows the leak.
    CHECK(tracked::stats().live() == 1);
}
