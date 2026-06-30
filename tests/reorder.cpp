#include <smallcanon/bitspan.hpp>
#include <smallcanon/reorder.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cassert>
#include <random>
#include <vector>

namespace {
    template<typename Graph, smallcanon::node_t NumNodes>
    struct ReorderCase {
        using graph_t = Graph;
        static constexpr smallcanon::node_t kNumNodes = NumNodes;

        static graph_t make_graph() {
            return graph_t{NumNodes};
        }
    };

    template<typename T>
    class ReorderTests : public testing::Test {};

    using ReorderTypes =
            testing::Types<ReorderCase<smallcanon::AdjMatrix8, 8>, ReorderCase<smallcanon::AdjMatrix16, 16>,
                           ReorderCase<smallcanon::AdjMatrix32, 32>, ReorderCase<smallcanon::AdjMatrix64, 64>,
                           ReorderCase<smallcanon::AdjMatrixHeap, 65>>;
    TYPED_TEST_SUITE(ReorderTests, ReorderTypes);

    template<typename Graph>
    void set_matrix_bit(Graph& graph, smallcanon::node_t u, smallcanon::node_t v) {
        smallcanon::BitSpan(graph.row(u)).set_bit(v);
    }

    template<typename Graph>
    void set_undirected_edge(Graph& graph, smallcanon::node_t u, smallcanon::node_t v) {
        assert(u != v);
        set_matrix_bit(graph, u, v);
        set_matrix_bit(graph, v, u);
    }

    template<typename Graph>
    bool get_matrix_bit(const Graph& graph, smallcanon::node_t u, smallcanon::node_t v) {
        return smallcanon::BitSpan(graph.row(u)).get_bit(v);
    }

    template<typename Graph, typename Rng>
    void fill_random_undirected_graph(Graph& graph, Rng& rng) {
        std::bernoulli_distribution is_set(0.35);
        for (const auto u: graph.nodes()) {
            for (const auto v: graph.nodes()) {
                if (u < v && is_set(rng)) {
                    set_undirected_edge(graph, u, v);
                }
            }
        }
    }

    template<typename Graph, typename Rng>
    std::vector<smallcanon::node_t> make_random_label_order(const Graph& graph, Rng& rng) {
        std::vector<smallcanon::node_t> labels(graph.num_nodes());
        std::ranges::copy(graph.nodes(), labels.begin());
        std::ranges::shuffle(labels, rng);
        return labels;
    }

    template<typename Graph>
    auto make_coloring_with_label_order(const Graph& graph, const std::vector<smallcanon::node_t>& labels) {
        typename smallcanon::MatchedColoring<Graph>::coloring_t coloring(graph.num_nodes());
        auto coloring_labels = coloring.labels();
        auto colors = coloring.colors();

        for (smallcanon::node_t pos = 0; pos < graph.num_nodes(); ++pos) {
            const auto label = labels[pos];
            coloring_labels[pos] = static_cast<typename decltype(coloring)::scolor_t>(label);
            colors[label] = static_cast<typename decltype(coloring)::scolor_t>(pos);
        }

        return coloring;
    }

    template<typename Graph>
    std::vector<smallcanon::node_t> make_nontrivial_label_order(const Graph& graph) {
        std::vector<smallcanon::node_t> labels(graph.num_nodes());
        std::ranges::copy(graph.nodes(), labels.begin());

        std::ranges::swap(labels[0], labels[3]);
        std::ranges::swap(labels[1], labels[graph.num_nodes() - 1]);
        std::ranges::swap(labels[2], labels[5]);

        return labels;
    }

    template<typename Graph>
    void expect_transpose_of(const Graph& transposed, const Graph& original) {
        ASSERT_EQ(transposed.num_nodes(), original.num_nodes());

        for (const auto u: original.nodes()) {
            for (const auto v: original.nodes()) {
                SCOPED_TRACE(testing::Message() << "u=" << u << ", v=" << v);
                EXPECT_EQ(get_matrix_bit(transposed, u, v), get_matrix_bit(original, v, u));
            }
        }
    }

    template<typename Graph>
    void expect_same_logical_matrix(const Graph& lhs, const Graph& rhs) {
        ASSERT_EQ(lhs.num_nodes(), rhs.num_nodes());

        for (const auto u: lhs.nodes()) {
            for (const auto v: lhs.nodes()) {
                SCOPED_TRACE(testing::Message() << "u=" << u << ", v=" << v);
                EXPECT_EQ(get_matrix_bit(lhs, u, v), get_matrix_bit(rhs, u, v));
            }
        }
    }

    template<typename Graph>
    void expect_symmetric_matrix(const Graph& graph) {
        for (const auto u: graph.nodes()) {
            for (const auto v: graph.nodes()) {
                SCOPED_TRACE(testing::Message() << "u=" << u << ", v=" << v);
                EXPECT_EQ(get_matrix_bit(graph, u, v), get_matrix_bit(graph, v, u));
            }
        }
    }

    template<typename Graph>
    void expect_reorder_of(const Graph& reordered, const Graph& original,
                           const std::vector<smallcanon::node_t>& labels) {
        ASSERT_EQ(reordered.num_nodes(), original.num_nodes());
        ASSERT_EQ(labels.size(), original.num_nodes());

        for (const auto u: original.nodes()) {
            for (const auto v: original.nodes()) {
                SCOPED_TRACE(testing::Message() << "u=" << u << ", v=" << v);
                EXPECT_EQ(get_matrix_bit(reordered, u, v), get_matrix_bit(original, labels[u], labels[v]));
            }
        }
    }
} // namespace

TYPED_TEST(ReorderTests, EmptyMatrixTransposesToEmptyMatrix) {
    const auto graph = TypeParam::make_graph();
    expect_symmetric_matrix(graph);

    const auto transposed = smallcanon::transpose(graph);

    expect_transpose_of(transposed, graph);
}

