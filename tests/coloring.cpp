#include <smallcanon/coloring.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <random>
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
            if (i == 0 && color != 0) {
                return testing::AssertionFailure() << "first label has color " << color << " instead of 0";
            }
            if (i > 0 && color != previous_color && color != i) {
                return testing::AssertionFailure()
                       << "label at position " << i << " has color " << color << " after color " << previous_color
                       << "; expected either " << previous_color << " or " << i;
            }
            previous_color = color;
        }

        return testing::AssertionSuccess();
    }

    template<typename Coloring>
    std::vector<smallcanon::color_t> snapshot_colors(const Coloring& coloring) {
        std::vector<smallcanon::color_t> colors(coloring.num_nodes());
        for (smallcanon::node_t u = 0; u < coloring.num_nodes(); ++u) {
            colors[u] = coloring.get_color(u);
        }
        return colors;
    }

    template<typename Coloring>
    testing::AssertionResult node_has_unique_color(const Coloring& coloring, smallcanon::node_t node) {
        const auto color = coloring.get_color(node);
        for (smallcanon::node_t other = 0; other < coloring.num_nodes(); ++other) {
            if (other != node && coloring.get_color(other) == color) {
                return testing::AssertionFailure()
                       << "node " << node << " has color " << color << ", also used by node " << other;
            }
        }
        return testing::AssertionSuccess();
    }

    template<typename Coloring>
    std::vector<smallcanon::color_t> consistency_preserving_colors_for_node(const Coloring& coloring,
                                                                            smallcanon::node_t node) {
        const auto n = coloring.num_nodes();
        smallcanon::node_t pos = 0;
        for (; pos < n; ++pos) {
            if (static_cast<smallcanon::node_t>(coloring.labels()[pos]) == node) {
                break;
            }
        }
        if (pos == n) {
            return {};
        }

        const auto old_color = coloring.get_color(node);
        smallcanon::node_t class_start = pos;
        while (class_start > 0 && coloring.color_at_label(class_start - 1) == old_color) {
            --class_start;
        }

        smallcanon::node_t class_end = pos;
        while (class_end + 1 < n && coloring.color_at_label(class_end + 1) == old_color) {
            ++class_end;
        }

        std::vector<smallcanon::color_t> colors{old_color};
        if (class_start < class_end) {
            colors.push_back(class_end);
        } else if (class_start > 0) {
            colors.push_back(coloring.color_at_label(class_start - 1));
        }
        return colors;
    }

    template<typename Coloring>
    testing::AssertionResult other_nodes_kept_colors(const Coloring& coloring,
                                                     std::span<const smallcanon::color_t> previous_colors,
                                                     smallcanon::node_t individualized_node) {
        if (previous_colors.size() != coloring.num_nodes()) {
            return testing::AssertionFailure() << "color snapshot has " << previous_colors.size() << " colors for "
                                               << coloring.num_nodes() << " nodes";
        }

        for (smallcanon::node_t other = 0; other < coloring.num_nodes(); ++other) {
            if (other == individualized_node) {
                continue;
            }
            if (coloring.get_color(other) != previous_colors[other]) {
                return testing::AssertionFailure() << "node " << other << " changed color from "
                                                   << previous_colors[other] << " to " << coloring.get_color(other);
            }
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

    coloring.individualize(1);
    coloring.individualize(2);
    coloring.individualize(3);

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

    EXPECT_EQ(coloring.set_color(4, 5), 0);
    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));

    EXPECT_EQ(coloring.set_color(1, 4), 0);
    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));

    EXPECT_EQ(coloring.set_color(3, 3), 0);
    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));

    EXPECT_EQ(coloring.set_color(2, 2), 0);
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

    constexpr auto last_node = TypeParam::expected_capacity - 1;

    EXPECT_EQ(coloring.set_color(last_node, last_node), 0);
    EXPECT_EQ(coloring.get_color(last_node), last_node);

    EXPECT_EQ(coloring.set_color(last_node, last_node), last_node);
    EXPECT_EQ(coloring.get_color(last_node), last_node);
    EXPECT_TRUE(coloring.is_consistent());
}

