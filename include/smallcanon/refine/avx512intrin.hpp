#pragma once

#include <xsimd/xsimd.hpp>

#if XSIMD_WITH_AVX512F

#include <immintrin.h>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/simd/sort.hpp"

namespace smallcanon::refine::avx512intrin {
    namespace xs = xsimd;

    using arch = xs::best_arch;
    using u64x8_t = xs::batch<uint64_t, arch>;
    using u32x16_t = xs::batch<uint32_t, arch>;
    using u16x32_t = xs::batch<uint16_t, arch>;
    using u8x64_t = xs::batch<uint8_t, arch>;

    template<uint8_t... Vs>
    using u8xconst = xs::batch_constant<uint8_t, arch, Vs...>;
    template<uint16_t... Vs>
    using u16xconst = xs::batch_constant<uint16_t, arch, Vs...>;
    template<uint32_t... Vs>
    using u32xconst = xs::batch_constant<uint32_t, arch, Vs...>;
    template<uint64_t... Vs>
    using u64xconst = xs::batch_constant<uint64_t, arch, Vs...>;

    static_assert(u8x64_t::size == 64);
    static_assert(u16x32_t::size == 32);
    static_assert(u32x16_t::size == 16);
    static_assert(u64x8_t::size == 8);

    template<typename SM, typename SC>
    void refine(const AdjMatrix<SM>& graph, Coloring<SC>& coloring) {
        static_assert(false); // need specialization
    }

    namespace graph8 {
        static uint64_t load_color(const Coloring8& coloring) {
            // this should become a simple load ... but this version should be more portable.
            uint64_t color = 0;
            for (int i = 0; i < 8; ++i) {
                color |= static_cast<uint64_t>(coloring.get_color(i)) << (8 * i);
            }
            return color;
        }

        static u64x8_t load_graph(const AdjMatrix8& graph) {
            // this should become a simple load ... but this version should be more portable.
            uint64_t single_graph = 0;
            for (int i = 0; i < 8; ++i) {
                single_graph |= static_cast<uint64_t>(*graph.row_upto_capacity(i).data()) << (8 * i);
            }

            const auto rows = (xs::broadcast(single_graph) >> u64xconst<0, 8, 16, 24, 32, 40, 48, 56>()) & 0xff;
            return rows * 0x0101010101010101;
        }

        static void set_color(Coloring8& coloring, uint64_t color) {
            for (int i = 0; i < 8; ++i, color >>= 8) {
                coloring.set_color(i, static_cast<uint8_t>(color));
            }
        }

        static uint64_t compute_color_mask_vec(uint64_t x) {
            u8x64_t colors = u8x64_t(xs::broadcast(x));
            constexpr uint64_t f = 0x0101010101010101;
            return (colors == u8x64_t(u64xconst<0, f, 2 * f, 3 * f, 4 * f, 5 * f, 6 * f, 7 * f>().as_batch())).mask();
        }

        static u64x8_t compute_fingerprints(const u64x8_t vgraph, const uint64_t color_uint64,
                                            const uint64_t disabled_colors) {

            uint64_t color_mask = compute_color_mask_vec(color_uint64);

            const u64x8_t masked = vgraph & color_mask;
            const u64x8_t color_counts = _mm512_popcnt_epi8(masked);
            const auto color_counts32 = //
                    ((color_counts & 0x00000000FFFFFFFF) << 4) | //
                    ((color_counts & 0xFFFFFFFF00000000) >> 25);

            const auto with_node_ids = color_counts32 | u64xconst<0, 1, 2, 3, 4, 5, 6, 7>();

            const u64x8_t colors_at_msb =
                    (xs::broadcast(color_uint64 | disabled_colors) << u64xconst<60, 52, 44, 36, 28, 20, 12, 4>()) &
                    0xf000000000000000;

            return with_node_ids | colors_at_msb;
        }

        static u8x64_t compute_prefixsum_u8x8(uint8_t same_fingerprint_as_pred) {
            // since we can easily fit 8 copies of 8 bits each into a register, we can abuse popcnt to compute a prefix
            // sum.
            u8x64_t masked = xs::bitwise_cast<uint8_t>(xs::broadcast_as<uint64_t>(0xff7f3f1f0f070301)) &
                             same_fingerprint_as_pred;
            return _mm512_popcnt_epi8(masked);
        }
    } // namespace graph8

