# Build flags, sanitizers and presets

Reference for what the build does to dsa's own code (tests, benchmarks,
self-containment TUs) and why. None of this reaches a project that links
`dsa::dsa`: consumers get the include path and C++20, nothing else.

Verified on GCC 16.2 and Clang 23.1, x86-64 Linux. Flag behaviour differs
between compilers and versions; when in doubt, try it on a 3-line file.

## How it fits together

```
CMakePresets.json        picks compiler, build type, DSA_SANITIZERS
        │
CMakeLists.txt           if top-level: CXX_EXTENSIONS OFF, include the modules
        │
cmake/Warnings.cmake     → INTERFACE target dsa_warnings   (compile flags)
cmake/Sanitizers.cmake   → INTERFACE target dsa_sanitizers (compile + link flags)
        │
our targets              target_link_libraries(x PRIVATE dsa::dsa dsa_warnings dsa_sanitizers)
```

`dsa_warnings` and `dsa_sanitizers` hold flags only, no code. Linking them
`PRIVATE` applies the flags to that one target and stops there. Third-party
code (Catch2, Google Benchmark) never links them, so our `-Werror` can't break
their build.

Both modules are only included when dsa is the top-level project
(`PROJECT_IS_TOP_LEVEL`), so `add_subdirectory(dsa)` creates no extra targets
or cache options in someone else's build.

## Presets

| Preset        | Compiler | Build type     | Compile line (besides warnings)                                        | Tests |
|---------------|----------|----------------|-------------------------------------------------------------------------|-------|
| `debug`       | g++      | Debug          | `-g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer` | yes |
| `debug-clang` | clang++  | Debug          | same as `debug`                                                         | yes |
| `debug-nosan` | g++      | Debug          | `-g`                                                                    | yes |
| `release`     | g++      | Release        | `-O3 -DNDEBUG`                                                          | yes |
| `release-clang` | clang++ | Release       | `-O3 -DNDEBUG`                                                          | yes |
| `bench`       | g++      | RelWithDebInfo | `-O2 -g -DNDEBUG -fno-omit-frame-pointer`                               | no  |

All of them add `-std=c++20` (not `gnu++20`) and the warning set below.
Output goes to `build/<preset>/`, with `compile_commands.json` for clangd.

```sh
cmake --preset debug                 # configure
cmake --build --preset debug         # build
ctest --preset debug                 # test
cmake --workflow --preset debug      # all three (every preset except bench)
```

Why each one exists:

- **debug / debug-clang.** Daily work. Two compilers because each one warns
  about different things and their sanitizer runtimes differ.
- **debug-nosan.** For valgrind, which runs the unmodified binary on a
  simulated CPU and conflicts with ASan's own malloc and shadow memory.
  Valgrind also catches reads of uninitialised memory, which ASan doesn't.
- **release.** The optimizer assumes UB never happens (no signed overflow, no
  null dereference, no aliasing between unrelated types). Code with latent UB
  often "works" at `-O0` and changes behaviour here. `NDEBUG` is set, so
  `DSA_ASSERT` is off.
- **bench.** Optimized like a real build, but with debug info (`-g`) and frame
  pointers so `perf` can show symbols and full call stacks. `NDEBUG` is set,
  so asserts don't distort timings.

`bench` sets `DSA_BUILD_TESTS=OFF` and `DSA_BUILD_BENCHMARKS=ON`: it builds
benchmarks only. Benchmarks link `dsa_warnings` but never `dsa_sanitizers`,
which would distort every timing. Google Benchmark is found or fetched like
Catch2 (pinned v1.10.0), with its own tests, install rules and `-Werror` off.

The test presets set `noTestsAction: error`: if no tests are registered,
`ctest` fails instead of reporting success.

### CMake build types

| `CMAKE_BUILD_TYPE` | Flags CMake adds (GCC/Clang) |
|--------------------|------------------------------|
| Debug              | `-g`                         |
| Release            | `-O3 -DNDEBUG`               |
| RelWithDebInfo     | `-O2 -g -DNDEBUG`            |
| MinSizeRel         | `-Os -DNDEBUG`               |

## Options

| Option                   | Default | Effect |
|--------------------------|---------|--------|
| `DSA_SANITIZERS`         | empty   | `;`-separated, e.g. `address;undefined`. Becomes `-fsanitize=address,undefined` on compile and link, plus `-fno-sanitize-recover=all -fno-omit-frame-pointer`. GCC/Clang only. |
| `DSA_BUILD_TESTS`        | `ON`    | Build `tests/` (Catch2 tests, death tests, self-containment check). |
| `DSA_BUILD_BENCHMARKS`   | `OFF`   | Build `benchmarks/` (Google Benchmark). |
| `DSA_WARNINGS_AS_ERRORS` | `ON`    | Adds `-Werror` (`/WX` on MSVC). Turn off to see all warnings at once instead of stopping at the first. |
| `DSA_ENABLE_ASSERTS`     | (unset) | A preprocessor macro, not a CMake option: pass `-DCMAKE_CXX_FLAGS=-DDSA_ENABLE_ASSERTS=1`. Unset means on unless `NDEBUG`. |

