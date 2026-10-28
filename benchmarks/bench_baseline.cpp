// Baseline: std::vector<int> push_back, with and without reserve().
// Proves the harness works and gives a reference point on this machine for
// the dsa::vector benchmarks later.
#include <benchmark/benchmark.h>

#include <cstdint>
#include <vector>

namespace {

void BM_std_vector_push_back(benchmark::State& state)
{
    const auto n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        std::vector<int> v;
        for (std::size_t i = 0; i < n; ++i)
            v.push_back(static_cast<int>(i));
        benchmark::DoNotOptimize(v.data());   // the vector must be considered "used"
        benchmark::ClobberMemory();           // and its writes must really happen
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

void BM_std_vector_push_back_reserved(benchmark::State& state)
{
    const auto n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        std::vector<int> v;
        v.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
            v.push_back(static_cast<int>(i));
        benchmark::DoNotOptimize(v.data());
        benchmark::ClobberMemory();
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

// 8 .. 1M elements, x8 steps: from "fits in L1" to "bigger than L2".
BENCHMARK(BM_std_vector_push_back)->RangeMultiplier(8)->Range(8, 1 << 20);
BENCHMARK(BM_std_vector_push_back_reserved)->RangeMultiplier(8)->Range(8, 1 << 20);

} // namespace