TYPED_TEST(ReorderTests, TransposesLogicalMatrixBits) {
    auto graph = TypeParam::make_graph();
    constexpr auto kLast = TypeParam::kNumNodes - 1;

    set_undirected_edge(graph, 0, 1);
    set_undirected_edge(graph, 0, kLast);
    set_undirected_edge(graph, 2, 5);
    set_undirected_edge(graph, 3, kLast);
    set_undirected_edge(graph, kLast - 1, kLast);

    expect_symmetric_matrix(graph);

    const auto transposed = smallcanon::transpose(graph);

    expect_transpose_of(transposed, graph);
}

TYPED_TEST(ReorderTests, DoubleTransposeRestoresLogicalMatrixBits) {
    auto graph = TypeParam::make_graph();
    constexpr auto kLast = TypeParam::kNumNodes - 1;

    set_undirected_edge(graph, 0, 2);
    set_undirected_edge(graph, 4, 1);
    set_undirected_edge(graph, kLast, 0);
    set_undirected_edge(graph, kLast - 2, kLast - 1);

    expect_symmetric_matrix(graph);

    const auto transposed = smallcanon::transpose(graph);
    const auto restored = smallcanon::transpose(transposed);

    expect_transpose_of(restored, transposed);
    expect_same_logical_matrix(restored, graph);
}

TYPED_TEST(ReorderTests, RandomMatricesTransposeAgainstBitOracle) {
    std::mt19937 rng(0x5A17C0DE);

    for (int iteration = 0; iteration < 32; ++iteration) {
        SCOPED_TRACE(testing::Message() << "iteration=" << iteration);

        auto graph = TypeParam::make_graph();
        fill_random_undirected_graph(graph, rng);
        expect_symmetric_matrix(graph);

        const auto transposed = smallcanon::transpose(graph);

        expect_transpose_of(transposed, graph);
    }
}

#if SMALLCANON_WITH_AVX512
TEST(ReorderNativeTests, TransposesU16x16Batch) {
    using namespace smallcanon::simd::avx512defs;

    auto graph = smallcanon::AdjMatrix16(16);
    set_undirected_edge(graph, 0, 1);
    set_undirected_edge(graph, 0, 15);
    set_undirected_edge(graph, 2, 5);
    set_undirected_edge(graph, 3, 15);
    set_undirected_edge(graph, 14, 15);

    expect_symmetric_matrix(graph);

    auto transposed = smallcanon::AdjMatrix16(16);
    native_to_matrix(transposed, smallcanon::transpose(matrix_to_native(graph)));

    expect_transpose_of(transposed, graph);
}

TEST(ReorderNativeTests, RandomU16x16BatchTransposesAgainstBitOracle) {
    using namespace smallcanon::simd::avx512defs;

    std::mt19937 rng(0xA512C0DE);

    for (int iteration = 0; iteration < 32; ++iteration) {
        SCOPED_TRACE(testing::Message() << "iteration=" << iteration);

        auto graph = smallcanon::AdjMatrix16(16);
        fill_random_undirected_graph(graph, rng);
        expect_symmetric_matrix(graph);

        auto transposed = smallcanon::AdjMatrix16(16);
        native_to_matrix(transposed, smallcanon::transpose(matrix_to_native(graph)));

        expect_transpose_of(transposed, graph);
    }
}
#endif

TYPED_TEST(ReorderTests, ReorderGraphWithIdentityLabelsPreservesLogicalMatrixBits) {
    auto graph = TypeParam::make_graph();
    constexpr auto kLast = TypeParam::kNumNodes - 1;

    set_undirected_edge(graph, 0, 1);
    set_undirected_edge(graph, 2, 5);
    set_undirected_edge(graph, 3, kLast);
    set_undirected_edge(graph, kLast, 0);
    set_undirected_edge(graph, kLast - 1, kLast);

    expect_symmetric_matrix(graph);

    std::vector<smallcanon::node_t> labels(graph.num_nodes());
    std::ranges::copy(graph.nodes(), labels.begin());
    auto coloring = make_coloring_with_label_order(graph, labels);

    const auto reordered = smallcanon::reorder_graph(graph, coloring);

    expect_same_logical_matrix(reordered, graph);
}

TYPED_TEST(ReorderTests, ReorderGraphUsesColoringLabelOrderForRowsAndColumns) {
    auto graph = TypeParam::make_graph();
    constexpr auto kLast = TypeParam::kNumNodes - 1;

    set_undirected_edge(graph, 0, 1);
    set_undirected_edge(graph, 3, kLast);
    set_undirected_edge(graph, 5, 2);
    set_undirected_edge(graph, kLast - 1, 5);

    expect_symmetric_matrix(graph);

    const auto labels = make_nontrivial_label_order(graph);
    auto coloring = make_coloring_with_label_order(graph, labels);

    const auto reordered = smallcanon::reorder_graph(graph, coloring);

    expect_reorder_of(reordered, graph, labels);
}

TYPED_TEST(ReorderTests, RandomMatricesReorderAgainstBitOracle) {
    std::mt19937 rng(0x7E57C0DE);

    for (int iteration = 0; iteration < 32; ++iteration) {
        SCOPED_TRACE(testing::Message() << "iteration=" << iteration);

        auto graph = TypeParam::make_graph();
        fill_random_undirected_graph(graph, rng);
        expect_symmetric_matrix(graph);

        const auto labels = make_random_label_order(graph, rng);
        auto coloring = make_coloring_with_label_order(graph, labels);

        const auto reordered = smallcanon::reorder_graph(graph, coloring);

        expect_reorder_of(reordered, graph, labels);
    }
}
