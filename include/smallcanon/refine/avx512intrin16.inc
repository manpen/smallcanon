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
    class AVX512<AdjMatrix16> {
    public:
        using graph_t = AdjMatrix16;
        using coloring_t = MatchedColoring<graph_t>::coloring_t;

    private:
        const graph_t& external_graph;
        const u16x32_t graph;
        const uint16_t enabled_eq_mask;

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
            enabled_eq_mask(static_cast<uint16_t>((2 << external_graph.num_nodes()) - 1)) {}

        void refine_starting_at(coloring_t& coloring, [[maybe_unused]] node_t start) const noexcept {
            refine(coloring);
        }

        void refine(coloring_t& external_coloring) const noexcept {
            const auto org_colors = zero_extend_low_u8_to_u32(import_color(external_coloring));

            auto degrees = zero_extend_low_u16_to_u32(popcnt(graph));

            const auto org_hash = hash_fmix32(org_colors) ^ hash_fmix32((org_colors + 1) * (degrees + 1) * 0x1237612);
            auto colors = org_hash;
            const auto n = external_graph.num_nodes();

            std::array<uint32_t, 16> hashes;
            for (node_t round = 0; round < n; round++) {
                auto new_colors = hash_fmix32(colors) ^ org_hash;
                xs::store_unaligned(hashes.data(), colors);

                for (node_t node = 0; node < n; node++) {
                    const uint16_t mask = external_graph.row_upto_capacity(node)[0];
                    new_colors = xs::select(xs::batch_bool<uint32_t, arch>::from_mask(mask), new_colors + hashes[node],
                                            new_colors);
                }

                colors = hash_fmix32(new_colors);
            }

            auto fingerprints = (colors & 0x0fff'fff0) | u32cont<0, 1>().as_batch() | (org_colors << 28);
            fingerprints = simd::sort::sort_details::sort_single_batch<16, uint32_t, true>(fingerprints);

            const u8x64_t labels = truncate_u32_to_low_u8(fingerprints & 0xf);

            const auto fps = (fingerprints >> 4);
            const auto fps_shifted = xs::swizzle(fps, u32cont<0, 0, 1>{});
            auto neq_mask = static_cast<uint16_t>((fps != fps_shifted).mask() & enabled_eq_mask);
            assert((neq_mask & 1) == 0);

            auto refined_colors = truncate_u16_to_low_u8(compute_prefixsum_u16x16(neq_mask));
            refined_colors |= (labels << 4);
            refined_colors = simd::sort::sort_details::sort_single_batch<16, uint8_t, true, arch>(refined_colors);

            export_color(external_coloring, refined_colors & 0xf, labels);
        }
    };

} // namespace smallcanon::refine::avx512intrin
#endif
