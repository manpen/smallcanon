#pragma once

#include <array>

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
    class AVX512<AdjMatrix16> {
    public:
        using graph_t = AdjMatrix16;
        using coloring_t = MatchedColoring<graph_t>::coloring_t;

    private:
        const graph_t& external_graph;
        const u16x16_t graph;
        const u32x16_t degrees;
        const uint16_t enabled_eq_mask;

        struct First16Lanes {
            static constexpr bool get(unsigned i, unsigned) {
                return i < 16;
            }
        };

        u8x16_t import_color(const coloring_t& external_coloring) const noexcept {
            static_assert(std::is_same_v<coloring_t::storage_t::scolor_t, uint8_t>);

            const auto colors = colors_to_native(external_coloring);

            const auto active = b8x16_t::from_mask((1 << external_graph.num_nodes()) - 1);
            return xs::select(active, colors, u8x16_t::broadcast(0xff));
        }

        static u16x32_t compute_prefixsum_u16x16(uint16_t inp) {
            struct PrefixSumMasks {
                static constexpr uint16_t get(unsigned i, unsigned) {
                    return static_cast<uint16_t>((static_cast<uint64_t>(2) << (i % 16)) - 1);
                }
            };

            const auto bitmask = xs::make_batch_constant<uint16_t, PrefixSumMasks, arch>().as_batch();
            const auto masked = xs::broadcast<uint16_t, arch>(inp) & bitmask;
            return popcnt(masked);
        }

        struct HashFingerprint {
            SMALLCANON_AVX512_ALIGNED std::array<uint32_t, 16> scratch_hashes;

            using color_table_c = u32x16const<0xfce9cc97u, 0x0a405340u, 0xa387faf7u, 0x3ebb5d4cu, //
                                              0xc7721bdau, 0xdf353671u, 0xd1dd1a87u, 0xa2ed0bf0u, //
                                              0x9831f026u, 0xf054bdf6u, 0x4633a43cu, 0xdbceb161u, //
                                              0x857ce90bu, 0x078e8307u, 0x5df3310fu, 0x635cda3eu //
                                              >;
            using degree_table_c = u32x16const<0x562cde78u, 0xe467a339u, 0x7af4fd6fu, 0x84fb128au, //
                                               0x92316d7au, 0x4805ef46u, 0x0fc1a065u, 0xd990e19du, //
                                               0xe78a1112u, 0xe3cffe07u, 0x883c11b6u, 0x732de3aau, //
                                               0x01424c6eu, 0xd523b8a6u, 0x797fdc9du, 0x1fb5f7d4u //
                                               >;

            const graph_t& external_graph;
            u32x16_t color_hashes;

            HashFingerprint(const graph_t& egraph, u32x16_t degrees, u32x16_t colors) :
                external_graph(egraph), color_hashes(xs::swizzle(color_table_c().as_batch(), colors) //
                                                     ^ xs::swizzle(degree_table_c().as_batch(), degrees)) {}

            HashFingerprint(const graph_t& egraph, u32x16_t colors) :
                external_graph(egraph), color_hashes(xs::swizzle(color_table_c().as_batch(), colors)) {}


            u32x16_t fingerprints(u32x16_t ranks, node_t n) noexcept {
                xs::store_aligned(scratch_hashes.data(), color_hashes);

                color_hashes = hash_fmix32(color_hashes);

                const uint16_t *buffer = external_graph.buffer().data();
#define STEP32(u)                                                                                                      \
    color_hashes = _mm512_mask_add_epi32(color_hashes, buffer[u], color_hashes, u32x16_t::broadcast(scratch_hashes[u]));

                STEP32(0);
                STEP32(1);
                STEP32(2);
                STEP32(3);
                STEP32(4);
                STEP32(5);
                STEP32(6);
                STEP32(7);
                STEP32(8);
                STEP32(9);
                STEP32(10);
                STEP32(11);
                STEP32(12);
                STEP32(13);
                STEP32(14);
                STEP32(15);
#undef STEP32

                return (color_hashes & 0x0fff'fff0) // new colors
                       | u32x16cont<0, 1>().as_batch() // ranks
                       | (ranks << 28); // old colors
            }
        };


    public:
        explicit AVX512(const graph_t& egraph) :
            external_graph(egraph), graph(matrix_to_native(egraph)), degrees(widen_to_u32(popcnt(graph))),
            enabled_eq_mask(static_cast<uint16_t>((2 << external_graph.num_nodes()) - 1)) {}

        void refine_starting_at(coloring_t& external_coloring, [[maybe_unused]] node_t start) noexcept {
            refine_impl<false>(external_coloring);
        }

        void refine(coloring_t& external_coloring) noexcept {
            refine_impl<true>(external_coloring);
        }

        template<bool First>
        void refine_impl(coloring_t& external_coloring) noexcept {
            const auto n = external_graph.num_nodes();

            uint16_t prev_neq_mask = 1; // impossible value

            u8x16_t colors_8b = import_color(external_coloring);
            auto colors_wide = widen_to_u16(colors_8b);

            u8x16_t labels_in_part_order;

            auto hasher = HashFingerprint16(external_graph, colors_wide);
            for ([[maybe_unused]] node_t round = 0; true; round++) {
                auto fingerprints = hasher.fingerprints(colors_wide, n);
                fingerprints = simd::sort::sort_details::sort_single_batch<16, uint16_t, true>(fingerprints);

                const auto fps = (fingerprints >> 4);
                const auto fps_shifted = xs::swizzle(fps, u16x16cont<0, 0, 1>{});

                const auto neq_mask = static_cast<uint16_t>((fps != fps_shifted).mask() & enabled_eq_mask);
                assert((neq_mask & 1) == 0);

                // solution has converged
                if (neq_mask == prev_neq_mask) {
                    break;
                }
                prev_neq_mask = neq_mask;

                labels_in_part_order = shrink_to_u8(fingerprints & 0xf);

                {
                    auto refined_colors = colors_from_mask16(neq_mask);
                    refined_colors |= (labels_in_part_order << 4);
                    refined_colors = simd::sort::sort_details::sort_single_batch<16>(refined_colors);
                    colors_8b = refined_colors & 0xf;
                }

                // solution is discrete
                if ((neq_mask | 1) == enabled_eq_mask) {
                    break;
                }

                colors_wide = widen_to_u16(colors_8b);
            }

            native_to_colors(external_coloring, colors_8b);
            native_to_labels(external_coloring, labels_in_part_order);
        }
    };

} // namespace smallcanon::refine::avx512intrin
#endif
