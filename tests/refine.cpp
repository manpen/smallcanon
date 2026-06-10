#include <smallcanon/refine/naive.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string_view>
#include <tuple>
#include <vector>

#include <xsimd/xsimd.hpp>
#include "smallcanon/parser.hpp"

#include "smallcanon/refine/avx512intrin.hpp"

namespace {
    struct RefinementNaiveScale {
        [[maybe_unused]]
        static constexpr std::string_view name = "naive";

        template<typename SM, typename SC>
        static void refine(const smallcanon::AdjMatrix<SM>& graph, smallcanon::Coloring<SC>& coloring) {
            smallcanon::refine::naive::refine(graph, coloring);
        }
    };

#if XSIMD_WITH_AVX512F
    struct RefinementAVX512Intrinsics {
        [[maybe_unused]]
        static constexpr std::string_view name = "avx512intrin";

        template<typename SM, typename SC>
        static void refine(const smallcanon::AdjMatrix<SM>& graph, smallcanon::Coloring<SC>& coloring) {
            smallcanon::refine::avx512intrin::refine(graph, coloring);
        }
    };
#endif


    template<typename Refinement, typename Graph>
    struct RefinementTestConfig {
        using refinement_t = Refinement;
        using graph_t = Graph;
        using coloring_t = typename smallcanon::MatchedColoring<graph_t>::coloring_t;
    };

    template<typename T>
    class RefinementTests : public testing::Test {
    protected:
        using refinement_t = typename T::refinement_t;
        using graph_t = typename T::graph_t;
        using coloring_t = typename T::coloring_t;

        static graph_t make_graph(smallcanon::node_t num_nodes) {
            return graph_t(num_nodes);
        }

        static graph_t make_graph(smallcanon::node_t num_nodes, const std::vector<smallcanon::edge_t>& edges) {
            graph_t graph(num_nodes);
            graph.add_edges(edges);
            return graph;
        }
    };

    using RefinementTestTypes =
            testing::Types<RefinementTestConfig<RefinementNaiveScale, smallcanon::AdjMatrix8>,
                           RefinementTestConfig<RefinementNaiveScale, smallcanon::AdjMatrix16>,
                           RefinementTestConfig<RefinementNaiveScale, smallcanon::AdjMatrix32>,
                           RefinementTestConfig<RefinementNaiveScale, smallcanon::AdjMatrixHeap>,
                           RefinementTestConfig<RefinementAVX512Intrinsics, smallcanon::AdjMatrix8>>;
    TYPED_TEST_SUITE(RefinementTests, RefinementTestTypes);

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
    const auto graph = TestFixture::make_graph(8);
    typename TestFixture::coloring_t coloring(graph.num_nodes());

    TestFixture::refinement_t::refine(graph, coloring);

    for (smallcanon::node_t u = 0; u < graph.num_nodes(); ++u) {
        EXPECT_EQ(coloring.get_color(u), 0) << "node " << u;
    }
}

TYPED_TEST(RefinementTests, PathFourSeparatesEndpointsFromMiddleNodes) {
    const std::vector<smallcanon::edge_t> edges{{0, 1}, {1, 2}, {2, 3}};
    const auto graph = TestFixture::make_graph(4, edges);
    typename TestFixture::coloring_t coloring(graph.num_nodes());

    TestFixture::refinement_t::refine(graph, coloring);

    EXPECT_EQ(coloring.get_color(0), coloring.get_color(3));
    EXPECT_EQ(coloring.get_color(1), coloring.get_color(2));
    EXPECT_NE(coloring.get_color(0), coloring.get_color(1));
}

