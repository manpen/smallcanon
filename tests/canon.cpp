#include <filesystem>
#include <fstream>
#include <random>
#include <smallcanon/ir.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "smallcanon/parser.hpp"

namespace {
    template<typename Graph, typename Refine>
    struct CanonCase {
        using graph_t = Graph;
        using coloring_t = typename smallcanon::MatchedColoring<graph_t>::coloring_t;
        using refine_t = Refine;

        static graph_t make_graph(smallcanon::node_t n, const std::vector<smallcanon::edge_t>& edges = {}) {
            graph_t graph(n);
            graph.add_edges(edges);
            return graph;
        }

        static auto make_solver(const graph_t& graph) {
            return smallcanon::solver::Solver<graph_t, refine_t>(graph);
        }
    };

    template<typename T>
    class CanonTests : public testing::Test {
    protected:
        using graph_t = typename T::graph_t;
        using coloring_t = typename T::coloring_t;
    };

    using CanonTypes = testing::Types<
#if XSIMD_WITH_AVX512F
            CanonCase<smallcanon::AdjMatrix8, smallcanon::refine::avx512intrin::AVX512<smallcanon::AdjMatrix8>>,
#endif
            CanonCase<smallcanon::AdjMatrix8, smallcanon::refine::Naive<smallcanon::AdjMatrix8>>,
            CanonCase<smallcanon::AdjMatrix16, smallcanon::refine::Naive<smallcanon::AdjMatrix16>>,
            CanonCase<smallcanon::AdjMatrixHeap, smallcanon::refine::Naive<smallcanon::AdjMatrixHeap>>>;
    TYPED_TEST_SUITE(CanonTests, CanonTypes);

    template<typename SC>
    void expect_discrete_coloring(const smallcanon::Coloring<SC>& coloring, smallcanon::node_t n) {
        std::vector<bool> seen(n, false);
        for (smallcanon::node_t u = 0; u < n; ++u) {
            const auto color = coloring.get_color(u);
            ASSERT_LT(color, n) << "node " << u;
            EXPECT_FALSE(seen[color]) << "duplicate color " << color;
            seen[color] = true;
        }
    }

    template<typename SM, typename SC>
    std::string canonical_adjacency(const smallcanon::AdjMatrix<SM>& graph, const smallcanon::Coloring<SC>& coloring) {
        const auto n = graph.num_nodes();
        std::vector<smallcanon::node_t> node_of_color(n);
        for (const auto u: graph.nodes()) {
            node_of_color[coloring.get_color(u)] = u;
        }

        std::string bits;
        bits.reserve(static_cast<std::size_t>(n * (n - 1) / 2));
        for (smallcanon::color_t cu = 0; cu < n; ++cu) {
            const auto u = node_of_color[cu];
            for (smallcanon::color_t cv = 0; cv < cu; ++cv) {
                const auto v = node_of_color[cv];
                bits.push_back(graph.has_edge(u, v) ? '1' : '0');
            }
        }
        return bits;
    }
} // namespace

TYPED_TEST(CanonTests, AlreadyDiscreteColoringReturnsDiscreteLeafWithoutSearch) {
    using Coloring = typename TestFixture::coloring_t;

    const auto graph = TypeParam::make_graph(4);
    Coloring coloring(graph.num_nodes());
    for (const auto u: graph.nodes()) {
        coloring.set_color(u, u);
    }

    auto solver = smallcanon::solver::Solver(graph);
    const auto leaf = solver.canonize(coloring);

    expect_discrete_coloring(leaf, graph.num_nodes());
    EXPECT_EQ(solver.get_stats().ir_nodes_visited, 0);
    EXPECT_EQ(solver.get_stats().best_leaf_update, 0);
    EXPECT_EQ(solver.get_stats().automorphisms, 0);
    EXPECT_EQ(solver.get_stats().group_size, 1);
    for (const auto u: graph.nodes()) {
        EXPECT_EQ(leaf.get_color(u), u);
    }
}

TYPED_TEST(CanonTests, CanonicalAdjacencyIsInvariantUnderNodePermutation) {
    using coloring_t = typename TestFixture::coloring_t;

    const auto graph = TypeParam::make_graph(6, {{0, 1}, {0, 2}, {1, 2}, {1, 3}, {2, 4}, {4, 5}});
    const std::vector<smallcanon::node_t> new_id_of{2, 5, 1, 4, 0, 3};
    const auto mapped_graph = graph.permuted(new_id_of);

    coloring_t coloring(graph.num_nodes());

    auto solver = TypeParam::make_solver(graph);
    const auto leaf = solver.canonize(coloring);

    coloring_t mapped_coloring(mapped_graph.num_nodes());

    auto mapped_solver = smallcanon::solver::Solver(mapped_graph);
    const auto mapped_leaf = mapped_solver.canonize(mapped_coloring);

    expect_discrete_coloring(leaf, graph.num_nodes());
    expect_discrete_coloring(mapped_leaf, mapped_graph.num_nodes());
    EXPECT_EQ(canonical_adjacency(graph, leaf), canonical_adjacency(mapped_graph, mapped_leaf));
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

TYPED_TEST(CanonTests, InvarianceNodePermutation) {
    using graph_t = typename TestFixture::graph_t;
    using coloring_t = typename TestFixture::coloring_t;

    std::mt19937_64 rng(123456);

    for (auto&& ds: {"curated.g6", "all_n8.g6", "all_m11.g6"}) {
        const auto dataset_path = std::filesystem::path(SMALLCANON_PROJECT_ROOT) / "datasets" / ds;
        std::ifstream curated(dataset_path);

        for (auto [name, var_graph]: smallcanon::read_graph_dataset(curated)) {
            std::visit(
                    [&](auto& graph) {
                        if constexpr (std::is_same_v<std::decay_t<decltype(graph)>, graph_t>) {
                            const auto n = graph.num_nodes();

                            if (n < 2 || std::uniform_real_distribution{0., 1.}(rng) > 0.25) {
                                // speedup test: skip ~3 out 4 instances
                                return;
                            }

                            coloring_t coloring(n);

                            if (std::uniform_real_distribution{0., 1.}(rng) > 0.5) {
                                // partially precolor half of all instances
                                auto num_colored_nodes =
                                        std::uniform_int_distribution<smallcanon::node_t>{0, n / 2}(rng);
                                for (auto i = num_colored_nodes; i; --i) {
                                    auto node = std::uniform_int_distribution<smallcanon::node_t>{0, n - 1}(rng);
                                    auto color = std::uniform_int_distribution<smallcanon::node_t>{0, n - 1}(rng);
                                    coloring.set_color(node, color);
                                }
                            }

                            auto [shuffled_graph, shuffled_coloring, mapping] = permute_graph(rng, graph, coloring);
                            (void) mapping;

                            auto solver = TypeParam::make_solver(graph);
                            const auto canon = solver.canonize(coloring);

                            auto solver_shuffled = TypeParam::make_solver(shuffled_graph);
                            const auto canon_shuffled = solver_shuffled.canonize(shuffled_coloring);

                            ASSERT_EQ(canonical_adjacency(graph, canon),
                                      canonical_adjacency(shuffled_graph, canon_shuffled))
                                    << name;
                        }
                    },
                    var_graph);
        }
    }
}
