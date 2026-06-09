#pragma once

#include <immintrin.h>

#include "smallcanon/adj_matrix.hpp"
#include "smallcanon/coloring.hpp"
#include "smallcanon/simd/sort.hpp"

#include <xsimd/xsimd.hpp>

#if XSIMD_WITH_AVX512F
namespace smallcanon::refine::avx512intrin {
    namespace xs = xsimd;

    using arch = xs::best_arch;
    using u64x8_t = xs::batch<uint64_t, arch>;
    using u32x16_t = xs::batch<uint32_t, arch>;
    using u16x32_t = xs::batch<uint16_t, arch>;

    using u8x64_t = xs::batch<uint8_t, arch>;

    static_assert(u8x64_t::size == 64);
    static_assert(u16x32_t::size == 32);
    static_assert(u32x16_t::size == 16);
    static_assert(u64x8_t::size == 8);


    template<typename SM, typename SC>
    void refine(const AdjMatrix<SM>& graph, Coloring<SC>& coloring) {
        static_assert(false);
    }

    uint64_t load_color8(const Coloring8& coloring) {
        uint64_t color = 0;
        for (int i = 0; i < 8; ++i) {
            color <<= 8;
            color |= static_cast<uint8_t>(coloring.get_color(i));
        }
        return color;
    }

    __m512i load_graph8(const AdjMatrix8& graph) {
        alignas(64) std::array<uint64_t, 8> rows{};

        for (int i = 0; i < graph.num_nodes(); ++i) {
            rows[i] = static_cast<uint64_t>(*graph.row(i).data()) * 0x0101010101010101;
        }

        return _mm512_load_epi64(rows.data());
    }


    void set_color8(Coloring8& coloring, uint64_t color) {
        for (int i = 0; i < 8; ++i) {
            coloring.set_color(i, static_cast<uint8_t>(color >> (8 * i)));
        }
    }

    template<>
    void refine(const AdjMatrix8& graph, Coloring8& coloring) {
        auto color_uint64 = load_color8(coloring);
        const u64x8_t vgraph = load_graph8(graph);

        auto prev_colors = color_uint64;
        for (int i = 0; i < graph.num_nodes() - 1; ++i) {
            u64x8_t fingerprints;
            {
                uint64_t color_mask = 0;
                for (int i = 0; i < 8; ++i) {
                    color_mask |= uint64_t{1} << (i + 8 * static_cast<uint8_t>(color_uint64 >> 8 * i));
                }


                const u64x8_t color_maskx = color_mask;

                const u64x8_t masked = vgraph & color_maskx;
                const u64x8_t color_counts = _mm512_popcnt_epi8(masked);
                const auto color_counts32 =
                        ((color_counts & 0x000000000FFFFFFFF) << 4) | ((color_counts & 0xFFFFFFFF00000000) >> 25);

                const auto with_node_ids =
                        color_counts32 | xsimd::batch_constant<uint64_t, arch, 0, 1, 2, 3, 4, 5, 6, 7>();

                const u64x8_t colors_at_msb = xs::swizzle(
                        xs::broadcast(color_uint64),
                        xsimd::batch_constant<uint64_t, arch, uint64_t{0x0} << 60, uint64_t{0x1} << 60,
                                              uint64_t{0x2} << 60, uint64_t{0x3} << 60, uint64_t{0x4} << 60,
                                              uint64_t{0x5} << 60, uint64_t{0x6} << 60, uint64_t{0x7} << 60>());

                fingerprints = with_node_ids | colors_at_msb;
            }

            fingerprints = simd::sort::sort_details::sort_single_batch<8>(fingerprints);

            u8x64_t prefixsum;
            {
                const auto fp_without_nodeid = fingerprints & 0xfffffffffffffff0;
                const u64x8_t shifted_fps =
                        xs::swizzle(fp_without_nodeid, xsimd::batch_constant<uint64_t, arch, 0, 0, 1, 2, 3, 4, 5, 6>{});

                const uint8_t eq_mask = static_cast<uint8_t>((fp_without_nodeid == shifted_fps).mask());
                u8x64_t masked = xs::bitwise_cast<uint8_t>(xs::broadcast_as<uint64_t>(0x7f3f1f0f07030100)) & eq_mask;
                prefixsum = _mm512_popcnt_epi8(masked);
            }

            {
                const auto node_ids = fingerprints & 0xf;
                const u64x8_t new_colors = static_cast<u64x8_t>(prefixsum) >> (8 * node_ids);
                color_uint64 = _mm_cvtsi128_si64(_mm512_cvtepi64_epi8(new_colors));
            }

            // quite if converged
            if (color_uint64 == prev_colors) {
                break;
            }
            prev_colors = color_uint64;
        }


        set_color8(coloring, color_uint64);
    }

} // namespace smallcanon::refine::avx512intrin
#endif
