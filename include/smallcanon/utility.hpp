#pragma once

#include <cstdint>
#include <iostream>

#define console_bold "\033[1m"
#define console_green "\033[32m"
#define console_blue "\033[36m"
#define console_orange "\033[33m"
#define console_bright_blue "\033[96m"
#define console_magenta "\033[35m"
#define console_red "\033[33m"
#define console_neutral "\033[0m"

#if defined(_MSC_VER)
#define SMALLCANON_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define SMALLCANON_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define SMALLCANON_ALWAYS_INLINE inline
#endif

#ifdef PRINT_DEBUG
#define DEBUG_STREAM std::clog
#else
#define DEBUG_STREAM                                                                                                   \
    if (true) {                                                                                                        \
    } else                                                                                                             \
        std::clog
#endif


constexpr SMALLCANON_ALWAYS_INLINE uint64_t read_le_u64(const unsigned char *p) {
    return (uint64_t{p[0]} << 0) | (uint64_t{p[1]} << 8) | (uint64_t{p[2]} << 16) | (uint64_t{p[3]} << 24) |
           (uint64_t{p[4]} << 32) | (uint64_t{p[5]} << 40) | (uint64_t{p[6]} << 48) | (uint64_t{p[7]} << 56);
}

constexpr SMALLCANON_ALWAYS_INLINE void write_le_u64(unsigned char *p, uint64_t value) {
    p[0] = static_cast<unsigned char>(value >> 0);
    p[1] = static_cast<unsigned char>(value >> 8);
    p[2] = static_cast<unsigned char>(value >> 16);
    p[3] = static_cast<unsigned char>(value >> 24);
    p[4] = static_cast<unsigned char>(value >> 32);
    p[5] = static_cast<unsigned char>(value >> 40);
    p[6] = static_cast<unsigned char>(value >> 48);
    p[7] = static_cast<unsigned char>(value >> 56);
}

// taken from https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
SMALLCANON_ALWAYS_INLINE uint32_t hash_fmix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}
