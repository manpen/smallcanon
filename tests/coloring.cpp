#include <smallcanon/coloring.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

namespace {
    template<typename Storage, smallcanon::node_t ExpectedCapacity>
    struct ColorStoreCase {
        using storage_t = Storage;
        static constexpr smallcanon::node_t expected_capacity = ExpectedCapacity;

        static Storage make_storage() {
            if constexpr (std::default_initializable<Storage>) {
                return Storage{};
            } else {
                return Storage{ExpectedCapacity};
            }
        }
    };

    template<typename T>
    class ColorStoreTests : public testing::Test {};

    using ColorStoreTypes = testing::Types<ColorStoreCase<smallcanon::details::FixedColorStore8, 8>,
                                           ColorStoreCase<smallcanon::details::FixedColorStore16, 16>,
                                           ColorStoreCase<smallcanon::details::FixedColorStore32, 32>,
                                           ColorStoreCase<smallcanon::details::FixedColorStore64, 64>,
                                           ColorStoreCase<smallcanon::details::FixedColorStore128, 128>,
                                           ColorStoreCase<smallcanon::details::ColorStoreHeap, 128>>;
    TYPED_TEST_SUITE(ColorStoreTests, ColorStoreTypes);

    template<typename T>
    class FixedColorStoreDefaultConstructibleTests : public testing::Test {};

    using FixedColorStoreTypes = testing::Types<ColorStoreCase<smallcanon::details::FixedColorStore8, 8>,
                                                ColorStoreCase<smallcanon::details::FixedColorStore16, 16>,
                                                ColorStoreCase<smallcanon::details::FixedColorStore32, 32>,
                                                ColorStoreCase<smallcanon::details::FixedColorStore64, 64>,
                                                ColorStoreCase<smallcanon::details::FixedColorStore128, 128>>;
    TYPED_TEST_SUITE(FixedColorStoreDefaultConstructibleTests, FixedColorStoreTypes);

    template<typename Coloring, typename Storage, smallcanon::node_t ExpectedCapacity>
    struct FixedColoringCase {
        using coloring_t = Coloring;
        using storage_t = Storage;
        static constexpr smallcanon::node_t expected_capacity = ExpectedCapacity;

        static Coloring make_coloring() {
            return Coloring{Storage{}};
        }
    };

    struct HeapColoringCase {
        using coloring_t = smallcanon::ColoringHeap;
        using storage_t = smallcanon::details::ColorStoreHeap;
        static constexpr smallcanon::node_t expected_capacity = 128;

        static coloring_t make_coloring() {
            return coloring_t{storage_t{expected_capacity}};
        }
    };

    template<typename T>
    class ColoringTests : public testing::Test {};

    using ColoringTypes =
            testing::Types<FixedColoringCase<smallcanon::Coloring8, smallcanon::details::FixedColorStore8, 8>,
                           FixedColoringCase<smallcanon::Coloring16, smallcanon::details::FixedColorStore16, 16>,
                           FixedColoringCase<smallcanon::Coloring32, smallcanon::details::FixedColorStore32, 32>,
                           FixedColoringCase<smallcanon::Coloring64, smallcanon::details::FixedColorStore64, 64>,
                           FixedColoringCase<smallcanon::Coloring128, smallcanon::details::FixedColorStore128, 128>,
                           HeapColoringCase>;
    TYPED_TEST_SUITE(ColoringTests, ColoringTypes);

    template<typename T>
    class FixedColoringDefaultConstructibleTests : public testing::Test {};

    using FixedColoringTypes =
            testing::Types<FixedColoringCase<smallcanon::Coloring8, smallcanon::details::FixedColorStore8, 8>,
                           FixedColoringCase<smallcanon::Coloring16, smallcanon::details::FixedColorStore16, 16>,
                           FixedColoringCase<smallcanon::Coloring32, smallcanon::details::FixedColorStore32, 32>,
                           FixedColoringCase<smallcanon::Coloring64, smallcanon::details::FixedColorStore64, 64>,
                           FixedColoringCase<smallcanon::Coloring128, smallcanon::details::FixedColorStore128, 128>>;
    TYPED_TEST_SUITE(FixedColoringDefaultConstructibleTests, FixedColoringTypes);
} // namespace

TYPED_TEST(FixedColorStoreDefaultConstructibleTests, IsDefaultConstructible) {
    using Storage = typename TypeParam::storage_t;

    static_assert(std::is_default_constructible_v<Storage>);
    static_assert(std::default_initializable<Storage>);
    [[maybe_unused]] Storage storage;
}

TEST(ColorStoreHeapTests, IsNotDefaultConstructible) {
    using Storage = smallcanon::details::ColorStoreHeap;

    static_assert(!std::is_default_constructible_v<Storage>);
    static_assert(!std::default_initializable<Storage>);
    static_assert(std::is_constructible_v<Storage, smallcanon::node_t>);

    [[maybe_unused]] Storage storage(128);
}

