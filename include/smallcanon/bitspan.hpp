#pragma once
#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <utility>

namespace smallcanon {
    template<std::unsigned_integral T>
    class BitSpan {
        T *begin_;
        T *end_;

    public:
        static constexpr size_t BITS_PER_WORD = sizeof(T) * 8;

        /// Creates a bit span over the half-open word range [begin, end).
        constexpr explicit BitSpan(T *begin, T *end) : begin_(begin), end_(end) {
            assert(begin_ <= end_);
        }

        /// Returns the number of addressable bits in the span.
        [[nodiscard]] constexpr size_t size() const noexcept {
            return static_cast<size_t>(end_ - begin_) * BITS_PER_WORD;
        }

        /// Returns whether bit i is set.
        [[nodiscard]] constexpr bool get_bit(size_t i) const noexcept {
            const auto [offset, mask] = offset_and_mask(i);
            return begin_[offset] & mask;
        }

        /// Returns the total number of set bits in all words covered by the span.
        [[nodiscard]] constexpr size_t count_ones() const noexcept {
            size_t sum = 0;
            for (auto it = begin_; it != end_; ++it) {
                sum += static_cast<size_t>(std::popcount(*it));
            }
            return sum;
        }

        /// Returns whether the span contains no set bits.
        [[nodiscard]] constexpr bool all_unset() const noexcept {
            return std::all_of(begin_, end_, [](auto& x) { return x == 0; });
        }

        /// Assigns value v to bit i and returns whether it was previously set.
        /// Prefer set_bit / unset_bit if you know the value at compile time.
        bool assign_bit(size_t i, bool v) noexcept {
            const auto [offset, mask] = offset_and_mask(i);

            auto& word = begin_[offset];
            const bool prev = word & mask;
            word &= ~mask;
            word |= T(v) * mask;
            return prev;
        }

        /// Sets bit i and returns whether it was previously set.
        bool set_bit(size_t i) noexcept {
            const auto [offset, mask] = offset_and_mask(i);

            auto& word = begin_[offset];
            const bool prev = word & mask;
            word |= mask;
            return prev;
        }

        /// Clears bit i and returns whether it was previously set.
        bool unset_bit(size_t i) noexcept {
            const auto [offset, mask] = offset_and_mask(i);

            auto& word = begin_[offset];
            const bool prev = word & mask;
            word &= ~mask;
            return prev;
        }

    private:
        /// Returns the backing word offset and bit mask for bit i.
        constexpr std::pair<size_t, T> offset_and_mask(size_t i) const noexcept {
            assert(i < size());
            const size_t offset = i / BITS_PER_WORD;
            const auto mask = static_cast<T>(T(1) << (i % BITS_PER_WORD));
            return {offset, mask};
        }
    };
} // namespace smallcanon
