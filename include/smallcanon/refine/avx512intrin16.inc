#pragma once

#include <array>

#include <xsimd/xsimd.hpp>

#if XSIMD_WITH_AVX512F

#include <smallcanon/adj_matrix.hpp>
#include <smallcanon/coloring.hpp>
#include <smallcanon/simd/sort.hpp>
#include <smallcanon/utility.hpp>

#include <smallcanon/simd/avx512defs.hpp>

namespace smallcanon::refine::avx512intrin {
    template<>
    class AVX512<AdjMatrix16> {
    public:
        using graph_t = AdjMatrix16;
        using coloring_t = MatchedColoring<graph_t>::coloring_t;

    private:
        const graph_t& external_graph;
        const u16x32_t graph;
        const u32x16_t degrees;
        const uint16_t enabled_eq_mask;

        using color_table_c = u32cont<0xfce9cc97u, 0x0a405340u, 0xa387faf7u, 0x3ebb5d4cu, //
                                      0xc7721bdau, 0xdf353671u, 0xd1dd1a87u, 0xa2ed0bf0u, //
                                      0x9831f026u, 0xf054bdf6u, 0x4633a43cu, 0xdbceb161u, //
                                      0x857ce90bu, 0x078e8307u, 0x5df3310fu, 0x635cda3eu //
                                      >;
        using degree_table_c = u32cont<0x562cde78u, 0xe467a339u, 0x7af4fd6fu, 0x84fb128au, //
                                       0x92316d7au, 0x4805ef46u, 0x0fc1a065u, 0xd990e19du, //
                                       0xe78a1112u, 0xe3cffe07u, 0x883c11b6u, 0x732de3aau, //
                                       0x01424c6eu, 0xd523b8a6u, 0x797fdc9du, 0x1fb5f7d4u //
                                       >;

        struct First16Lanes {
            static constexpr bool get(unsigned i, unsigned) {
                return i < 16;
            }
        };

        u8x64_t import_color(const coloring_t& external_coloring) const noexcept {
            static_assert(std::is_same_v<coloring_t::storage_t::scolor_t, uint8_t>);
            constexpr auto mask = xs::make_batch_bool_constant<uint8_t, First16Lanes, arch>();

            const auto colors = xs::load<uint8_t, arch>(external_coloring.colors().data(), mask, xs::unaligned_mode{});
            const auto active = (u8cont<0, 1>().as_batch() < static_cast<uint8_t>(external_graph.num_nodes()));

            return xs::select(active, colors, xs::broadcast<uint8_t, arch>(0xff));
        }

        static u32x16_t zero_extend_low_u8_to_u32(const u8x64_t values) noexcept {
            return {_mm512_cvtepu8_epi32(_mm512_castsi512_si128(values))};
        }

        static u32x16_t zero_extend_low_u16_to_u32(const u16x32_t values) noexcept {
            return {_mm512_cvtepu16_epi32(_mm512_castsi512_si256(values))};
        }

        static u8x64_t truncate_u32_to_low_u8(const u32x16_t values) noexcept {
            return {_mm512_zextsi128_si512(_mm512_cvtepi32_epi8(values))};
        }

        static u8x64_t truncate_u16_to_low_u8(const u16x32_t values) noexcept {
            return {_mm512_zextsi256_si512(_mm512_cvtepi16_epi8(values))};
        }

        static void export_color(coloring_t& external_coloring, u8x64_t coloring, u8x64_t labels) {
            static_assert(std::is_same_v<coloring_t::storage_t::scolor_t, uint8_t>);
            constexpr auto mask = xs::make_batch_bool_constant<uint8_t, First16Lanes, arch>();
            xs::store(external_coloring.colors().data(), coloring, mask, xs::unaligned_mode{});
            xs::store(external_coloring.labels().data(), labels, mask, xs::unaligned_mode{});
        }

        static u16x32_t import_graph(const graph_t& external_graph) {
            static_assert(std::is_same_v<graph_t::storage_t::word_t, uint16_t>);
            constexpr auto mask = xs::make_batch_bool_constant<uint16_t, First16Lanes, arch>();
            return xs::load(external_graph.buffer().data(), mask, xs::unaligned_mode{});
        }

