#pragma once

#include <xsimd/xsimd.hpp>

#if SMALLCANON_WITH_AVX512

#include <smallcanon/adj_matrix.hpp>
#include <smallcanon/coloring.hpp>
#include <smallcanon/simd/sort.hpp>
#include <smallcanon/utility.hpp>

#include <smallcanon/refine/avx512_colors_from_mask.inc>
#include <smallcanon/simd/avx512defs.hpp>

namespace smallcanon::refine::avx512intrin {
    template<>
    class AVX512<AdjMatrix8> {
    public:
        using graph_t = AdjMatrix8;
        using coloring_t = MatchedColoring<graph_t>::coloring_t;

    private:
        const u64x8_t graph;
        const uint64_t disabled_colors;
        const uint8_t enabled_eq_mask;

        static u64x8_t import_graph(const AdjMatrix8& graph) {
            static_assert(std::is_same_v<typename std::decay_t<decltype(graph)>::word_t, uint8_t>);

            // this should become a simple load ... but this version should be more portable.
            uint64_t single_graph = read_le_u64(graph.buffer().data());
            return widen_to_u64(single_graph) * 0x0101010101010101;
        }

        SMALLCANON_ALWAYS_INLINE
        static uint64_t compute_color_mask_vec(uint64_t x) {
            auto colors = u8x64_t(u64x8_t::broadcast(x));
            constexpr uint64_t f = 0x0101010101010101;
            return (colors == u8x64_t(u64x8cont<0, f>().as_batch())).mask();
        }

        SMALLCANON_ALWAYS_INLINE
        static u64x8_t pack_low3_from_bytes(u64x8_t x) noexcept {
            x = (x | (x >> 5)) & 0x003f003f003f003full;
            x = (x | (x >> 10)) & 0x00000fff00000fffull;
            x = (x | (x >> 20)) & 0x0000000000ffffffull;

            return x;
        }

        SMALLCANON_ALWAYS_INLINE
        static u32x8_t compute_fingerprints(const u64x8_t vgraph, const uint64_t color_uint64,
                                            const uint64_t disabled_colors) noexcept {

            uint64_t color_mask = compute_color_mask_vec(color_uint64);

            const u64x8_t masked = vgraph & color_mask;
            const u64x8_t color_counts = u64x8_t(popcnt(u8x64_t(masked)));
            const auto color_counts32 = shrink_to_u32(pack_low3_from_bytes(color_counts) << 3);

            const auto colors_at_msb = widen_to_u32(color_uint64 | disabled_colors) << 28;

            return color_counts32 | colors_at_msb | u32x8cont<0, 1>();
        }


    public:
        explicit AVX512(const graph_t& egraph) :
            graph(import_graph(egraph)),
            disabled_colors((egraph.num_nodes() == 8) ? 0 : (~static_cast<uint64_t>(0)) << (8 * egraph.num_nodes())),
            enabled_eq_mask(static_cast<uint8_t>((2 << egraph.num_nodes()) - 1)) //
        {}

        void refine_starting_at(coloring_t& coloring, [[maybe_unused]] node_t start) const noexcept {
            refine(coloring);
        }

        void refine(Coloring8& external_coloring) const noexcept {
            auto color_uint64 = colors_to_native(external_coloring);

            // Store the previous value of `same_fingerprint_as_pred`.
            // To force completion of at least one iteration, we choose an impossible value:
            // LSB can never be one, so this will always be false in the first iteration
            uint8_t prev_new_color_mask = 1;

            // We refine until either (i) the coloring is discrete or (ii) the coloring is converged.
            bool is_discrete = false;
            uint64_t node_ids_by_rank;
            while (!is_discrete) {
                // Each fingerprint uses 64 bits. The MSB 3 bits encode the old color, the lowest 3 bits the node id.
                // The remaining bits are implementation specific and encode some (permutation invariant) information
                // about each node's neighborhood.
                const auto fingerprints = compute_fingerprints(graph, color_uint64, disabled_colors);

                // By sorting the fingerprint, we move identical fingerprints (up to node id in the LSBs) next to each
                // other
                const auto sorted_fingerprints = simd::sort::sort_details::sort_single_batch<8>(fingerprints);

                // Split fingerprints into node ids and remainder
                node_ids_by_rank = shrink_to_u8(sorted_fingerprints & 7);
                const auto fingerprints_wo_node_ids = sorted_fingerprints >> 3;

                // We compare each fingerprint to its predecessor (i.e. the element with next lower rank).
                // We finally obtain an 8-bit integer (same_fingerprint_as_pred), where a one in the i-th bit
                // encodes that a new color starts with the node of rank i.
                const auto shifted_fps = xs::swizzle(fingerprints_wo_node_ids, u32x8cont<0, 0, 1>{});
                const auto new_color_mask =
                        static_cast<uint8_t>((fingerprints_wo_node_ids != shifted_fps).mask()) & enabled_eq_mask;
                assert((new_color_mask & 1) == 0);


                // Check for convergence. Observe that we can only refine but not rearrange colors. Hence, the same
                // pattern in same_fingerprint_as_pred implies the same values in node_ids_by_rank.
                if (new_color_mask == prev_new_color_mask) {
                    // computation converged; break
                    break;
                }
                prev_new_color_mask = new_color_mask;

                const auto new_colors = colors_from_mask8_scalar(new_color_mask);
                auto combined = u8x16_t(u64x2_t::broadcast(new_colors | (node_ids_by_rank << 4)));
                combined = smallcanon::simd::sort::sort_details::sort_single_batch<8>(combined);
                color_uint64 = xs::get<0>(u64x2_t(combined)) & 0x0f0f0f0f0f0f0f0f;

                is_discrete = (new_color_mask | 1) == enabled_eq_mask;
            }

            native_to_colors(external_coloring, color_uint64);
            native_to_labels(external_coloring, node_ids_by_rank);
        }
    };

} // namespace smallcanon::refine::avx512intrin
#endif
