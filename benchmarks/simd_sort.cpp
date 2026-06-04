#include <benchmark/benchmark.h>
#include <smallcanon/simd/sort.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>

namespace {
    template<typename T, size_t kItems>
    void bm_simd_sort(benchmark::State& state) {
        static_assert(std::is_same_v<T, uint16_t> || std::is_same_v<T, uint32_t> || std::is_same_v<T, uint64_t>);

        using batch_t = xsimd::batch<T, xsimd::default_arch>;
        constexpr size_t kLanes = batch_t::size;
        constexpr size_t kVectors = (kItems + kLanes - 1) / kLanes;
        constexpr size_t kStorageItems = kVectors * kLanes;

        std::array<T, kStorageItems> data;
        std::generate(data.begin(), data.end(), [value = T{}]() mutable { return value++; });

        for (auto _: state) {
            benchmark::DoNotOptimize(data.data());
            smallcanon::simd::sort::sort<kItems, true, T>(data.data());
            benchmark::ClobberMemory();
        }

        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kItems));
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kItems * sizeof(T)));
    }

#define SMALLCANON_BENCHMARK_SIMD_SORT_TYPE(T)                                                                         \
    BENCHMARK_TEMPLATE(bm_simd_sort, T, 8);                                                                            \
    BENCHMARK_TEMPLATE(bm_simd_sort, T, 16);                                                                           \
    BENCHMARK_TEMPLATE(bm_simd_sort, T, 32);                                                                           \
    BENCHMARK_TEMPLATE(bm_simd_sort, T, 64);                                                                           \
    BENCHMARK_TEMPLATE(bm_simd_sort, T, 128);                                                                          \
    BENCHMARK_TEMPLATE(bm_simd_sort, T, 256)

    SMALLCANON_BENCHMARK_SIMD_SORT_TYPE(uint16_t);
    SMALLCANON_BENCHMARK_SIMD_SORT_TYPE(uint32_t);
    SMALLCANON_BENCHMARK_SIMD_SORT_TYPE(uint64_t);

#undef SMALLCANON_BENCHMARK_SIMD_SORT_TYPE
} // namespace
