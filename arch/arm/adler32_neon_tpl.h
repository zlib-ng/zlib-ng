/* Copyright (C) 1995-2011, 2016 Mark Adler
 * Copyright (C) 2017 ARM Holdings Inc.
 * Copyright (C) 2025 Nathan Moinvaziri
 * Authors:
 *   Adenilson Cavalcanti <adenilson.cavalcanti@arm.com>
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Nathan Moinvaziri <nathan@nathanm.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "neon_intrins.h"
#include "adler32_p.h"

#ifdef USE_DOTPROD
/* Multiplication table for dotprod - must be uint8_t for vdotq_u32 */
static const uint8_t ALIGNED_(64) taps[64] = {
#else
static const uint16_t ALIGNED_(64) taps[64] = {
#endif
    64, 63, 62, 61, 60, 59, 58, 57,
    56, 55, 54, 53, 52, 51, 50, 49,
    48, 47, 46, 45, 44, 43, 42, 41,
    40, 39, 38, 37, 36, 35, 34, 33,
    32, 31, 30, 29, 28, 27, 26, 25,
    24, 23, 22, 21, 20, 19, 18, 17,
    16, 15, 14, 13, 12, 11, 10, 9,
    8, 7, 6, 5, 4, 3, 2, 1 };

Z_FORCEINLINE static uint32_t adler32_copy_impl(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len, const int COPY) {
    /* split Adler-32 into component sums */
    uint32_t sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    /* in case user likes doing a byte at a time, keep it fast */
    if (UNLIKELY(len == 1))
        return adler32_copy_tail(adler, dst, src, 1, sum2, 1, 1, COPY);

    /* in case short lengths are provided, keep it somewhat fast */
    if (UNLIKELY(len < 16))
        return adler32_copy_tail(adler, dst, src, len, sum2, 1, 15, COPY);

    uint32_t pair[2];

    /* Split Adler-32 into component sums, it can be supplied by
     * the caller sites (e.g. in a PNG file).
     */
    pair[0] = adler;
    pair[1] = sum2;

    /* If memory is not SIMD aligned, do scalar sums to an aligned
     * offset, provided that doing so doesn't completely eliminate
     * SIMD operation. Aligned loads are still faster on ARM, even
     * when there's no explicit aligned load instruction. Note:
     * the code currently emits an alignment hint in the instruction
     * for exactly 256 bits when supported by the compiler. Several ARM
     * SIPs have small penalties for cacheline crossing loads as well (so
     * really 512 bits is the optimal alignment of the buffer). 32 bytes
     * should strike a balance, though. The Cortex-A8 and Cortex-A9
     * processors are documented to benefit from 128 bit and 64 bit
     * alignment, but it's unclear which other SIPs will benefit from it.
     * In the copying variant we use fallback to 4x loads and 4x stores,
     * as ld1x4 seems to block ILP when stores are in the mix */
    size_t align_diff = MIN(ALIGN_DIFF(src, 32), len);
    size_t n = NMAX_ALIGNED32;
    if (align_diff) {
        adler32_copy_align(&pair[0], dst, src, align_diff, &pair[1], 31, COPY);

        if (COPY)
            dst += align_diff;
        src += align_diff;
        len -= align_diff;
        n = ALIGN_DOWN(n - align_diff, 32);
    }

    while (len >= 16) {
        n = MIN(len, n);

#ifdef USE_DOTPROD
        /* Use 4 independent accumulator sets to break dependency chains
         * and allow better instruction-level parallelism */
        uint32x4_t adacc_a = vdupq_n_u32(0);
        uint32x4_t adacc_b = vdupq_n_u32(0);
        uint32x4_t adacc_c = vdupq_n_u32(0);
        uint32x4_t adacc_d = vdupq_n_u32(0);
        uint32x4_t s2acc_a = vdupq_n_u32(0);
        uint32x4_t s2acc_b = vdupq_n_u32(0);
        uint32x4_t s2acc_c = vdupq_n_u32(0);
        uint32x4_t s2acc_d = vdupq_n_u32(0);
        uint32x4_t s1sums_a = vdupq_n_u32(0);
        uint32x4_t s1sums_b = vdupq_n_u32(0);
        uint32x4_t s1sums_c = vdupq_n_u32(0);
        uint32x4_t s1sums_d = vdupq_n_u32(0);

        adacc_a = vsetq_lane_u32(pair[0], adacc_a, 0);
        s2acc_a = vsetq_lane_u32(pair[1], s2acc_a, 0);

        /* Load multiplication tables as uint8x16_t for dotprod */
        uint8x16_t t0 = vld1q_u8(taps);
        uint8x16_t t1 = vld1q_u8(taps + 16);
        uint8x16_t t2 = vld1q_u8(taps + 32);
        uint8x16_t t3 = vld1q_u8(taps + 48);

        /* Vector of ones for s1 accumulation */
        uint8x16_t ones = vdupq_n_u8(1);
#else
        uint32x4_t adacc = vdupq_n_u32(0);
        uint32x4_t s2acc = vdupq_n_u32(0);

        adacc = vsetq_lane_u32(pair[0], adacc, 0);
        s2acc = vsetq_lane_u32(pair[1], s2acc, 0);

        uint32x4_t s3acc = vdupq_n_u32(0);
        uint32x4_t adacc_prev = adacc;

        uint32x4_t s2acc_0 = vdupq_n_u32(0);
        uint32x4_t s2acc_1 = vdupq_n_u32(0);
        uint32x4_t s2acc_2 = vdupq_n_u32(0);

        uint16x8_t s2_0, s2_1, s2_2, s2_3;
        s2_0 = s2_1 = s2_2 = s2_3 = vdupq_n_u16(0);

        uint16x8_t s2_4, s2_5, s2_6, s2_7;
        s2_4 = s2_5 = s2_6 = s2_7 = vdupq_n_u16(0);
#endif

        size_t num_iter = (n >> 4) >> 2;
        int rem = (n >> 4) & 3;

        for (size_t i = 0; i < num_iter; ++i) {
            uint8x16_t d0, d1, d2, d3;

            /* In the copying variant we use 4x loads and 4x stores,
             * as ld1x4 seems to block ILP when stores are in the mix */
            if (COPY) {
                d0 = vld1q_u8_ex(src, 128);
                d1 = vld1q_u8_ex(src + 16, 128);
                d2 = vld1q_u8_ex(src + 32, 128);
                d3 = vld1q_u8_ex(src + 48, 128);

                vst1q_u8(dst, d0);
                vst1q_u8(dst + 16, d1);
                vst1q_u8(dst + 32, d2);
                vst1q_u8(dst + 48, d3);
                dst += 64;
            } else {
                uint8x16x4_t d0_d3 = vld1q_u8_x4_ex(src, 256);
                d0 = d0_d3.val[0];
                d1 = d0_d3.val[1];
                d2 = d0_d3.val[2];
                d3 = d0_d3.val[3];
            }

#ifdef USE_DOTPROD
            /* Each 16-byte chunk uses its own accumulator set so that
             * successive dotprod instructions are independent and can
             * be pipelined without stalling on the previous result */
            s1sums_a = vaddq_u32(s1sums_a, adacc_a);
            adacc_a = vdotq_u32(adacc_a, d0, ones);
            s2acc_a = vdotq_u32(s2acc_a, d0, t0);

            s1sums_b = vaddq_u32(s1sums_b, adacc_b);
            adacc_b = vdotq_u32(adacc_b, d1, ones);
            s2acc_b = vdotq_u32(s2acc_b, d1, t1);

            s1sums_c = vaddq_u32(s1sums_c, adacc_c);
            adacc_c = vdotq_u32(adacc_c, d2, ones);
            s2acc_c = vdotq_u32(s2acc_c, d2, t2);

            s1sums_d = vaddq_u32(s1sums_d, adacc_d);
            adacc_d = vdotq_u32(adacc_d, d3, ones);
            s2acc_d = vdotq_u32(s2acc_d, d3, t3);
#else
            /* Unfortunately it doesn't look like there's a direct sum 8 bit to 32
             * bit instruction, we'll have to make due summing to 16 bits first */
            uint16x8x2_t hsum, hsum_fold;
            hsum.val[0] = vpaddlq_u8(d0);
            hsum.val[1] = vpaddlq_u8(d1);

            hsum_fold.val[0] = vpadalq_u8(hsum.val[0], d2);
            hsum_fold.val[1] = vpadalq_u8(hsum.val[1], d3);

            adacc = vpadalq_u16(adacc, hsum_fold.val[0]);
            s3acc = vaddq_u32(s3acc, adacc_prev);
            adacc = vpadalq_u16(adacc, hsum_fold.val[1]);

            /* If we do straight widening additions to the 16 bit values, we don't incur
             * the usual penalties of a pairwise add. We can defer the multiplications
             * until the very end. These will not overflow because we are incurring at
             * most 408 loop iterations (NMAX / 64), and a given lane is only going to be
             * summed into once. This means for the maximum input size, the largest value
             * we will see is 255 * 102 = 26010, safely under uint16 max */
            s2_0 = vaddw_u8(s2_0, vget_low_u8(d0));
            s2_1 = vaddw_high_u8(s2_1, d0);
            s2_2 = vaddw_u8(s2_2, vget_low_u8(d1));
            s2_3 = vaddw_high_u8(s2_3, d1);
            s2_4 = vaddw_u8(s2_4, vget_low_u8(d2));
            s2_5 = vaddw_high_u8(s2_5, d2);
            s2_6 = vaddw_u8(s2_6, vget_low_u8(d3));
            s2_7 = vaddw_high_u8(s2_7, d3);
#endif

#ifndef USE_DOTPROD
            adacc_prev = adacc;
#endif
            src += 64;
        }

#ifdef USE_DOTPROD
        /* Combine 4 independent accumulator sets into single vectors
         * for the remainder loop and final reduction */
        uint32x4_t adacc = vaddq_u32(vaddq_u32(adacc_a, adacc_b), vaddq_u32(adacc_c, adacc_d));
        uint32x4_t s2acc = vaddq_u32(vaddq_u32(s2acc_a, s2acc_b), vaddq_u32(s2acc_c, s2acc_d));
        uint32x4_t s1sums = vaddq_u32(vaddq_u32(s1sums_a, s1sums_b), vaddq_u32(s1sums_c, s1sums_d));
        uint32x4_t s3acc = vshlq_n_u32(s1sums, 6);
        uint32x4_t adacc_prev = adacc;
#else
        s3acc = vshlq_n_u32(s3acc, 6);
#endif

        if (rem) {
            uint32x4_t s3acc_0 = vdupq_n_u32(0);
            while (rem--) {
                uint8x16_t d0 = vld1q_u8_ex(src, 128);
                if (COPY) {
                    vst1q_u8(dst, d0);
                    dst += 16;
                }

#ifdef USE_DOTPROD
                s3acc_0 = vaddq_u32(s3acc_0, adacc_prev);
                adacc = vdotq_u32(adacc, d0, ones);
                s2acc = vdotq_u32(s2acc, d0, t3);
#else
                uint16x8_t hsum;
                hsum = vpaddlq_u8(d0);
                s2_6 = vaddw_u8(s2_6, vget_low_u8(d0));
                s2_7 = vaddw_high_u8(s2_7, d0);
                adacc = vpadalq_u16(adacc, hsum);
                s3acc_0 = vaddq_u32(s3acc_0, adacc_prev);
#endif

                adacc_prev = adacc;
                src += 16;
            }

            s3acc_0 = vshlq_n_u32(s3acc_0, 4);
            s3acc = vaddq_u32(s3acc_0, s3acc);
        }

#ifdef USE_DOTPROD
        /* Dotprod computes weighted sums inline, so final reduction is simple */
        s2acc = vaddq_u32(s2acc, s3acc);
        pair[0] = vaddvq_u32(adacc);
        pair[1] = vaddvq_u32(s2acc);
#else
        uint16x8x4_t t0_t3 = vld1q_u16_x4_ex(taps, 256);
        uint16x8x4_t t4_t7 = vld1q_u16_x4_ex(taps + 32, 256);

        s2acc = vmlal_high_u16(s2acc, t0_t3.val[0], s2_0);
        s2acc_0 = vmlal_u16(s2acc_0, vget_low_u16(t0_t3.val[0]), vget_low_u16(s2_0));
        s2acc_1 = vmlal_high_u16(s2acc_1, t0_t3.val[1], s2_1);
        s2acc_2 = vmlal_u16(s2acc_2, vget_low_u16(t0_t3.val[1]), vget_low_u16(s2_1));

        s2acc = vmlal_high_u16(s2acc, t0_t3.val[2], s2_2);
        s2acc_0 = vmlal_u16(s2acc_0, vget_low_u16(t0_t3.val[2]), vget_low_u16(s2_2));
        s2acc_1 = vmlal_high_u16(s2acc_1, t0_t3.val[3], s2_3);
        s2acc_2 = vmlal_u16(s2acc_2, vget_low_u16(t0_t3.val[3]), vget_low_u16(s2_3));

        s2acc = vmlal_high_u16(s2acc, t4_t7.val[0], s2_4);
        s2acc_0 = vmlal_u16(s2acc_0, vget_low_u16(t4_t7.val[0]), vget_low_u16(s2_4));
        s2acc_1 = vmlal_high_u16(s2acc_1, t4_t7.val[1], s2_5);
        s2acc_2 = vmlal_u16(s2acc_2, vget_low_u16(t4_t7.val[1]), vget_low_u16(s2_5));

        s2acc = vmlal_high_u16(s2acc, t4_t7.val[2], s2_6);
        s2acc_0 = vmlal_u16(s2acc_0, vget_low_u16(t4_t7.val[2]), vget_low_u16(s2_6));
        s2acc_1 = vmlal_high_u16(s2acc_1, t4_t7.val[3], s2_7);
        s2acc_2 = vmlal_u16(s2acc_2, vget_low_u16(t4_t7.val[3]), vget_low_u16(s2_7));

        s2acc = vaddq_u32(s2acc_0, s2acc);
        s2acc_2 = vaddq_u32(s2acc_1, s2acc_2);
        s2acc = vaddq_u32(s2acc, s2acc_2);

        s2acc = vaddq_u32(s2acc, s3acc);
        pair[0] = vaddvq_u32(adacc);
        pair[1] = vaddvq_u32(s2acc);
#endif

        pair[0] %= BASE;
        pair[1] %= BASE;

        len -= (n >> 4) << 4;
        n = NMAX_ALIGNED32;
    }

    /* Process tail (len < 16).  */
    return adler32_copy_tail(pair[0], dst, src, len, pair[1], len != 0 || align_diff, 15, COPY);
}
