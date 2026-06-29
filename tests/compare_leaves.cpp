#include <smallcanon/compare_leaves.hpp>

#include <gtest/gtest.h>

#include <compare>
#include <random>

namespace {
    template<typename Graph, smallcanon::node_t NumNodes, smallcanon::node_t OtherNumNodes = NumNodes - 1>
    struct CompareLeavesCase {
        using graph_t = Graph;
        static constexpr smallcanon::node_t kNumNodes = NumNodes;
        static constexpr smallcanon::node_t kOtherNumNodes = OtherNumNodes;

        static graph_t make_graph(smallcanon::node_t n = NumNodes) {
            return graph_t{n};
        }
    };

    template<typename T>
    class CompareLeavesTests : public testing::Test {};

    using CompareLeavesTypes =
            testing::Types<CompareLeavesCase<smallcanon::AdjMatrix8, 8>, CompareLeavesCase<smallcanon::AdjMatrix16, 16>,
                           CompareLeavesCase<smallcanon::AdjMatrix32, 32>,
                           CompareLeavesCase<smallcanon::AdjMatrix64, 64>,
                           CompareLeavesCase<smallcanon::AdjMatrixHeap, 65, 64>>;
    TYPED_TEST_SUITE(CompareLeavesTests, CompareLeavesTypes);

    template<typename Graph>
    void toggle_edge(Graph& graph, smallcanon::node_t u, smallcanon::node_t v) {
        if (graph.has_edge(u, v)) {
            graph.remove_edge(u, v);
        } else {
            graph.add_edge(u, v);
        }
    }

    template<typename Graph, typename Rng>
    Graph make_random_graph(smallcanon::node_t n, Rng& rng) {
        Graph graph(n);
        std::bernoulli_distribution is_set(0.35);

        for (const auto u: graph.nodes()) {
            for (const auto v: graph.nodes()) {
                if (u < v && is_set(rng)) {
                    graph.add_edge(u, v);
                }
            }
        }

        return graph;
    }

    template<typename Graph, typename Rng>
    void toggle_random_edge(Graph& graph, Rng& rng) {
        std::uniform_int_distribution<smallcanon::node_t> node_dist(0, graph.num_nodes() - 1);

        auto u = node_dist(rng);
        auto v = node_dist(rng);
        while (u == v) {
            v = node_dist(rng);
        }

        toggle_edge(graph, u, v);
    }

    template<typename Left, typename Right>
    void expect_compare_equal(const Left& lhs, const Right& rhs) {
        EXPECT_EQ(smallcanon::compare::compare_leaves(lhs, rhs), std::strong_ordering::equal);
        EXPECT_EQ(smallcanon::compare::compare_leaves(rhs, lhs), std::strong_ordering::equal);
    }

    template<typename Left, typename Right>
    void expect_compare_not_equal(const Left& lhs, const Right& rhs) {
        EXPECT_NE(smallcanon::compare::compare_leaves(lhs, rhs), std::strong_ordering::equal);
        EXPECT_NE(smallcanon::compare::compare_leaves(rhs, lhs), std::strong_ordering::equal);
    }
} // namespace

TYPED_TEST(CompareLeavesTests, DeterministicEqualMatricesCompareEqual) {
    auto lhs = TypeParam::make_graph();
    lhs.add_edge(0, 1);
    lhs.add_edge(2, TypeParam::kNumNodes - 1);

    const auto rhs = lhs.copy();

    expect_compare_equal(lhs, rhs);
}

TYPED_TEST(CompareLeavesTests, DeterministicDifferentMatricesCompareNotEqual) {
    auto lhs = TypeParam::make_graph();
    lhs.add_edge(0, 1);
    lhs.add_edge(2, TypeParam::kNumNodes - 1);

    auto rhs = lhs.copy();
    toggle_edge(rhs, 1, TypeParam::kNumNodes - 1);

    expect_compare_not_equal(lhs, rhs);
}

TYPED_TEST(CompareLeavesTests, DeterministicDifferentNodeCountsCompareNotEqual) {
    const auto lhs = TypeParam::make_graph(TypeParam::kNumNodes);
    const auto rhs = TypeParam::make_graph(TypeParam::kOtherNumNodes);

    expect_compare_not_equal(lhs, rhs);
}

TYPED_TEST(CompareLeavesTests, RandomEqualMatricesCompareEqual) {
    using Graph = typename TypeParam::graph_t;
    std::mt19937 rng(0xC0FFEE);

    for (int iteration = 0; iteration < 32; ++iteration) {
        SCOPED_TRACE(testing::Message() << "iteration=" << iteration);

        const auto lhs = make_random_graph<Graph>(TypeParam::kNumNodes, rng);
        const auto rhs = lhs.copy();

        expect_compare_equal(lhs, rhs);
    }
}

TYPED_TEST(CompareLeavesTests, RandomDifferentMatricesCompareNotEqual) {
    using Graph = typename TypeParam::graph_t;
    std::mt19937 rng(0xBAD5EED);

    for (int iteration = 0; iteration < 32; ++iteration) {
        SCOPED_TRACE(testing::Message() << "iteration=" << iteration);

        auto lhs = make_random_graph<Graph>(TypeParam::kNumNodes, rng);
        auto rhs = lhs.copy();
        toggle_random_edge(rhs, rng);

        expect_compare_not_equal(lhs, rhs);
    }
}

TYPED_TEST(CompareLeavesTests, RandomDifferentNodeCountsCompareNotEqual) {
    using Graph = typename TypeParam::graph_t;
    std::mt19937 rng(0x51A7E);
    std::uniform_int_distribution<smallcanon::node_t> node_count_dist(2, TypeParam::kNumNodes);

    for (int iteration = 0; iteration < 32; ++iteration) {
        SCOPED_TRACE(testing::Message() << "iteration=" << iteration);

        const auto lhs_nodes = node_count_dist(rng);
        auto rhs_nodes = node_count_dist(rng);
        while (rhs_nodes == lhs_nodes) {
            rhs_nodes = node_count_dist(rng);
        }

        const auto lhs = make_random_graph<Graph>(lhs_nodes, rng);
        const auto rhs = make_random_graph<Graph>(rhs_nodes, rng);

        expect_compare_not_equal(lhs, rhs);
    }
}
