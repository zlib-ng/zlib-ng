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

/* Force post-indexed loads; the inlined longest_match loop otherwise emits a
 * separate add per iteration. */
#ifdef COMPARE256_NEON_POSTINDEX
#  define LOAD_16B_PAIR(a, b, s0, s1, off) \
    __asm__("ldr %q0, [%2, %3]\n\t" \
            "ldr %q1, [%2], #16" \
            : "=w"(a), "=w"(b), "+r"(s1) : "r"(off) : "memory")
#else
#  define LOAD_16B_PAIR(a, b, s0, s1, off) do { \
    Z_UNUSED(off); \
    (a) = vld1q_u8(s0); \
    (b) = vld1q_u8(s1); \
    (s0) += 16; \
    (s1) += 16; \
} while (0)
#endif

Z_FORCEINLINE static uint32_t compare256_neon_static(const uint8_t *src0, const uint8_t *src1) {
    uint32_t len = 0;
#ifdef COMPARE256_NEON_POSTINDEX
    intptr_t offset = (intptr_t)src0 - (intptr_t)src1;
#else
    intptr_t offset = 0;
#endif

    do {
        uint8x16_t a, b, cmp;
        uint64_t lane;

        LOAD_16B_PAIR(a, b, src0, src1, offset);

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

#undef LOAD_16B_PAIR
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
