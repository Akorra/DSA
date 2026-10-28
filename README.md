# dsa

A header-only C++20 library of data structures and algorithms, built to
understand how the standard library works from the inside and where its
design trade-offs cost performance.

Most structures are built twice: a **std-faithful** version that honours the
same guarantees as its std counterpart, and a **variant** that deliberately
drops some of those guarantees (often in favour of a data-oriented layout).
Benchmarking the two against each other and against std shows exactly what
each guarantee costs.

## Goals

- **Understand std.** Know what each std container promises (complexity,
  iterator and reference invalidation, exception safety), how the major
  implementations deliver it, and why.
- **Understand layout.** Know how each structure sits in memory and how that
  interacts with caches, allocation and branch prediction.
- **Know the trade-offs.** For every guarantee dropped, document why, what it
  bought, and when you would or wouldn't make that trade.
- **Data-oriented where it makes sense.** Contiguous storage, SoA layouts,
  handles instead of pointers, and batch-friendly APIs.
- **Modular like std.** Every public header is self-contained; include only
  what you use (`#include <dsa/flat_hash_map.hpp>`). This is enforced by the
  build.
- **Measured, not assumed.** Every performance claim is backed by a benchmark,
  including the cases where dsa loses.
- **Applied.** Use the library to solve real problems, so that understanding
  turns into fluency at choosing the right structure.

## Non-goals

- Replacing `<type_traits>`, `<concepts>`, `<iterator>` or `<memory>`
  primitives. dsa uses std for those freely.
- Full allocator-model parity with std. Allocator support is added where it
  teaches something (see Phase 0), not everywhere.
- ABI stability or production readiness.

## Using the library

dsa is header-only. From CMake:

```cmake
add_subdirectory(path/to/dsa)          # or FetchContent
target_link_libraries(my_app PRIVATE dsa::dsa)
```

Then include only the modules you need:

```cpp
#include <dsa/vector.hpp>
#include <dsa/algorithm/sort.hpp>
```

Linking `dsa::dsa` adds only the include path and C++20. dsa's own warning,
`-Werror` and sanitizer settings apply to its tests and benchmarks, never to
your targets.

Everything lives in `namespace dsa`; implementation details live in
`namespace dsa::detail` and `include/dsa/detail/`, and are not part of the
public interface.

## Building

Requirements: CMake 3.25+, Ninja, and a C++20 compiler (GCC 12+, Clang 15+ or
MSVC 19.34+). Catch2 and Google Benchmark are fetched automatically if not
installed.

| Preset        | Purpose                                                  |
|---------------|----------------------------------------------------------|
| `debug`       | Day-to-day work. ASan + UBSan, warnings as errors.       |
| `debug-clang` | Same, with Clang. Run both compilers regularly.          |
| `debug-nosan` | Plain debug, for valgrind or toolchains without ASan.    |
| `release`     | Tests at `-O3 -DNDEBUG`; catches UB that only shows once the optimizer runs. |
| `bench`       | Benchmarks only, RelWithDebInfo plus frame pointers so profilers have symbols and stacks. |

Each preset builds into `build/<preset>/`.

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

