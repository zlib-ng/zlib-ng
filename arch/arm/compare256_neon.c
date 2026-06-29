/* compare256_neon.c - NEON version of compare256
 * Copyright (C) 2022 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zmemory.h"
#include "deflate.h"
#include "fallback_builtins.h"

#if defined(ARM_NEON)
#include "neon_intrins.h"

#if defined(ARCH_ARM) && defined(ARCH_64BIT) && (!defined(_MSC_VER) || defined(__clang__))
#  define COMPARE256_NEON_POSTINDEX
#endif

Z_FORCEINLINE static uint32_t compare256_neon_static(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;
#ifdef COMPARE256_NEON_POSTINDEX
    intptr_t offset = (intptr_t)src0 - (intptr_t)src1;
#endif

    do {
        uint8x16_t a, b, cmp;
        uint64_t lane;

        /* Force post-indexed loads; the inlined longest_match loop otherwise emits a
         * separate add per iteration. */
#ifdef COMPARE256_NEON_POSTINDEX
        __asm__("ldr %q0, [%2, %3]\n\t"
                "ldr %q1, [%2], #16"
                : "=w"(a), "=w"(b), "+r"(src1)
                : "r"(offset)
                : "memory");
#else
        a = vld1q_u8(src0);
        b = vld1q_u8(src1);
        src0 += 16, src1 += 16;
#endif

        cmp = veorq_u8(a, b);

        lane = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 0);
        if (lane)
            return len + zng_first_diff_byte64(lane);
        len += 8;
        lane = vgetq_lane_u64(vreinterpretq_u64_u8(cmp), 1);
        if (lane)
            return len + zng_first_diff_byte64(lane);
        len += 8;
    } while (len < 256);

    return 256;
}

#undef COMPARE256_NEON_POSTINDEX

Z_INTERNAL uint32_t compare256_neon(const uint8_t *src0, const uint8_t *src1) {
    return compare256_neon_static(src0, src1);
}

#define LONGEST_MATCH       longest_match_neon
#define COMPARE256          compare256_neon_static

#include "match_tpl.h"

#define LONGEST_MATCH_ROLL
#define LONGEST_MATCH       longest_match_roll_neon
#define COMPARE256          compare256_neon_static

#include "match_tpl.h"

#endif
