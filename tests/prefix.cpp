#include <smallcanon/simd/prefix.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <random>

#include <xsimd/xsimd.hpp>

namespace {
    template<typename T, typename A>
    struct PrefixCase {
        using value_t = T;
        using arch_t = A;
    };

    struct PrefixSum {
        template<typename T, typename A>
        static xsimd::batch<T, A> simd(xsimd::batch<T, A> data) {
            return smallcanon::simd::incl_prefix_sum<T, A>(data);
        }

        template<typename T>
        static T scalar(T lhs, T rhs) {
            return static_cast<T>(lhs + rhs);
        }
    };

    struct PrefixMax {
        template<typename T, typename A>
        static xsimd::batch<T, A> simd(xsimd::batch<T, A> data) {
            return smallcanon::simd::incl_prefix_max<T, A>(data);
        }

        template<typename T>
        static T scalar(T lhs, T rhs) {
            return std::max(lhs, rhs);
        }
    };

    struct PrefixMin {
        template<typename T, typename A>
        static xsimd::batch<T, A> simd(xsimd::batch<T, A> data) {
            return smallcanon::simd::incl_prefix_min<T, A>(data);
        }

        template<typename T>
        static T scalar(T lhs, T rhs) {
            return std::min(lhs, rhs);
        }
    };

    template<typename T>
    class PrefixTests : public testing::Test {};
    using PrefixTypes = testing::Types<
#if SMALLCANON_WITH_AVX512
            PrefixCase<std::uint8_t, xsimd::avx512f>, //
            PrefixCase<std::uint16_t, xsimd::avx512f>, //
            PrefixCase<std::uint32_t, xsimd::avx512f>, //
            PrefixCase<std::uint64_t, xsimd::avx512f>, //
#endif
            PrefixCase<std::uint8_t, xsimd::avx2>, //
            PrefixCase<std::uint16_t, xsimd::avx2>, //
            PrefixCase<std::uint32_t, xsimd::avx2>, //
            PrefixCase<std::uint64_t, xsimd::avx2>>;

    TYPED_TEST_SUITE(PrefixTests, PrefixTypes);

    template<typename PrefixOp, typename T, typename A>
    void expect_inclusive_prefix_for(const std::array<T, xsimd::batch<T, A>::size>& input) {
        using batch_t = xsimd::batch<T, A>;
        constexpr auto lanes = batch_t::size;

        const auto result = PrefixOp::template simd<T, A>(batch_t::load_unaligned(input.data()));

        std::array<T, lanes> actual{};
        result.store_unaligned(actual.data());

        T expected = input[0];
        for (std::size_t i = 0; i < lanes; ++i) {
            if (i > 0) {
                expected = PrefixOp::scalar(expected, input[i]);
            }
            EXPECT_EQ(actual[i], expected) << "lane " << i;
        }
    }

    template<typename T, typename A>
    void expect_all_inclusive_prefixes_for(const std::array<T, xsimd::batch<T, A>::size>& input) {
        expect_inclusive_prefix_for<PrefixSum, T, A>(input);
        expect_inclusive_prefix_for<PrefixMax, T, A>(input);
        expect_inclusive_prefix_for<PrefixMin, T, A>(input);
    }
} // namespace

TYPED_TEST(PrefixTests, ComputesInclusivePrefixForIncreasingPattern) {
    using value_t = typename TypeParam::value_t;
    using arch_t = typename TypeParam::arch_t;
    constexpr auto lanes = xsimd::batch<value_t, arch_t>::size;

    std::array<value_t, lanes> input{};
    for (std::size_t i = 0; i < lanes; ++i) {
        input[i] = static_cast<value_t>((i % 5) + 1);
    }

    expect_all_inclusive_prefixes_for<value_t, arch_t>(input);
}

TYPED_TEST(PrefixTests, HandlesZerosAndNonuniformValues) {
    using value_t = typename TypeParam::value_t;
    using arch_t = typename TypeParam::arch_t;
    constexpr auto lanes = xsimd::batch<value_t, arch_t>::size;

    std::array<value_t, lanes> input{};
    for (std::size_t i = 0; i < lanes; ++i) {
        input[i] = static_cast<value_t>((i % 3 == 0) ? 0 : (i % 7) + 1);
    }

    expect_all_inclusive_prefixes_for<value_t, arch_t>(input);
}

TYPED_TEST(PrefixTests, HandlesRandomizedValues) {
    using value_t = typename TypeParam::value_t;
    using arch_t = typename TypeParam::arch_t;
    constexpr auto lanes = xsimd::batch<value_t, arch_t>::size;
    constexpr auto max_value = static_cast<std::uint64_t>(std::numeric_limits<value_t>::max()) / lanes;

    std::mt19937_64 rng(42 + 17 * sizeof(value_t) + 31 * lanes);
    std::uniform_int_distribution<std::uint64_t> dist(0, max_value);

    for (std::size_t repeat = 0; repeat < 1000; ++repeat) {
        std::array<value_t, lanes> input{};
        for (std::size_t i = 0; i < lanes; ++i) {
            input[i] = static_cast<value_t>(dist(rng));
        }

        SCOPED_TRACE(testing::Message() << "repeat " << repeat);
        expect_all_inclusive_prefixes_for<value_t, arch_t>(input);
        if (testing::Test::HasFailure()) {
            return;
        }
    }
}
