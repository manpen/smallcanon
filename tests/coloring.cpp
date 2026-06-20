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
            return Storage{ExpectedCapacity};
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

    template<typename Coloring, smallcanon::node_t ExpectedCapacity>
    struct ColoringCase {
        using coloring_t = Coloring;
        static constexpr smallcanon::node_t expected_capacity = ExpectedCapacity;

        static Coloring make_coloring() {
            return Coloring{ExpectedCapacity};
        }
    };

    template<typename T>
    class ColoringTests : public testing::Test {};

    using ColoringTypes =
            testing::Types<ColoringCase<smallcanon::Coloring8, 8>, ColoringCase<smallcanon::Coloring16, 16>,
                           ColoringCase<smallcanon::Coloring32, 32>, ColoringCase<smallcanon::Coloring64, 64>,
                           ColoringCase<smallcanon::Coloring128, 128>, ColoringCase<smallcanon::ColoringHeap, 128>>;
    TYPED_TEST_SUITE(ColoringTests, ColoringTypes);
} // namespace

TYPED_TEST(ColorStoreTests, IsConstructibleFromCapacity) {
    using Storage = typename TypeParam::storage_t;

    static_assert(std::is_constructible_v<Storage, smallcanon::node_t>);
    [[maybe_unused]] Storage storage(TypeParam::expected_capacity);
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

TYPED_TEST(ColoringTests, IsConstructibleFromCapacityButNotDefaultConstructible) {
    using Coloring = typename TypeParam::coloring_t;

    static_assert(!std::is_default_constructible_v<Coloring>);
    static_assert(!std::default_initializable<Coloring>);
    static_assert(std::is_constructible_v<Coloring, smallcanon::node_t>);
    [[maybe_unused]] Coloring coloring(TypeParam::expected_capacity);
}

TYPED_TEST(ColoringTests, ReportsCapacity) {
    const auto coloring = TypeParam::make_coloring();

    EXPECT_EQ(coloring.num_nodes(), TypeParam::expected_capacity);
    EXPECT_EQ(coloring.capacity(), TypeParam::expected_capacity);
}

TEST(ColoringTests, FixedColoringTracksActiveNodesSeparatelyFromCapacity) {
    smallcanon::Coloring8 coloring(5);

    EXPECT_EQ(coloring.num_nodes(), 5);
    EXPECT_EQ(coloring.capacity(), 8);

    coloring.set_color(1, 1);
    coloring.set_color(2, 2);
    coloring.set_color(3, 3);
    coloring.buffer()[5] = 4;
    coloring.buffer()[6] = 4;
    coloring.buffer()[7] = 4;

    EXPECT_EQ(coloring.first_available_color(), 4);

    auto copied = coloring.copy();
    EXPECT_EQ(copied.num_nodes(), coloring.num_nodes());
    EXPECT_EQ(copied.capacity(), coloring.capacity());
    EXPECT_EQ(copied.first_available_color(), 4);
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

TYPED_TEST(ColoringTests, CopyPreservesColorsAndIsIndependent) {
    auto coloring = TypeParam::make_coloring();
    constexpr auto last_node = TypeParam::expected_capacity - 1;

    coloring.set_color(0, 1);
    coloring.set_color(last_node, last_node);

    auto copied = coloring.copy();

    EXPECT_EQ(copied.capacity(), coloring.capacity());
    EXPECT_EQ(copied.get_color(0), 1);
    EXPECT_EQ(copied.get_color(last_node), last_node);

    copied.set_color(0, 2);

    EXPECT_EQ(coloring.get_color(0), 1);
    EXPECT_EQ(copied.get_color(0), 2);
}

TYPED_TEST(ColoringTests, ComputeInverseOfDiscreteMapsColorsBackToNodes) {
    auto coloring = TypeParam::make_coloring();

    constexpr smallcanon::node_t n = 6;
    coloring.set_color(0, 4);
    coloring.set_color(1, 2);
    coloring.set_color(2, 0);
    coloring.set_color(3, 5);
    coloring.set_color(4, 1);
    coloring.set_color(5, 3);

    const auto inverse = coloring.compute_inverse_of_discrete(n);

    EXPECT_EQ(inverse.get_color(0), 2);
    EXPECT_EQ(inverse.get_color(1), 4);
    EXPECT_EQ(inverse.get_color(2), 1);
    EXPECT_EQ(inverse.get_color(3), 5);
    EXPECT_EQ(inverse.get_color(4), 0);
    EXPECT_EQ(inverse.get_color(5), 3);
}
