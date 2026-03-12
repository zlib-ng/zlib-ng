/* crc32_armv8_pmull_eor3.c -- ARMv8 CRC32 using PMULL + EOR3 (SHA3 extension)
 * Copyright (C) 2025 Peter Cawley
 *   https://github.com/corsix/fast-crc32
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * This uses EOR3 (3-way XOR) from ARMv8.2-A SHA3 extension to save instructions.
 * Uses 3-way parallel scalar CRC + 9 PMULL vector lanes, processing 192 bytes/iter.
 */

#ifdef ARM_PMULL_EOR3

#include "zbuild.h"
#include "zutil.h"
#include "acle_intrins.h"
#include "neon_intrins.h"
#include "crc32_armv8_p.h"

/* Carryless multiply low 64 bits: a[0] * b[0] */
static inline uint64x2_t clmul_lo(uint64x2_t a, uint64x2_t b) {
#ifdef _MSC_VER
    return vreinterpretq_u64_p128(vmull_p64(
        vget_low_p64(vreinterpret_p64_u64(a)),
        vget_low_p64(vreinterpret_p64_u64(b))));
#else
    return vreinterpretq_u64_p128(vmull_p64(
        vget_lane_p64(vreinterpret_p64_u64(vget_low_u64(a)), 0),
        vget_lane_p64(vreinterpret_p64_u64(vget_low_u64(b)), 0)));
#endif
}

/* Carryless multiply high 64 bits: a[1] * b[1] */
static inline uint64x2_t clmul_hi(uint64x2_t a, uint64x2_t b) {
    return vreinterpretq_u64_p128(vmull_high_p64(vreinterpretq_p64_u64(a), vreinterpretq_p64_u64(b)));
}

/* Carryless multiply of two 32-bit scalars: a * b (returns 64-bit result in 128-bit vector) */
static inline uint64x2_t clmul_scalar(uint32_t a, uint32_t b) {
#ifdef _MSC_VER
    return vreinterpretq_u64_p128(vmull_p64(vdup_n_p64((poly64_t)a), vdup_n_p64((poly64_t)b)));
#else
    return vreinterpretq_u64_p128(vmull_p64((poly64_t)a, (poly64_t)b));
#endif
}

/* Compute x^n mod P (CRC-32 polynomial) in log(n) time, where P = 0x104c11db7 */
static uint32_t xnmodp(uint64_t n) {
  uint64_t stack = ~(uint64_t)1;
  uint32_t acc, low;
  for (; n > 191; n = (n >> 1) - 16) {
    stack = (stack << 1) + (n & 1);
  }
  stack = ~stack;
  acc = ((uint32_t)0x80000000) >> (n & 31);
  for (n >>= 5; n; --n) {
    acc = __crc32w(acc, 0);
  }
  while ((low = stack & 1), stack >>= 1) {
    poly8x8_t x = vreinterpret_p8_u64(vmov_n_u64(acc));
    uint64_t y = vgetq_lane_u64(vreinterpretq_u64_p16(vmull_p8(x, x)), 0);
    acc = __crc32d(0, y << low);
  }
  return acc;
}

/* Shift CRC forward by nbytes: equivalent to appending nbytes of zeros to the data stream */
static inline uint64x2_t crc_shift(uint32_t crc, size_t nbytes) {
  Assert(nbytes >= 5, "crc_shift requires nbytes >= 5");
  return clmul_scalar(crc, xnmodp(nbytes * 8 - 33));
}

