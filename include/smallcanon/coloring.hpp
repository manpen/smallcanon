#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <concepts>
#include <limits>
#include <memory>
#include <span>
#include <vector>

#include <smallcanon/graph.hpp>

#include "adj_matrix.hpp"
#include "utility.hpp"

namespace smallcanon {
    /// Coloring maps nodes to colors backed by a configurable storage type.
    /// Only colors in the range [0, capacity) are guaranteed to be storable because storage may use a type smaller than
    /// color_t internally.
    template<typename Storage>
    class Coloring {
        using storage_t = Storage;
        using scolor_t = typename storage_t::scolor_t;
        storage_t storage;

    public:
        constexpr Coloring() = delete;

        /// Creates a coloring for capacity nodes.
        constexpr explicit Coloring(node_t capacity) : storage(capacity) {}

        constexpr Coloring(Coloring&&) = default;
        constexpr Coloring& operator=(Coloring&&) = default;

        /// Returns a mutable view of the color buffer.
        constexpr std::span<scolor_t> buffer() noexcept {
            return storage.buffer();
        }

        /// Returns a read-only view of the color buffer.
        constexpr std::span<const scolor_t> buffer() const noexcept {
            return storage.buffer();
        }

        /// Returns the number of nodes supported.
        [[nodiscard]] node_t capacity() const noexcept {
            return static_cast<node_t>(storage.buffer().size());
        }

        /// Returns the current color of node u.
        [[nodiscard]] color_t get_color(node_t u) const noexcept {
            assert(u < capacity());
            return static_cast<color_t>(storage.buffer()[u]);
        }

        [[nodiscard]] color_t operator[](const node_t u) const noexcept {
            return get_color(u);
        }

        /// Returns the `lab` equivalent of a discrete coloring.
        [[nodiscard]] Coloring compute_inverse_of_discrete(node_t n) const noexcept {
            auto lab = Coloring(n);
            std::fill_n(lab.buffer().begin(), lab.buffer().size(), n);
            for (node_t u = 0; u < n; ++u) {
                [[maybe_unused]] const color_t old = lab.set_color(get_color(u), u);
                assert(old == n);
            }
            return lab;
        }

        /// Sets the color of node u and returns the previous color.
        /// Warning: Only colors in the range [0, capacity) are guaranteed to be storable,
        /// because storage may use a type smaller than color_t internally.
        color_t set_color(node_t u, color_t new_color) noexcept {
            // the storage may use a smaller internal type for colors
            assert(u < capacity());
            assert(new_color < capacity());

            auto& color = storage.buffer()[u];
            const auto prev = static_cast<color_t>(color);
            color = static_cast<scolor_t>(new_color);
            return prev;
        }

        /// Copy the coloring; we do not use the copy-constructor to avoid performance bugs.
        [[nodiscard]] constexpr Coloring copy() const {
            Coloring copied(capacity());
            std::ranges::copy(storage.buffer(), copied.storage.buffer().begin());
            return copied;
        }

        /// Two colorings are equal if they map to the exact same colors.
        template<typename SC>
        constexpr bool operator==(const Coloring<SC>& rhs) const noexcept {
            return std::ranges::equal(storage.buffer(), rhs.storage.buffer());
        }


        color_t first_available_color() {
            const auto& colors = buffer();
            const node_t n = capacity();

            // TODO this is super horrible
            std::vector<char> used(static_cast<std::size_t>(n), false);

            for (color_t c: colors) {
                used[static_cast<std::size_t>(c)] = true;
            }

            color_t first_available_color = 0;
            while (used[static_cast<std::size_t>(first_available_color)]) {
                ++first_available_color;
            }

            return first_available_color;
        }

        /// Prints the coloring
        void print(const size_t n) const noexcept {
#ifdef DEBUG_STREAM
            std::vector<std::pair<node_t, color_t>> vertex_with_colors;
            node_t v = 0;
            for (auto c: buffer())
                if (v < n)
                    vertex_with_colors.push_back({v++, c});
            std::sort(vertex_with_colors.begin(), vertex_with_colors.end(),
                      [](const auto& a, const auto& b) { return a.second < b.second; });
            color_t last_col = -1;
            bool print_col = false;
            DEBUG_STREAM << "c ";
            for (const auto& [vertex, color]: vertex_with_colors) {
                if (color != last_col)
                    print_col = !print_col;
                last_col = color;
                DEBUG_STREAM << (print_col ? console_orange : console_bright_blue) << vertex << " " << console_neutral;
            }
            DEBUG_STREAM << "\n";
#endif
        }

