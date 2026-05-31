#include <smallcanon/refine/naive.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string_view>
#include <tuple>
#include <vector>

#include "smallcanon/parser.hpp"

namespace {
    struct NaiveScalarRefinement {
        [[maybe_unused]]
        static constexpr std::string_view name = "naive_scalar";

        template<typename SM, typename SC>
        static void refine(const smallcanon::AdjMatrix<SM>& graph, smallcanon::Coloring<SC>& coloring) {
            smallcanon::refine::naive_scalar(graph, coloring);
        }
    };

    template<typename T>
    class RefinementTests : public testing::Test {};

    using RefinementImplementations = testing::Types<NaiveScalarRefinement>;
    TYPED_TEST_SUITE(RefinementTests, RefinementImplementations);

    template<typename SC>
    std::vector<smallcanon::color_t> collect_colors(const smallcanon::Coloring<SC>& coloring,
                                                    smallcanon::node_t num_nodes) {
        std::vector<smallcanon::color_t> colors;
        colors.reserve(num_nodes);
        for (smallcanon::node_t u = 0; u < num_nodes; ++u) {
            colors.push_back(coloring.get_color(u));
        }
        return colors;
    }
} // namespace

TYPED_TEST(RefinementTests, EmptyGraphKeepsAllNodesInOneColorClass) {
    const auto graph = smallcanon::make_adj_matrix8();
    smallcanon::Coloring8 coloring(graph.num_nodes());

    TypeParam::refine(graph, coloring);

    for (smallcanon::node_t u = 0; u < graph.num_nodes(); ++u) {
        EXPECT_EQ(coloring.get_color(u), 0) << "node " << u;
    }
}

TYPED_TEST(RefinementTests, PathFourSeparatesEndpointsFromMiddleNodes) {
    const std::vector<smallcanon::edge_t> edges{{0, 1}, {1, 2}, {2, 3}};
    const auto graph = smallcanon::make_adj_matrix8(4, edges);
    smallcanon::Coloring8 coloring(graph.num_nodes());

    TypeParam::refine(graph, coloring);

    EXPECT_EQ(coloring.get_color(0), coloring.get_color(3));
    EXPECT_EQ(coloring.get_color(1), coloring.get_color(2));
    EXPECT_NE(coloring.get_color(0), coloring.get_color(1));
}

TYPED_TEST(RefinementTests, StarSeparatesCenterFromLeaves) {
    const std::vector<smallcanon::edge_t> edges{{0, 1}, {0, 2}, {0, 3}, {0, 4}};
    const auto graph = smallcanon::make_adj_matrix8(5, edges);
    smallcanon::Coloring8 coloring(graph.num_nodes());

    TypeParam::refine(graph, coloring);

    // we expect refinement to smallest possible colors; hence, for the star colors 0, 1
    const smallcanon::color_t leaf_color = (coloring.get_color(0) == 0) ? 1 : 0;

    for (smallcanon::node_t leaf = 1; leaf < 5; ++leaf) {
        EXPECT_EQ(coloring.get_color(leaf), leaf_color) << "leaf " << leaf;
    }
}

TYPED_TEST(RefinementTests, CycleKeepsSymmetricNodesInOneColorClass) {
    const std::vector<smallcanon::edge_t> edges{{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    const auto graph = smallcanon::make_adj_matrix8(4, edges);
    smallcanon::Coloring8 coloring(graph.num_nodes());

    TypeParam::refine(graph, coloring);

    for (const auto u: graph.nodes()) {
        EXPECT_EQ(coloring.get_color(u), 0) << "node " << u;
    }
}

TYPED_TEST(RefinementTests, InitialColorsArePartOfTheFingerprint) {
    const auto graph = smallcanon::make_adj_matrix8(2);
    smallcanon::Coloring8 coloring(graph.num_nodes());
    coloring.set_color(0, 1);

    TypeParam::refine(graph, coloring);

    EXPECT_EQ(coloring.get_color(0), 1);
    for (smallcanon::node_t u = 1; u < graph.num_nodes(); ++u) {
        EXPECT_EQ(coloring.get_color(u), 0) << "node " << u;
    }
}

TYPED_TEST(RefinementTests, DefaultNodeCount) {
    const auto graph = smallcanon::make_adj_matrix128();
    smallcanon::Coloring128 coloring(graph.num_nodes());

    TypeParam::refine(graph, coloring);

    EXPECT_EQ(collect_colors(coloring, graph.num_nodes()), std::vector<smallcanon::color_t>(graph.num_nodes(), 0));
}

TYPED_TEST(RefinementTests, SupportsHeapBackedGraphAndColoring) {
    const std::vector<smallcanon::edge_t> edges{{0, 1}, {1, 2}, {2, 3}};
    const auto graph = smallcanon::make_adjmatrix_heap(4, edges);
    smallcanon::ColoringHeap coloring(graph.num_nodes());

    TypeParam::refine(graph, coloring);

    EXPECT_EQ(collect_colors(coloring, 4), (std::vector<smallcanon::color_t>{0, 1, 1, 0}));
}


template<typename SM, typename SC>
std::tuple<smallcanon::AdjMatrix<SM>, smallcanon::Coloring<SC>, std::vector<smallcanon::node_t>>
permute_graph(auto&& rng, const smallcanon::AdjMatrix<SM>& graph, const smallcanon::Coloring<SC>& coloring) {
    auto mapping = graph.nodes() | std::ranges::to<std::vector<smallcanon::node_t>>();
    std::ranges::shuffle(mapping, rng);

    auto mapped_graph = smallcanon::AdjMatrix<SM>(graph.num_nodes());
    auto mapped_color = coloring.copy();

    mapped_graph.add_edges(graph.edges() | std::views::transform([&](const auto& e) {
                               return std::make_pair(mapping[e.first], mapping[e.second]);
                           }));

    for (auto u: graph.nodes()) {
        mapped_color.set_color(mapping[u], coloring.get_color(u));
    }

    return {std::move(mapped_graph), std::move(mapped_color), std::move(mapping)};
}

TYPED_TEST(RefinementTests, InvarianceNodePermutation) {
    const auto dataset_path = std::filesystem::path(SMALLCANON_PROJECT_ROOT) / "datasets" / "curated.g6";
    std::ifstream curated(dataset_path);

    std::mt19937_64 rng(123456);

    for (auto [name, var_graph]: smallcanon::read_graph_dataset(curated) | std::views::take(5000)) {
        std::visit(
                [&](auto&& graph) {
                    auto coloring = smallcanon::ColoringHeap(graph.num_nodes());
                    auto [mapped_graph, mapped_coloring, mapping] = permute_graph(rng, graph, coloring);

                    TypeParam::refine(graph, coloring);
                    TypeParam::refine(mapped_graph, mapped_coloring);

                    auto mapped_back = mapped_coloring.copy();
                    for (auto org: graph.nodes()) {
                        mapped_back.set_color(org, mapped_coloring.get_color(mapping[org]));
                    }

                    ASSERT_EQ(coloring, mapped_back);
                },
                var_graph);
    }
}