cmake --workflow --preset debug   # the same three steps in one command
```

Every preset except `bench` has a workflow preset. `ctest` fails if
no tests are registered, so a broken test setup can't pass silently.

### Build options

Set by the presets; override with `-D` when configuring. They only exist when
dsa is the top-level project.

| Option                   | Default | Effect                                              |
|--------------------------|---------|-----------------------------------------------------|
| `DSA_SANITIZERS`         | empty   | `;`-separated list, e.g. `address;undefined`. Added to compile and link, with `-fno-sanitize-recover=all` and `-fno-omit-frame-pointer`. GCC/Clang only. |
| `DSA_WARNINGS_AS_ERRORS` | `ON`    | `-Werror` (`/WX` on MSVC) on dsa's own targets.     |
| `DSA_BUILD_TESTS`        | `ON`    | Build `tests/` (`OFF` in `bench`).                  |
| `DSA_BUILD_BENCHMARKS`   | `OFF`   | Build `benchmarks/` (`ON` in `bench`).              |

Warning flags are in `cmake/Warnings.cmake` (target `dsa_warnings`) and
sanitizer flags in `cmake/Sanitizers.cmake` (target `dsa_sanitizers`). dsa's
own targets link both `PRIVATE`. GNU extensions are off (`-std=c++20`).

[`cmake/README.md`](cmake/README.md) explains every flag, sanitizer and preset
in detail: what each catches, runtime options, valgrind, pitfalls.

### VS Code

Install the recommended extensions (VS Code will prompt): CMake Tools,
clangd, CodeLLDB and C/C++ (used for its GDB/MSVC debugger only; its
IntelliSense is disabled in favour of clangd).

- Pick a preset from the CMake Tools status bar; configure runs on open.
- Build with F7, run tests from the Testing sidebar (CTest integration).
- Debug a test executable: select it as the launch target in the status bar,
  then F5. Put a test name in `args` in `.vscode/launch.json` to run just one.
- clangd reads `build/debug/compile_commands.json`. If you mostly use another
  preset, change the path in `.vscode/settings.json`.

## Testing

Tests use Catch2 v3. Each module gets its own test executable
(`tests/<area>/test_<module>.cpp`, registered with `dsa_add_test` in
`tests/CMakeLists.txt`), and every `TEST_CASE` shows up individually in CTest.
Behaviour that must abort (a failing `DSA_ASSERT`) uses `dsa_add_death_test`,
which runs the binary through `cmake/DeathTest.cmake` and checks its output.

Three kinds of checks run on every build:

1. **Unit tests** for behaviour, edge cases and invariants.
2. **Lifetime and exception-safety tests** with `dsa::test::tracked`
   (`tests/support/tracked.hpp`). It counts every construction, copy, move and
   destruction, can throw on the Nth copy/move, and flags double-destroy and
   use-after-destroy. `tests/support/test_tracked.cpp` shows how, using
   `std::vector` as the reference.
3. **Header self-containment.** For every public header the build generates a
   translation unit that includes only that header. A header that relies on
   something it didn't include fails to compile. This is what keeps the
   library modular.

Recommended additionally for containers: **differential tests** that apply
the same random sequence of operations to a dsa container and its std
counterpart and compare after every step.

## Benchmarking

Benchmarks use Google Benchmark, one executable per module in `benchmarks/`
(registered with `dsa_add_benchmark`).

```sh
cmake --preset bench && cmake --build --preset bench
./build/bench/benchmarks/bench_baseline --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true
```

On a laptop or desktop, the CPU governor makes timings noisy (Google
Benchmark warns "CPU scaling is enabled"). For results you want to compare,
switch to the performance governor first:
`sudo cpupower frequency-set -g performance` (back with `-g powersave`).

Guidelines: vary element size (4 B, 64 B, 256 B+) and element count (fits in
L1, L2, LLC, RAM); test hot and cold caches; include realistic access patterns
(random lookups, mixed insert/erase), not only sequential fills; record
machine, compiler and flags with every result.

## Repository layout

```
include/dsa/              public headers, one per module (<dsa/vector.hpp>)
include/dsa/algorithm/    algorithms (<dsa/algorithm/sort.hpp>)
include/dsa/graph/        graph representations and algorithms
include/dsa/memory/       allocators and uninitialized-memory algorithms
include/dsa/detail/       internals, not public API
tests/                    Catch2 tests, mirroring include/dsa/
tests/support/            test helpers (tracked element type, ...)
benchmarks/               Google Benchmark executables
docs/design/              one design note per module (start from TEMPLATE.md)
cmake/                    warning and sanitizer settings
```

## Conventions

- Naming follows std: `snake_case` types and functions, std member names and
  signatures for std-faithful types (`push_back`, `emplace`, `size_type`, ...).
- One public header per module. Each includes `<dsa/config.hpp>` and
  everything else it uses, and nothing it doesn't.
- `DSA_ASSERT(cond, "message")` for preconditions and invariants. On by
  default, off when `NDEBUG` is defined; `-DDSA_ENABLE_ASSERTS=1` forces it on
  (e.g. hardened release builds), `=0` forces it off. When off, `cond` is
  still type-checked but never evaluated, so it must have no side effects.
  Wrap a condition containing a top-level comma in extra parentheses:
  `DSA_ASSERT((std::is_same_v<T, U>), "...")`.
- Variants that drop std guarantees say so in a comment at the top of the
  header and in their design note.

## Definition of done

A work item is ✅ implemented when it has:

1. The header, compiling warning-free on GCC and Clang.
2. Tests: behaviour, edge cases (empty, one element, growth boundaries), and
   for containers, lifetime and exception-safety tests with `tracked`.
3. All tests passing under the `debug` (sanitizers) and `release` presets.
4. A benchmark against std (or the closest std equivalent), where relevant.
5. A design note in `docs/design/`, from the template.

## Roadmap

Legend: ⬜ not started · 🚧 in progress · ✅ implemented.
**(std)** marks std-faithful versions; unmarked items are variants or
structures std doesn't provide.

### Phase 0: Infrastructure and foundations

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Build system, presets, CI, VS Code setup | `CMakeLists.txt`, `CMakePresets.json`, `cmake/` | 🚧 |
| Test harness: Catch2, `tracked` element type, header self-containment check | `tests/support/tracked.hpp` | ⬜ |
| Benchmark harness and std baseline | `benchmarks/` | ⬜ |
| Config and assertion macros | `dsa/config.hpp`, `dsa/detail/assert.hpp` | ✅ |
| Differential-testing helper (random ops vs. std) | `tests/support/differential.hpp` | ⬜ |
| Uninitialized-memory algorithms (`uninitialized_copy`, `_move`, `destroy`, ...) with exception safety **(std)** | `dsa/memory/uninitialized.hpp` | ⬜ |
| Relocation helpers (trivially relocatable optimisation) | `dsa/memory/relocate.hpp` | ⬜ |
| Allocator support via `std::allocator_traits` | (used by containers) | ⬜ |
| Arena / bump allocator | `dsa/memory/arena.hpp` | ⬜ |
| Fixed-size pool allocator | `dsa/memory/pool.hpp` | ⬜ |

### Phase 1: Contiguous and sequence containers

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Dynamic array **(std)**: growth policy, strong exception guarantee, `move_if_noexcept` | `dsa/vector.hpp` | ⬜ |
| Fixed-capacity inline vector | `dsa/inplace_vector.hpp` | ⬜ |
| Small vector (inline buffer, heap fallback) | `dsa/small_vector.hpp` | ⬜ |
| Ring buffer / circular queue | `dsa/ring_buffer.hpp` | ⬜ |
| Deque **(std)**: block map, reference stability at ends | `dsa/deque.hpp` | ⬜ |
| Doubly linked list **(std)**: splice, sentinel node | `dsa/list.hpp` | ⬜ |
| Singly linked list **(std)** | `dsa/forward_list.hpp` | ⬜ |
| Intrusive linked list | `dsa/intrusive_list.hpp` | ⬜ |
| Container adaptors: stack, queue **(std)** | `dsa/stack.hpp`, `dsa/queue.hpp` | ⬜ |
| Dynamic bitset with fast set-bit iteration | `dsa/dynamic_bitset.hpp` | ⬜ |

### Phase 2: Data-oriented containers

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Structure-of-arrays vector | `dsa/soa_vector.hpp` | ⬜ |
| Slot map (generational handles, contiguous storage) | `dsa/slot_map.hpp` | ⬜ |
| Sparse set | `dsa/sparse_set.hpp` | ⬜ |
| Stable vector / chunked storage (stable references, contiguous chunks) | `dsa/stable_vector.hpp` | ⬜ |

### Phase 3: Hashing

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Hash functions and mixing (identity vs. avalanche, why it matters for open addressing) | `dsa/hash.hpp` | ⬜ |
| Chained hash map/set **(std)**: buckets, reference stability, `rehash`, `load_factor` | `dsa/unordered_map.hpp`, `dsa/unordered_set.hpp` | ⬜ |
| Open addressing, linear probing, tombstones | `dsa/detail/open_addressing.hpp` | ⬜ |
| Robin Hood hashing variant | `dsa/robin_hood_map.hpp` | ⬜ |
| Swiss-table flat hash map/set (control bytes, SIMD group probing) | `dsa/flat_hash_map.hpp`, `dsa/flat_hash_set.hpp` | ⬜ |
| Benchmarks vs. std and a third-party map (abseil or boost) | `benchmarks/bench_hash_map.cpp` | ⬜ |

### Phase 4: Ordered structures

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Binary search tree (unbalanced, as a baseline) | `dsa/detail/bst.hpp` | ⬜ |
| Red-black tree core | `dsa/detail/rb_tree.hpp` | ⬜ |
| Ordered map/set/multimap **(std)** on the red-black tree | `dsa/map.hpp`, `dsa/set.hpp` | ⬜ |
| Sorted flat map/set (C++23 `std::flat_map` design) | `dsa/flat_map.hpp`, `dsa/flat_set.hpp` | ⬜ |
| B-tree map/set | `dsa/btree_map.hpp`, `dsa/btree_set.hpp` | ⬜ |
| Skip list | `dsa/skip_list.hpp` | ⬜ |
| AVL tree (comparison with red-black) | `dsa/detail/avl_tree.hpp` | ⬜ |

### Phase 5: Heaps and priority queues

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Heap algorithms: `push_heap`, `pop_heap`, `make_heap` (O(n) build) **(std)** | `dsa/algorithm/heap.hpp` | ⬜ |
| Priority queue adaptor **(std)** | `dsa/priority_queue.hpp` | ⬜ |
| d-ary heap | `dsa/dary_heap.hpp` | ⬜ |
| Indexed heap with decrease-key (for Dijkstra, Prim) | `dsa/indexed_heap.hpp` | ⬜ |
| Pairing heap | `dsa/pairing_heap.hpp` | ⬜ |

### Phase 6: Specialised structures

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Union-find (path compression, union by rank/size) | `dsa/union_find.hpp` | ⬜ |
| Trie (pointer-based, then array/compact layout) | `dsa/trie.hpp` | ⬜ |
| Fenwick tree (binary indexed tree) | `dsa/fenwick_tree.hpp` | ⬜ |
| Segment tree (with lazy propagation) | `dsa/segment_tree.hpp` | ⬜ |
| Sparse table (O(1) range-min query) | `dsa/sparse_table.hpp` | ⬜ |
| LRU cache (hash map + list, then a flat variant) | `dsa/lru_cache.hpp` | ⬜ |
| Bloom filter | `dsa/bloom_filter.hpp` | ⬜ |

### Phase 7: Algorithms

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Binary search family: `lower_bound`, `upper_bound`, `equal_range` **(std)** | `dsa/algorithm/search.hpp` | ⬜ |
| Branchless and Eytzinger-layout binary search | `dsa/algorithm/search.hpp` | ⬜ |
| Partition, stable partition, rotate **(std)** | `dsa/algorithm/partition.hpp` | ⬜ |
| Simple sorts: insertion, selection (as building blocks and baselines) | `dsa/algorithm/sort.hpp` | ⬜ |
| Merge sort and `stable_sort` **(std)** | `dsa/algorithm/sort.hpp` | ⬜ |
| Heap sort | `dsa/algorithm/sort.hpp` | ⬜ |
| Introsort, `std::sort`-style **(std)** | `dsa/algorithm/sort.hpp` | ⬜ |
| Pattern-defeating quicksort (pdqsort) | `dsa/algorithm/sort.hpp` | ⬜ |
| Radix sort (LSD, for integer keys) | `dsa/algorithm/radix_sort.hpp` | ⬜ |
| Selection: quickselect / `nth_element` **(std)** | `dsa/algorithm/select.hpp` | ⬜ |
| Two-pointer and sliding-window utilities | `dsa/algorithm/window.hpp` | ⬜ |
| String search: KMP, rolling hash (Rabin-Karp) | `dsa/algorithm/string_search.hpp` | ⬜ |

### Phase 8: Graphs

| Work item | Header | Implemented |
|-----------|--------|-------------|
| Adjacency-list graph | `dsa/graph/adjacency_list.hpp` | ⬜ |
| CSR (compressed sparse row) graph | `dsa/graph/csr_graph.hpp` | ⬜ |
| BFS and DFS (iterative), visitor-based | `dsa/graph/traversal.hpp` | ⬜ |
| Topological sort (Kahn and DFS) and cycle detection | `dsa/graph/topological_sort.hpp` | ⬜ |
| Dijkstra (indexed heap vs. lazy-deletion binary heap) | `dsa/graph/shortest_path.hpp` | ⬜ |
| Bellman-Ford | `dsa/graph/shortest_path.hpp` | ⬜ |
| Minimum spanning tree: Kruskal (union-find) and Prim | `dsa/graph/mst.hpp` | ⬜ |
| Strongly connected components (Tarjan) | `dsa/graph/scc.hpp` | ⬜ |

### Phase 9: Applied practice

Solve problems in each category using dsa structures (not std), keeping
solutions in `practice/`. The point is fluency in recognising which structure
a problem needs.

| Work item | Uses | Implemented |
|-----------|------|-------------|
| Arrays, two pointers, sliding window | vector, ring_buffer, window utilities | ⬜ |
| Hashing: counting, dedup, two-sum style lookups | flat_hash_map / set | ⬜ |
| Stacks and monotonic stacks/queues | stack, deque | ⬜ |
| Binary search on answer | search algorithms | ⬜ |
| Heaps: top-k, k-way merge, scheduling | priority_queue, dary_heap | ⬜ |
| Intervals and ordered queries | map, btree_map, flat_map | ⬜ |
| Trees and tries | trie, ordered structures | ⬜ |
| Graphs: BFS/DFS, shortest paths, topological order | graph module | ⬜ |
| Range queries | fenwick_tree, segment_tree, sparse_table | ⬜ |
| Connectivity | union_find | ⬜ |
| Dynamic programming (state layout and memory access) | vector, soa_vector | ⬜ |
| Design problems: LRU cache, rate limiter, ... | lru_cache, ring_buffer | ⬜ |

## Reading list

- Microsoft STL source (github.com/microsoft/STL): the most readable of the
  major std implementations. libc++ is the next most approachable.
- Matt Kulukundis, "Designing a Fast, Efficient, Cache-friendly Hash Table,
  Step by Step" (CppCon 2017): the Swiss table design.
- Abseil design notes on Swiss tables and B-tree containers.
- Orson Peters, "Pattern-defeating Quicksort" (paper and repository).
- Mike Acton, "Data-Oriented Design and C++" (CppCon 2014).
- Ulrich Drepper, "What Every Programmer Should Know About Memory".
- Cormen, Leiserson, Rivest and Stein, *Introduction to Algorithms*, and
  Sedgewick and Wayne, *Algorithms*, for the algorithmic theory.
