#pragma once
#include <algorithm>
#include <bit>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <generator>
#include <span>
#include <type_traits>
#include <utility>

namespace smallcanon {
    // iterates over all bits that are set in v
    template<std::unsigned_integral T>
    std::generator<int> iterate_set_bits(T v) {
        while (v) {
            const auto pos = std::countr_zero(v);
            v &= v - 1; // remove least significant one
            co_yield pos;
        }
    }

    template<typename T, size_t Extent = std::dynamic_extent>
        requires std::unsigned_integral<std::remove_cv_t<T>>
    class BitSpan {
        using unsigned_t = std::remove_cv_t<T>;

        std::span<T, Extent> words_;

    public:
        static constexpr size_t BITS_PER_WORD = sizeof(unsigned_t) * 8;

        /// Creates a bit span over the half-open word range [begin, end).
        constexpr explicit BitSpan(T *begin, T *end) : words_(begin, end) {
            assert(begin <= end);
        }

        /// Creates a bit span over the half-open word range [begin, end).
        constexpr explicit BitSpan(std::span<T, Extent> span) : words_(span) {}


        /// Returns the number of addressable bits in the span.
        [[nodiscard]] constexpr size_t size() const noexcept {
            return words_.size() * BITS_PER_WORD;
        }

        /// Returns whether bit i is set.
        [[nodiscard]] constexpr bool get_bit(size_t i) const noexcept {
            const auto [offset, mask] = offset_and_mask(i);
            return words_[offset] & mask;
        }

        /// Returns the total number of set bits in all words covered by the span.
        [[nodiscard]] constexpr size_t count_ones() const noexcept {
            size_t sum = 0;
            for (const auto word: words_) {
                sum += static_cast<size_t>(std::popcount(word));
            }
            return sum;
        }

        /// Returns whether the span contains no set bits.
        [[nodiscard]] constexpr bool all_unset() const noexcept {
            return std::all_of(words_.begin(), words_.end(), [](const auto& x) { return x == 0; });
        }

        /// Returns a generator with the positions of all set bits.
        [[nodiscard]] std::generator<size_t> iterate_set_bits() const {
            for (size_t word_index = 0; word_index < words_.size(); ++word_index) {
                for (const auto bit: smallcanon::iterate_set_bits(words_[word_index])) {
                    co_yield (word_index * BITS_PER_WORD) + static_cast<size_t>(bit);
                }
            }
        }

        /// Assigns value v to bit i and returns whether it was previously set.
        /// Prefer set_bit / unset_bit if you know the value at compile time.
        bool assign_bit(size_t i, bool v) noexcept
            requires(!std::is_const_v<T>)
        {
            const auto [offset, mask] = offset_and_mask(i);

            auto& word = words_[offset];
            const bool prev = word & mask;
            word &= ~mask;
            word |= T(v) * mask;
            return prev;
        }

        /// Sets bit i and returns whether it was previously set.
        bool set_bit(size_t i) noexcept
            requires(!std::is_const_v<T>)
        {
            const auto [offset, mask] = offset_and_mask(i);

            auto& word = words_[offset];
            const bool prev = word & mask;
            word |= mask;
            return prev;
        }

        /// Clears bit i and returns whether it was previously set.
        bool unset_bit(size_t i) noexcept
            requires(!std::is_const_v<T>)
        {
            const auto [offset, mask] = offset_and_mask(i);

            auto& word = words_[offset];
            const bool prev = word & mask;
            word &= ~mask;
            return prev;
        }

    private:
        /// Returns the backing word offset and bit mask for bit i.
        constexpr std::pair<size_t, unsigned_t> offset_and_mask(size_t i) const noexcept {
            assert(i < size());
            const size_t offset = i / BITS_PER_WORD;
            const auto mask = static_cast<unsigned_t>(unsigned_t{1} << (i % BITS_PER_WORD));
            return {offset, mask};
        }
    };

    template<typename T>
        requires std::unsigned_integral<std::remove_cv_t<T>>
    class BitSpan<T, 1> {
        using unsigned_t = std::remove_cv_t<T>;

        T& word_;

    public:
        static constexpr size_t BITS_PER_WORD = sizeof(unsigned_t) * 8;

        /// Creates a bit span over a single backing word.
        constexpr explicit BitSpan(T *begin, T *end) : word_(word_from_range(begin, end)) {
            assert(begin + 1 == end);
        }

        /// Creates a bit span over a single backing word.
        constexpr explicit BitSpan(std::span<T, 1> span) : word_(span[0]) {}

        /// Returns the number of addressable bits in the span.
        [[nodiscard]] constexpr size_t size() const noexcept {
            return BITS_PER_WORD;
        }

        /// Returns whether bit i is set.
        [[nodiscard]] constexpr bool get_bit(size_t i) const noexcept {
            const auto mask = mask_of(i);
            return word_ & mask;
        }

        /// Returns the total number of set bits in the backing word.
        [[nodiscard]] constexpr size_t count_ones() const noexcept {
            return static_cast<size_t>(std::popcount(word_));
        }

        /// Returns whether the span contains no set bits.
        [[nodiscard]] constexpr bool all_unset() const noexcept {
            return word_ == 0;
        }

        /// Returns a generator with the positions of all set bits.
        [[nodiscard]] std::generator<size_t> iterate_set_bits() const {
            for (const auto bit: smallcanon::iterate_set_bits(word_)) {
                co_yield static_cast<size_t>(bit);
            }
        }

        /// Assigns value v to bit i and returns whether it was previously set.
        /// Prefer set_bit / unset_bit if you know the value at compile time.
        bool assign_bit(size_t i, bool v) noexcept
            requires(!std::is_const_v<T>)
        {
            const auto mask = mask_of(i);

            const bool prev = word_ & mask;
            word_ &= ~mask;
            word_ |= T(v) * mask;
            return prev;
        }

        /// Sets bit i and returns whether it was previously set.
        bool set_bit(size_t i) noexcept
            requires(!std::is_const_v<T>)
        {
            const auto mask = mask_of(i);

            const bool prev = word_ & mask;
            word_ |= mask;
            return prev;
        }

        /// Clears bit i and returns whether it was previously set.
        bool unset_bit(size_t i) noexcept
            requires(!std::is_const_v<T>)
        {
            const auto mask = mask_of(i);

            const bool prev = word_ & mask;
            word_ &= ~mask;
            return prev;
        }

    private:
        /// Returns the single backing word for a one-word range.
        static constexpr T& word_from_range(T *begin, T *end) noexcept {
            assert(begin + 1 == end);
            return *begin;
        }

        /// Returns the bit mask for bit i.
        constexpr unsigned_t mask_of(size_t i) const noexcept {
            assert(i < size());
            return static_cast<unsigned_t>(unsigned_t{1} << i);
        }
    };

    template<typename T>
        requires std::unsigned_integral<std::remove_cv_t<T>>
    BitSpan(T *, T *) -> BitSpan<T>;

    template<typename T, size_t Extent>
        requires std::unsigned_integral<std::remove_cv_t<T>>
    BitSpan(std::span<T, Extent>) -> BitSpan<T, Extent>;


} // namespace smallcanon