TYPED_TEST(RefinementTests, StarSeparatesCenterFromLeaves) {
    const std::vector<smallcanon::edge_t> edges{{0, 1}, {0, 2}, {0, 3}, {0, 4}};
    const auto graph = TestFixture::make_graph(5, edges);
    typename TestFixture::coloring_t coloring(graph.num_nodes());

    TestFixture::refinement_t::refine(graph, coloring);

    // we expect refinement to smallest possible colors; hence, for the star colors 0, 1
    const smallcanon::color_t leaf_color = (coloring.get_color(0) == 0) ? 1 : 0;

    for (smallcanon::node_t leaf = 1; leaf < 5; ++leaf) {
        EXPECT_EQ(coloring.get_color(leaf), leaf_color) << "leaf " << leaf;
    }
}

TYPED_TEST(RefinementTests, CycleKeepsSymmetricNodesInOneColorClass) {
    const std::vector<smallcanon::edge_t> edges{{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    const auto graph = TestFixture::make_graph(4, edges);
    typename TestFixture::coloring_t coloring(graph.num_nodes());

    TestFixture::refinement_t::refine(graph, coloring);

    for (const auto u: graph.nodes()) {
        EXPECT_EQ(coloring.get_color(u), 0) << "node " << u;
    }
}

TYPED_TEST(RefinementTests, InitialColorsAreRespectedEmpty) {
    const auto graph = TestFixture::make_graph(2);
    typename TestFixture::coloring_t coloring(graph.num_nodes());
    coloring.set_color(0, 1);

    TestFixture::refinement_t::refine(graph, coloring);

    const auto color_of_other = coloring.get_color(1);

    EXPECT_NE(coloring.get_color(0), color_of_other);
    for (smallcanon::node_t u = 1; u < graph.num_nodes(); ++u) {
        EXPECT_EQ(coloring.get_color(u), color_of_other) << "node " << u;
    }

    EXPECT_TRUE(coloring.get_color(0) < 2);
    EXPECT_TRUE(coloring.get_color(1) < 2);
}


TYPED_TEST(RefinementTests, InitialColorsAreRespectedCircle) {
    const auto graph = TestFixture::make_graph(3, {{0, 1}, {1, 2}, {2, 0}});
    typename TestFixture::coloring_t coloring(graph.num_nodes());
    coloring.set_color(1, 2);

    TestFixture::refinement_t::refine(graph, coloring);

    const auto color_of_other = coloring.get_color(0);

    EXPECT_EQ(coloring.get_color(0), color_of_other);
    EXPECT_NE(coloring.get_color(1), color_of_other);
    EXPECT_EQ(coloring.get_color(2), color_of_other);

    EXPECT_TRUE(coloring.get_color(0) < 2);
    EXPECT_TRUE(coloring.get_color(1) < 2);
}

TYPED_TEST(RefinementTests, DefaultNodeCount) {
    const auto graph = TestFixture::make_graph(8);
    typename TestFixture::coloring_t coloring(graph.num_nodes());

    TestFixture::refinement_t::refine(graph, coloring);

    EXPECT_EQ(collect_colors(coloring, graph.num_nodes()), std::vector<smallcanon::color_t>(graph.num_nodes(), 0));
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
    using graph_t = typename TestFixture::graph_t;
    const auto dataset_path = std::filesystem::path(SMALLCANON_PROJECT_ROOT) / "datasets" / "curated.g6";
    std::ifstream curated(dataset_path);

    std::mt19937_64 rng(123456);

    for (auto [name, var_graph]: smallcanon::read_graph_dataset(curated)) {
        std::visit(
                [&](auto&& graph) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(graph)>, graph_t>) {
                        typename TestFixture::coloring_t coloring(graph.num_nodes());
                        auto [mapped_graph, mapped_coloring, mapping] = permute_graph(rng, graph, coloring);

                        TestFixture::refinement_t::refine(graph, coloring);
                        TestFixture::refinement_t::refine(mapped_graph, mapped_coloring);

                        auto mapped_back = mapped_coloring.copy();
                        for (auto org: graph.nodes()) {
                            mapped_back.set_color(org, mapped_coloring.get_color(mapping[org]));
                        }

                        ASSERT_EQ(coloring, mapped_back);
                    }
                },
                var_graph);
    }
}
