#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <concepts>
#include <initializer_list>
#include <memory>
#include <span>

#include <smallcanon/bitspan.hpp>
#include <smallcanon/graph.hpp>

namespace smallcanon {
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
        static constexpr bool LOOPS_ALLOWED = false;
        static constexpr node_t MAX_NODES = storage_t::MAX_NODES;

    private:
        Storage storage{};
        node_t n_;

    public:
        /// Creates an adjacency matrix from an existing storage object.
        constexpr explicit AdjMatrix(node_t num_nodes) : storage(num_nodes), n_(num_nodes) {
            assert(storage.row_capacity() % storage_t::BITS_PER_WORD == 0);
            assert(num_nodes <= storage.row_capacity());
        }

        /// Returns the number of nodes
        [[nodiscard]] constexpr node_t num_nodes() const noexcept {
            return n_;
        }

        /// Returns a view of the nodes (iterating from [0 .. num_nodes()))
        [[nodiscard]] constexpr std::ranges::iota_view<node_t, node_t> nodes() const noexcept {
            return std::ranges::iota_view{node_t(0), num_nodes()};
        }

        /// Returns the number of rows
        [[nodiscard]] constexpr node_t capacity() const noexcept {
            return storage.row_capacity();
        }

        /// Returns a mutable view of the whole backing word buffer.
        /// DANGER! Uphold all invariants
        [[nodiscard]] constexpr std::span<word_t> buffer() noexcept {
            return storage.buffer();
        }

        /// Returns a read-only view of the whole backing word buffer.
        [[nodiscard]] constexpr std::span<const word_t> buffer() const noexcept {
            return storage.buffer();
        }

        /// Returns a mutable view of the backing words for row u.
        /// DANGER! Uphold all invariants
        [[nodiscard]] constexpr std::span<word_t> row(node_t u) noexcept {
            assert(u < n_);
            return storage.row(u);
        }

        /// Returns a read-only view of the backing words for row u.
        [[nodiscard]] constexpr std::span<const word_t> row(node_t u) const noexcept {
            assert(u < n_);
            return storage.row(u);
        }

        /// Iterates over all undirected edges, yielding each edge {u,v} only once with u <= v.
        [[nodiscard]] std::generator<edge_t> edges() const noexcept {
            for (node_t u: nodes()) {
                for (node_t v: neighbors_of(u)) {
                    if (v > u)
                        break;
                    co_yield {u, v};
                }
            }
        }

        /// Returns whether edge {u, v} is present.
        [[nodiscard]] constexpr bool has_edge(node_t u, node_t v) const noexcept {
            assert(u < n_);
            assert(v < n_);
            return BitSpan(storage.row(u)).get_bit(v);
        }

        /// Returns the number of neighbors of node u; linear time in row-size
        [[nodiscard]] constexpr node_t count_degree(node_t u) const noexcept {
            assert(u < storage.row_capacity());
            return static_cast<node_t>(BitSpan(storage.row(u)).count_ones());
        }

        /// Iterates over all neighbors of node u in ascending order.
        [[nodiscard]] std::generator<node_t> neighbors_of(node_t u) const noexcept {
            assert(u < storage.row_capacity());
            const BitSpan bits(storage.row(u));
            for (const auto v: bits.iterate_set_bits()) {
                assert(v < n_);
                co_yield static_cast<node_t>(v);
            }
        }

        /// Returns a rearranged copy of the graph. Assumes that new_id_of is a permutation of [0, n).
        [[nodiscard]] constexpr AdjMatrix permuted(std::span<const node_t> new_id_of) const noexcept {
            assert(new_id_of.size() == num_nodes());
            auto mapped = AdjMatrix(num_nodes());

            for (const auto& [u, v]: edges()) {
                bool already_exists = mapped.add_edge(new_id_of[u], new_id_of[v]);
                assert(!already_exists);
            }

            return mapped;
        }

        /// Adds edge {u, v} and returns whether it was already present.
        constexpr bool add_edge(node_t u, node_t v) noexcept {
            assert(u < n_);
            assert(v < n_);
            assert(LOOPS_ALLOWED || u != v);
            const auto prev1 = BitSpan(storage.row(u)).set_bit(v);
            [[maybe_unused]] const auto prev2 = BitSpan(storage.row(v)).set_bit(u);
            assert(prev1 == prev2 || (LOOPS_ALLOWED && u == v));
            return prev1;
        }

