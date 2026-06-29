#include <smallcanon/adj_matrix.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <compare>
#include <span>
#include <vector>

namespace {
    template<typename Storage, smallcanon::node_t NumNodes>
    struct AdjMatrixCase {
        using storage_t = Storage;
        using matrix_t = smallcanon::AdjMatrix<storage_t>;
        static constexpr smallcanon::node_t kNumNodes = NumNodes;

        static matrix_t make_matrix() {
            return matrix_t{NumNodes};
        }
    };

    template<typename T>
    class AdjMatrixTests : public testing::Test {};

    using AdjMatrixTypes = testing::Types<AdjMatrixCase<smallcanon::details::FixedStorage8, 8>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage16, 16>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage32, 32>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage64, 64>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage128, 128>,
                                          AdjMatrixCase<smallcanon::details::HeapStorage, 256>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage8, 7>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage16, 15>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage32, 31>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage64, 63>,
                                          AdjMatrixCase<smallcanon::details::FixedStorage128, 127>,
                                          AdjMatrixCase<smallcanon::details::HeapStorage, 255>>;
    TYPED_TEST_SUITE(AdjMatrixTests, AdjMatrixTypes);

    template<typename Matrix>
    std::vector<smallcanon::node_t> collect_neighbors_of(const Matrix& matrix, smallcanon::node_t u) {
        std::vector<smallcanon::node_t> neighbors;
        for (const auto v: matrix.neighbors_of(u)) {
            neighbors.push_back(v);
        }
        return neighbors;
    }

    template<typename Matrix>
    std::vector<smallcanon::edge_t> collect_edges(const Matrix& matrix) {
        std::vector<smallcanon::edge_t> edges;
        for (const auto& edge: matrix.edges()) {
            edges.push_back(edge);
        }
        return edges;
    }
} // namespace

TYPED_TEST(AdjMatrixTests, ExposesWholeMatrixBuffer) {
    using Matrix = typename TypeParam::matrix_t;
    auto matrix = TypeParam::make_matrix();

    const std::span<typename Matrix::word_t> buffer = matrix.buffer();
    const auto expected_words = TypeParam::kNumNodes * TypeParam::kNumNodes / Matrix::kBitsPerWord;

    EXPECT_GE(buffer.size(), expected_words);
    EXPECT_TRUE(std::ranges::all_of(buffer, [](auto word) { return word == 0; }));
}

TYPED_TEST(AdjMatrixTests, ExposesWholeMatrixBufferConst) {
    using Matrix = typename TypeParam::matrix_t;
    const auto matrix = TypeParam::make_matrix();

    const std::span<const typename Matrix::word_t> buffer = matrix.buffer();
    const auto expected_words = TypeParam::kNumNodes * TypeParam::kNumNodes / Matrix::kBitsPerWord;

    EXPECT_GE(buffer.size(), expected_words);
    EXPECT_TRUE(std::ranges::all_of(buffer, [](auto word) { return word == 0; }));
}

TYPED_TEST(AdjMatrixTests, ExposesRows) {
    using Matrix = typename TypeParam::matrix_t;
    auto matrix = TypeParam::make_matrix();

    const std::span<typename Matrix::word_t> row = matrix.row(0);
    const auto expected_words_per_row = TypeParam::kNumNodes / Matrix::kBitsPerWord;

    EXPECT_GE(row.size(), expected_words_per_row);
}

TYPED_TEST(AdjMatrixTests, ExposesRowsConst) {
    using Matrix = typename TypeParam::matrix_t;
    auto matrix = TypeParam::make_matrix();

    const std::span<const typename Matrix::word_t> row = matrix.row(0);
    const auto expected_words_per_row = TypeParam::kNumNodes / Matrix::kBitsPerWord;

    EXPECT_GE(row.size(), expected_words_per_row);
}

TYPED_TEST(AdjMatrixTests, ReportsCapacity) {
    const auto matrix = TypeParam::make_matrix();
    EXPECT_GE(matrix.capacity(), TypeParam::kNumNodes);
}

TYPED_TEST(AdjMatrixTests, ReportsNumNodes) {
    const auto matrix = TypeParam::make_matrix();
    EXPECT_GE(matrix.num_nodes(), TypeParam::kNumNodes);
}

TYPED_TEST(AdjMatrixTests, ComparesEqualBuffersAsEqual) {
    const auto lhs = TypeParam::make_matrix();
    const auto rhs = TypeParam::make_matrix();

    EXPECT_EQ(lhs <=> rhs, std::strong_ordering::equal);
    EXPECT_EQ(lhs, rhs);
    EXPECT_LE(lhs, rhs);
    EXPECT_GE(lhs, rhs);
}

