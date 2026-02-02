/* compare256.c -- 256 byte memory comparison with match length return
 * Copyright (C) 2020 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zendian.h"
#include "deflate.h"
#include "fallback_builtins.h"

/* 8-bit integer comparison for hardware without unaligned loads */
static inline uint32_t compare256_8_static(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;

    do {
        if (src0[0] != src1[0])
            return len;
        if (src0[1] != src1[1])
            return len + 1;
        if (src0[2] != src1[2])
            return len + 2;
        if (src0[3] != src1[3])
            return len + 3;
        if (src0[4] != src1[4])
            return len + 4;
        if (src0[5] != src1[5])
            return len + 5;
        if (src0[6] != src1[6])
            return len + 6;
        if (src0[7] != src1[7])
            return len + 7;
        src0 += 8, src1 += 8, len += 8;
    } while (len < 256);

    return 256;
}

/* 64-bit integer comparison for hardware with unaligned loads */
static inline uint32_t compare256_64_static(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;

    do {
        uint64_t sv = zng_memread_8(src0);
        uint64_t mv = zng_memread_8(src1);
        uint64_t diff = sv ^ mv;
        if (diff)
            return len + zng_ctz64(Z_U64_TO_LE(diff)) / 8;
        src0 += 8, src1 += 8, len += 8;

        sv = zng_memread_8(src0);
        mv = zng_memread_8(src1);
        diff = sv ^ mv;
        if (diff)
            return len + zng_ctz64(Z_U64_TO_LE(diff)) / 8;
        src0 += 8, src1 += 8, len += 8;
    } while (len < 256);

    return 256;
}

#if OPTIMAL_CMP == 8
#  define COMPARE256 compare256_8_static
#else
#  define COMPARE256 compare256_64_static
#endif

#ifdef WITH_ALL_FALLBACKS
Z_INTERNAL uint32_t compare256_8(const uint8_t *src0, const uint8_t *src1) {
    return compare256_8_static(src0, src1);
}

Z_INTERNAL uint32_t compare256_64(const uint8_t *src0, const uint8_t *src1) {
    return compare256_64_static(src0, src1);
}
#endif

Z_INTERNAL uint32_t compare256_c(const uint8_t *src0, const uint8_t *src1) {
    return COMPARE256(src0, src1);
}

// Generate longest_match_c
#define LONGEST_MATCH       longest_match_c
#include "match_tpl.h"

// Generate longest_match_slow_c
#define LONGEST_MATCH_SLOW
#define LONGEST_MATCH       longest_match_slow_c
#include "match_tpl.h"
