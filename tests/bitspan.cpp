#include <smallcanon/bitspan.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

template<typename T>
class BitSpanTests : public testing::Test {
    static_assert(std::is_unsigned_v<T>);
};
using BitSpanWordTypes = testing::Types<std::uint16_t, std::uint32_t, std::uint64_t>;
TYPED_TEST_SUITE(BitSpanTests, BitSpanWordTypes);

template<typename T>
std::vector<int> collect_set_bits(T value) {
    std::vector<int> bits;
    for (const auto bit: smallcanon::iterate_set_bits(value)) {
        bits.push_back(bit);
    }
    return bits;
}

template<typename T, size_t Extent>
std::vector<size_t> collect_span_set_bits(const smallcanon::BitSpan<T, Extent>& span) {
    std::vector<size_t> bits;
    for (const auto bit: span.iterate_set_bits()) {
        bits.push_back(bit);
    }
    return bits;
}

TYPED_TEST(BitSpanTests, IterateSetBitsReturnsNoBitsForZero) {
    EXPECT_TRUE(collect_set_bits(TypeParam{0}).empty());
}

TYPED_TEST(BitSpanTests, IterateSetBitsReturnsSingleSetBitPosition) {
    EXPECT_EQ(collect_set_bits(TypeParam{1}), std::vector<int>{0});
    EXPECT_EQ(collect_set_bits(static_cast<TypeParam>(TypeParam{1} << 3)), std::vector<int>{3});
}

TYPED_TEST(BitSpanTests, IterateSetBitsReturnsPositionsInAscendingOrder) {
    const auto value = static_cast<TypeParam>((TypeParam{1} << 0) | (TypeParam{1} << 2) | (TypeParam{1} << 5));

    EXPECT_EQ(collect_set_bits(value), (std::vector<int>{0, 2, 5}));
}

TYPED_TEST(BitSpanTests, IterateSetBitsHandlesHighestRepresentableBit) {
    constexpr auto highest_bit = std::numeric_limits<TypeParam>::digits - 1;
    const auto value = static_cast<TypeParam>((TypeParam{1} << 1) | (TypeParam{1} << highest_bit));

    EXPECT_EQ(collect_set_bits(value), (std::vector<int>{1, highest_bit}));
}

TYPED_TEST(BitSpanTests, ExposesBitsPerWordForWordType) {
    EXPECT_EQ(smallcanon::BitSpan<TypeParam>::BITS_PER_WORD, std::numeric_limits<TypeParam>::digits);
}

TYPED_TEST(BitSpanTests, SizeReturnsTotalNumberOfBitsInRange) {
    std::array<TypeParam, 3> words{};
    const smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_EQ(span.size(), words.size() * std::numeric_limits<TypeParam>::digits);
}

TYPED_TEST(BitSpanTests, DeducesStaticExtentFromStaticSpan) {
    constexpr size_t word_count = 2;
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, word_count> words{TypeParam{1}, TypeParam{1} << 1};
    std::span<TypeParam, word_count> word_span(words);
    const smallcanon::BitSpan span(word_span);

    static_assert(std::is_same_v<decltype(span), const smallcanon::BitSpan<TypeParam, word_count>>);
    EXPECT_EQ(span.size(), word_count * bits_per_word);
    EXPECT_EQ(collect_span_set_bits(span), (std::vector<size_t>{0, bits_per_word + 1}));
}

TYPED_TEST(BitSpanTests, SingleWordStaticExtentUsesSingleWordInterface) {
    constexpr auto highest_bit = std::numeric_limits<TypeParam>::digits - 1;
    std::array<TypeParam, 1> words{TypeParam{1}};
    std::span<TypeParam, 1> word_span(words);
    smallcanon::BitSpan span(word_span);

    static_assert(std::is_same_v<decltype(span), smallcanon::BitSpan<TypeParam, 1>>);
    EXPECT_EQ(span.size(), std::numeric_limits<TypeParam>::digits);
    EXPECT_TRUE(span.get_bit(0));

    EXPECT_FALSE(span.set_bit(highest_bit));
    EXPECT_EQ(collect_span_set_bits(span), (std::vector<size_t>{0, highest_bit}));

    EXPECT_TRUE(span.unset_bit(0));
    EXPECT_FALSE(span.assign_bit(1, true));
    EXPECT_EQ(collect_span_set_bits(span), (std::vector<size_t>{1, highest_bit}));
}

TYPED_TEST(BitSpanTests, SizeIsZeroForEmptyRange) {
    std::array<TypeParam, 1> words{};
    const smallcanon::BitSpan span(words.data(), words.data());

    EXPECT_EQ(span.size(), 0);
}

TYPED_TEST(BitSpanTests, GetBitReadsBitsAcrossWordBoundaries) {
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, 2> words{TypeParam{1}, TypeParam{1} << 1};
    const smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_TRUE(span.get_bit(0));
    EXPECT_FALSE(span.get_bit(1));
    EXPECT_FALSE(span.get_bit(bits_per_word));
    EXPECT_TRUE(span.get_bit(bits_per_word + 1));
}

TYPED_TEST(BitSpanTests, CountOnesSumsBitsAcrossAllWords) {
    std::array<TypeParam, 3> words{
            TypeParam{0},
            TypeParam{0b1011},
            std::numeric_limits<TypeParam>::max(),
    };
    const smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_EQ(span.count_ones(), std::numeric_limits<TypeParam>::digits + 3);
}

