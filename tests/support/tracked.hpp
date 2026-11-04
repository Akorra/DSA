// tracked: an element type that counts its own special member calls and
// detects lifetime bugs. Test-only; it never ships in include/dsa/.
//
// Use it as the element type of a container under test, then assert on
// stats(). Call reset() at the start of every test, while no tracked object
// of that type is alive (otherwise live() goes negative later).
//
// Each instantiation has its own counters: tracked and tracked_throwing_move
// don't see each other's calls. The counters are plain statics, so this is
// NOT thread-safe.
//
// How lifetime bugs are detected: every live object's address is kept in a
// registry (a std::set) *outside* the objects. A ctor inserts `this`, the dtor
// erases it. A dtor that finds nothing to erase is a double destroy, and any
// use of an address that isn't registered is a use-after-destroy.
// Why not a "destroyed" flag inside the object? Checking it means reading an
// object after its destructor ran, which is UB, and GCC deletes the flag's
// store at -O2 even through volatile. The registry check only uses the
// address, never the dead object's memory.
//
// Limit: a double destroy is itself UB. When the optimizer can see the whole
// thing (both destructor calls inlined into one function), GCC and Clang at
// -O2 both miss it, even with the registry (checked 2026-11-04). Detection
// is only reliable in the unoptimized debug presets, which also run ASan.
// Treat a release-only pass as weaker evidence for lifetime bugs.
#pragma once

#include <exception>
#include <set>

namespace dsa::test {

// The only exception tracked ever throws, so a test can tell an injected
// failure from a real one.
struct injected_error : std::exception {
    const char* what() const noexcept override { return "dsa::test::injected_error"; }
};

// All counts since the last reset(). A ctor that throws is not counted: the
// object never existed. A copy/move *assignment* that throws leaves the
// target unchanged and is not counted either.
struct tracked_stats {
    long default_ctor = 0;
    long value_ctor   = 0;
    long copy_ctor    = 0;
    long move_ctor    = 0;
    long copy_assign  = 0;
    long move_assign  = 0;
    long dtor         = 0;
    // Lifetime bugs seen: destroying twice; constructing on top of a live
    // object (it was never destroyed); copying/moving/assigning from, reading,
    // or assigning to a destroyed object; and copying/moving from a moved-from
    // object (its value is gone, so reading it is almost always a container
    // bug). Assigning *to* a moved-from object and destroying it are fine:
    // that's what std allows.
    long violations = 0;

    long constructed() const { return default_ctor + value_ctor + copy_ctor + move_ctor; }
    long live() const { return constructed() - dtor; }   // objects alive right now
};

template <bool NoexceptMove>
class basic_tracked {
public:
    basic_tracked() : value_(0)
    {
        born();
        ++s_.stats.default_ctor;
    }

    explicit basic_tracked(int v) : value_(v)
    {
        born();
        ++s_.stats.value_ctor;
    }

    basic_tracked(const basic_tracked& other)
    {
        check_source(other);
        maybe_throw_copy();          // before born(): if it throws, *this never existed
        born();
        value_ = other.value_;
        ++s_.stats.copy_ctor;
    }

    basic_tracked(basic_tracked&& other) noexcept(NoexceptMove)
    {
        check_source(other);
        maybe_throw_move();
        born();
        value_ = other.value_;
        other.moved_from_ = true;
        other.value_ = moved_value;
        ++s_.stats.move_ctor;
    }

    basic_tracked& operator=(const basic_tracked& other)
    {
        check_target();
        check_source(other);
        maybe_throw_copy();          // strong guarantee: on throw, *this is untouched
        value_ = other.value_;
        moved_from_ = false;         // assigning revives a moved-from object
        ++s_.stats.copy_assign;
        return *this;
    }

    basic_tracked& operator=(basic_tracked&& other) noexcept(NoexceptMove)
    {
        check_target();
        // Self-move is exempt: std::swap(a, a) does `a = std::move(a)` while
        // a is moved-from, and std algorithms are allowed to self-swap.
        if (this != &other)
            check_source(other);
        maybe_throw_move();
        if (this != &other) {        // self-move: leave the value as it was
            value_ = other.value_;
            moved_from_ = false;
            other.moved_from_ = true;
            other.value_ = moved_value;
        }
        ++s_.stats.move_assign;
        return *this;
    }

    ~basic_tracked()
    {
        if (registry().erase(this) == 0)
            ++s_.stats.violations;   // double destroy (or never constructed)
        ++s_.stats.dtor;
    }

    // -1 after being moved from. Reading a destroyed object is a violation.
    int value() const
    {
        if (!is_alive(this))
            ++s_.stats.violations;
        return value_;
    }

    bool moved_from() const { return moved_from_; }

    friend bool operator==(const basic_tracked& a, const basic_tracked& b)
    {
        return a.value() == b.value();
    }

    static tracked_stats& stats() { return s_.stats; }

    // Zeroes the counters and cancels the throw plans. Deliberately leaves the
    // registry alone: it describes which objects really exist, and that
    // doesn't change because a test wants fresh counts.
    static void reset() { s_ = {}; }

    // The n-th copy (ctor or assignment, counted from now, 1-based) throws
    // injected_error. n <= 0 disables it. The plan fires once.
    static void throw_on_copy(long n)
    {
        s_.copies_seen = 0;
        s_.throw_copy_at = n;
    }

    // Same for moves. A noexcept move that threw would call std::terminate,
    // so this only exists for tracked_throwing_move.
    static void throw_on_move(long n)
        requires(!NoexceptMove)
    {
        s_.moves_seen = 0;
        s_.throw_move_at = n;
    }

private:
    static constexpr int moved_value = -1;

    struct shared {
        tracked_stats stats{};
        long copies_seen   = 0;
        long throw_copy_at = 0;
        long moves_seen    = 0;
        long throw_move_at = 0;
    };

    // Addresses of every object of this type that is alive right now. A
    // function-local static, so it's constructed on first use and can't hit
    // the static initialization order problem from another TU's globals.
    // Inserting allocates; if that throws inside the noexcept move, the test
    // terminates, which is acceptable for a test type.
    static std::set<const basic_tracked*>& registry()
    {
        static std::set<const basic_tracked*> r;
        return r;
    }

    static bool is_alive(const basic_tracked* p) { return registry().contains(p); }

    void born()
    {
        if (!registry().insert(this).second)
            ++s_.stats.violations;   // constructed on top of a live object
    }

    static void check_source(const basic_tracked& other)
    {
        if (!is_alive(&other) || other.moved_from_)
            ++s_.stats.violations;
    }

    void check_target() const
    {
        if (!is_alive(this))
            ++s_.stats.violations;
    }

    static void maybe_throw_copy()
    {
        if (s_.throw_copy_at > 0 && ++s_.copies_seen == s_.throw_copy_at) {
            s_.throw_copy_at = 0;
            throw injected_error{};
        }
    }

    static void maybe_throw_move()
    {
        // if constexpr: in the noexcept variant there is no throw expression
        // at all, so the compiler can't warn about throw-in-noexcept.
        if constexpr (!NoexceptMove) {
            if (s_.throw_move_at > 0 && ++s_.moves_seen == s_.throw_move_at) {
                s_.throw_move_at = 0;
                throw injected_error{};
            }
        }
    }

    int  value_      = 0;
    bool moved_from_ = false;

    static inline shared s_{};
};

using tracked               = basic_tracked<true>;    // like most real types
using tracked_throwing_move = basic_tracked<false>;   // move may throw

} // namespace dsa::test