TYPED_TEST(ColoringTests, SetColorWorksAtHighestNodeAndColor) {
    auto coloring = TypeParam::make_coloring();
    constexpr auto last_node = TypeParam::expected_capacity - 1;
    constexpr auto last_color = TypeParam::expected_capacity - 1;

    EXPECT_EQ(coloring.set_color(last_node, last_color), 0);
    EXPECT_EQ(coloring.get_color(last_node), last_color);
}

TYPED_TEST(ColoringTests, RandomSetColorPreservesAssignedColorOtherColorsAndConsistency) {
    static constexpr int kRepeats = 300;

    auto rng = std::mt19937_64{0x5eed5eedULL + TypeParam::expected_capacity};

    for (int repetition = 0; repetition < kRepeats; ++repetition) {
        auto coloring = TypeParam::make_coloring();
        const auto n =
                std::uniform_int_distribution<smallcanon::node_t>{coloring.num_nodes() / 2, coloring.num_nodes()}(rng);
        auto node_dist = std::uniform_int_distribution<smallcanon::node_t>{0, n - 1};
        auto col_dist = std::uniform_int_distribution<smallcanon::color_t>{0, n - 1};

        for (smallcanon::node_t step = 0; step < 2 * n; ++step) {
            const auto node = node_dist(rng);
            const auto col = col_dist(rng);

            const auto copied = coloring.copy();
            coloring.set_color(node, col);

            for (smallcanon::node_t u = 0; u < n; ++u) {
                if (u == node) {
                    ASSERT_EQ(coloring.get_color(u), col);
                } else {
                    ASSERT_EQ(coloring.get_color(u), copied.get_color(u));
                }
            }
        }
    }
}

TYPED_TEST(ColoringTests, IndividualizeCreatesUniqueColorWithoutChangingOtherNodeColors) {
    auto coloring = TypeParam::make_coloring();

    const auto n = coloring.num_nodes();
    coloring.colors()[n - 4] = static_cast<typename TypeParam::coloring_t::scolor_t>(n - 4);
    coloring.colors()[n - 3] = static_cast<typename TypeParam::coloring_t::scolor_t>(n - 4);
    coloring.colors()[n - 2] = static_cast<typename TypeParam::coloring_t::scolor_t>(n - 2);
    coloring.colors()[n - 1] = static_cast<typename TypeParam::coloring_t::scolor_t>(n - 2);
    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));

    const smallcanon::node_t first_node = n - 5;
    const auto colors_before_first = snapshot_colors(coloring);
    coloring.individualize(first_node);

    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));
    EXPECT_TRUE(node_has_unique_color(coloring, first_node));
    EXPECT_TRUE(other_nodes_kept_colors(coloring, colors_before_first, first_node));

    const smallcanon::node_t second_node = n - 3;
    const auto colors_before_second = snapshot_colors(coloring);
    coloring.individualize(second_node);

    EXPECT_TRUE(coloring.is_consistent());
    ASSERT_TRUE(labels_are_sorted_by_color(coloring));
    EXPECT_TRUE(node_has_unique_color(coloring, second_node));
    EXPECT_TRUE(other_nodes_kept_colors(coloring, colors_before_second, second_node));
}

TYPED_TEST(ColoringTests, CopyPreservesColorsAndIsIndependent) {
    auto coloring = TypeParam::make_coloring();
    constexpr auto last_node = TypeParam::expected_capacity - 1;

    coloring.set_color(last_node, last_node);
    coloring.set_color(0, last_node - 1);
    EXPECT_TRUE(coloring.is_consistent());

    auto copied = coloring.copy();

    EXPECT_EQ(copied.capacity(), coloring.capacity());
    EXPECT_EQ(copied.get_color(0), last_node - 1);
    EXPECT_EQ(copied.get_color(last_node), last_node);

    copied.set_color(last_node - 1, last_node - 2);

    EXPECT_EQ(coloring.get_color(last_node - 1), 0);
    EXPECT_EQ(copied.get_color(last_node - 1), last_node - 2);
    EXPECT_TRUE(copied.is_consistent());
}
