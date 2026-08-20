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
            : "=w"(a), "=w"(b), "+r"(s1) \
            : "r"(off) \
            : "memory")
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
    uint64_t diff;

    /* Check first 16 bytes using 64-bit scalar loads to eliminate SIMD cross-domain latency */
    diff = zng_memread_8(src0) ^ zng_memread_8(src1);
    if (UNLIKELY(diff))
        return zng_first_diff_byte64(diff);

    diff = zng_memread_8(src0 + 8) ^ zng_memread_8(src1 + 8);
    if (UNLIKELY(diff))
        return 8 + zng_first_diff_byte64(diff);

    src0 += 16;
    src1 += 16;

#ifdef COMPARE256_NEON_POSTINDEX
    intptr_t offset = (intptr_t)src0 - (intptr_t)src1;
#else
    intptr_t offset = 0;
#endif
    uint8x16_t a0, b0, a1, b1;
    uint8x16_t cmp0, cmp1;
    uint64_t lane0, lane1;
    uint32_t len = 16;

    /* Check next 32 bytes in 16-byte steps with scalar early exit */
    LOAD_16B_PAIR(a0, b0, src0, src1, offset);
    cmp0 = veorq_u8(a0, b0);
    lane0 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp0), 0);
    if (UNLIKELY(lane0))
        return len + zng_first_diff_byte64(lane0);
    lane1 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp0), 1);
    if (UNLIKELY(lane1))
        return len + 8 + zng_first_diff_byte64(lane1);
    len += 16;

    LOAD_16B_PAIR(a0, b0, src0, src1, offset);
    cmp0 = veorq_u8(a0, b0);
    lane0 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp0), 0);
    if (UNLIKELY(lane0))
        return len + zng_first_diff_byte64(lane0);
    lane1 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp0), 1);
    if (UNLIKELY(lane1))
        return len + 8 + zng_first_diff_byte64(lane1);
    len += 16;

    /* 2x unrolled loop (32 bytes per iteration) */
    do {
        LOAD_16B_PAIR(a0, b0, src0, src1, offset);
        LOAD_16B_PAIR(a1, b1, src0, src1, offset);

        cmp0 = veorq_u8(a0, b0);
        cmp1 = veorq_u8(a1, b1);

#if defined(ARCH_ARM) && defined(ARCH_64BIT)
        uint8x16_t any_diff = vorrq_u8(cmp0, cmp1);
        if (LIKELY(vmaxvq_u8(any_diff) == 0)) {
            len += 32;
            continue;
        }
#endif

        lane0 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp0), 0);
        if (UNLIKELY(lane0))
            return len + zng_first_diff_byte64(lane0);
        lane1 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp0), 1);
        if (UNLIKELY(lane1))
            return len + 8 + zng_first_diff_byte64(lane1);
        lane0 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp1), 0);
        if (UNLIKELY(lane0))
            return len + 16 + zng_first_diff_byte64(lane0);
        lane1 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp1), 1);
        if (UNLIKELY(lane1))
            return len + 24 + zng_first_diff_byte64(lane1);

        len += 32;
    } while (len < 240);

    /* Check remaining 16 bytes */
    LOAD_16B_PAIR(a0, b0, src0, src1, offset);
    cmp0 = veorq_u8(a0, b0);
    lane0 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp0), 0);
    if (UNLIKELY(lane0))
        return len + zng_first_diff_byte64(lane0);
    lane1 = vgetq_lane_u64(vreinterpretq_u64_u8(cmp0), 1);
    if (UNLIKELY(lane1))
        return len + 8 + zng_first_diff_byte64(lane1);

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
