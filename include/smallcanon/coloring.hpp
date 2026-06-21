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
    /// Only colors in the range [0, num_nodes) are accepted by the public API because storage may use a type smaller
    /// than color_t internally.
    template<typename Storage>
    class Coloring {
    public:
        using storage_t = Storage;
        using scolor_t = typename storage_t::scolor_t;

    private:
        node_t num_nodes_;
        storage_t colors_;
        storage_t labels_;

        constexpr void initialize_labels() noexcept {
            auto labels = labels_.buffer();
            for (node_t u = 0; u < num_nodes(); ++u) {
                labels[u] = static_cast<scolor_t>(u);
            }
        }

        [[nodiscard]] constexpr node_t label_at(node_t pos) const noexcept {
            assert(pos < num_nodes());
            return static_cast<node_t>(labels_.buffer()[pos]);
        }

        [[nodiscard]] constexpr node_t label_position(node_t u) const noexcept {
            for (node_t pos = 0; pos < num_nodes(); ++pos) {
                if (label_at(pos) == u) {
                    return pos;
                }
            }
            assert(false);
            return num_nodes();
        }

    public:
        constexpr Coloring() = delete;

        /// Creates a coloring for num_nodes nodes.
        constexpr explicit Coloring(node_t num_nodes) : num_nodes_(num_nodes), colors_(num_nodes), labels_(num_nodes) {
            assert(num_nodes_ <= capacity());
            assert(labels_.buffer().size() == colors_.buffer().size());
            initialize_labels();
        }

        constexpr Coloring(Coloring&&) = default;
        constexpr Coloring& operator=(Coloring&&) = default;

        /// Returns a mutable view of the color buffer.
        constexpr auto colors() noexcept {
            return colors_.buffer();
        }

        /// Returns a read-only view of the color buffer.
        constexpr auto colors() const noexcept {
            return colors_.buffer();
        }

        /// Returns a mutable view of the labels sorted by color.
        constexpr auto labels() noexcept {
            return labels_.buffer();
        }

        /// Returns a read-only view of the labels sorted by color.
        constexpr auto labels() const noexcept {
            return labels_.buffer();
        }

        /// Returns the number of active nodes.
        [[nodiscard]] constexpr node_t num_nodes() const noexcept {
            return num_nodes_;
        }

        /// Returns the number of nodes supported by the backing storage.
        [[nodiscard]] node_t capacity() const noexcept {
            return static_cast<node_t>(colors_.buffer().size());
        }

        /// Returns the current color of node u.
        [[nodiscard]] color_t get_color(node_t u) const noexcept {
            assert(u < num_nodes());
            return static_cast<color_t>(colors_.buffer()[u]);
        }

        [[nodiscard]] color_t operator[](const node_t u) const noexcept {
            return get_color(u);
        }

        [[nodiscard]] constexpr color_t color_at_label(node_t pos) const noexcept {
            return static_cast<color_t>(colors_.buffer()[label_at(pos)]);
        }

        /// Sets the color of node u and returns the previous color.
        /// Warning: Only colors in the range [0, num_nodes) are guaranteed to be storable,
        /// because storage may use a type smaller than color_t internally.
        color_t set_color(node_t u, color_t new_color) noexcept {
            // the storage may use a smaller internal type for colors
            assert(u < num_nodes());
            assert(new_color < num_nodes());

            auto& color = colors_.buffer()[u];
            const auto prev = static_cast<color_t>(color);
            if (prev == new_color) {
                return prev;
            }

            color = static_cast<scolor_t>(new_color);
            auto labels = labels_.buffer();
            auto pos = label_position(u);
            if (new_color < prev) {
                while (pos > 0 && color_at_label(pos - 1) >= new_color) {
                    labels[pos] = labels[pos - 1];
                    --pos;
                }
            } else {
                while (pos + 1 < num_nodes() && color_at_label(pos + 1) <= new_color) {
                    labels[pos] = labels[pos + 1];
                    ++pos;
                }
            }
            labels[pos] = static_cast<scolor_t>(u);
            return prev;
        }

        /// Copy the coloring; we do not use the copy-constructor to avoid performance bugs.
        [[nodiscard]] constexpr Coloring copy() const {
            Coloring copied(num_nodes());
            std::ranges::copy(colors_.buffer(), copied.colors_.buffer().begin());
            std::ranges::copy(labels_.buffer(), copied.labels_.buffer().begin());
            return copied;
        }

        /// Two colorings are equal if they map to the exact same colors.
        template<typename SC>
        constexpr bool operator==(const Coloring<SC>& rhs) const noexcept {
            return num_nodes() == rhs.num_nodes() &&
                   std::ranges::equal(colors().first(num_nodes()), rhs.colors().first(rhs.num_nodes()));
        }

        /// Moves node u to its own new color class at the end of the label order.
        constexpr void individualize(const node_t u) noexcept {
            const node_t n = num_nodes();
            assert(u < n);

            const color_t new_color = color_at_label(n - 1) + 1;
            assert(new_color < n);

            auto labels = labels_.buffer();
            for (auto pos = label_position(u); pos + 1 < n; ++pos) {
                labels[pos] = labels[pos + 1];
            }

            labels[n - 1] = static_cast<scolor_t>(u);
            colors_.buffer()[u] = static_cast<scolor_t>(new_color);
        }

        /// Returns false if a violation of the coloring assumptions was detected
        [[nodiscard]] constexpr bool is_consistent() const noexcept {
            const auto n = num_nodes();
            for (node_t u = 0; u < n; ++u) {
                if (get_color(u) >= n)
                    return false;
            }

            // heuristic check to detect duplicates: the sum of labels needs to be (n choose 2)
            uint64_t sum = 0;
            for (node_t u = 0; u < n; ++u) {
                if (label_at(u) >= n)
                    return false;
                sum += label_at(u);
            }
            if (sum != static_cast<uint64_t>(n) * (n - 1) / 2)
                return false;

            for (node_t i = 1; i < n; ++i) {
                const auto u = label_at(i - 1);
                const auto v = label_at(i);

                if (get_color(u) == get_color(v))
                    continue;

                if (get_color(u) + 1 != get_color(v))
                    return false;
            }

            return true;
        }

        /// Prints the coloring
        void print(const size_t n) const noexcept {
#ifdef PRINT_DEBUG
            std::vector<std::pair<node_t, color_t>> vertex_with_colors;
            node_t v = 0;
            for (auto c: colors())
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
#ifdef PRINT_DEBUG
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
            constexpr auto buffer() noexcept {
                auto *begin = buffer_.data();
                return std::span<scolor_t, Capacity>(begin, Capacity);
            }

            /// Returns a read-only view of the color buffer.
            constexpr auto buffer() const noexcept {
                const auto *begin = buffer_.data();
                return std::span<const scolor_t, Capacity>(begin, begin + Capacity);
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
