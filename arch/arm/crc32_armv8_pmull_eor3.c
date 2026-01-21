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
#include "crc32.h"

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

Z_INTERNAL Z_TARGET_PMULL_EOR3 uint32_t crc32_armv8_pmull_eor3(uint32_t crc, const uint8_t *buf, size_t len) {
    uint32_t crc0 = ~crc;

    if (UNLIKELY(len == 1)) {
        crc0 = __crc32b(crc0, *buf);
        return ~crc0;
    }

    /* Align to 16-byte boundary for vector path */
    uintptr_t align_diff = ALIGN_DIFF(buf, 16);
    if (align_diff) {
        if (len && (align_diff & 1)) {
            crc0 = __crc32b(crc0, *buf++);
            len--;
        }

        if (len >= 2 && (align_diff & 2)) {
            crc0 = __crc32h(crc0, *((uint16_t*)buf));
            buf += 2;
            len -= 2;
        }

        if (len >= 4 && (align_diff & 4)) {
            crc0 = __crc32w(crc0, *((uint32_t*)buf));
            len -= 4;
            buf += 4;
        }

        if (len >= 8 && (align_diff & 8)) {
            crc0 = __crc32d(crc0, *((uint64_t*)buf));
            buf += 8;
            len -= 8;
        }
    }

    /* 3-way scalar CRC + 9-way PMULL folding (192 bytes/iter) */
    if (len >= 192) {
        const uint8_t *end = buf + len;
        size_t blk = len / 192;                  /* Number of 192-byte blocks */
        size_t klen = blk * 16;                  /* Scalar stride per CRC lane */
        const uint8_t *buf2 = buf + klen * 3;    /* Vector data starts after scalar lanes */
        uint32_t crc1 = 0, crc2 = 0;
        uint64x2_t vc0, vc1, vc2;
        uint64_t vc;

        /* Load first 9 vector chunks (144 bytes) */
        uint64x2_t x0 = vld1q_u64((const uint64_t*)buf2), y0;
        uint64x2_t x1 = vld1q_u64((const uint64_t*)(buf2 + 16)), y1;
        uint64x2_t x2 = vld1q_u64((const uint64_t*)(buf2 + 32)), y2;
        uint64x2_t x3 = vld1q_u64((const uint64_t*)(buf2 + 48)), y3;
        uint64x2_t x4 = vld1q_u64((const uint64_t*)(buf2 + 64)), y4;
        uint64x2_t x5 = vld1q_u64((const uint64_t*)(buf2 + 80)), y5;
        uint64x2_t x6 = vld1q_u64((const uint64_t*)(buf2 + 96)), y6;
        uint64x2_t x7 = vld1q_u64((const uint64_t*)(buf2 + 112)), y7;
        uint64x2_t x8 = vld1q_u64((const uint64_t*)(buf2 + 128)), y8;
        uint64x2_t k;
        /* k = {x^144 mod P, x^144+64 mod P} for 144-byte fold */
        { static const uint64_t ALIGNED_(16) k_[] = {0x26b70c3d, 0x3f41287a}; k = vld1q_u64(k_); }
        buf2 += 144;

        /* Fold 9 vectors + 3-way parallel scalar CRC */
        if (blk > 1) {
            /* Only form a limit pointer when we have at least 2 blocks. */
            const uint8_t *limit = buf + klen - 32;
            while (buf <= limit) {
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
                x0 = veor3q_u64(x0, y0, vld1q_u64((const uint64_t*)buf2));
                x1 = veor3q_u64(x1, y1, vld1q_u64((const uint64_t*)(buf2 + 16)));
                x2 = veor3q_u64(x2, y2, vld1q_u64((const uint64_t*)(buf2 + 32)));
                x3 = veor3q_u64(x3, y3, vld1q_u64((const uint64_t*)(buf2 + 48)));
                x4 = veor3q_u64(x4, y4, vld1q_u64((const uint64_t*)(buf2 + 64)));
                x5 = veor3q_u64(x5, y5, vld1q_u64((const uint64_t*)(buf2 + 80)));
                x6 = veor3q_u64(x6, y6, vld1q_u64((const uint64_t*)(buf2 + 96)));
                x7 = veor3q_u64(x7, y7, vld1q_u64((const uint64_t*)(buf2 + 112)));
                x8 = veor3q_u64(x8, y8, vld1q_u64((const uint64_t*)(buf2 + 128)));

                /* 3-way parallel scalar CRC (16 bytes each) */
                crc0 = __crc32d(crc0, *(const uint64_t*)buf);
                crc1 = __crc32d(crc1, *(const uint64_t*)(buf + klen));
                crc2 = __crc32d(crc2, *(const uint64_t*)(buf + klen * 2));
                crc0 = __crc32d(crc0, *(const uint64_t*)(buf + 8));
                crc1 = __crc32d(crc1, *(const uint64_t*)(buf + klen + 8));
                crc2 = __crc32d(crc2, *(const uint64_t*)(buf + klen * 2 + 8));
                buf += 16;
                buf2 += 144;
            }
        }

        /* Reduce 9 vectors to 1 using tree reduction */
        /* Step 1: x0 = fold(x0, x1), shift x2..x8 down */
        { static const uint64_t ALIGNED_(16) k_[] = {0xae689191, 0xccaa009e}; k = vld1q_u64(k_); }
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
        { static const uint64_t ALIGNED_(16) k_[] = {0xf1da05aa, 0x81256527}; k = vld1q_u64(k_); }
        y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
        y4 = clmul_lo(x4, k), x4 = clmul_hi(x4, k);
        x0 = veor3q_u64(x0, y0, x2);
        x4 = veor3q_u64(x4, y4, x6);

        /* Step 4: final fold (x0, x4) -> x0 */
        { static const uint64_t ALIGNED_(16) k_[] = {0x8f352d95, 0x1d9513d7}; k = vld1q_u64(k_); }
        y0 = clmul_lo(x0, k), x0 = clmul_hi(x0, k);
        x0 = veor3q_u64(x0, y0, x4);

        /* Process final scalar chunk */
        crc0 = __crc32d(crc0, *(const uint64_t*)buf);
        crc1 = __crc32d(crc1, *(const uint64_t*)(buf + klen));
        crc2 = __crc32d(crc2, *(const uint64_t*)(buf + klen * 2));
        crc0 = __crc32d(crc0, *(const uint64_t*)(buf + 8));
        crc1 = __crc32d(crc1, *(const uint64_t*)(buf + klen + 8));
        crc2 = __crc32d(crc2, *(const uint64_t*)(buf + klen * 2 + 8));

        /* Shift and combine 3 scalar CRCs */
        vc0 = crc_shift(crc0, klen * 2 + blk * 144);
        vc1 = crc_shift(crc1, klen + blk * 144);
        vc2 = crc_shift(crc2, blk * 144);
        vc = vgetq_lane_u64(veor3q_u64(vc0, vc1, vc2), 0);

        /* Final reduction: 128-bit vector + scalar CRCs -> 32-bit */
        crc0 = __crc32d(0, vgetq_lane_u64(x0, 0));
        crc0 = __crc32d(crc0, vc ^ vgetq_lane_u64(x0, 1));
        buf = buf2;
        len = end - buf;
    }

    /* 3-way scalar CRC (24 bytes/iter) */
    if (len >= 80) {
        size_t klen = ((len - 8) / 24) * 8;   /* Stride for 3-way parallel */
        uint32_t crc1 = 0, crc2 = 0;
        uint64x2_t vc0, vc1;
        uint64_t vc;

        /* 3-way parallel scalar CRC */
        do {
            crc0 = __crc32d(crc0, *(const uint64_t*)buf);
            crc1 = __crc32d(crc1, *(const uint64_t*)(buf + klen));
            crc2 = __crc32d(crc2, *(const uint64_t*)(buf + klen * 2));
            buf += 8;
            len -= 24;
        } while (len >= 32);

        /* Combine the 3 CRCs */
        vc0 = crc_shift(crc0, klen * 2 + 8);
        vc1 = crc_shift(crc1, klen + 8);
        vc = vgetq_lane_u64(veorq_u64(vc0, vc1), 0);

        /* Process final 8 bytes with combined CRC */
        buf += klen * 2;
        crc0 = crc2;
        crc0 = __crc32d(crc0, *(const uint64_t*)buf ^ vc);
        buf += 8;
        len -= 8;
    }

    /* Process remaining bytes */
    while (len >= 8) {
        crc0 = __crc32d(crc0, *((uint64_t*)buf));
        len -= 8;
        buf += 8;
    }

    if (len & 4) {
        crc0 = __crc32w(crc0, *((uint32_t*)buf));
        buf += 4;
    }

    if (len & 2) {
        crc0 = __crc32h(crc0, *((uint16_t*)buf));
        buf += 2;
    }

    if (len & 1) {
        crc0 = __crc32b(crc0, *buf);
    }

    return ~crc0;
}

Z_INTERNAL Z_TARGET_PMULL_EOR3 uint32_t crc32_copy_armv8_pmull_eor3(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc = crc32_armv8_pmull_eor3(crc, src, len);
    memcpy(dst, src, len);
    return crc;
}
#endif