    void refine(const AdjMatrix8& graph, Coloring8& coloring) {
        const u64x8_t vgraph = graph8::load_graph(graph);

        // ternary required as "uint64_t << 64" is UB :(
        const uint64_t disabled_colors =
                (graph.num_nodes() == 8) ? 0 : (~static_cast<uint64_t>(0)) << (8 * graph.num_nodes());
        const auto enabled_eq_mask = static_cast<uint8_t>((2 << graph.num_nodes()) - 1);

        auto color_uint64 = graph8::load_color(coloring);

        if (!color_uint64) {
            color_uint64 = xs::get<0>(u64x8_t{_mm512_popcnt_epi8(vgraph)});
        }

        // Store the previous value of `same_fingerprint_as_pred`.
        // To force completion of at least one iteration, we choose an impossible value:
        // LSB can never be one, so this will always be false in the first iteration
        uint64_t prev_same_fingerprint_as_pred = 1;

        // We refine until either (i) the coloring is discrete or (ii) the coloring is converged.
        while (true) {
            // Each fingerprint uses 64 bits. The MSB 3 bits encode the old color, the lowest 3 bits the node id.
            // The remaining bits are implementation specific and encode some (permutation invariant) information
            // about each node's neighborhood.
            const auto fingerprints = graph8::compute_fingerprints(vgraph, color_uint64, disabled_colors);

            // By sorting the fingerprint, we move identical fingerprints (up to node id in the LSBs) next to each other
            const auto sorted_fingerprints = simd::sort::sort_details::sort_single_batch<8>(fingerprints);

            // Split fingerprints into node ids and remainder
            const auto node_ids_by_rank = sorted_fingerprints & 0xf;
            const auto fingerprints_wo_node_ids = sorted_fingerprints & 0xfffffffffffffff0;

            uint8_t same_fingerprint_as_pred;
            {
                // We compare each fingerprint to its predecessor (i.e. the element with next lower rank).
                // We finally obtain an 8-bit integer (same_fingerprint_as_pred), where a one in the i-th bit encodes
                // that a new color starts with the node of rank i.
                const u64x8_t shifted_fps = xs::swizzle(fingerprints_wo_node_ids, u64xconst<0, 0, 1, 2, 3, 4, 5, 6>{});
                same_fingerprint_as_pred =
                        static_cast<uint8_t>((fingerprints_wo_node_ids != shifted_fps).mask()) & enabled_eq_mask;
                assert((same_fingerprint_as_pred & 1) == 0);


                // Check for convergence. Observe that we can only refine but not rearrange colors. Hence, the same
                // pattern in same_fingerprint_as_pred implies the same values in node_ids_by_rank.
                if (same_fingerprint_as_pred == prev_same_fingerprint_as_pred) {
                    // computation converged; break
                    break;
                }

                // Check for discrete
                if ((same_fingerprint_as_pred | 1) == enabled_eq_mask) {
                    // got discrete coloring, so order of node ids in fingerprint is color
                    color_uint64 = xs::get<0>(node_ids_by_rank);
                    break;
                }

                prev_same_fingerprint_as_pred = same_fingerprint_as_pred;
            }

            // we have 8 copies of prefix sums of u8x8
            const auto prefixsum = graph8::compute_prefixsum_u8x8(same_fingerprint_as_pred);

            // nice trick to avoid sorting back from fingerprint order back to node id order:
            // we interpret each prefixsum copy as a uint64_t. In i-th lane now takes care of
            // the node u of rank i and moves its color to the u-th byte while zeroing out all others.
            // Then we recombine the 8 uint64_t values by a horizontal sum
            {
                u64x8_t new_colors =
                        (static_cast<u64x8_t>(prefixsum) >> u64xconst<0, 8, 16, 24, 32, 40, 48, 56>()) & 0xff;
                const auto shifted = new_colors << (8 * node_ids_by_rank);
                color_uint64 = xs::reduce_add(shifted);
            }
        }


        graph8::set_color(coloring, color_uint64);
    }

} // namespace smallcanon::refine::avx512intrin
#endif
