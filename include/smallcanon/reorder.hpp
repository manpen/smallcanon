#pragma once

#include <smallcanon/adj_matrix.hpp>
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

    SMALLCANON_ALWAYS_INLINE static uint64_t transpose_uint64(uint64_t x) {
        uint64_t t;
        t = (x ^ (x >> 7)) & 0x00AA00AA00AA00AAULL;
        x = x ^ t ^ (t << 7);
        t = (x ^ (x >> 14)) & 0x0000CCCC0000CCCCULL;
        x = x ^ t ^ (t << 14);
        t = (x ^ (x >> 28)) & 0x00000000F0F0F0F0ULL;
        x = x ^ t ^ (t << 28);
        return x;
    }

    template<>
    inline auto transpose(const AdjMatrix8& matrix) {
        auto copied = matrix.copy();
        uint64_t x = read_le_u64(matrix.buffer().data());

        x = transpose_uint64(x);

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
        mat = transpose_uint64(mat);
        mat = to_uint64(_mm_shuffle_pi8(to_m64(mat), to_m64(labels)));

        auto copied = g.copy();
        write_le_u64(copied.buffer().data(), mat);
        return copied;
    }

#endif


} // namespace smallcanon
