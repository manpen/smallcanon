#include <benchmark/benchmark.h>
#include <smallcanon/simd/avx512defs.hpp>
#include <smallcanon/simd/sort.hpp>
#include <smallcanon/utility.hpp>

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

    template<typename Batch>
    void bm_simd_sort_batch(benchmark::State& state) {
        using value_t = typename Batch::value_type;
        using arch_t = typename Batch::arch_type;

        constexpr size_t kLanes = Batch::size;
        constexpr size_t kSortItems = std::min(kLanes, size_t{32});

        std::array<value_t, kLanes> data;
        std::generate(data.begin(), data.end(), [value = value_t{}]() mutable { return value++; });

        auto values = Batch::load_unaligned(data.data());
        for (auto _: state) {
            benchmark::DoNotOptimize(values);
            auto sorted =
                    smallcanon::simd::sort::sort_details::sort_single_batch<kSortItems, value_t, true, arch_t>(values);
            benchmark::DoNotOptimize(sorted);
        }

        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kLanes));
        state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(kLanes * sizeof(value_t)));
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

#if SMALLCANON_WITH_AVX512
    namespace avx512 = smallcanon::simd::avx512defs;

    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u8x16_t);
    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u8x32_t);
    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u8x64_t);

    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u16x8_t);
    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u16x16_t);
    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u16x32_t);

    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u32x4_t);
    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u32x8_t);
    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u32x16_t);

    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u64x2_t);
    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u64x4_t);
    BENCHMARK_TEMPLATE(bm_simd_sort_batch, avx512::u64x8_t);
#endif
} // namespace
