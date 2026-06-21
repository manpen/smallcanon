#include <smallcanon/coloring.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

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

    template<typename Coloring>
    testing::AssertionResult labels_are_sorted_by_color(const Coloring& coloring) {
        std::vector<bool> seen(coloring.num_nodes(), false);
        smallcanon::color_t previous_color = 0;

        for (smallcanon::node_t i = 0; i < coloring.num_nodes(); ++i) {
            const auto label = static_cast<smallcanon::node_t>(coloring.labels()[i]);
            if (label >= coloring.num_nodes()) {
                return testing::AssertionFailure() << "label " << label << " is outside active range";
            }
            if (seen[label]) {
                return testing::AssertionFailure() << "label " << label << " appears more than once";
            }
            seen[label] = true;

            const auto color = coloring.get_color(label);
            if (i > 0 && color < previous_color) {
                return testing::AssertionFailure()
                       << "label at position " << i << " has color " << color << " after color " << previous_color;
            }
            previous_color = color;
        }

        return testing::AssertionSuccess();
    }
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

    using Buffer = decltype(std::declval<Storage&>().buffer());
    static_assert(std::ranges::contiguous_range<Buffer>);
    static_assert(std::same_as<std::ranges::range_value_t<Buffer>, Color>);
    static_assert(std::same_as<std::ranges::range_reference_t<Buffer>, Color&>);

    auto buffer = storage.buffer();

    EXPECT_EQ(buffer.size(), TypeParam::expected_capacity);
    EXPECT_TRUE(std::ranges::all_of(buffer, [](auto color) { return color == 0; }));
}

TYPED_TEST(ColorStoreTests, ExposesConstBufferRange) {
    using Storage = typename TypeParam::storage_t;
    using Color = typename Storage::scolor_t;

    using Buffer = decltype(std::declval<const Storage&>().buffer());
    static_assert(std::ranges::contiguous_range<Buffer>);
    static_assert(std::same_as<std::ranges::range_value_t<Buffer>, Color>);
    static_assert(std::same_as<std::ranges::range_reference_t<Buffer>, const Color&>);

    const Storage storage = TypeParam::make_storage();
    auto buffer = storage.buffer();

    EXPECT_EQ(buffer.size(), TypeParam::expected_capacity);
}

TYPED_TEST(ColorStoreTests, MutableWritesAreVisibleThroughConstRange) {
    using Storage = typename TypeParam::storage_t;
    using Color = typename Storage::scolor_t;
    Storage storage = TypeParam::make_storage();

    auto mutable_buffer = storage.buffer();
    mutable_buffer[0] = static_cast<Color>(7);
    mutable_buffer[TypeParam::expected_capacity - 1] = static_cast<Color>(13);

    const auto& const_storage = storage;
    auto const_buffer = const_storage.buffer();

    EXPECT_EQ(const_buffer.data(), mutable_buffer.data());
    EXPECT_EQ(const_buffer[0], static_cast<Color>(7));
    EXPECT_EQ(const_buffer[TypeParam::expected_capacity - 1], static_cast<Color>(13));
}

TYPED_TEST(ColoringTests, IsConstructibleFromCapacityButNotDefaultConstructible) {
    using Coloring = typename TypeParam::coloring_t;

    static_assert(!std::is_default_constructible_v<Coloring>);
    static_assert(!std::default_initializable<Coloring>);
    static_assert(std::is_constructible_v<Coloring, smallcanon::node_t>);
    Coloring coloring(TypeParam::expected_capacity);

    EXPECT_TRUE(coloring.is_consistent());
}

TYPED_TEST(ColoringTests, ReportsCapacity) {
    const auto coloring = TypeParam::make_coloring();

    EXPECT_TRUE(coloring.is_consistent());
    EXPECT_EQ(coloring.num_nodes(), TypeParam::expected_capacity);
    EXPECT_EQ(coloring.capacity(), TypeParam::expected_capacity);
}

TYPED_TEST(ColoringTests, NewColoringInitializesLabelsToActiveNodes) {
    const auto coloring = TypeParam::make_coloring();

    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));
    for (smallcanon::node_t u = 0; u < coloring.num_nodes(); ++u) {
        EXPECT_EQ(coloring.labels()[u], u);
    }
}

TEST(ColoringTests, FixedColoringTracksActiveNodesSeparatelyFromCapacity) {
    smallcanon::Coloring8 coloring(5);

    EXPECT_EQ(coloring.num_nodes(), 5);
    EXPECT_EQ(coloring.capacity(), 8);

    coloring.set_color(1, 1);
    coloring.set_color(2, 2);
    coloring.set_color(3, 3);

    EXPECT_TRUE(coloring.is_consistent());

    coloring.colors()[5] = 4;
    coloring.colors()[6] = 4;
    coloring.colors()[7] = 4;

    EXPECT_TRUE(coloring.is_consistent());

    auto copied = coloring.copy();
    EXPECT_TRUE(copied.is_consistent());
    EXPECT_EQ(copied.num_nodes(), coloring.num_nodes());
    EXPECT_EQ(copied.capacity(), coloring.capacity());
}

TEST(ColoringTests, SetColorMaintainsLabelsSortedByColor) {
    smallcanon::Coloring8 coloring(6);

    EXPECT_EQ(coloring.set_color(4, 2), 0);
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));

    EXPECT_EQ(coloring.set_color(1, 2), 0);
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));

    EXPECT_EQ(coloring.set_color(3, 1), 0);
    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));

    EXPECT_EQ(coloring.set_color(4, 0), 2);
    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));
}

TYPED_TEST(ColoringTests, NewColoringInitializesColorsToZero) {
    const auto coloring = TypeParam::make_coloring();

    EXPECT_TRUE(coloring.is_consistent());
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

TYPED_TEST(ColoringTests, IndividualizeMovesNodeToLastLabelWithNextColor) {
    auto coloring = TypeParam::make_coloring();

    constexpr smallcanon::node_t first_node = 3;
    const auto previous_last = static_cast<smallcanon::node_t>(coloring.labels()[coloring.num_nodes() - 1]);
    const auto first_color = coloring.get_color(previous_last) + 1;

    coloring.individualize(first_node);

    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));
    EXPECT_EQ(coloring.labels()[coloring.num_nodes() - 1], first_node);
    EXPECT_EQ(coloring.get_color(first_node), first_color);

    constexpr smallcanon::node_t second_node = 1;
    const auto previous_second_last = static_cast<smallcanon::node_t>(coloring.labels()[coloring.num_nodes() - 1]);
    const auto second_color = coloring.get_color(previous_second_last) + 1;

    coloring.individualize(second_node);

    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));
    EXPECT_EQ(coloring.labels()[coloring.num_nodes() - 1], second_node);
    EXPECT_EQ(coloring.get_color(second_node), second_color);
    EXPECT_EQ(coloring.get_color(first_node), first_color);
}

TYPED_TEST(ColoringTests, CopyPreservesColorsAndIsIndependent) {
    auto coloring = TypeParam::make_coloring();
    constexpr auto last_node = TypeParam::expected_capacity - 1;

    coloring.set_color(0, 1);
    EXPECT_TRUE(coloring.is_consistent());
    coloring.set_color(last_node, last_node);

    auto copied = coloring.copy();

    EXPECT_EQ(copied.capacity(), coloring.capacity());
    EXPECT_EQ(copied.get_color(0), 1);
    EXPECT_EQ(copied.get_color(last_node), last_node);

    copied.set_color(0, 2);

    EXPECT_EQ(coloring.get_color(0), 1);
    EXPECT_EQ(copied.get_color(0), 2);
}