TYPED_TEST(AdjMatrixTests, ComparesBuffersLexicographically) {
    auto lhs = TypeParam::make_matrix();
    auto rhs = TypeParam::make_matrix();

    lhs.buffer()[0] = static_cast<typename TypeParam::matrix_t::word_t>(1);
    rhs.buffer()[0] = static_cast<typename TypeParam::matrix_t::word_t>(2);

    EXPECT_EQ(lhs <=> rhs, std::strong_ordering::less);
    EXPECT_LT(lhs, rhs);
    EXPECT_GT(rhs, lhs);
    EXPECT_NE(lhs, rhs);
}

TYPED_TEST(AdjMatrixTests, ComparesWholeBufferLexicographically) {
    auto lhs = TypeParam::make_matrix();
    auto rhs = TypeParam::make_matrix();

    lhs.buffer()[0] = static_cast<typename TypeParam::matrix_t::word_t>(1);
    rhs.buffer()[0] = static_cast<typename TypeParam::matrix_t::word_t>(1);
    lhs.buffer().back() = static_cast<typename TypeParam::matrix_t::word_t>(1);
    rhs.buffer().back() = static_cast<typename TypeParam::matrix_t::word_t>(2);

    EXPECT_EQ(lhs <=> rhs, std::strong_ordering::less);
    EXPECT_LT(lhs, rhs);
}

TYPED_TEST(AdjMatrixTests, IterateNodes) {
    const auto matrix = TypeParam::make_matrix();

    smallcanon::node_t expect = 0;
    for (auto u: matrix.nodes()) {
        EXPECT_EQ(u, expect++);
    }

    EXPECT_EQ(expect, matrix.num_nodes());
}

TYPED_TEST(AdjMatrixTests, NewMatrixHasNoEdges) {
    const auto matrix = TypeParam::make_matrix();

    EXPECT_FALSE(matrix.has_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(1, 0));
    EXPECT_FALSE(matrix.has_edge(TypeParam::kNumNodes - 2, TypeParam::kNumNodes - 1));
    EXPECT_FALSE(matrix.has_edge(TypeParam::kNumNodes - 1, TypeParam::kNumNodes - 2));
}

TYPED_TEST(AdjMatrixTests, AddEdgeSetsBothDirectionsAndReturnsPreviousState) {
    auto matrix = TypeParam::make_matrix();

    EXPECT_FALSE(matrix.add_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(1, 0));

    EXPECT_TRUE(matrix.add_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(1, 0));
}

TYPED_TEST(AdjMatrixTests, AddEdgeWorksAtHighestValidNodeIndex) {
    auto matrix = TypeParam::make_matrix();

    constexpr auto kU = TypeParam::kNumNodes - 2;
    constexpr auto kV = TypeParam::kNumNodes - 1;

    EXPECT_FALSE(matrix.add_edge(kU, kV));

    EXPECT_TRUE(matrix.has_edge(kU, kV));
    EXPECT_TRUE(matrix.has_edge(kV, kU));
}

TYPED_TEST(AdjMatrixTests, AddEdgesAddsNewEdgesAndReportsNewEdgeCount) {
    auto matrix = TypeParam::make_matrix();
    std::vector<smallcanon::edge_t> edges{{0, 1}, {0, 2}, {0, 1}, {1, 2}};

    EXPECT_EQ(matrix.add_edges(edges), 3);

    EXPECT_TRUE(matrix.has_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(1, 0));
    EXPECT_TRUE(matrix.has_edge(0, 2));
    EXPECT_TRUE(matrix.has_edge(2, 0));
    EXPECT_TRUE(matrix.has_edge(1, 2));
    EXPECT_TRUE(matrix.has_edge(2, 1));
}

TYPED_TEST(AdjMatrixTests, AddEdgesOnlyCountsEdgesNotAlreadyPresent) {
    auto matrix = TypeParam::make_matrix();

    ASSERT_FALSE(matrix.add_edge(0, 1));

    std::vector<smallcanon::edge_t> edges{{0, 1}, {1, 2}};

    EXPECT_EQ(matrix.add_edges(edges), 1);

    EXPECT_TRUE(matrix.has_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(1, 2));
}

TYPED_TEST(AdjMatrixTests, RemoveEdgeClearsBothDirectionsAndReturnsPreviousState) {
    auto matrix = TypeParam::make_matrix();

    ASSERT_FALSE(matrix.add_edge(0, 1));

    EXPECT_TRUE(matrix.remove_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(1, 0));

    EXPECT_FALSE(matrix.remove_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(1, 0));
}

TYPED_TEST(AdjMatrixTests, RemoveEdgesRemovesPresentEdgesAndReportsRemovedEdgeCount) {
    auto matrix = TypeParam::make_matrix();

    ASSERT_FALSE(matrix.add_edge(0, 1));
    ASSERT_FALSE(matrix.add_edge(0, 2));
    ASSERT_FALSE(matrix.add_edge(1, 2));

    std::vector<smallcanon::edge_t> edges{{0, 1}, {1, 2}, {1, 2}, {0, 1}};

    EXPECT_EQ(matrix.remove_edges(edges), 2);

    EXPECT_FALSE(matrix.has_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(1, 0));
    EXPECT_TRUE(matrix.has_edge(0, 2));
    EXPECT_FALSE(matrix.has_edge(1, 2));
    EXPECT_FALSE(matrix.has_edge(2, 1));
}

