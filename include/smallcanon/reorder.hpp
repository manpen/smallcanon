#pragma once

#include <smallcanon/adj_matrix.hpp>
#include <smallcanon/simd/avx512defs.hpp>
#include <xsimd/xsimd.hpp>

#include "coloring.hpp"

namespace smallcanon {

    template<typename S>
    inline auto transpose(const AdjMatrix<S>& g) {
        auto copied = AdjMatrix<S>(g.num_nodes());
        for (const auto u: g.nodes()) {
            const BitSpan row(g.row(u));
            for (const auto v: row.iterate_set_bits()) {
                if (v >= g.num_nodes()) {
                    break;
                }
                BitSpan(copied.row(static_cast<node_t>(v))).set_bit(u);
            }
        }
        return copied;
    }

    SMALLCANON_ALWAYS_INLINE static uint64_t transpose(uint64_t x) {
        uint64_t t;
        t = (x ^ (x >> 7)) & 0x00AA00AA00AA00AAULL;
        x = x ^ t ^ (t << 7);
        t = (x ^ (x >> 14)) & 0x0000CCCC0000CCCCULL;
        x = x ^ t ^ (t << 14);
        t = (x ^ (x >> 28)) & 0x00000000F0F0F0F0ULL;
        x = x ^ t ^ (t << 28);
        return x;
    }

#if SMALLCANON_WITH_AVX512 // TODO: we actually only need the type definition; there's no avx512 necessary
    using simd::avx512defs::u16x16_t;

    namespace reorder_details {
        template<size_t Stride>
        SMALLCANON_ALWAYS_INLINE constexpr uint16_t transpose_mask() noexcept {
            uint16_t mask = 0;
            for (size_t i = 0; i < 16; ++i) {
                if ((i & Stride) == 0) {
                    mask |= static_cast<uint16_t>(uint16_t{1} << i);
                }
            }
            return mask;
        }

        template<size_t Stride>
        SMALLCANON_ALWAYS_INLINE u16x16_t transpose_stage(u16x16_t mat) noexcept {
            using namespace simd::avx512defs;

            struct Swizzle {
                static constexpr uint16_t get(unsigned i, unsigned) {
                    return static_cast<uint16_t>(i ^ Stride);
                }
            };

            struct LowerHalf {
                static constexpr bool get(unsigned i, unsigned) {
                    return (i & Stride) == 0;
                }
            };

            constexpr auto swizzle_indices = xsimd::make_batch_constant<uint16_t, Swizzle, arch256>();
            constexpr auto lower_half = xsimd::make_batch_bool_constant<uint16_t, LowerHalf, arch256>();

            const auto partner = xsimd::swizzle(mat, swizzle_indices);
            const auto field_mask = u16x16_t::broadcast(transpose_mask<Stride>());
            const auto lower_t = ((mat >> Stride) ^ partner) & field_mask;
            const auto pair_t = xsimd::select(lower_half, lower_t, xsimd::swizzle(lower_t, swizzle_indices));

            return xsimd::select(lower_half, mat ^ (pair_t << Stride), mat ^ pair_t);
        }
    } // namespace reorder_details

    inline auto transpose(u16x16_t mat) noexcept {
        mat = reorder_details::transpose_stage<8>(mat);
        mat = reorder_details::transpose_stage<4>(mat);
        mat = reorder_details::transpose_stage<2>(mat);
        mat = reorder_details::transpose_stage<1>(mat);
        return mat;
    }

    template<>
    inline auto transpose(const AdjMatrix16& matrix) {
        using namespace simd::avx512defs;

        auto copied = matrix.copy();
        native_to_matrix(copied, transpose(matrix_to_native(matrix)));
        return copied;
    }
#endif

    template<>
    inline auto transpose(const AdjMatrix8& matrix) {
        auto copied = matrix.copy();
        uint64_t x = read_le_u64(matrix.buffer().data());
        x = transpose(x);
        write_le_u64(copied.buffer().data(), x);
        return copied;
    }

    /// Reorders the graph according to the label order of col
    template<typename S>
    inline auto reorder_graph(const AdjMatrix<S>& g, const typename MatchedColoring<AdjMatrix<S>>::coloring_t& col) {
        const auto labels = col.labels();

        auto copied = AdjMatrix<S>(g.num_nodes());
        for (auto u: g.nodes()) {
            const auto& src = g.row(static_cast<node_t>(labels[u]));
            std::ranges::copy(src, copied.row(u).begin());
        }

        copied = transpose(copied);

        auto row_reordered_transpose = AdjMatrix<S>(g.num_nodes());
        for (auto u: g.nodes()) {
            const auto& src = copied.row(static_cast<node_t>(labels[u]));
            std::ranges::copy(src, row_reordered_transpose.row(u).begin());
        }

        return transpose(row_reordered_transpose);
    }

#if XSIMD_WITH_SSE3


    template<>
    inline auto reorder_graph(const AdjMatrix8& g, const Coloring8& col) {
        const auto labels = read_le_u64(col.labels().data());
        auto mat = read_le_u64(g.buffer().data());

        auto to_uint64 = [](__m64 x) { return std::bit_cast<uint64_t>(x); };
        auto to_m64 = [](uint64_t x) { return std::bit_cast<__m64>(x); };

        mat = to_uint64(_mm_shuffle_pi8(to_m64(mat), to_m64(labels)));
        mat = transpose(mat);
        mat = to_uint64(_mm_shuffle_pi8(to_m64(mat), to_m64(labels)));

        // mat = transpose(mat); // this transpose can be skipped since we have symmetric mats (undirected graphs)

        auto copied = g.copy();
        write_le_u64(copied.buffer().data(), mat);
        return copied;
    }

#endif

#if SMALLCANON_WITH_AVX512

    template<>
    inline auto reorder_graph(const AdjMatrix16& g, const Coloring16& col) {
        auto mat = simd::avx512defs::matrix_to_native(g);

        const auto labels = simd::avx512defs::widen_to_u16(simd::avx512defs::labels_to_native(col));
        mat = xsimd::swizzle(mat, labels);
        mat = transpose(mat);
        mat = xsimd::swizzle(mat, labels);

        // mat = transpose(mat); // this transpose can be skipped since we have symmetric mats (undirected graphs)

        auto copied = g.copy();
        simd::avx512defs::native_to_matrix(copied, mat);
        return copied;
    }


#endif


} // namespace smallcanon