Override any of these on top of a preset:

```sh
cmake --preset debug -DDSA_WARNINGS_AS_ERRORS=OFF
cmake --preset debug -DDSA_SANITIZERS="undefined"      # UBSan only
```

## Warnings (`cmake/Warnings.cmake`)

Every warning here points at a real class of bug. Each one has a cost (some
false positives, some casts to write), which is the point: an implicit
conversion should be a visible decision.

### GCC and Clang

| Flag | Catches | Notes |
|------|---------|-------|
| `-Wall` | Common mistakes: unused variables, uninitialised use, missing `return`, misleading indentation, ... | Despite the name, not "all". |
| `-Wextra` | Unused parameters, signed/unsigned compare, missing field initialisers, ... | GCC includes `-Wimplicit-fallthrough` here; Clang doesn't. |
| `-Wpedantic` | Non-ISO extensions: VLAs, zero-length arrays, stray `;` | Keeps the code portable to MSVC. |
| `-Wshadow` | A local hides another variable or member | Classic source of "I assigned it but it didn't change". |
| `-Wconversion` | Implicit narrowing: `size_t → int`, `double → float`, `int → char` | On Clang, also turns on the signedness checks below. |
| `-Wsign-conversion` | Implicit signed ↔ unsigned: `int → size_t` | In C++, GCC's `-Wconversion` doesn't include this, so it's listed separately. |
| `-Wnon-virtual-dtor` | Class with virtual functions but non-virtual destructor | `delete base_ptr` on it is UB. |
| `-Woverloaded-virtual` | Derived overload hides a base virtual of the same name | |
| `-Wold-style-cast` | `(T)x` | Forces `static_cast` / `reinterpret_cast`, which say what kind of cast it is and are greppable. |
| `-Wnull-dereference` | Dereference of a pointer the compiler proves is null | Path-based: GCC finds these only with optimisation (silent at `-O0`, warns at `-O2`), so mostly in `release`. Clang's version catches far less. |
| `-Wdouble-promotion` | `float` silently promoted to `double` | Matters for performance and on FPUs without double support. |
| `-Wformat=2` | `printf`-family format/argument mismatch, non-literal format strings | |
| `-Wimplicit-fallthrough` | `switch` case falling into the next without `[[fallthrough]]` | |

### GCC only

| Flag | Catches |
|------|---------|
| `-Wduplicated-cond` | `if (a) ... else if (a)` |
| `-Wduplicated-branches` | `if`/`else` with identical bodies |
| `-Wlogical-op` | Suspicious logic: `x == 1 && x == 2`, `a \|\| a` |
| `-Wcast-align=strict` | Cast that increases alignment (`char* → int*`). Plain `-Wcast-align` is silent on x86-64 because misaligned loads work there; `=strict` warns on every target. |

These are GCC-only because Clang doesn't know them. Clang warns about unknown
`-W` options, and with `-Werror` that warning becomes a build failure.

### Clang only

| Flag | Notes |
|------|-------|
| `-Wcast-align` | Clang rejects `=strict`. Its version only fires on C-style casts, which `-Wold-style-cast` already bans, so in practice GCC is the one checking alignment. |

### MSVC

`/W4` (high warning level) and `/permissive-` (strict standard conformance).

### Silencing a warning

Fix the code first. If the warning is a false positive, make the intent
explicit (`static_cast`, `[[maybe_unused]]`, `[[fallthrough]]`). Only as a
last resort, suppress it locally with `#pragma GCC diagnostic push/ignored/pop`
and a comment saying why. Never remove a flag from the list to make one site
compile.

## Sanitizers (`cmake/Sanitizers.cmake`)

A sanitizer is compile-time instrumentation plus a runtime library. The
compiler inserts checks around memory accesses or arithmetic; the runtime
holds the bookkeeping and prints the report. Hence the flags go on **both**
lines: compile inserts the checks, link pulls in the runtime. Leave out the
link flag and the build fails with undefined `__asan_*` / `__ubsan_*` symbols.

### What each one catches

| Sanitizer | Catches | Doesn't catch | Slowdown |
|-----------|---------|---------------|----------|
| `address` (ASan) | Heap/stack/global buffer overflow, use-after-free, double free, use-after-return (with the option below). Includes LeakSanitizer on Linux: leaks reported at exit. | Reads of uninitialised memory (use valgrind or MSan). Overflows that land inside another valid object. | ~2x |
| `undefined` (UBSan) | Signed overflow, invalid shifts, integer divide by zero, null dereference, misaligned access, out-of-bounds index on arrays of known size, invalid `bool`/enum values, reaching `__builtin_unreachable`. | Unsigned wraparound (defined behaviour, not UB). Most memory errors. | small |
| `thread` (TSan) | Data races | Can't combine with ASan. Not used yet; there's no concurrency in dsa. | 5-15x |
| `memory` (MSan) | Uninitialised reads | Clang only; needs every library, including the standard library, built with MSan. Not used. | ~3x |

