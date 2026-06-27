#pragma once

#include <smallcanon/simd/continuation_helper.hpp>
#include <smallcanon/utility.hpp>
#include <xsimd/xsimd.hpp>

#define SMALLCANON_AVX512_ALIGNED alignas(64)

namespace smallcanon::simd::avx512defs {
#if XSIMD_WITH_AVX512F


#define SMALLCANON_WITH_AVX512 1
    namespace xs = xsimd;

    using arch = xs::best_arch;
    using arch128 = xs::avx512vl_128;
    using arch256 = xs::avx512vl_256;

#define SMALLCANON_NEW_POPCNT(T, WIDTH, AWIDTH)                                                                        \
    SMALLCANON_ALWAYS_INLINE static T popcnt(const T v) noexcept {                                                     \
        return _mm##AWIDTH##_popcnt_epi##WIDTH(v);                                                                     \
    }

#define SMALLCANON_NEW_BATCH(WIDTH, NUM, AWIDTH, ARCH)                                                                 \
    using u##WIDTH##x##NUM##_t = xs::batch<uint##WIDTH##_t, ARCH>;                                                     \
    static_assert(u##WIDTH##x##NUM##_t::size == NUM);                                                                  \
    template<uint##WIDTH##_t... Vs>                                                                                    \
    using u##WIDTH##x##NUM##const = xs::batch_constant<uint##WIDTH##_t, ARCH, Vs...>;                                  \
    template<uint##WIDTH##_t... Vs>                                                                                    \
        requires(sizeof...(Vs) > 1)                                                                                    \
    using u##WIDTH##x##NUM##cont = typename continuation::const_continuation<uint##WIDTH##_t, ARCH, Vs...>::value_t;   \
    SMALLCANON_NEW_POPCNT(u##WIDTH##x##NUM##_t, WIDTH, AWIDTH)

#define SMALLCANON_NEW_BATCHES(WIDTH, N512, N256, N128)                                                                \
    SMALLCANON_NEW_BATCH(WIDTH, N512, 512, arch);                                                                      \
    SMALLCANON_NEW_BATCH(WIDTH, N256, 256, arch256);                                                                   \
    SMALLCANON_NEW_BATCH(WIDTH, N128, , arch128);

    SMALLCANON_NEW_BATCHES(64, 8, 4, 2);
    SMALLCANON_NEW_BATCHES(32, 16, 8, 4);
    SMALLCANON_NEW_BATCHES(16, 32, 16, 8);
    SMALLCANON_NEW_BATCHES(8, 64, 32, 16);

#undef SMALLCANON_NEW_BATCHES
#undef SMALLCANON_NEW_BATCH

    SMALLCANON_ALWAYS_INLINE static u32x8_t shrink_to_u32(u64x8_t d) noexcept {
        return _mm512_cvtepi64_epi32(d);
    }

    SMALLCANON_ALWAYS_INLINE static u64x8_t widen_to_u64(u32x8_t d) noexcept {
        return _mm512_cvtepu32_epi64(d);
    }

    SMALLCANON_ALWAYS_INLINE static u32x16_t widen_to_u32(const u8x16_t values) noexcept {
        return {_mm512_cvtepu8_epi32(values)};
    }

    SMALLCANON_ALWAYS_INLINE static u32x16_t widen_to_u32(const u16x16_t values) noexcept {
        return {_mm512_cvtepu16_epi32(values)};
    }

    SMALLCANON_ALWAYS_INLINE static u8x16_t shrink_to_u8(const u32x16_t values) noexcept {
        return {_mm512_cvtepi32_epi8(values)};
    }

    SMALLCANON_ALWAYS_INLINE static u8x16_t shrink_to_u8(const u16x16_t values) noexcept {
        return {_mm256_cvtepi16_epi8(values)};
    }

    SMALLCANON_ALWAYS_INLINE u32x16_t hash_fmix32(u32x16_t h) {
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;

        return h;
    }
#else
#define SMALLCANON_WITH_AVX512 0
#endif
} // namespace smallcanon::simd::avx512defs
