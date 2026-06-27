#pragma once

#include <array>

#include <xsimd/xsimd.hpp>

#if SMALLCANON_WITH_AVX512

#include <smallcanon/simd/avx512defs.hpp>
#include <smallcanon/simd/prefix.hpp>

namespace smallcanon::refine::avx512intrin {

    /// Builds a lookup table for colors_from_mask8().
    ///
    /// The input mask marks ranks at which a new fingerprint block starts. Bit zero is expected to be clear
    /// because the first rank is always the first color block. For each byte lane i, the resulting packed
    /// uint64_t contains the highest set mask bit at or before i; equivalently, each rank receives the
    /// start rank of its current block.
    /// Example: mask 0b00101000 produces bytes [0, 0, 0, 3, 3, 5, 5, 5].
    consteval static std::array<uint64_t, 128> colors_from_mask8_lookup() {
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

    /// Converts an 8-rank new-color mask into per-rank color ids.
    ///
    /// The mask uses bit i to indicate that rank i starts a new color block. The return value contains eight
    /// identical packed u8x8 color vectors, one in each 64-bit lane of the u8x64 result, so callers can
    /// reinterpret it as either bytes or uint64_t lanes. Color ids are block start ranks, not dense color
    /// numbers.
    static u8x64_t colors_from_mask8(uint8_t x) noexcept {
        constexpr auto table = colors_from_mask8_lookup();
        return u8x64_t(u64x8_t::broadcast(table[x / 2]));
    }

    /// Converts a 16-rank new-color mask into per-rank color ids.
    ///
    /// The mask uses bit i to indicate that rank i starts a new color block. The first 16 byte lanes of the
    /// return value hold the most recent block-start rank for each rank; higher byte lanes are unspecified
    /// for this helper's callers. Color ids are block start ranks, matching colors_from_mask8().
    static u8x16_t colors_from_mask16(const uint16_t neq_mask) noexcept {
        constexpr auto node_ids = u8x16cont<0, 1>();
        const auto select_mask = xs::batch_bool<uint8_t, arch128>::from_mask(neq_mask);
        const auto new_start = xs::select(select_mask, node_ids.as_batch(), xs::broadcast<uint8_t, arch128>(0));
        return simd::incl_prefix_max<uint8_t, arch128, 16>(new_start);
    }

} // namespace smallcanon::refine::avx512intrin

#endif