TYPED_TEST(BitSpanTests, AllUnsetReturnsTrueForEmptyRange) {
    std::array<TypeParam, 1> words{TypeParam{1}};
    const smallcanon::BitSpan span(words.data(), words.data());

    EXPECT_TRUE(span.all_unset());
}

TYPED_TEST(BitSpanTests, AllUnsetReturnsTrueWhenNoBitsAreSet) {
    std::array<TypeParam, 3> words{};
    const smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_TRUE(span.all_unset());
}

TYPED_TEST(BitSpanTests, AllUnsetReturnsFalseWhenAnyBitIsSet) {
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, 3> words{0, TypeParam{1} << (bits_per_word / 2), 0};
    const smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_FALSE(span.all_unset());
}

TYPED_TEST(BitSpanTests, SpanIterateSetBitsReturnsNoBitsForEmptyRange) {
    std::array<TypeParam, 1> words{std::numeric_limits<TypeParam>::max()};
    const smallcanon::BitSpan span(words.data(), words.data());

    EXPECT_TRUE(collect_span_set_bits(span).empty());
}

TYPED_TEST(BitSpanTests, SpanIterateSetBitsReturnsNoBitsWhenAllWordsAreZero) {
    std::array<TypeParam, 3> words{};
    const smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_TRUE(collect_span_set_bits(span).empty());
}

TYPED_TEST(BitSpanTests, SpanIterateSetBitsReturnsGlobalPositionsAcrossWords) {
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, 3> words{
            static_cast<TypeParam>((TypeParam{1} << 0) | (TypeParam{1} << 3)),
            static_cast<TypeParam>(TypeParam{1} << 1),
            static_cast<TypeParam>((TypeParam{1} << 2) | (TypeParam{1} << (bits_per_word - 1))),
    };
    const smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_EQ(collect_span_set_bits(span),
              (std::vector<size_t>{0, 3, bits_per_word + 1, (2 * bits_per_word) + 2, (3 * bits_per_word) - 1}));
}

TYPED_TEST(BitSpanTests, SpanIterateSetBitsReflectsMutations) {
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, 2> words{};
    smallcanon::BitSpan span(words.data(), words.data() + words.size());

    span.set_bit(1);
    span.set_bit(bits_per_word);
    span.set_bit((2 * bits_per_word) - 1);
    span.unset_bit(bits_per_word);

    EXPECT_EQ(collect_span_set_bits(span), (std::vector<size_t>{1, (2 * bits_per_word) - 1}));
}

TYPED_TEST(BitSpanTests, AssignBitSetsAndClearsRequestedBitAndReturnsPreviousValue) {
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, 2> words{};
    smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_FALSE(span.assign_bit(bits_per_word + 2, true));
    EXPECT_EQ(words[0], 0);
    EXPECT_EQ(words[1], TypeParam{1} << 2);
    EXPECT_TRUE(span.get_bit(bits_per_word + 2));

    EXPECT_TRUE(span.assign_bit(bits_per_word + 2, false));
    EXPECT_EQ(words[0], 0);
    EXPECT_EQ(words[1], 0);
    EXPECT_FALSE(span.get_bit(bits_per_word + 2));
}

TYPED_TEST(BitSpanTests, AssignBitLeavesOtherBitsUnchanged) {
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, 2> words{
            std::numeric_limits<TypeParam>::max(),
            std::numeric_limits<TypeParam>::max(),
    };
    smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_TRUE(span.assign_bit(bits_per_word + 3, false));

    EXPECT_EQ(words[0], std::numeric_limits<TypeParam>::max());
    EXPECT_EQ(words[1], static_cast<TypeParam>(std::numeric_limits<TypeParam>::max() & ~(TypeParam{1} << 3)));
}

TYPED_TEST(BitSpanTests, SetBitSetsRequestedBitAndReturnsPreviousValue) {
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, 2> words{};
    smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_FALSE(span.set_bit(bits_per_word));
    EXPECT_EQ(words[0], 0);
    EXPECT_EQ(words[1], TypeParam{1});
    EXPECT_TRUE(span.get_bit(bits_per_word));

    EXPECT_TRUE(span.set_bit(bits_per_word));
    EXPECT_EQ(words[0], 0);
    EXPECT_EQ(words[1], TypeParam{1});
}

TYPED_TEST(BitSpanTests, SetBitLeavesOtherBitsUnchanged) {
    std::array<TypeParam, 1> words{TypeParam{0b1010}};
    smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_FALSE(span.set_bit(2));

    EXPECT_EQ(words[0], TypeParam{0b1110});
}

TYPED_TEST(BitSpanTests, UnsetBitClearsRequestedBitAndReturnsPreviousValue) {
    constexpr auto bits_per_word = std::numeric_limits<TypeParam>::digits;
    std::array<TypeParam, 2> words{
            std::numeric_limits<TypeParam>::max(),
            std::numeric_limits<TypeParam>::max(),
    };
    smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_TRUE(span.unset_bit(bits_per_word + 1));
    EXPECT_EQ(words[0], std::numeric_limits<TypeParam>::max());
    EXPECT_EQ(words[1], static_cast<TypeParam>(std::numeric_limits<TypeParam>::max() & ~(TypeParam{1} << 1)));
    EXPECT_FALSE(span.get_bit(bits_per_word + 1));

    EXPECT_FALSE(span.unset_bit(bits_per_word + 1));
    EXPECT_EQ(words[0], std::numeric_limits<TypeParam>::max());
    EXPECT_EQ(words[1], static_cast<TypeParam>(std::numeric_limits<TypeParam>::max() & ~(TypeParam{1} << 1)));
}
