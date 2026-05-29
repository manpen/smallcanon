#include <smallcanon/adj_matrix.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <span>
#include <type_traits>

namespace {
    template<typename Storage, smallcanon::node_t ExpectedCapacity>
    struct FixedStorageCase {
        using storage_t = Storage;
        static constexpr smallcanon::node_t expected_capacity = ExpectedCapacity;
    };

    template<typename T>
    class FixedStorageTests : public testing::Test {};

    using FixedStorageTypes = testing::Types<FixedStorageCase<smallcanon::details::FixedStorage8, 8>,
                                             FixedStorageCase<smallcanon::details::FixedStorage16, 16>,
                                             FixedStorageCase<smallcanon::details::FixedStorage32, 32>,
                                             FixedStorageCase<smallcanon::details::FixedStorage64, 64>,
                                             FixedStorageCase<smallcanon::details::FixedStorage128, 128>>;
    TYPED_TEST_SUITE(FixedStorageTests, FixedStorageTypes);
} // namespace

TYPED_TEST(FixedStorageTests, IsDefaultConstructible) {
    using Storage = typename TypeParam::storage_t;

    static_assert(std::is_default_constructible_v<Storage>);
    [[maybe_unused]] Storage storage;
}

TYPED_TEST(FixedStorageTests, ExposesWordProperties) {
    using Storage = typename TypeParam::storage_t;

    static_assert(std::is_unsigned_v<typename Storage::word_t>);
    EXPECT_EQ(Storage::BITS_PER_WORD, sizeof(typename Storage::word_t) * 8);
}

TYPED_TEST(FixedStorageTests, ReportsExpectedRowCapacity) {
    typename TypeParam::storage_t storage;

    EXPECT_EQ(storage.row_capacity(), TypeParam::expected_capacity);
}

TYPED_TEST(FixedStorageTests, BufferCoversTheWholeMatrix) {
    using Storage = typename TypeParam::storage_t;
    Storage storage;

    const std::span<typename Storage::word_t> buffer = storage.buffer();
    const auto expected_words = TypeParam::expected_capacity * TypeParam::expected_capacity / Storage::BITS_PER_WORD;

    EXPECT_EQ(buffer.size(), expected_words);
}

TYPED_TEST(FixedStorageTests, BufferIsZeroInitialized) {
    typename TypeParam::storage_t storage;

    EXPECT_TRUE(std::ranges::all_of(storage.buffer(), [](auto word) { return word == 0; }));
}

TYPED_TEST(FixedStorageTests, RowCoversOneMatrixRow) {
    using Storage = typename TypeParam::storage_t;
    Storage storage;

    const std::span<typename Storage::word_t> row = storage.row(0);
    const auto expected_words_per_row = TypeParam::expected_capacity / Storage::BITS_PER_WORD;

    EXPECT_EQ(row.size(), expected_words_per_row);
}

TYPED_TEST(FixedStorageTests, RowsReferenceConsecutivePartsOfBuffer) {
    using Storage = typename TypeParam::storage_t;
    Storage storage;

    const auto words_per_row = TypeParam::expected_capacity / Storage::BITS_PER_WORD;
    const std::span<typename Storage::word_t> buffer = storage.buffer();

    for (smallcanon::node_t i = 0; i < TypeParam::expected_capacity; ++i) {
        const std::span<typename Storage::word_t> row = storage.row(i);

        EXPECT_EQ(row.data(), buffer.data() + static_cast<std::ptrdiff_t>(i * words_per_row));
        EXPECT_EQ(row.size(), words_per_row);
    }
}

TYPED_TEST(FixedStorageTests, RowWritesAreVisibleThroughBuffer) {
    using Storage = typename TypeParam::storage_t;
    Storage storage;

    const auto words_per_row = TypeParam::expected_capacity / Storage::BITS_PER_WORD;
    std::span<typename Storage::word_t> row = storage.row(TypeParam::expected_capacity - 1);
    row[0] = static_cast<typename Storage::word_t>(0b1010);

    const std::span<typename Storage::word_t> buffer = storage.buffer();
    const auto row_offset = (TypeParam::expected_capacity - 1) * words_per_row;

    EXPECT_EQ(buffer[row_offset], static_cast<typename Storage::word_t>(0b1010));
}
