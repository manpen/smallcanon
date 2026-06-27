#include <smallcanon/refine/avx512intrin.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <random>

namespace {
#if XSIMD_WITH_AVX512F
    template<std::size_t Bits>
    struct ColorsFromMask;

    template<>
    struct ColorsFromMask<8> {
        using mask_t = std::uint8_t;

        static smallcanon::refine::avx512intrin::u8x64_t call(mask_t mask) noexcept {
            return smallcanon::refine::avx512intrin::colors_from_mask8(mask);
        }
    };

    template<>
    struct ColorsFromMask<16> {
        using mask_t = std::uint16_t;

        static smallcanon::refine::avx512intrin::u8x64_t call(mask_t mask) noexcept {
            return smallcanon::refine::avx512intrin::colors_from_mask16(mask);
        }
    };

    template<std::size_t Bits>
    concept SupportedColorsFromMask =
            std::has_single_bit(Bits) && requires(typename ColorsFromMask<Bits>::mask_t mask) {
                { ColorsFromMask<Bits>::call(mask) } -> std::same_as<smallcanon::refine::avx512intrin::u8x64_t>;
            };

    template<std::size_t Bits>
        requires SupportedColorsFromMask<Bits>
    using MaskFor = typename ColorsFromMask<Bits>::mask_t;

    template<std::size_t Bits>
    struct ColorsFromMaskCase {
        static constexpr std::size_t kBits = Bits;
    };

    template<typename T>
    class ColorsFromMaskTests : public testing::Test {};

    template<typename T>
    class ColorsFromMaskHalfWidthTests : public testing::Test {};

    using ColorsFromMaskTypes = testing::Types<ColorsFromMaskCase<8>, ColorsFromMaskCase<16>>;
    TYPED_TEST_SUITE(ColorsFromMaskTests, ColorsFromMaskTypes);

    using ColorsFromMaskHalfWidthTypes = testing::Types<ColorsFromMaskCase<16>>;
    TYPED_TEST_SUITE(ColorsFromMaskHalfWidthTests, ColorsFromMaskHalfWidthTypes);

    template<std::size_t Bits>
        requires SupportedColorsFromMask<Bits>
    std::array<std::uint8_t, Bits> expected_colors(MaskFor<Bits> mask) {
        std::array<std::uint8_t, Bits> expected{};
        std::uint8_t current_color = 0;

        for (std::size_t i = 0; i < Bits; ++i) {
            if (i != 0 && ((mask >> i) & MaskFor<Bits>{1}) != 0) {
                current_color = static_cast<std::uint8_t>(i);
            }
            expected[i] = current_color;
        }

        return expected;
    }

    template<std::size_t Bits>
        requires SupportedColorsFromMask<Bits>
    std::array<std::uint8_t, Bits> actual_colors(MaskFor<Bits> mask) {
        std::array<std::uint8_t, smallcanon::refine::avx512intrin::u8x64_t::size> lanes{};
        ColorsFromMask<Bits>::call(mask).store_unaligned(lanes.data());

        std::array<std::uint8_t, Bits> actual{};
        std::copy_n(lanes.begin(), Bits, actual.begin());
        return actual;
    }

    template<std::size_t Bits>
        requires SupportedColorsFromMask<Bits>
    void expect_colors_from_mask(MaskFor<Bits> mask) {
        mask &= static_cast<MaskFor<Bits>>(~MaskFor<Bits>{1});

        const auto actual = actual_colors<Bits>(mask);
        const auto expected = expected_colors<Bits>(mask);

        for (std::size_t i = 0; i < Bits; ++i) {
            EXPECT_EQ(actual[i], expected[i]) << "Bits=" << Bits << ", lane=" << i << ", mask=" << +mask;
        }
    }

    template<std::size_t Bits>
        requires SupportedColorsFromMask<Bits>
    void expect_first_half_matches_half_width(MaskFor<Bits> mask) {
        static_assert(Bits > 8);
        constexpr std::size_t HalfBits = Bits / 2;
        static_assert(SupportedColorsFromMask<HalfBits>);

        mask &= static_cast<MaskFor<Bits>>(~MaskFor<Bits>{1});
        const auto low_mask = static_cast<MaskFor<HalfBits>>(mask);

        const auto actual_full = actual_colors<Bits>(mask);
        const auto actual_half = actual_colors<HalfBits>(low_mask);

        for (std::size_t i = 0; i < HalfBits; ++i) {
            EXPECT_EQ(actual_full[i], actual_half[i])
                    << "Bits=" << Bits << ", lane=" << i << ", mask=" << +mask << ", low_mask=" << +low_mask;
        }
    }
#endif
} // namespace

#if XSIMD_WITH_AVX512F
TYPED_TEST(ColorsFromMaskTests, MatchesScalarReferenceForAllValidMasks) {
    constexpr auto Bits = TypeParam::kBits;
    using mask_t = MaskFor<Bits>;

    if constexpr (Bits <= 16) {
        for (std::uint64_t mask = 0; mask < (std::uint64_t{1} << Bits); mask += 2) {
            expect_colors_from_mask<Bits>(static_cast<mask_t>(mask));
        }
    } else {
        GTEST_SKIP() << "exhaustive mask enumeration is only enabled up to 16 bits";
    }
}

TYPED_TEST(ColorsFromMaskTests, MatchesScalarReferenceForRandomMasks) {
    constexpr auto Bits = TypeParam::kBits;
    using mask_t = MaskFor<Bits>;

    std::mt19937_64 rng(42 + 17 * Bits);

    for (std::size_t repeat = 0; repeat < 5000; ++repeat) {
        SCOPED_TRACE(testing::Message() << "repeat " << repeat);
        expect_colors_from_mask<Bits>(static_cast<mask_t>(rng()));
    }
}

TYPED_TEST(ColorsFromMaskHalfWidthTests, FirstHalfMatchesHalfWidthFunctionForAllValidMasks) {
    constexpr auto Bits = TypeParam::kBits;
    using mask_t = MaskFor<Bits>;

    if constexpr (Bits <= 16) {
        for (std::uint64_t mask = 0; mask < (std::uint64_t{1} << Bits); mask += 2) {
            expect_first_half_matches_half_width<Bits>(static_cast<mask_t>(mask));
        }
    } else {
        GTEST_SKIP() << "exhaustive mask enumeration is only enabled up to 16 bits";
    }
}

TYPED_TEST(ColorsFromMaskHalfWidthTests, FirstHalfMatchesHalfWidthFunctionForRandomMasks) {
    constexpr auto Bits = TypeParam::kBits;
    using mask_t = MaskFor<Bits>;

    std::mt19937_64 rng(42 + 17 * Bits + 12345);

    for (std::size_t repeat = 0; repeat < 5000; ++repeat) {
        SCOPED_TRACE(testing::Message() << "repeat " << repeat);
        expect_first_half_matches_half_width<Bits>(static_cast<mask_t>(rng()));
    }
}
#else
TEST(ColorsFromMaskTests, Avx512Unavailable) {
    GTEST_SKIP() << "colors_from_mask helpers require XSIMD_WITH_AVX512F";
}
#endif
