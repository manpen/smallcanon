#pragma once

#include <xsimd/xsimd.hpp>
#include <smallcanon/simd/sort_single_batch.hpp>

namespace smallcanon::simd::sort {
    namespace sort_details {
        // TODO: The bitonic sorting network is explored in a DFS-style fashion; the compiler can almost certainly
        // produce better code (reusing constants), if we do it in a BFS style.
        namespace xs = xsimd;

        template<std::unsigned_integral T, typename A, size_t kVectors>
        std::pair<std::span<xs::batch<T, A>, kVectors / 2>, std::span<xs::batch<T, A>, kVectors / 2>>
        split_span(std::span<xs::batch<T, A>, kVectors>& span) {
            static_assert(kVectors > 1);
            static_assert(std::has_single_bit(kVectors));

            return {span.template subspan<0, kVectors / 2>(), span.template subspan<kVectors / 2, kVectors / 2>()};
        }

        template<std::unsigned_integral T, size_t Stride, size_t Lanes>
        consteval std::pair<std::array<T, Lanes>, uint64_t>
        compute_merge_swizzle() {
            static_assert(Lanes < 64); // blendmask only supports 64bits

            std::array<T, Lanes> swizzle;
            uint64_t blend = 0;

            constexpr T mask = Stride;

            // produce identity map
            for (size_t i = 0; i < Lanes; ++i) {
                swizzle[i] = static_cast<T>(i ^ mask);
                blend |= static_cast<uint64_t>(i > (i ^ mask)) << i;
            }

            return {swizzle, blend};
        }

        template<bool kAscending, size_t Stride, std::unsigned_integral T, typename A>
        auto merge_layer_batch(xs::batch<T, A> batch) {
            alignas(A::alignment()) //
                constexpr auto swizzle_blend = compute_merge_swizzle<T, Stride, decltype(batch)::size>();

            const auto swizzle_indices = decltype(batch)::load_aligned(swizzle_blend.first.data());
            const auto blend_mask = xs::batch_bool<T, A>::from_mask(swizzle_blend.second);

            auto swizzled = xs::swizzle(batch, swizzle_indices);
            const auto minv = xs::min(batch, swizzled);
            const auto maxv = xs::max(batch, swizzled);

            if constexpr (kAscending) {
                batch = xs::select(blend_mask, maxv, minv);
            } else {
                batch = xs::select(blend_mask, minv, maxv);
            }

            return batch;
        }



        template<bool kAscending, std::unsigned_integral T, typename A>
        auto merge_single_batch(xs::batch<T, A> batch) {
            constexpr size_t kLanes = decltype(batch)::size;

            // clang-format off
            if constexpr (kLanes > 32) {batch = merge_layer_batch<kAscending, 32>(batch);}
            if constexpr (kLanes > 16) {batch = merge_layer_batch<kAscending, 16>(batch);}
            if constexpr (kLanes >  8) {batch = merge_layer_batch<kAscending,  8>(batch);}
            if constexpr (kLanes >  4) {batch = merge_layer_batch<kAscending,  4>(batch);}
            if constexpr (kLanes >  2) {batch = merge_layer_batch<kAscending,  2>(batch);}
            if constexpr (kLanes >  1) {batch = merge_layer_batch<kAscending,  1>(batch);}
            // clang-format on

            return batch;
        }

        template<bool kAscending, size_t kVectors, std::unsigned_integral T, typename A>
        void merge_vecs(std::span<xs::batch<T, A>, kVectors>& vecs) {
            if constexpr (kVectors == 1) {
                vecs[0] = merge_single_batch<kAscending>(vecs[0]);

            } else {
                auto [first_half, second_half] = split_span(vecs);

                for (size_t i = 0; i < kVectors / 2; ++i) {
                    const auto min = xs::min(first_half[i], second_half[i]);
                    const auto max = xs::max(first_half[i], second_half[i]);

                    if constexpr (kAscending) {
                        first_half[i] = min;
                        second_half[i] = max;
                    } else {
                        first_half[i] = max;
                        second_half[i] = min;
                    }
                }

                merge_vecs<kAscending>(first_half);
                merge_vecs<kAscending>(second_half);
            }
        }

        template<bool kAscending, size_t kVectors, std::unsigned_integral T, typename A>
        void sort_vecs(std::span<xs::batch<T, A>, kVectors>& vecs) {
            using batch_t = xs::batch<T, A>;
            static_assert(std::has_single_bit(kVectors));

            if constexpr (kVectors == 1) {
                // base case
                constexpr size_t kLanes = batch_t::size;
                vecs[0] = sort_single_batch<kLanes, T, kAscending>(vecs[0]);

            } else {
                auto [first_half, second_half] = split_span(vecs);

                sort_vecs<true>(first_half);
                sort_vecs<false>(second_half);

                merge_vecs<kAscending>(vecs);
            }
        }
    } // namespace sort_details

    // N has to be a power of two
    template<size_t N, std::unsigned_integral T, bool kAscending = true, typename A = xsimd::default_arch>
    auto sort(T *data) {
        static_assert(std::has_single_bit(N));

        using batch_t = xsimd::batch<T, A>;
        constexpr size_t kLanes = batch_t::size;
        constexpr size_t kVectors = (N + kLanes - 1) / kLanes;

        std::array<batch_t, kVectors> vecs;
        for (size_t i = 0; i < kVectors; ++i) {
            vecs[i] = batch_t::load_unaligned(data + i * kLanes);
        }

        if constexpr (kVectors == 1) {
            vecs[0] = sort_details::sort_single_batch<N, T, kAscending, A>(vecs[0]);
        } else {
            std::span<batch_t, kVectors> span(vecs);
            sort_details::sort_vecs<kAscending, kVectors, T, A>(span);
        }

        for (size_t i = 0; i < kVectors; ++i) {
            vecs[i].store_unaligned(data + i * kLanes);
        }

        return data;
    }
} // namespace smallcanon::simd::sort