Z_FORCEINLINE static Z_TARGET_PMULL_EOR3 uint32_t crc32_copy_impl(uint32_t crc, uint8_t *dst, const uint8_t *src,
                                                                  size_t len, const int COPY) {
    uint32_t crc0 = ~crc;

    if (UNLIKELY(len == 1)) {
        if (COPY)
            *dst = *src;
        crc0 = __crc32b(crc0, *src);
        return ~crc0;
    }

    /* Align to 16-byte boundary for vector path */
    uintptr_t align_diff = ALIGN_DIFF(src, 16);
    if (align_diff)
        crc0 = crc32_armv8_align(crc0, &dst, &src, &len, align_diff, COPY);

    /* 3-way scalar CRC + 9-way PMULL folding (192 bytes/iter) */
    if (len >= 192) {
        size_t blk = len / 192;                   /* Number of 192-byte blocks */
        size_t klen = blk * 16;                   /* Scalar stride per CRC lane */
        const uint8_t *end = src + len;
        const uint8_t *src0 = src;
        const uint8_t *src1 = src + klen;
        const uint8_t *src2 = src + klen * 2;
        const uint8_t *srcv = src + klen * 3;     /* Vector data starts after scalar lanes */
        uint32_t crc1 = 0, crc2 = 0;
        uint64x2_t vc0, vc1, vc2;
        uint64_t vc;

        /* Load first 9 vector chunks (144 bytes) */
        uint64x2_t x0 = vld1q_u64_ex((const uint64_t*)srcv, 128), y0;
        uint64x2_t x1 = vld1q_u64_ex((const uint64_t*)(srcv + 16), 128), y1;
        uint64x2_t x2 = vld1q_u64_ex((const uint64_t*)(srcv + 32), 128), y2;
        uint64x2_t x3 = vld1q_u64_ex((const uint64_t*)(srcv + 48), 128), y3;
        uint64x2_t x4 = vld1q_u64_ex((const uint64_t*)(srcv + 64), 128), y4;
        uint64x2_t x5 = vld1q_u64_ex((const uint64_t*)(srcv + 80), 128), y5;
        uint64x2_t x6 = vld1q_u64_ex((const uint64_t*)(srcv + 96), 128), y6;
        uint64x2_t x7 = vld1q_u64_ex((const uint64_t*)(srcv + 112), 128), y7;
        uint64x2_t x8 = vld1q_u64_ex((const uint64_t*)(srcv + 128), 128), y8;
        uint64x2_t k;
        /* k = {x^144 mod P, x^144+64 mod P} for 144-byte fold */
        { static const uint64_t ALIGNED_(16) k_[] = {0x26b70c3d, 0x3f41287a}; k = vld1q_u64_ex(k_, 128); }

        /* Per-region dst pointers */
        uint8_t *dst0 = dst;
        uint8_t *dst1 = NULL;
        uint8_t *dst2 = NULL;
        uint8_t *dst_v = NULL;

        if (COPY) {
            dst1 = dst + klen;
            dst2 = dst + klen * 2;
            dst_v = dst + klen * 3;
            vst1q_u8(dst_v, vreinterpretq_u8_u64(x0));
            vst1q_u8(dst_v + 16, vreinterpretq_u8_u64(x1));
            vst1q_u8(dst_v + 32, vreinterpretq_u8_u64(x2));
            vst1q_u8(dst_v + 48, vreinterpretq_u8_u64(x3));
            vst1q_u8(dst_v + 64, vreinterpretq_u8_u64(x4));
            vst1q_u8(dst_v + 80, vreinterpretq_u8_u64(x5));
            vst1q_u8(dst_v + 96, vreinterpretq_u8_u64(x6));
            vst1q_u8(dst_v + 112, vreinterpretq_u8_u64(x7));
            vst1q_u8(dst_v + 128, vreinterpretq_u8_u64(x8));
            dst_v += 144;
        }
        srcv += 144;

        /* Fold 9 vectors + 3-way parallel scalar CRC */
        if (blk > 1) {
            /* Only form a limit pointer when we have at least 2 blocks. */
            const uint8_t *limit = src0 + klen - 32;
            while (src0 <= limit) {
                /* Fold all 9 vector lanes using PMULL */
                y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
                y1 = clmul_lo(x1, k), x1 = clmul_hi(x1, k);
                y2 = clmul_lo(x2, k), x2 = clmul_hi(x2, k);
                y3 = clmul_lo(x3, k), x3 = clmul_hi(x3, k);
                y4 = clmul_lo(x4, k), x4 = clmul_hi(x4, k);
                y5 = clmul_lo(x5, k), x5 = clmul_hi(x5, k);
                y6 = clmul_lo(x6, k), x6 = clmul_hi(x6, k);
                y7 = clmul_lo(x7, k), x7 = clmul_hi(x7, k);
                y8 = clmul_lo(x8, k), x8 = clmul_hi(x8, k);

                /* EOR3: combine hi*k, lo*k, and new data in one instruction */
                {
                    uint64x2_t d0 = vld1q_u64_ex((const uint64_t*)srcv, 128);
                    uint64x2_t d1 = vld1q_u64_ex((const uint64_t*)(srcv + 16), 128);
                    uint64x2_t d2 = vld1q_u64_ex((const uint64_t*)(srcv + 32), 128);
                    uint64x2_t d3 = vld1q_u64_ex((const uint64_t*)(srcv + 48), 128);
                    uint64x2_t d4 = vld1q_u64_ex((const uint64_t*)(srcv + 64), 128);
                    uint64x2_t d5 = vld1q_u64_ex((const uint64_t*)(srcv + 80), 128);
                    uint64x2_t d6 = vld1q_u64_ex((const uint64_t*)(srcv + 96), 128);
                    uint64x2_t d7 = vld1q_u64_ex((const uint64_t*)(srcv + 112), 128);
                    uint64x2_t d8 = vld1q_u64_ex((const uint64_t*)(srcv + 128), 128);
                    if (COPY) {
                        vst1q_u8(dst_v, vreinterpretq_u8_u64(d0));
                        vst1q_u8(dst_v + 16, vreinterpretq_u8_u64(d1));
                        vst1q_u8(dst_v + 32, vreinterpretq_u8_u64(d2));
                        vst1q_u8(dst_v + 48, vreinterpretq_u8_u64(d3));
                        vst1q_u8(dst_v + 64, vreinterpretq_u8_u64(d4));
                        vst1q_u8(dst_v + 80, vreinterpretq_u8_u64(d5));
                        vst1q_u8(dst_v + 96, vreinterpretq_u8_u64(d6));
                        vst1q_u8(dst_v + 112, vreinterpretq_u8_u64(d7));
                        vst1q_u8(dst_v + 128, vreinterpretq_u8_u64(d8));
                        dst_v += 144;
                    }
                    x0 = veor3q_u64(x0, y0, d0);
                    x1 = veor3q_u64(x1, y1, d1);
                    x2 = veor3q_u64(x2, y2, d2);
                    x3 = veor3q_u64(x3, y3, d3);
                    x4 = veor3q_u64(x4, y4, d4);
                    x5 = veor3q_u64(x5, y5, d5);
                    x6 = veor3q_u64(x6, y6, d6);
                    x7 = veor3q_u64(x7, y7, d7);
                    x8 = veor3q_u64(x8, y8, d8);
                }

                /* 3-way parallel scalar CRC (16 bytes each) */
                {
                    uint64_t s0a = *(const uint64_t*)src0;
                    uint64_t s0b = *(const uint64_t*)(src0 + 8);
                    uint64_t s1a = *(const uint64_t*)src1;
                    uint64_t s1b = *(const uint64_t*)(src1 + 8);
                    uint64_t s2a = *(const uint64_t*)src2;
                    uint64_t s2b = *(const uint64_t*)(src2 + 8);
                    if (COPY) {
                        memcpy(dst0, &s0a, 8);
                        memcpy(dst0 + 8, &s0b, 8);
                        dst0 += 16;
                        memcpy(dst1, &s1a, 8);
                        memcpy(dst1 + 8, &s1b, 8);
                        dst1 += 16;
                        memcpy(dst2, &s2a, 8);
                        memcpy(dst2 + 8, &s2b, 8);
                        dst2 += 16;
                    }
                    crc0 = __crc32d(crc0, s0a);
                    crc0 = __crc32d(crc0, s0b);
                    crc1 = __crc32d(crc1, s1a);
                    crc1 = __crc32d(crc1, s1b);
                    crc2 = __crc32d(crc2, s2a);
                    crc2 = __crc32d(crc2, s2b);
                }
                src0 += 16;
                src1 += 16;
                src2 += 16;
                srcv += 144;
            }
        }

        /* Reduce 9 vectors to 1 using tree reduction */
        /* Step 1: x0 = fold(x0, x1), shift x2..x8 down */
        { static const uint64_t ALIGNED_(16) k_[] = {0xae689191, 0xccaa009e}; k = vld1q_u64_ex(k_, 128); }
        y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
        x0 = veor3q_u64(x0, y0, x1);
        x1 = x2, x2 = x3, x3 = x4, x4 = x5, x5 = x6, x6 = x7, x7 = x8;

        /* Step 2: fold pairs (x0,x1), (x2,x3), (x4,x5), (x6,x7) */
        y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
        y2 = clmul_lo(x2, k), x2 = clmul_hi(x2, k);
        y4 = clmul_lo(x4, k), x4 = clmul_hi(x4, k);
        y6 = clmul_lo(x6, k), x6 = clmul_hi(x6, k);
        x0 = veor3q_u64(x0, y0, x1);
        x2 = veor3q_u64(x2, y2, x3);
        x4 = veor3q_u64(x4, y4, x5);
        x6 = veor3q_u64(x6, y6, x7);

        /* Step 3: fold pairs (x0,x2), (x4,x6) */
        { static const uint64_t ALIGNED_(16) k_[] = {0xf1da05aa, 0x81256527}; k = vld1q_u64_ex(k_, 128); }
        y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
        y4 = clmul_lo(x4, k), x4 = clmul_hi(x4, k);
        x0 = veor3q_u64(x0, y0, x2);
        x4 = veor3q_u64(x4, y4, x6);

        /* Step 4: final fold (x0, x4) -> x0 */
        { static const uint64_t ALIGNED_(16) k_[] = {0x8f352d95, 0x1d9513d7}; k = vld1q_u64_ex(k_, 128); }
        y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
        x0 = veor3q_u64(x0, y0, x4);

        /* Process final scalar chunk */
        {
            uint64_t s0a = *(const uint64_t*)src0;
            uint64_t s0b = *(const uint64_t*)(src0 + 8);
            uint64_t s1a = *(const uint64_t*)src1;
            uint64_t s1b = *(const uint64_t*)(src1 + 8);
            uint64_t s2a = *(const uint64_t*)src2;
            uint64_t s2b = *(const uint64_t*)(src2 + 8);
            if (COPY) {
                memcpy(dst0, &s0a, 8);
                memcpy(dst0 + 8, &s0b, 8);
                memcpy(dst1, &s1a, 8);
                memcpy(dst1 + 8, &s1b, 8);
                memcpy(dst2, &s2a, 8);
                memcpy(dst2 + 8, &s2b, 8);
            }
            crc0 = __crc32d(crc0, s0a);
            crc0 = __crc32d(crc0, s0b);
            crc1 = __crc32d(crc1, s1a);
            crc1 = __crc32d(crc1, s1b);
            crc2 = __crc32d(crc2, s2a);
            crc2 = __crc32d(crc2, s2b);
        }

        /* Shift and combine 3 scalar CRCs */
        vc0 = crc_shift(crc0, klen * 2 + blk * 144);
        vc1 = crc_shift(crc1, klen + blk * 144);
        vc2 = crc_shift(crc2, blk * 144);
        vc = vgetq_lane_u64(veor3q_u64(vc0, vc1, vc2), 0);

        /* Final reduction: 128-bit vector + scalar CRCs -> 32-bit */
        crc0 = __crc32d(0, vgetq_lane_u64(x0, 0));
        crc0 = __crc32d(crc0, vc ^ vgetq_lane_u64(x0, 1));
        if (COPY)
            dst += blk * 192;
        src = srcv;
        len = end - srcv;
    }

    /* 3-way scalar CRC (24 bytes/iter) */
    if (len >= 80) {
        size_t klen = ((len - 8) / 24) * 8;   /* Stride for 3-way parallel */
        const uint8_t *buf0 = src;
        const uint8_t *buf1 = src + klen;
        const uint8_t *buf2 = src + klen * 2;
        uint32_t crc1 = 0, crc2 = 0;
        uint64x2_t vc0, vc1;
        uint64_t vc;

        /* Per-lane dst pointers */
        uint8_t *dst0 = dst;
        uint8_t *dst1 = NULL;
        uint8_t *dst2 = NULL;
        if (COPY) {
            dst1 = dst + klen;
            dst2 = dst + klen * 2;
        }

        /* 3-way parallel scalar CRC */
        do {
            uint64_t v0 = *(const uint64_t*)buf0;
            uint64_t v1 = *(const uint64_t*)buf1;
            uint64_t v2 = *(const uint64_t*)buf2;
            if (COPY) {
                memcpy(dst0, &v0, 8);
                dst0 += 8;
                memcpy(dst1, &v1, 8);
                dst1 += 8;
                memcpy(dst2, &v2, 8);
                dst2 += 8;
            }
            crc0 = __crc32d(crc0, v0);
            crc1 = __crc32d(crc1, v1);
            crc2 = __crc32d(crc2, v2);
            buf0 += 8;
            buf1 += 8;
            buf2 += 8;
            len -= 24;
        } while (len >= 32);

        /* Combine the 3 CRCs */
        vc0 = crc_shift(crc0, klen * 2 + 8);
        vc1 = crc_shift(crc1, klen + 8);
        vc = vgetq_lane_u64(veorq_u64(vc0, vc1), 0);

        /* Process final 8 bytes with combined CRC */
        crc0 = crc2;
        {
            uint64_t vf = *(const uint64_t*)buf2;
            if (COPY)
                memcpy(dst2, &vf, 8);
            crc0 = __crc32d(crc0, vf ^ vc);
        }
        src = buf2 + 8;
        len -= 8;
        if (COPY)
            dst = dst2 + 8;
    }

    /* Process remaining bytes */
    return crc32_armv8_tail(crc0, dst, src, len, COPY);
}

Z_INTERNAL Z_TARGET_PMULL_EOR3 uint32_t crc32_armv8_pmull_eor3(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_copy_impl(crc, NULL, buf, len, 0);
}

Z_INTERNAL Z_TARGET_PMULL_EOR3 uint32_t crc32_copy_armv8_pmull_eor3(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
#if OPTIMAL_CMP >= 32
    return crc32_copy_impl(crc, dst, src, len, 1);
#else
    /* Without unaligned access, interleaved stores get decomposed into byte ops */
    crc = crc32_armv8_pmull_eor3(crc, src, len);
    memcpy(dst, src, len);
    return crc;
#endif
}
#endif
