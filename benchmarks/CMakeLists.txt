# Benchmark harness. Only included when dsa is the top-level project and
# DSA_BUILD_BENCHMARKS is ON (the bench preset turns it on).

include(FetchContent)

# Google Benchmark's own build options: no self-tests (they want gtest),
# no install rules, and no -Werror of its own on our compilers.
set(BENCHMARK_ENABLE_TESTING       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS   OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR        OFF CACHE BOOL "" FORCE)
set(BENCHMARK_INSTALL_DOCS         OFF CACHE BOOL "" FORCE)

# Installed copy if there is one, otherwise this exact tag (same pattern as Catch2).
FetchContent_Declare(benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.10.0      # pinned
    GIT_SHALLOW    TRUE
    SYSTEM
    FIND_PACKAGE_ARGS 1.8)
FetchContent_MakeAvailable(benchmark)

# dsa_add_benchmark(<name> <source>...)
#   One executable per module, built at the preset's optimisation level, with
#   our warnings. No sanitizers: they'd distort every timing.
function(dsa_add_benchmark name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE
        dsa::dsa dsa_warnings benchmark::benchmark_main)
endfunction()

dsa_add_benchmark(bench_baseline bench_baseline.cpp)
