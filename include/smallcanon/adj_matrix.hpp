#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
#include <span>

#include <smallcanon/bitspan.hpp>

namespace smallcanon {
    using node_t = uint32_t;
    using edgeid_t = uint32_t;
    static_assert(sizeof(edgeid_t) >= sizeof(node_t)); // technically, we want node_t to fit twice into edgeid; but that
                                                       // seems wasteful for small graphs


    /// AdjMatrix implements an adjacency matrix based on an external storage type.
    /// We are generic over the storage type to allow both stack- and heap-based allocations.
    /// The assumption that stack-based allocation is only used for small graphs, where copying the matrix is cheap.
    /// The storage type chooses the `word_t`, which is the unit of bit-parallelism during scalar execution.
    /// The row_capacity (i.e. the number of bits per row) needs to be ..
    ///  i)  a power of two,
    ///  ii) a multiple of the storage type.
    template<typename Storage>
    class AdjMatrix {
    public:
        using storage_t = Storage;
        using word_t = typename Storage::word_t;
        static constexpr size_t BITS_PER_WORD = Storage::BITS_PER_WORD;
        static constexpr bool LOOPS_ALLOWED = true;

    private:
        Storage storage{};

    public:
        constexpr AdjMatrix()
            requires std::default_initializable<storage_t>
        {
            assert(storage.row_capacity() % storage_t::BITS_PER_WORD == 0);
        }

        constexpr explicit AdjMatrix(storage_t&& s) : storage(s) {
            assert(storage.row_capacity() % storage_t::BITS_PER_WORD == 0);
        }

        constexpr std::span<word_t> buffer() noexcept {
            return storage.buffer();
        }

        constexpr std::span<const word_t> buffer() const noexcept {
            return storage.buffer();
        }

        constexpr std::span<word_t> row(node_t i) noexcept {
            return storage.row(i);
        }

        constexpr std::span<const word_t> row(node_t i) const noexcept {
            return storage.row(i);
        }

        [[nodiscard]] constexpr bool has_edge(node_t u, node_t v) const noexcept {
            return BitSpan(storage.row(u)).get_bit(v);
        }

        [[nodiscard]] constexpr node_t count_degree(node_t u) const noexcept {
            return static_cast<node_t>(BitSpan(storage.row(u)).count_ones());
        }

        constexpr bool add_edge(node_t u, node_t v) noexcept {
            assert(LOOPS_ALLOWED || u != v);
            const auto prev1 = BitSpan(storage.row(u)).set_bit(v);
            const auto prev2 = BitSpan(storage.row(v)).set_bit(u);
            assert(prev1 == prev2 || (LOOPS_ALLOWED && u == v));
            return prev1;
        }

        constexpr bool remove_edge(node_t u, node_t v) noexcept {
            assert(LOOPS_ALLOWED || u != v);
            const auto prev1 = BitSpan(storage.row(u)).unset_bit(v);
            const auto prev2 = BitSpan(storage.row(v)).unset_bit(u);
            assert(prev1 == prev2 || (LOOPS_ALLOWED && u == v));
            (void) prev2;
            return prev1;
        }
    };

    namespace details {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// Fixed storage (without heap)
        class HeapStorage {
        public:
            using word_t = uint32_t;
            static constexpr size_t BITS_PER_WORD = 8 * sizeof(word_t);
            static constexpr node_t CAPACITY_OF_SMALLEST_GRAPH = BITS_PER_WORD;

        private:
            node_t row_capacity_;
            std::unique_ptr<word_t[]> buffer_;

        public:
            HeapStorage() = delete;

            explicit constexpr HeapStorage(node_t row_capacity) :
                row_capacity_(std::max(CAPACITY_OF_SMALLEST_GRAPH, std::bit_ceil(row_capacity))),
                buffer_(std::make_unique<word_t[]>(row_capacity_ * row_capacity_ / BITS_PER_WORD)) {
                assert(row_capacity_ > 0);
            }

            [[nodiscard]] constexpr std::span<word_t> buffer() noexcept {
                return {buffer_.get(), row_capacity_ * row_capacity_ / BITS_PER_WORD};
            }

            [[nodiscard]] constexpr std::span<const word_t> buffer() const noexcept {
                return {buffer_.get(), row_capacity_ * row_capacity_ / BITS_PER_WORD};
            }

            [[nodiscard]] constexpr std::span<word_t> row(size_t i) noexcept {
                const auto words_per_row = row_capacity_ / BITS_PER_WORD;
                const auto begin = buffer_.get() + i * words_per_row;
                return {begin, begin + words_per_row};
            }

            [[nodiscard]] constexpr std::span<const word_t> row(size_t i) const noexcept {
                const auto words_per_row = row_capacity_ / BITS_PER_WORD;
                const auto begin = buffer_.get() + i * words_per_row;
                return {begin, begin + words_per_row};
            }

            [[nodiscard]] constexpr node_t row_capacity() const noexcept {
                return row_capacity_;
            }
        };

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// Fixed storage (without heap)
        template<std::unsigned_integral Word, node_t Capacity = sizeof(Word) * 8>
        class FixedStorage {
        public:
            using word_t = Word;
            static constexpr size_t BITS_PER_WORD = 8 * sizeof(word_t);

        private:
            static_assert(std::has_single_bit(Capacity)); // is power of two
            static constexpr size_t Words = Capacity * Capacity / BITS_PER_WORD;
            std::array<word_t, Words> buffer_ = {};

        public:
            [[nodiscard]] constexpr std::span<word_t> buffer() noexcept {
                return {buffer_};
            }

            [[nodiscard]] constexpr std::span<const word_t> buffer() const noexcept {
                return {buffer_};
            }

            [[nodiscard]] constexpr std::span<word_t> row(size_t i) noexcept {
                constexpr auto words_per_row = Capacity / BITS_PER_WORD;
                const auto begin = buffer_.begin() + (i * words_per_row);
                return {begin, begin + words_per_row};
            }

            [[nodiscard]] constexpr std::span<const word_t> row(size_t i) const noexcept {
                constexpr auto words_per_row = Capacity / BITS_PER_WORD;
                const auto begin = buffer_.begin() + (i * words_per_row);
                return {begin, begin + words_per_row};
            }

            [[nodiscard]] constexpr node_t row_capacity() const noexcept {
                return Capacity;
            }
        };

        using FixedStorage8 = FixedStorage<uint8_t>;
        using FixedStorage16 = FixedStorage<uint16_t>;
        using FixedStorage32 = FixedStorage<uint32_t>;
        using FixedStorage64 = FixedStorage<uint64_t>;
        using FixedStorage128 = FixedStorage<uint64_t, 128>;
    } // namespace details

    using AdjMatrix8 = AdjMatrix<details::FixedStorage8>;
    using AdjMatrix16 = AdjMatrix<details::FixedStorage16>;
    using AdjMatrix32 = AdjMatrix<details::FixedStorage32>;
    using AdjMatrix64 = AdjMatrix<details::FixedStorage64>;
    using AdjMatrix128 = AdjMatrix<details::FixedStorage128>;
    using AdjMatrixHeap = AdjMatrix<details::HeapStorage>;
} // namespace smallcanon
