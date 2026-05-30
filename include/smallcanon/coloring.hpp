#pragma once
#include <array>
#include <cassert>
#include <concepts>
#include <limits>
#include <memory>
#include <span>

#include <smallcanon/graph.hpp>

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
        /// Creates a coloring using default-constructed storage.
        /// All colors are initialized to 0
        constexpr Coloring()
            requires std::default_initializable<storage_t>
        {}

        /// Creates a coloring from a storage type.
        constexpr explicit Coloring(storage_t&& s) : storage(std::move(s)) {}


        /// Returns the number of nodes supported.
        [[nodiscard]] node_t capacity() const noexcept {
            return static_cast<node_t>(storage.buffer().size());
        }

        /// Returns the current color of node u.
        [[nodiscard]] color_t get_color(node_t u) const noexcept {
            assert(u < capacity());
            return static_cast<color_t>(storage.buffer()[u]);
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

            /// Disallows copying fixed color storage.
            constexpr FixedColorStore(const FixedColorStore&) = delete;

            /// Moves fixed color storage.
            constexpr FixedColorStore(FixedColorStore&&) = default;

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
} // namespace smallcanon
