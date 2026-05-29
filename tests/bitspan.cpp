#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

#include <smallcanon/bitspan.hpp>

template<typename T>
class BitSpanTests : public testing::Test {
    static_assert(std::is_unsigned_v<T>);
};
using BitSpanWordTypes = testing::Types<std::uint16_t, std::uint32_t, std::uint64_t>;
TYPED_TEST_SUITE(BitSpanTests, BitSpanWordTypes);

TYPED_TEST(BitSpanTests, ExposesBitsPerWordForWordType) {
    EXPECT_EQ(smallcanon::BitSpan<TypeParam>::BITS_PER_WORD, std::numeric_limits<TypeParam>::digits);
}

TYPED_TEST(BitSpanTests, SizeReturnsTotalNumberOfBitsInRange) {
    std::array<TypeParam, 3> words{};
    const smallcanon::BitSpan span(words.data(), words.data() + words.size());

    EXPECT_EQ(span.size(), words.size() * std::numeric_limits<TypeParam>::digits);
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