        /// Prints a color
        void print(const size_t n, const color_t col) const noexcept {
#ifdef DEBUG_STREAM
            std::vector<std::pair<node_t, color_t>> vertex_with_colors;
            DEBUG_STREAM << "c " << console_orange;
            for (node_t v = 0; v < n; ++v) {
                if (v >= n)
                    break;
                if (get_color(v) != col)
                    continue;
                DEBUG_STREAM << v << " ";
            }
            DEBUG_STREAM << console_neutral;
            DEBUG_STREAM << "\n";
#endif
        }
    };

    namespace details {
        class ColorStoreHeap {
        public:
            using scolor_t = color_t;

        private:
            node_t capacity_;
            std::unique_ptr<scolor_t[]> buffer_;

        public:
            /// Disallows creating heap storage without an explicit capacity.
            ColorStoreHeap() = delete;

            /// Allocates zero-initialized heap storage for capacity colors.
            explicit constexpr ColorStoreHeap(node_t capacity) :
                capacity_(capacity), buffer_(std::make_unique<scolor_t[]>(capacity_)) {}

            /// Disallows copying heap storage.
            constexpr ColorStoreHeap(const ColorStoreHeap&) = delete;

            /// Moves heap storage ownership.
            constexpr ColorStoreHeap(ColorStoreHeap&&) = default;
            constexpr ColorStoreHeap& operator=(ColorStoreHeap&&) = default;

            /// Returns a mutable view of the color buffer.
            constexpr std::span<scolor_t> buffer() noexcept {
                auto *begin = buffer_.get();
                return std::span(begin, begin + capacity_);
            }

            /// Returns a read-only view of the color buffer.
            constexpr std::span<const scolor_t> buffer() const noexcept {
                const auto *begin = buffer_.get();
                return std::span(begin, begin + capacity_);
            }
        };

        template<std::unsigned_integral T, node_t Capacity>
        class FixedColorStore {
        public:
            using scolor_t = T;
            static_assert(std::numeric_limits<T>::max() >= Capacity);

        private:
            /// Stores exactly Capacity colors without heap allocation.
            /// Only colors in the range [0, Capacity) are guaranteed to be storable because T may be smaller than
            /// color_t.
            std::array<T, Capacity> buffer_ = {};

        public:
            /// Creates zero-initialized fixed color storage.
            constexpr explicit FixedColorStore() = default;

            /// Compatibility with Heap Storage constructor
            constexpr explicit FixedColorStore([[maybe_unused]] node_t capacity) {
                assert(capacity <= Capacity);
            }

            /// Disallows copying fixed color storage.
            constexpr FixedColorStore(const FixedColorStore&) = delete;

            /// Moves fixed color storage.
            constexpr FixedColorStore(FixedColorStore&&) = default;
            constexpr FixedColorStore& operator=(FixedColorStore&&) = default;

            /// Returns a mutable view of the color buffer.
            constexpr std::span<scolor_t> buffer() noexcept {
                auto *begin = buffer_.data();
                return std::span(begin, begin + Capacity);
            }

            /// Returns a read-only view of the color buffer.
            constexpr std::span<const scolor_t> buffer() const noexcept {
                const auto *begin = buffer_.data();
                return std::span(begin, begin + Capacity);
            }
        };

        using FixedColorStore8 = FixedColorStore<uint8_t, 8>;
        using FixedColorStore16 = FixedColorStore<uint8_t, 16>;
        using FixedColorStore32 = FixedColorStore<uint8_t, 32>;
        using FixedColorStore64 = FixedColorStore<uint8_t, 64>;
        using FixedColorStore128 = FixedColorStore<uint8_t, 128>;
    } // namespace details

    using Coloring8 = Coloring<details::FixedColorStore8>;
    using Coloring16 = Coloring<details::FixedColorStore16>;
    using Coloring32 = Coloring<details::FixedColorStore32>;
    using Coloring64 = Coloring<details::FixedColorStore64>;
    using Coloring128 = Coloring<details::FixedColorStore128>;
    using ColoringHeap = Coloring<details::ColorStoreHeap>;

    template<typename G>
    struct MatchedColoring {};

    // clang-format off
    template<> struct MatchedColoring<AdjMatrix8>    { using coloring_t = Coloring8   ; };
    template<> struct MatchedColoring<AdjMatrix16>   { using coloring_t = Coloring16  ; };
    template<> struct MatchedColoring<AdjMatrix32>   { using coloring_t = Coloring32  ; };
    template<> struct MatchedColoring<AdjMatrix64>   { using coloring_t = Coloring64  ; };
    template<> struct MatchedColoring<AdjMatrix128>  { using coloring_t = Coloring128 ; };
    template<> struct MatchedColoring<AdjMatrixHeap> { using coloring_t = ColoringHeap; };
    // clang-format on
} // namespace smallcanon
