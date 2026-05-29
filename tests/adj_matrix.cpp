#include <smallcanon/adj_matrix.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <span>
#include <type_traits>

namespace {
    template<typename Matrix, smallcanon::node_t ExpectedCapacity>
    struct AdjMatrixCase {
        using matrix_t = Matrix;
        static constexpr smallcanon::node_t expected_capacity = ExpectedCapacity;
    };

    template<typename T>
    class FixedAdjMatrixTests : public testing::Test {};

    using FixedAdjMatrixTypes =
            testing::Types<AdjMatrixCase<smallcanon::AdjMatrix8, 8>, AdjMatrixCase<smallcanon::AdjMatrix16, 16>,
                           AdjMatrixCase<smallcanon::AdjMatrix32, 32>, AdjMatrixCase<smallcanon::AdjMatrix64, 64>,
                           AdjMatrixCase<smallcanon::AdjMatrix128, 128>>;
    TYPED_TEST_SUITE(FixedAdjMatrixTests, FixedAdjMatrixTypes);
} // namespace

TYPED_TEST(FixedAdjMatrixTests, IsDefaultConstructible) {
    using Matrix = typename TypeParam::matrix_t;
    static_assert(std::is_default_constructible_v<Matrix>);
    [[maybe_unused]] Matrix matrix;
}

TYPED_TEST(FixedAdjMatrixTests, ExposesWholeMatrixBuffer) {
    using Matrix = typename TypeParam::matrix_t;
    Matrix matrix;

    const std::span<typename Matrix::word_t> buffer = matrix.buffer();
    const auto expected_words = TypeParam::expected_capacity * TypeParam::expected_capacity / Matrix::BITS_PER_WORD;

    EXPECT_EQ(buffer.size(), expected_words);
    EXPECT_TRUE(std::ranges::all_of(buffer, [](auto word) { return word == 0; }));
}

TYPED_TEST(FixedAdjMatrixTests, ExposesRows) {
    using Matrix = typename TypeParam::matrix_t;
    Matrix matrix;

    const std::span<typename Matrix::word_t> row = matrix.row(0);
    const auto expected_words_per_row = TypeParam::expected_capacity / Matrix::BITS_PER_WORD;

    EXPECT_EQ(row.size(), expected_words_per_row);
}

TYPED_TEST(FixedAdjMatrixTests, NewMatrixHasNoEdges) {
    typename TypeParam::matrix_t matrix;

    EXPECT_FALSE(matrix.has_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(1, 0));
    EXPECT_FALSE(matrix.has_edge(TypeParam::expected_capacity - 2, TypeParam::expected_capacity - 1));
    EXPECT_FALSE(matrix.has_edge(TypeParam::expected_capacity - 1, TypeParam::expected_capacity - 2));
}

TYPED_TEST(FixedAdjMatrixTests, AddEdgeSetsBothDirectionsAndReturnsPreviousState) {
    typename TypeParam::matrix_t matrix;

    EXPECT_FALSE(matrix.add_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(1, 0));

    EXPECT_TRUE(matrix.add_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(0, 1));
    EXPECT_TRUE(matrix.has_edge(1, 0));
}

TYPED_TEST(FixedAdjMatrixTests, AddEdgeWorksAtHighestValidNodeIndex) {
    typename TypeParam::matrix_t matrix;
    constexpr auto u = TypeParam::expected_capacity - 2;
    constexpr auto v = TypeParam::expected_capacity - 1;

    EXPECT_FALSE(matrix.add_edge(u, v));

    EXPECT_TRUE(matrix.has_edge(u, v));
    EXPECT_TRUE(matrix.has_edge(v, u));
}

TYPED_TEST(FixedAdjMatrixTests, RemoveEdgeClearsBothDirectionsAndReturnsPreviousState) {
    typename TypeParam::matrix_t matrix;

    ASSERT_FALSE(matrix.add_edge(0, 1));

    EXPECT_TRUE(matrix.remove_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(1, 0));

    EXPECT_FALSE(matrix.remove_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(0, 1));
    EXPECT_FALSE(matrix.has_edge(1, 0));
}

TYPED_TEST(FixedAdjMatrixTests, SupportsSelfLoops) {
    typename TypeParam::matrix_t matrix;

    EXPECT_FALSE(matrix.add_edge(2, 2));
    EXPECT_TRUE(matrix.has_edge(2, 2));

    EXPECT_TRUE(matrix.remove_edge(2, 2));
    EXPECT_FALSE(matrix.has_edge(2, 2));
}

TYPED_TEST(FixedAdjMatrixTests, CountDegreeCountsSetBitsInRow) {
    typename TypeParam::matrix_t matrix;

    EXPECT_EQ(matrix.count_degree(0), 0);

    matrix.add_edge(0, 1);
    matrix.add_edge(0, 2);

    EXPECT_EQ(matrix.count_degree(0), 2);
    EXPECT_EQ(matrix.count_degree(1), 1);
    EXPECT_EQ(matrix.count_degree(2), 1);
}

TYPED_TEST(FixedAdjMatrixTests, ConstMatrixCanReadEdgesAndDegree) {
    typename TypeParam::matrix_t matrix;
    matrix.add_edge(0, 1);

    const auto& const_matrix = matrix;

    EXPECT_TRUE(const_matrix.has_edge(0, 1));
    EXPECT_TRUE(const_matrix.has_edge(1, 0));
    EXPECT_EQ(const_matrix.count_degree(0), 1);
}
