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

static inline uint32_t compare256_power8_static(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;

    do {
        vector unsigned char vsrc0, vsrc1, diff;
        uint64_t lane;

        vsrc0 = vec_xl(0, src0);
        vsrc1 = vec_xl(0, src1);

        diff = vec_xor(vsrc0, vsrc1);

        lane = vec_extract((vector unsigned long long)diff, 0);
        if (lane)
            return len + zng_first_diff_byte64(lane);
        len += 8;
        lane = vec_extract((vector unsigned long long)diff, 1);
        if (lane)
            return len + zng_first_diff_byte64(lane);
        len += 8;

        src0 += 16;
        src1 += 16;
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