        /// Removes edge {u, v} and returns whether it was present.
        constexpr bool remove_edge(node_t u, node_t v) noexcept {
            assert(u < n_);
            assert(v < n_);
            assert(LOOPS_ALLOWED || u != v);
            const auto prev1 = BitSpan(storage.row(u)).unset_bit(v);
            const auto prev2 = BitSpan(storage.row(v)).unset_bit(u);
            assert(prev1 == prev2 || (LOOPS_ALLOWED && u == v));
            (void) prev2;
            return prev1;
        }

        /// Adds all edge of the provided range.
        /// Existing or duplicate edges are acceptable (and are ignored)
        /// Returns the number of new edges inserted.
        template<edge_range_c R>
        constexpr edgeid_t add_edges(R&& edges) noexcept {
            edgeid_t new_edges = 0;
            for (const auto& [u, v]: edges) {
                const bool existed_before = add_edge(u, v);
                new_edges += !existed_before;
            }
            return new_edges;
        }

        /// Remove all edges in of the range provided.
        /// Non-existing edges or duplicates are acceptable.
        /// Returns the number of edges acutally deleted.
        template<edge_range_c R>
        constexpr edgeid_t remove_edges(R&& edges) noexcept {
            edgeid_t removed_edges = 0;
            for (auto& [u, v]: edges) {
                const bool existed_before = remove_edge(u, v);
                removed_edges += existed_before;
            }
            return removed_edges;
        }
    };

    namespace details {
        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// Fixed storage (without heap)
        class HeapStorage {
        public:
            using word_t = uint32_t;
            static constexpr size_t BITS_PER_WORD = 8 * sizeof(word_t);
            static constexpr node_t CAPACITY_OF_SMALLEST_GRAPH = BITS_PER_WORD;
            static constexpr node_t MAX_NODES = std::numeric_limits<node_t>::max();

        private:
            node_t row_capacity_;
            std::unique_ptr<word_t[]> buffer_;

        public:
            HeapStorage() = delete;

            /// Allocates zero-initialized heap storage for at least row_capacity nodes.
            explicit constexpr HeapStorage(node_t row_capacity) :
                row_capacity_(std::max(CAPACITY_OF_SMALLEST_GRAPH, std::bit_ceil(row_capacity))),
                buffer_(std::make_unique<word_t[]>(row_capacity_ * row_capacity_ / BITS_PER_WORD)) {
                assert(row_capacity_ > 0);
            }

            /// Returns a mutable view of the whole backing word buffer.
            [[nodiscard]] constexpr std::span<word_t> buffer() noexcept {
                return {buffer_.get(), row_capacity_ * row_capacity_ / BITS_PER_WORD};
            }

            /// Returns a read-only view of the whole backing word buffer.
            [[nodiscard]] constexpr std::span<const word_t> buffer() const noexcept {
                return {buffer_.get(), row_capacity_ * row_capacity_ / BITS_PER_WORD};
            }

            /// Returns a mutable view of the backing words for row i.
            [[nodiscard]] constexpr std::span<word_t> row(size_t i) noexcept {
                const auto words_per_row = row_capacity_ / BITS_PER_WORD;
                const auto begin = buffer_.get() + i * words_per_row;
                return {begin, begin + words_per_row};
            }

            /// Returns a read-only view of the backing words for row i.
            [[nodiscard]] constexpr std::span<const word_t> row(size_t i) const noexcept {
                const auto words_per_row = row_capacity_ / BITS_PER_WORD;
                const auto begin = buffer_.get() + i * words_per_row;
                return {begin, begin + words_per_row};
            }

            /// Returns the number of bits available in each matrix row.
            [[nodiscard]] constexpr node_t row_capacity() const noexcept {
                return row_capacity_;
            }
        };

        ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        /// Fixed storage (without heap)
        template<std::unsigned_integral Word, node_t Capacity = sizeof(Word) * 8>
        class FixedStorage {
        public:
            using word_t = Word;
            static constexpr size_t BITS_PER_WORD = 8 * sizeof(word_t);
            static constexpr node_t CAPACITY = Capacity;
            static constexpr node_t MAX_NODES = CAPACITY;

        private:
            static_assert(std::has_single_bit(Capacity)); // is power of two
            static constexpr size_t Words = Capacity * Capacity / BITS_PER_WORD;
            std::array<word_t, Words> buffer_ = {};

        public:
            FixedStorage() = default;

            /// Constructs a Fixed Storage
            explicit constexpr FixedStorage([[maybe_unused]] node_t n) noexcept {
                assert(n <= Capacity);
            }

            /// Returns a mutable view of the whole backing word buffer.
            [[nodiscard]] constexpr std::span<word_t> buffer() noexcept {
                return {buffer_};
            }

            /// Returns a read-only view of the whole backing word buffer.
            [[nodiscard]] constexpr std::span<const word_t> buffer() const noexcept {
                return {buffer_};
            }

            /// Returns a mutable view of the backing words for row i.
            [[nodiscard]] constexpr std::span<word_t> row(size_t i) noexcept {
                constexpr auto words_per_row = Capacity / BITS_PER_WORD;
                const auto begin = buffer_.begin() + (i * words_per_row);
                return {begin, begin + words_per_row};
            }

            /// Returns a read-only view of the backing words for row i.
            [[nodiscard]] constexpr std::span<const word_t> row(size_t i) const noexcept {
                constexpr auto words_per_row = Capacity / BITS_PER_WORD;
                const auto begin = buffer_.begin() + (i * words_per_row);
                return {begin, begin + words_per_row};
            }

            /// Returns the number of bits available in each matrix row.
            [[nodiscard]] constexpr node_t row_capacity() const noexcept {
                return Capacity;
            }
        };

        using FixedStorage8 = FixedStorage<uint8_t>;
        using FixedStorage16 = FixedStorage<uint16_t>;
        using FixedStorage32 = FixedStorage<uint32_t>;
        using FixedStorage64 = FixedStorage<uint64_t>;
        using FixedStorage128 = FixedStorage<uint64_t, 128>;

        template<typename S, edge_range_c R>
        AdjMatrix<S> make_adj_matrix(node_t n, R&& edges) {
            AdjMatrix<S> graph(n);
            graph.add_edges(std::forward<R>(edges));
            return graph;
        }
    } // namespace details

    using AdjMatrix8 = AdjMatrix<details::FixedStorage8>;
    using AdjMatrix16 = AdjMatrix<details::FixedStorage16>;
    using AdjMatrix32 = AdjMatrix<details::FixedStorage32>;
    using AdjMatrix64 = AdjMatrix<details::FixedStorage64>;
    using AdjMatrix128 = AdjMatrix<details::FixedStorage128>;
    using AdjMatrixHeap = AdjMatrix<details::HeapStorage>;

    using AdjMatrixVariant =
            std::variant<AdjMatrix8, AdjMatrix16, AdjMatrix32, AdjMatrix64, AdjMatrix128, AdjMatrixHeap>;

    // convenience function to construct a heap-allocated graph from a range of edges
    template<edge_range_c R = std::initializer_list<edge_t>>
    AdjMatrixHeap make_adjmatrix_heap(node_t n, R&& edges = {}) {
        return details::make_adj_matrix<details::HeapStorage>(n, std::forward<R>(edges));
    }

    template<edge_range_c R = std::initializer_list<edge_t>>
    auto make_adj_matrix8(node_t n = 8, R&& edges = {}) {
        return details::make_adj_matrix<details::FixedStorage8>(n, std::forward<R>(edges));
    }

    template<edge_range_c R = std::initializer_list<edge_t>>
    auto make_adj_matrix16(node_t n = 16, R&& edges = {}) {
        return details::make_adj_matrix<details::FixedStorage16>(n, std::forward<R>(edges));
    }

    template<edge_range_c R = std::initializer_list<edge_t>>
    auto make_adj_matrix32(node_t n = 32, R&& edges = {}) {
        return details::make_adj_matrix<details::FixedStorage32>(n, std::forward<R>(edges));
    }

    template<edge_range_c R = std::initializer_list<edge_t>>
    auto make_adj_matrix64(node_t n = 64, R&& edges = {}) {
        return details::make_adj_matrix<details::FixedStorage64>(n, std::forward<R>(edges));
    }

    template<edge_range_c R = std::initializer_list<edge_t>>
    auto make_adj_matrix128(node_t n = 128, R&& edges = {}) {
        return details::make_adj_matrix<details::FixedStorage128>(n, std::forward<R>(edges));
    }

} // namespace smallcanon
