#pragma once

#include <xsimd/xsimd.hpp>

#if XSIMD_WITH_AVX512F

#include <smallcanon/adj_matrix.hpp>
#include <smallcanon/coloring.hpp>
#include <smallcanon/simd/sort.hpp>
#include <smallcanon/utility.hpp>

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

        static uint64_t import_color(const Coloring8& coloring) {
            return read_le_u64(coloring.colors().data());
        }

        static void export_color(Coloring8& coloring, uint64_t color, uint64_t labels) {
            static_assert(std::is_same_v<typename std::decay_t<decltype(coloring)>::scolor_t, uint8_t>);
            write_le_u64(coloring.colors().data(), color);
            write_le_u64(coloring.labels().data(), labels);
        }

        static u64x8_t import_graph(const AdjMatrix8& graph) {
            static_assert(std::is_same_v<typename std::decay_t<decltype(graph)>::word_t, uint8_t>);

            // this should become a simple load ... but this version should be more portable.
            uint64_t single_graph = read_le_u64(graph.buffer().data());
            const auto rows = (xs::broadcast(single_graph) >> u64cont<0, 8>()) & 0xff;
            return rows * 0x0101010101010101;
        }

        static uint64_t compute_color_mask_vec(uint64_t x) {
            u8x64_t colors = u8x64_t(xs::broadcast(x));
            constexpr uint64_t f = 0x0101010101010101;
            return (colors == u8x64_t(u64cont<0, f>().as_batch())).mask();
        }

        static u64x8_t compute_fingerprints(const u64x8_t vgraph, const uint64_t color_uint64,
                                            const uint64_t disabled_colors) {

            uint64_t color_mask = compute_color_mask_vec(color_uint64);

            const u64x8_t masked = vgraph & color_mask;
            const u64x8_t color_counts = u64x8_t(popcnt(u8x64_t(masked)));
            const auto color_counts32 = //
                    ((color_counts & 0x00000000FFFFFFFF) << 4) | //
                    ((color_counts & 0xFFFFFFFF00000000) >> 25);

            const auto with_node_ids = color_counts32 | u64cont<0, 1>();

            const u64x8_t colors_at_msb =
                    (xs::broadcast(color_uint64 | disabled_colors) << u64cont<60, 52>()) & 0xf000000000000000;

            return with_node_ids | colors_at_msb;
        }


        // In preliminary tests, the lookup table is ~1ns faster, but at the cost of using 2kb of constants.
        // Investigate later again
        consteval static std::array<uint64_t, 128> popcnt_lookup() {
            std::array<uint64_t, 128> result{};
            for (uint64_t idx = 0; idx < 256; idx += 2) {
                uint64_t popcnt = 0;
                uint64_t pattern = idx;
                for (uint64_t i = 0; pattern; ++i, pattern >>= 1) {
                    if (!(pattern & 1))
                        continue;
                    for (uint j = i; j < 8; ++j) {
                        popcnt &= ~(uint64_t(0xff) << (8 * j));
                        popcnt |= uint64_t(i) << 8 * j;
                    }
                }
                result[idx / 2] = popcnt;
            }
            return result;
        }

        static u8x64_t compute_prefixsum_u8x8(uint8_t x) {
            constexpr auto table = popcnt_lookup();
            return u8x64_t(xs::broadcast(table[x / 2]));
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
            auto color_uint64 = import_color(external_coloring);

            // Store the previous value of `same_fingerprint_as_pred`.
            // To force completion of at least one iteration, we choose an impossible value:
            // LSB can never be one, so this will always be false in the first iteration
            uint8_t prev_new_color_mask = 1;

            // We refine until either (i) the coloring is discrete or (ii) the coloring is converged.
            bool is_discrete = false;
            u64x8_t node_ids_by_rank;
            while (!is_discrete) {
                // Each fingerprint uses 64 bits. The MSB 3 bits encode the old color, the lowest 3 bits the node id.
                // The remaining bits are implementation specific and encode some (permutation invariant) information
                // about each node's neighborhood.
                const auto fingerprints = compute_fingerprints(graph, color_uint64, disabled_colors);

                // By sorting the fingerprint, we move identical fingerprints (up to node id in the LSBs) next to each
                // other
                const auto sorted_fingerprints = simd::sort::sort_details::sort_single_batch<8>(fingerprints);

                // Split fingerprints into node ids and remainder
                node_ids_by_rank = sorted_fingerprints & 0xf;
                const auto fingerprints_wo_node_ids = sorted_fingerprints & 0xfffffffffffffff0;

                uint8_t new_color_mask;
                {
                    // We compare each fingerprint to its predecessor (i.e. the element with next lower rank).
                    // We finally obtain an 8-bit integer (same_fingerprint_as_pred), where a one in the i-th bit
                    // encodes that a new color starts with the node of rank i.
                    const u64x8_t shifted_fps = xs::swizzle(fingerprints_wo_node_ids, u64cont<0, 0, 1>{});
                    new_color_mask =
                            static_cast<uint8_t>((fingerprints_wo_node_ids != shifted_fps).mask()) & enabled_eq_mask;
                    assert((new_color_mask & 1) == 0);


                    // Check for convergence. Observe that we can only refine but not rearrange colors. Hence, the same
                    // pattern in same_fingerprint_as_pred implies the same values in node_ids_by_rank.
                    if (new_color_mask == prev_new_color_mask) {
                        // computation converged; break
                        break;
                    }

                    prev_new_color_mask = new_color_mask;
                }

                // we have 8 copies of prefix sums of u8x8
                const auto prefixsum = compute_prefixsum_u8x8(new_color_mask);

                // nice trick to avoid sorting back from fingerprint order back to node id order:
                // we interpret each prefixsum copy as a uint64_t. In i-th lane now takes care of
                // the node u of rank i and moves its color to the u-th byte while zeroing out all others.
                // Then we recombine the 8 uint64_t values by a horizontal sum
                {
                    u64x8_t new_colors = (static_cast<u64x8_t>(prefixsum) >> u64cont<0, 8>()) & 0xff;
                    const auto shifted = new_colors << (8 * node_ids_by_rank);
                    color_uint64 = xs::reduce_add(shifted);
                }

                is_discrete = (new_color_mask | 1) == enabled_eq_mask;
            }

            const uint64_t labels = xs::reduce_add(node_ids_by_rank << u64cont<0, 8>());

            export_color(external_coloring, color_uint64, labels);
        }
    };

} // namespace smallcanon::refine::avx512intrin
#endif
