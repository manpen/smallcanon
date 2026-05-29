#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

#include <smallcanon/bits.hpp>

template <typename T>
class UnsignedIntTests : public testing::Test {
    static_assert(std::is_unsigned_v<T>);
};
using UnsignedIntegerTypes = testing::Types<std::uint16_t, std::uint32_t, std::uint64_t>;
TYPED_TEST_SUITE(UnsignedIntTests, UnsignedIntegerTypes);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// round_up_power_of_two
TYPED_TEST(UnsignedIntTests, ReturnsZeroForZero) {
    EXPECT_EQ(round_up_power_of_two<TypeParam>(0), 0);
}

TYPED_TEST(UnsignedIntTests, LeavesPowersOfTwoUnchanged) {
    EXPECT_EQ(round_up_power_of_two<TypeParam>(1), 1);
    EXPECT_EQ(round_up_power_of_two<TypeParam>(2), 2);
    EXPECT_EQ(round_up_power_of_two<TypeParam>(4), 4);
    EXPECT_EQ(round_up_power_of_two<TypeParam>(8), 8);
    EXPECT_EQ(round_up_power_of_two<TypeParam>(TypeParam{1} << 8), TypeParam{1} << 8);
}

TYPED_TEST(UnsignedIntTests, RoundsValuesBetweenPowersOfTwoUp) {
    EXPECT_EQ(round_up_power_of_two<TypeParam>(3), 4);
    EXPECT_EQ(round_up_power_of_two<TypeParam>(5), 8);
    EXPECT_EQ(round_up_power_of_two<TypeParam>(9), 16);
    EXPECT_EQ(round_up_power_of_two<TypeParam>((TypeParam{1} << 8) + 1), TypeParam{1} << 9);
}

TYPED_TEST(UnsignedIntTests, HandlesHighestRepresentablePowerOfTwo) {
    constexpr auto highest_power_of_two =
        TypeParam{1} << (std::numeric_limits<TypeParam>::digits - 1);

    EXPECT_EQ(round_up_power_of_two<TypeParam>(highest_power_of_two), highest_power_of_two);
    EXPECT_EQ(round_up_power_of_two<TypeParam>(highest_power_of_two - 1), highest_power_of_two);
}
