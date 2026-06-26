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
            smallcanon::refine::Naive<smallcanon::AdjMatrix<SM>> r{graph};
            r.refine(coloring);
        }
    };

#if XSIMD_WITH_AVX512F
    struct RefinementAVX512Intrinsics {
        [[maybe_unused]]
        static constexpr std::string_view name = "avx512intrin";

        template<typename SM, typename SC>
        static void refine(const smallcanon::AdjMatrix<SM>& graph, smallcanon::Coloring<SC>& coloring) {
            smallcanon::refine::avx512intrin::AVX512<smallcanon::AdjMatrix<SM>> refine{graph};
            refine.refine(coloring);
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

    using RefinementTestTypes = testing::Types<
#if XSIMD_WITH_AVX512F
            RefinementTestConfig<RefinementAVX512Intrinsics, smallcanon::AdjMatrix8>,
            RefinementTestConfig<RefinementAVX512Intrinsics, smallcanon::AdjMatrix16>,
#endif
            RefinementTestConfig<RefinementNaiveScale, smallcanon::AdjMatrix8>,
            RefinementTestConfig<RefinementNaiveScale, smallcanon::AdjMatrix16>,
            RefinementTestConfig<RefinementNaiveScale, smallcanon::AdjMatrix32>,
            RefinementTestConfig<RefinementNaiveScale, smallcanon::AdjMatrixHeap>>;
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

    template<typename LSC, typename RSC>
    testing::AssertionResult colorings_imply_same_partition(const smallcanon::Coloring<LSC>& lhs,
                                                            const smallcanon::Coloring<RSC>& rhs,
                                                            smallcanon::node_t num_nodes) {
        for (smallcanon::node_t u = 0; u < num_nodes; ++u) {
            for (smallcanon::node_t v = u + 1; v < num_nodes; ++v) {
                const bool same_lhs_color = lhs.get_color(u) == lhs.get_color(v);
                const bool same_rhs_color = rhs.get_color(u) == rhs.get_color(v);
                if (same_lhs_color != same_rhs_color) {
                    return testing::AssertionFailure()
                           << "nodes " << u << " and " << v << " disagree: lhs colors are " << lhs.get_color(u)
                           << " and " << lhs.get_color(v) << ", rhs colors are " << rhs.get_color(u) << " and "
                           << rhs.get_color(v);
                }
            }
        }

        return testing::AssertionSuccess();
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

    EXPECT_TRUE(coloring.get_color(0) < 3);
    EXPECT_TRUE(coloring.get_color(1) < 3);
}

TYPED_TEST(RefinementTests, DefaultNodeCount) {
    const auto graph = TestFixture::make_graph(8);
    typename TestFixture::coloring_t coloring(graph.num_nodes());

    TestFixture::refinement_t::refine(graph, coloring);

    EXPECT_EQ(collect_colors(coloring, graph.num_nodes()), std::vector<smallcanon::color_t>(graph.num_nodes(), 0));
}

TYPED_TEST(RefinementTests, MatchesNaivePartitionOnCuratedDataset) {
    using graph_t = typename TestFixture::graph_t;
    using coloring_t = typename TestFixture::coloring_t;

    if constexpr (std::is_same_v<typename TestFixture::refinement_t, RefinementNaiveScale>) {
        return;
    }

    const auto dataset_path = std::filesystem::path(SMALLCANON_PROJECT_ROOT) / "datasets" / "curated.g6";
    std::ifstream curated(dataset_path);
    ASSERT_TRUE(curated.is_open()) << dataset_path;

    std::mt19937_64 rng(2345);

    for (auto [name, var_graph]: smallcanon::read_graph_dataset(curated)) {
        std::visit(
                [&](auto&& graph) {
                    if constexpr (std::is_same_v<std::decay_t<decltype(graph)>, graph_t>) {
                        const auto n = graph.num_nodes();
                        coloring_t coloring(n);
                        coloring_t naive_coloring(n);

                        if (std::uniform_real_distribution{0., 1.}(rng) > 0.5) {
                            // partially precolor half of all instances
                            auto num_colored_nodes = std::uniform_int_distribution<smallcanon::node_t>{0, n / 2}(rng);
                            for (auto i = num_colored_nodes; i; --i) {
                                auto node = std::uniform_int_distribution<smallcanon::node_t>{0, n - 1}(rng);
                                auto color = std::uniform_int_distribution<smallcanon::node_t>{0, n - 1}(rng);
                                coloring.set_color(node, color);
                                naive_coloring.set_color(node, color);
                            }
                        }

                        TestFixture::refinement_t::refine(graph, coloring);

                        {
                            smallcanon::refine::Naive<graph_t> naive{graph};
                            naive.refine(naive_coloring);
                        }

                        ASSERT_TRUE(colorings_imply_same_partition(coloring, naive_coloring, n)) << name;
                    }
                },
                var_graph);
    }
}

template<typename SM, typename SC>
std::tuple<smallcanon::AdjMatrix<SM>, smallcanon::Coloring<SC>, std::vector<smallcanon::node_t>>
permute_graph(auto&& rng, const smallcanon::AdjMatrix<SM>& graph, const smallcanon::Coloring<SC>& coloring) {
    auto mapping = graph.nodes() | std::ranges::to<std::vector<smallcanon::node_t>>();
    std::ranges::shuffle(mapping, rng);

    auto mapped_graph = graph.permuted({mapping});

    auto mapped_color = coloring.copy();
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
                        const auto n = graph.num_nodes();
                        typename TestFixture::coloring_t coloring(graph.num_nodes());

                        // partially precolor half of all instances
                        auto num_colored_nodes = std::uniform_int_distribution<smallcanon::node_t>{0, n / 2}(rng);
                        for (auto i = num_colored_nodes; i; --i) {
                            auto node = std::uniform_int_distribution<smallcanon::node_t>{0, n - 1}(rng);
                            auto color = std::uniform_int_distribution<smallcanon::node_t>{0, n - 1}(rng);
                            coloring.set_color(node, color);
                        }

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
