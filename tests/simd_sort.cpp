#include <gtest/gtest.h>
#include <smallcanon/simd/sort.hpp>

#include <bit>
#include <random>


namespace {
    template<size_t N, typename T, bool Ascending, typename A>
    struct SimdSortTestCases {
        static constexpr size_t kItems = N;
        static constexpr bool kAscending = Ascending;
        using value_t = T;
        using arch_t = A;
    };

    template<typename T>
    class SimdSortTests : public testing::Test {};

    // TODO: Add uint8_t!!
    using SimdSortTypes = testing::Types<
#if SMALLCANON_WITH_AVX512
            // AVX512 (to ensure architecture is properly handled)
            SimdSortTestCases<32, uint16_t, true, xsimd::avx512f>, //
            SimdSortTestCases<16, uint32_t, true, xsimd::avx512f>, //
            SimdSortTestCases<8, uint64_t, true, xsimd::avx512f>, //

            // multi register test
            SimdSortTestCases<64, uint64_t, true, xsimd::avx512f>, //
            SimdSortTestCases<64, uint64_t, false, xsimd::avx512f>, //
#endif

            // AVX2 Ascending
            SimdSortTestCases<16, uint16_t, true, xsimd::avx2>, //
            SimdSortTestCases<8, uint16_t, true, xsimd::avx2>, //
            SimdSortTestCases<4, uint16_t, true, xsimd::avx2>, //
            SimdSortTestCases<8, uint32_t, true, xsimd::avx2>, //
            SimdSortTestCases<4, uint32_t, true, xsimd::avx2>, //
            SimdSortTestCases<2, uint32_t, true, xsimd::avx2>, //
            SimdSortTestCases<4, uint64_t, true, xsimd::avx2>, //
            SimdSortTestCases<2, uint64_t, true, xsimd::avx2>, //

            // AVX2 Descending
            SimdSortTestCases<16, uint16_t, false, xsimd::avx2>, //
            SimdSortTestCases<8, uint16_t, false, xsimd::avx2>, //
            SimdSortTestCases<4, uint16_t, false, xsimd::avx2>, //
            SimdSortTestCases<8, uint32_t, false, xsimd::avx2>, //
            SimdSortTestCases<4, uint32_t, false, xsimd::avx2>, //
            SimdSortTestCases<2, uint32_t, false, xsimd::avx2>, //
            SimdSortTestCases<4, uint64_t, false, xsimd::avx2>, //
            SimdSortTestCases<2, uint64_t, false, xsimd::avx2>, //

            // multi-register
            SimdSortTestCases<64, uint64_t, true, xsimd::avx2>, //
            SimdSortTestCases<64, uint64_t, false, xsimd::avx2>>;
    TYPED_TEST_SUITE(SimdSortTests, SimdSortTypes);
} // namespace

template<size_t NumItems, typename T, bool kAscending = true, typename A = xsimd::default_arch>
void test_pattern(uint64_t pattern) {
    constexpr size_t kLanes = xsimd::batch<T, A>::size;
    constexpr size_t kElements = std::max(NumItems, kLanes);

    std::array<T, kElements> data;

    for (size_t i = 0; i < kElements; i++) {
        data[i] = static_cast<T>((pattern >> (i % NumItems)) & 1); // 1 iff the i-th bit is set
    }

    smallcanon::simd::sort::sort<NumItems, kAscending, T, A>(data.data());

    const size_t num_ones = static_cast<size_t>(std::popcount(pattern));
    const size_t num_zeros = NumItems - num_ones;

    const size_t first_block = kAscending ? num_zeros : num_ones;
    for (size_t base = 0; base < kElements; base += NumItems) {
        for (size_t i = 0; i < first_block; i++) {
            ASSERT_EQ(data[base + i], static_cast<T>(!kAscending)) << i << " " << base << " " << pattern;
        }

        for (size_t i = first_block; i < NumItems; i++) {
            ASSERT_EQ(data[base + i], static_cast<T>(kAscending)) << i << " " << base << " " << pattern;
        }
    }
}


TYPED_TEST(SimdSortTests, ZeroOne) {
    using value_t = typename TypeParam::value_t;
    using arch_t = typename TypeParam::arch_t;
    constexpr size_t kItems = TypeParam::kItems;
    constexpr bool kAscending = TypeParam::kAscending;

    if constexpr (kItems <= 16) {
        // exhaustive
        for (size_t pattern = 0; pattern < (size_t(1) << kItems); ++pattern) {
            test_pattern<kItems, value_t, kAscending, arch_t>(pattern);
        }
    } else if constexpr (kItems <= 64) {
        // randomized
        std::mt19937_64 rng(42 + 13 * kItems + static_cast<size_t>(kAscending) * 12345);
        for (size_t repeat = 0; repeat < 5000; repeat++) {
            const auto pattern = static_cast<value_t>(rng());
            test_pattern<kItems, value_t, kAscending, arch_t>(pattern);
        }
    } else {
        // pattern only supports 64bit, but that's currently good enough
        static_assert(false);
    }
}