TYPED_TEST(AdjMatrixTests, RemoveEdgesOnlyCountsEdgesPresentAtRemovalTime) {
    auto matrix = TypeParam::make_matrix();

    ASSERT_FALSE(matrix.add_edge(0, 1));

    std::vector<smallcanon::edge_t> edges{{1, 2}, {0, 1}, {0, 1}};

    EXPECT_EQ(matrix.remove_edges(edges), 1);

    EXPECT_FALSE(matrix.has_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(1, 0));
}

TYPED_TEST(AdjMatrixTests, CountDegreeCountsSetBitsInRow) {
    auto matrix = TypeParam::make_matrix();

    EXPECT_EQ(matrix.count_degree(0), 0);

    matrix.add_edge(0, 1);
    matrix.add_edge(0, 2);

    EXPECT_EQ(matrix.count_degree(0), 2);
    EXPECT_EQ(matrix.count_degree(1), 1);
    EXPECT_EQ(matrix.count_degree(2), 1);
}

TYPED_TEST(AdjMatrixTests, ConstMatrixCanReadEdgesAndDegree) {
    auto matrix = TypeParam::make_matrix();

    matrix.add_edge(0, 1);

    const auto& const_matrix = matrix;

    EXPECT_TRUE(const_matrix.has_edge(0, 1));
    EXPECT_TRUE(const_matrix.has_edge(1, 0));
    EXPECT_EQ(const_matrix.count_degree(0), 1);
}

TYPED_TEST(AdjMatrixTests, NeighborsOfReturnsNoNodesForIsolatedNode) {
    auto matrix = TypeParam::make_matrix();

    EXPECT_TRUE(collect_neighbors_of(matrix, 0).empty());
}

TYPED_TEST(AdjMatrixTests, NeighborsOfReturnsAdjacentNodesInAscendingOrder) {
    auto matrix = TypeParam::make_matrix();
    constexpr auto kLast = TypeParam::kNumNodes - 1;

    matrix.add_edge(0, kLast);
    matrix.add_edge(0, 2);
    matrix.add_edge(0, 1);

    EXPECT_EQ(collect_neighbors_of(matrix, 0), (std::vector<smallcanon::node_t>{1, 2, kLast}));
}

TYPED_TEST(AdjMatrixTests, NeighborsOfReturnsAdjacentNodesForNonzeroNode) {
    auto matrix = TypeParam::make_matrix();

    matrix.add_edge(2, 0);
    matrix.add_edge(2, 1);

    EXPECT_EQ(collect_neighbors_of(matrix, 2), (std::vector<smallcanon::node_t>{0, 1}));
}

TYPED_TEST(AdjMatrixTests, EdgesReturnsNoEdgesForEmptyMatrix) {
    auto matrix = TypeParam::make_matrix();

    EXPECT_TRUE(collect_edges(matrix).empty());
}

TYPED_TEST(AdjMatrixTests, EdgesReturnsEachUndirectedEdgeOnce) {
    auto matrix = TypeParam::make_matrix();
    constexpr auto kLast = TypeParam::kNumNodes - 1;

    matrix.add_edge(0, 1);
    matrix.add_edge(0, 2);
    matrix.add_edge(kLast - 1, kLast);

    EXPECT_EQ(collect_edges(matrix), (std::vector<smallcanon::edge_t>{{1, 0}, {2, 0}, {kLast, kLast - 1}}));
}

TYPED_TEST(AdjMatrixTests, EdgesReturnsEdgesForNonzeroSource) {
    auto matrix = TypeParam::make_matrix();

    matrix.add_edge(1, 2);
    matrix.add_edge(0, 2);

    EXPECT_EQ(collect_edges(matrix), (std::vector<smallcanon::edge_t>{{2, 0}, {2, 1}}));
}

TYPED_TEST(AdjMatrixTests, PermutedMapsEdgesToNewNodeIds) {
    auto matrix = TypeParam::make_matrix();

    matrix.add_edge(0, 1);
    matrix.add_edge(1, 3);
    matrix.add_edge(2, 4);
    matrix.add_edge(0, 4);

    auto new_id_of = matrix.nodes() | std::ranges::to<std::vector<smallcanon::node_t>>();
    new_id_of[0] = 3;
    new_id_of[1] = 0;
    new_id_of[2] = 5;
    new_id_of[3] = 2;
    new_id_of[4] = 1;
    new_id_of[5] = 4;

    const auto mapped = matrix.permuted(new_id_of);

    EXPECT_EQ(mapped.num_nodes(), matrix.num_nodes());
    EXPECT_EQ(collect_edges(mapped), (std::vector<smallcanon::edge_t>{{2, 0}, {3, 0}, {3, 1}, {5, 1}}));
}
