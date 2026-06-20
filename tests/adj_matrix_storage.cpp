#include <smallcanon/adj_matrix.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
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

TEST(HeapStorageTests, IsConstructibleFromCapacityButNotDefaultConstructible) {
    using Storage = smallcanon::details::HeapStorage;

    static_assert(!std::is_default_constructible_v<Storage>);
    static_assert(std::is_constructible_v<Storage, smallcanon::node_t>);

    [[maybe_unused]] Storage storage(32);
}

TEST(HeapStorageTests, ExposesWordProperties) {
    using Storage = smallcanon::details::HeapStorage;

    static_assert(std::is_unsigned_v<Storage::word_t>);
    EXPECT_EQ(Storage::BITS_PER_WORD, sizeof(Storage::word_t) * 8);
    EXPECT_EQ(Storage::CAPACITY_OF_SMALLEST_GRAPH, Storage::BITS_PER_WORD);
}

TEST(HeapStorageTests, RoundsRowCapacityUpToAtLeastSmallestGraphCapacity) {
    using Storage = smallcanon::details::HeapStorage;

    const std::array<std::pair<smallcanon::node_t, smallcanon::node_t>, 6> cases{{
            {1, 32},
            {31, 32},
            {32, 32},
            {33, 64},
            {64, 64},
            {65, 128},
    }};

    for (const auto& [requested_capacity, expected_capacity]: cases) {
        const Storage storage(requested_capacity);

        EXPECT_EQ(storage.row_capacity(), expected_capacity);
    }
}

TEST(HeapStorageTests, BufferCoversTheWholeMatrix) {
    using Storage = smallcanon::details::HeapStorage;
    Storage storage(33);

    const std::span<Storage::word_t> buffer = storage.buffer();
    const auto expected_words = storage.row_capacity() * storage.row_capacity() / Storage::BITS_PER_WORD;

    EXPECT_EQ(buffer.size(), expected_words);
}

TEST(HeapStorageTests, BufferIsZeroInitialized) {
    smallcanon::details::HeapStorage storage(33);

    EXPECT_TRUE(std::ranges::all_of(storage.buffer(), [](auto word) { return word == 0; }));
}

TEST(HeapStorageTests, RowCoversOneMatrixRow) {
    using Storage = smallcanon::details::HeapStorage;
    Storage storage(33);

    const std::span<Storage::word_t> row = storage.row(0);
    const auto expected_words_per_row = storage.row_capacity() / Storage::BITS_PER_WORD;

    EXPECT_EQ(row.size(), expected_words_per_row);
}

TEST(HeapStorageTests, RowsReferenceConsecutivePartsOfBuffer) {
    using Storage = smallcanon::details::HeapStorage;
    Storage storage(33);

    const auto words_per_row = storage.row_capacity() / Storage::BITS_PER_WORD;
    const std::span<Storage::word_t> buffer = storage.buffer();

    for (smallcanon::node_t i = 0; i < storage.row_capacity(); ++i) {
        const std::span<Storage::word_t> row = storage.row(i);

        EXPECT_EQ(row.data(), buffer.data() + static_cast<std::ptrdiff_t>(i * words_per_row));
        EXPECT_EQ(row.size(), words_per_row);
    }
}

TEST(HeapStorageTests, RowWritesAreVisibleThroughBuffer) {
    using Storage = smallcanon::details::HeapStorage;
    Storage storage(33);

    const auto words_per_row = storage.row_capacity() / Storage::BITS_PER_WORD;
    std::span<Storage::word_t> row = storage.row(storage.row_capacity() - 1);
    row[0] = static_cast<Storage::word_t>(0b1010);

    const std::span<Storage::word_t> buffer = storage.buffer();
    const auto row_offset = (storage.row_capacity() - 1) * words_per_row;

    EXPECT_EQ(buffer[row_offset], static_cast<Storage::word_t>(0b1010));
}

TEST(HeapStorageTests, ConstBufferAndRowExposeStorage) {
    using Storage = smallcanon::details::HeapStorage;
    Storage storage(33);
    storage.row(1)[0] = static_cast<Storage::word_t>(0b1001);

    const auto& const_storage = storage;
    const std::span<const Storage::word_t> buffer = const_storage.buffer();
    const std::span<const Storage::word_t> row = const_storage.row(1);

    EXPECT_EQ(row.data(), buffer.data() + static_cast<std::ptrdiff_t>(storage.row_capacity() / Storage::BITS_PER_WORD));
    EXPECT_EQ(row[0], static_cast<Storage::word_t>(0b1001));
}
