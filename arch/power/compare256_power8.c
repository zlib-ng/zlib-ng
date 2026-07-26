/* compare256_power8.c - Power8 VSX version of compare256
 * Copyright (C) 2026 Contributors to the zlib-ng project
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef POWER8_VSX

#include "zbuild.h"
#include "zendian.h"
#include "fallback_builtins.h"
#include "deflate.h"

#include <altivec.h>

#if BYTE_ORDER == LITTLE_ENDIAN
#  define DIFF_DWORD_FIRST      1
#  define DIFF_DWORD_SECOND     0
#else
#  define DIFF_DWORD_FIRST      0
#  define DIFF_DWORD_SECOND     1
#endif

static inline vector unsigned long long load16_power8(const uint8_t *src) {
    return vec_xl_be(0, (const unsigned long long *)src);
}

static inline uint32_t first_diff16_power8(vector unsigned long long diff) {
    uint64_t lane = vec_extract(diff, DIFF_DWORD_FIRST);
    if (lane)
        return zng_first_diff_byte64(lane);
    return 8 + zng_first_diff_byte64(vec_extract(diff, DIFF_DWORD_SECOND));
}

static inline uint32_t compare256_power8_static(const uint8_t *src0, const uint8_t *src1) {
    const vector unsigned long long vzero = { 0, 0 };
    vector unsigned long long diff0, diff1;
    uint32_t len = 16;
    uint64_t lane;

    diff0 = vec_xor(load16_power8(src0), load16_power8(src1));

    lane = vec_extract(diff0, DIFF_DWORD_FIRST);
    if (lane)
        return zng_first_diff_byte64(lane);
    lane = vec_extract(diff0, DIFF_DWORD_SECOND);
    if (lane)
        return 8 + zng_first_diff_byte64(lane);

    do {
        diff0 = vec_xor(load16_power8(src0 + len), load16_power8(src1 + len));
        diff1 = vec_xor(load16_power8(src0 + len + 16), load16_power8(src1 + len + 16));

        if (!vec_all_eq(vec_or(diff0, diff1), vzero)) {
            if (!vec_all_eq(diff0, vzero))
                return len + first_diff16_power8(diff0);
            return len + 16 + first_diff16_power8(diff1);
        }

        len += 32;
    } while (len < 240);

    diff0 = vec_xor(load16_power8(src0 + 240), load16_power8(src1 + 240));
    if (!vec_all_eq(diff0, vzero))
        return 240 + first_diff16_power8(diff0);

    return 256;
}

Z_INTERNAL uint32_t compare256_power8(const uint8_t *src0, const uint8_t *src1) {
    return compare256_power8_static(src0, src1);
}

#define LONGEST_MATCH       longest_match_power8
#define COMPARE256          compare256_power8_static

#include "match_tpl.h"

#define LONGEST_MATCH_ROLL
#define LONGEST_MATCH       longest_match_roll_power8
#define COMPARE256          compare256_power8_static

#include "match_tpl.h"

#endif
