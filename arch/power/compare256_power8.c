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

static inline uint32_t compare256_power8_static(const uint8_t *src0, const uint8_t *src1) {
    vector unsigned long long vsrc0, vsrc1, diff;
    uint32_t len = 16;
    uint64_t lane;

    diff = vec_xor(load16_power8(src0), load16_power8(src1));

    lane = vec_extract(diff, DIFF_DWORD_FIRST);
    if (lane)
        return zng_first_diff_byte64(lane);
    lane = vec_extract(diff, DIFF_DWORD_SECOND);
    if (lane)
        return 8 + zng_first_diff_byte64(lane);

    do {
        src0 += 16, src1 += 16;

        vsrc0 = load16_power8(src0);
        vsrc1 = load16_power8(src1);

        if (!vec_all_eq(vsrc0, vsrc1)) {
            diff = vec_xor(vsrc0, vsrc1);

            lane = vec_extract(diff, DIFF_DWORD_FIRST);
            if (lane)
                return len + zng_first_diff_byte64(lane);
            lane = vec_extract(diff, DIFF_DWORD_SECOND);
            return len + 8 + zng_first_diff_byte64(lane);
        }

        len += 16;
    } while (len < 256);

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