        static u16x32_t compute_prefixsum_u16x16(uint16_t inp) {
            struct PrefixSumMasks {
                static constexpr uint16_t get(unsigned i, unsigned) {
                    return static_cast<uint16_t>((static_cast<uint64_t>(2) << (i % 16)) - 1);
                }
            };

            const auto bitmask = xs::make_batch_constant<uint16_t, PrefixSumMasks, arch>().as_batch();
            const auto masked = xs::broadcast(inp) & bitmask;
            return popcnt(masked);
        }

    public:
        explicit AVX512(const graph_t& egraph) :
            external_graph(egraph), graph(import_graph(egraph)),
            degrees(xs::swizzle(degree_table_c().as_batch(), zero_extend_low_u16_to_u32(popcnt(graph)))),
            enabled_eq_mask(static_cast<uint16_t>((2 << external_graph.num_nodes()) - 1)) {}

        void refine_starting_at(coloring_t& external_coloring, [[maybe_unused]] node_t start) const noexcept {
            refine_impl(external_coloring, 0);
        }

        void refine(coloring_t& external_coloring) const noexcept {
            refine_impl(external_coloring, 8);
        }


        void refine_impl(coloring_t& external_coloring, node_t min_rounds) const noexcept {
            auto ranks = zero_extend_low_u8_to_u32(import_color(external_coloring));

            const auto org_hash = xs::swizzle(color_table_c().as_batch(), ranks) ^ degrees;

            auto colors = org_hash;
            const auto n = external_graph.num_nodes();


            u32x16_t fingerprints;
            uint16_t neq_mask;

            uint16_t prev_neq_mask = 1; // impossible value

            u8x64_t labels;
            u8x64_t refined_colors;


            SMALLCANON_AVX512_ALIGNED
            std::array<uint32_t, 16> hashes;
            for (node_t round = 0; true; round++) {
                xs::store_aligned(hashes.data(), colors);
                auto new_colors = hash_fmix32(colors);

#define SMALLCANON_AVX512G16_STEP(u)                                                                                   \
    {                                                                                                                  \
        const uint16_t mask = external_graph.row_upto_capacity(u)[0];                                                  \
        new_colors = xs::select(xs::batch_bool<uint32_t, arch>::from_mask(mask), new_colors + hashes[u], new_colors);  \
    }
                SMALLCANON_AVX512G16_STEP(0)
                SMALLCANON_AVX512G16_STEP(1)
                SMALLCANON_AVX512G16_STEP(2)
                SMALLCANON_AVX512G16_STEP(3)
                SMALLCANON_AVX512G16_STEP(4)
                SMALLCANON_AVX512G16_STEP(5)
                SMALLCANON_AVX512G16_STEP(6)
                SMALLCANON_AVX512G16_STEP(7)
                SMALLCANON_AVX512G16_STEP(8)

                for (node_t node = 9; node < n; node++) {
                    SMALLCANON_AVX512G16_STEP(node);
                }
#undef SMALLCANON_AVX512G16_STEP

                colors = new_colors;

                fingerprints = (colors & 0x0fff'fff0) | u32cont<0, 1>().as_batch() | (ranks << 28);
                fingerprints = simd::sort::sort_details::sort_single_batch<16, uint32_t, true>(fingerprints);

                const auto fps = (fingerprints >> 4);
                const auto fps_shifted = xs::swizzle(fps, u32cont<0, 0, 1>{});

                neq_mask = static_cast<uint16_t>((fps != fps_shifted).mask() & enabled_eq_mask);
                assert((neq_mask & 1) == 0);

                if ((neq_mask | 1) == enabled_eq_mask) {
                    // std::cout << 'd' << round << std::endl;
                    //  discrete
                    break;
                }

                if (neq_mask == prev_neq_mask) {
                    // std::cout << 'c' << round << std::endl;
                    //  converged
                    break;
                }

                prev_neq_mask = neq_mask;


                labels = truncate_u32_to_low_u8(fingerprints & 0xf);
                refined_colors = truncate_u16_to_low_u8(compute_prefixsum_u16x16(neq_mask));
                refined_colors |= (labels << 4);
                refined_colors = simd::sort::sort_details::sort_single_batch<16, uint8_t, true, arch>(refined_colors);
                refined_colors &= 0xf;

                ranks = zero_extend_low_u8_to_u32(refined_colors);
            }


            export_color(external_coloring, refined_colors, labels);
        }
    };

} // namespace smallcanon::refine::avx512intrin
#endif