TYPED_TEST(ColorStoreTests, ExposesMutableBufferRange) {
    using Storage = typename TypeParam::storage_t;
    using Color = typename Storage::scolor_t;
    Storage storage = TypeParam::make_storage();

    static_assert(std::ranges::range<decltype(std::declval<Storage&>().buffer())>);
    static_assert(std::same_as<decltype(std::declval<Storage&>().buffer()), std::span<Color>>);

    std::span<Color> buffer = storage.buffer();

    EXPECT_EQ(buffer.size(), TypeParam::expected_capacity);
    EXPECT_TRUE(std::ranges::all_of(buffer, [](auto color) { return color == 0; }));
}

TYPED_TEST(ColorStoreTests, ExposesConstBufferRange) {
    using Storage = typename TypeParam::storage_t;
    using Color = typename Storage::scolor_t;

    static_assert(std::ranges::range<decltype(std::declval<const Storage&>().buffer())>);
    static_assert(std::same_as<decltype(std::declval<const Storage&>().buffer()), std::span<const Color>>);

    const Storage storage = TypeParam::make_storage();
    std::span<const Color> buffer = storage.buffer();

    EXPECT_EQ(buffer.size(), TypeParam::expected_capacity);
}

TYPED_TEST(ColorStoreTests, MutableWritesAreVisibleThroughConstRange) {
    using Storage = typename TypeParam::storage_t;
    using Color = typename Storage::scolor_t;
    Storage storage = TypeParam::make_storage();

    std::span<Color> mutable_buffer = storage.buffer();
    mutable_buffer[0] = static_cast<Color>(7);
    mutable_buffer[TypeParam::expected_capacity - 1] = static_cast<Color>(13);

    const auto& const_storage = storage;
    std::span<const Color> const_buffer = const_storage.buffer();

    EXPECT_EQ(const_buffer.data(), mutable_buffer.data());
    EXPECT_EQ(const_buffer[0], static_cast<Color>(7));
    EXPECT_EQ(const_buffer[TypeParam::expected_capacity - 1], static_cast<Color>(13));
}

TYPED_TEST(FixedColoringDefaultConstructibleTests, IsDefaultConstructible) {
    using Coloring = typename TypeParam::coloring_t;

    static_assert(std::is_default_constructible_v<Coloring>);
    static_assert(std::default_initializable<Coloring>);
    [[maybe_unused]] Coloring coloring;
}

TEST(ColoringHeapTests, IsNotDefaultConstructible) {
    using Coloring = smallcanon::ColoringHeap;

    static_assert(!std::is_default_constructible_v<Coloring>);
    static_assert(!std::default_initializable<Coloring>);
    static_assert(std::is_constructible_v<Coloring, smallcanon::details::ColorStoreHeap&&>);

    [[maybe_unused]] Coloring coloring{smallcanon::details::ColorStoreHeap{128}};
}

TYPED_TEST(ColoringTests, CanBeConstructedFromStorage) {
    using Coloring = typename TypeParam::coloring_t;
    using Storage = typename TypeParam::storage_t;

    static_assert(std::is_constructible_v<Coloring, Storage&&>);
    [[maybe_unused]] Coloring coloring = TypeParam::make_coloring();
}

TYPED_TEST(ColoringTests, ReportsCapacity) {
    const auto coloring = TypeParam::make_coloring();

    EXPECT_EQ(coloring.capacity(), TypeParam::expected_capacity);
}

TYPED_TEST(ColoringTests, NewColoringInitializesColorsToZero) {
    const auto coloring = TypeParam::make_coloring();

    EXPECT_EQ(coloring.get_color(0), 0);
    EXPECT_EQ(coloring.get_color(TypeParam::expected_capacity - 1), 0);
}

TYPED_TEST(ColoringTests, SetColorReturnsPreviousColorAndUpdatesColor) {
    auto coloring = TypeParam::make_coloring();

    EXPECT_EQ(coloring.set_color(1, 3), 0);
    EXPECT_EQ(coloring.get_color(1), 3);

    EXPECT_EQ(coloring.set_color(1, 5), 3);
    EXPECT_EQ(coloring.get_color(1), 5);
}

TYPED_TEST(ColoringTests, SetColorWorksAtHighestNodeAndColor) {
    auto coloring = TypeParam::make_coloring();
    constexpr auto last_node = TypeParam::expected_capacity - 1;
    constexpr auto last_color = TypeParam::expected_capacity - 1;

    EXPECT_EQ(coloring.set_color(last_node, last_color), 0);
    EXPECT_EQ(coloring.get_color(last_node), last_color);
}
