/* compare256_rle.h -- 256 byte run-length encoding comparison
 * Copyright (C) 2022 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zendian.h"
#include "zmemory.h"
#include "fallback_builtins.h"

typedef uint32_t (*compare256_rle_func)(const uint8_t* src0, const uint8_t* src1);

/* 8-bit RLE comparison for hardware without unaligned loads */
static inline uint32_t compare256_rle_8(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;
    uint8_t val = *src0;

    do {
        if (val != src1[0])
            return len;
        if (val != src1[1])
            return len + 1;
        if (val != src1[2])
            return len + 2;
        if (val != src1[3])
            return len + 3;
        if (val != src1[4])
            return len + 4;
        if (val != src1[5])
            return len + 5;
        if (val != src1[6])
            return len + 6;
        if (val != src1[7])
            return len + 7;
        src1 += 8, len += 8;
    } while (len < 256);

    return 256;
}

/* 64-bit RLE comparison for hardware with unaligned loads */
static inline uint32_t compare256_rle_64(const uint8_t *src0, const uint8_t *src1) {
    uint32_t src0_cmp32, len = 0;
    uint16_t src0_cmp;
    uint64_t sv, mv, diff;

    src0_cmp = zng_memread_2(src0);
    src0_cmp32 = ((uint32_t)src0_cmp << 16) | src0_cmp;
    sv = ((uint64_t)src0_cmp32 << 32) | src0_cmp32;

    do {
        mv = zng_memread_8(src1);
        diff = sv ^ mv;
        if (diff)
            return len + zng_ctz64(Z_U64_TO_LE(diff)) / 8;

        src1 += 8, len += 8;
    } while (len < 256);

    return 256;
}
