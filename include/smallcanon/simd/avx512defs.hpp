#pragma once

#include <smallcanon/simd/continuation_helper.hpp>
#include <xsimd/xsimd.hpp>

namespace smallcanon::simd::avx512defs {
#if XSIMD_WITH_AVX512F
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

    template<uint8_t... Vs>
        requires(sizeof...(Vs) > 1)
    using u8cont = typename continuation::const_continuation<uint8_t, arch, Vs...>::value_t;
    template<uint16_t... Vs>
        requires(sizeof...(Vs) > 1)
    using u16cont = typename continuation::const_continuation<uint16_t, arch, Vs...>::value_t;
    template<uint32_t... Vs>
        requires(sizeof...(Vs) > 1)
    using u32cont = typename continuation::const_continuation<uint32_t, arch, Vs...>::value_t;
    template<uint64_t... Vs>
        requires(sizeof...(Vs) > 1)
    using u64cont = typename continuation::const_continuation<uint64_t, arch, Vs...>::value_t;

    static_assert(u8x64_t::size == 64);
    static_assert(u16x32_t::size == 32);
    static_assert(u32x16_t::size == 16);
    static_assert(u64x8_t::size == 8);

    inline u8x64_t popcnt(const u8x64_t v) noexcept {
        return _mm512_popcnt_epi8(v);
    }
    inline u16x32_t popcnt(const u16x32_t v) noexcept {
        return _mm512_popcnt_epi16(v);
    }
    inline u32x16_t popcnt(const u32x16_t v) noexcept {
        return _mm512_popcnt_epi32(v);
    }
    inline u64x8_t popcnt(const u64x8_t v) noexcept {
        return _mm512_popcnt_epi64(v);
    }


    SMALLCANON_ALWAYS_INLINE u32x16_t hash_fmix32(u32x16_t h) {
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;

        return h;
    }

#endif
} // namespace smallcanon::simd::avx512defs