ASan, TSan and MSan each need their own shadow-memory layout and their own
`malloc`, so only one of them can be in a build. UBSan combines with any of
them.

### The companion flags

- **`-fno-sanitize-recover=all`.** By default UBSan prints the error and
  *continues*, so the test still passes and the report scrolls past.
  With this flag the first error aborts, the test fails, and CI goes red.
  `all` rather than `undefined` so it covers every sanitizer in the list.
  ASan aborts on the first error anyway.
- **`-fno-omit-frame-pointer`.** Keeps a frame pointer in every function, so
  the runtime can walk the stack quickly and reports show the full call
  chain. `bench` uses it for the same reason: `perf` stacks.

### Runtime options (set in the test presets)

Sanitizer runtimes read options from environment variables at start-up.

| Variable | Value | Why |
|----------|-------|-----|
| `ASAN_OPTIONS` | `detect_stack_use_after_return=1` | Also catches using a pointer to a local after the function returned. Off by default on some toolchains because it costs memory. |
|  | `strict_init_order=1` | Reports a global constructor that reads a global from another file before it's initialised, even when the link order happened to make it work. |
| `UBSAN_OPTIONS` | `print_stacktrace=1` | UBSan prints only the source line by default; this adds the call stack. |

Other useful ones when debugging by hand: `ASAN_OPTIONS=detect_leaks=0` (turn
off leak reports while chasing something else), `halt_on_error=1`.

### Reading a report

The first stack in an ASan report is *where the bad access happened*. The
second ("allocated by", "freed by") is *where that memory came from*. For a
use-after-free, read both: the bug is usually a pointer that outlived what it
pointed into, such as an iterator kept across a reallocation.

### Valgrind (`debug-nosan`)

```sh
cmake --workflow --preset debug-nosan
valgrind --leak-check=full --track-origins=yes --error-exitcode=1 \
    ./build/debug-nosan/tests/<test_binary>
```

`--track-origins=yes` shows where an uninitialised value came from.
`--error-exitcode=1` makes it fail like a test would.

## Header self-containment (`cmake/SelfContained.cmake`)

For every `include/dsa/**/*.hpp` the build generates
`build/<preset>/tests/selfcontained/<header>.cpp` containing only

```cpp
#include <dsa/the/header.hpp>
#include <dsa/the/header.hpp>
```

and compiles it (target `dsa_selfcontained`, part of the normal build, with
our warnings and sanitizers). It fails if the header uses something it didn't
include itself, or if `#pragma once` is missing. In a test file a header can
silently "borrow" an include from another header included before it; alone
in a file it can't.

The header list uses `GLOB_RECURSE ... CONFIGURE_DEPENDS`, so a new header is
picked up on the next build without re-running CMake by hand.

## Seeing what flags a target actually gets

```sh
cmake --build --preset debug -- -v          # print every compile/link command
grep '"command"' build/debug/compile_commands.json
```

When a flag doesn't behave as expected, look at the real command line before
anything else.

## Not enabled, worth knowing

| Flag | What it does | Why not here (yet) |
|------|--------------|--------------------|
| `-D_GLIBCXX_ASSERTIONS` | Bounds and precondition checks inside libstdc++ (`operator[]`, `front()` on empty, ...) | Useful in tests that compare against std; may be added to `debug`. |
| `-D_GLIBCXX_DEBUG` | Full libstdc++ debug mode: checked iterators, invalidation detection | Changes the ABI of std types; everything must be built with it. |
| `-D_FORTIFY_SOURCE=3` | Run-time bounds checks on `memcpy`, `strcpy`, ... | Needs optimisation; a hardening flag for shipped binaries. |
| `-fstack-protector-strong` | Canary that detects stack smashing on return | Hardening for shipped binaries; ASan covers this in tests. |
| `-ftrivial-auto-var-init=pattern` | Fills uninitialised locals with a pattern | Hides the bug valgrind/MSan would find; better for production. |
| `-fhardened` (GCC 14+) | Turns on a set of hardening flags at once | Same. |
| `-fsanitize=integer` (Clang) | Unsigned overflow and implicit conversions at run time | Not UB, so many false positives; worth a try on hashing code. |

## Common pitfalls

- **A generator expression must be one argument.** `$<cond:-Wa -Wb>` split
  over several lines breaks. Put the flags in a list variable and quote it:
  `"$<cond:${list}>"`; the `;` inside still expands to separate flags.
- **The optimizer can remove the UB before the sanitizer sees it.** GCC folds
  `x + 1 > 0` into `x >= 0`, so there's no overflow at run time and nothing to
  report. A sanitizer only checks operations that actually run.
- **`-D` overrides don't stick.** `cmake --preset debug -DDSA_SANITIZERS=undefined`
  works for that configure, but the next plain `cmake --preset debug`
  re-applies the preset's value. For a lasting change, add a preset. If a
  build dir seems confused, `rm -rf build/<preset>`.
- **Unknown warning flags.** A GCC-only flag passed to Clang becomes a build
  failure under `-Werror`. Put compiler-specific flags in the matching list.
