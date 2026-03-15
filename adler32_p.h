/* adler32_p.h -- Private inline functions and macros shared with
 *                different computation of the Adler-32 checksum
 *                of a data stream.
 * Copyright (C) 1995-2011, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ADLER32_P_H
#define ADLER32_P_H

#include "zendian.h"

#define BASE 65521U     /* largest prime smaller than 65536 */
#define NMAX 5552
/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */
#define NMAX_ALIGNED32 (NMAX & ~31)
/* NMAX rounded down to a multiple of 32 is 5536 */

#define ADLER_DO1(sum1, sum2, buf, i)  {(sum1) += buf[(i)]; (sum2) += (sum1);}
#define ADLER_DO2(sum1, sum2, buf, i)  {ADLER_DO1(sum1, sum2, buf, i); ADLER_DO1(sum1, sum2, buf, i+1);}
#define ADLER_DO4(sum1, sum2, buf, i)  {ADLER_DO2(sum1, sum2, buf, i); ADLER_DO2(sum1, sum2, buf, i+2);}
#define ADLER_DO8(sum1, sum2, buf, i)  {ADLER_DO4(sum1, sum2, buf, i); ADLER_DO4(sum1, sum2, buf, i+4);}
#define ADLER_DO16(sum1, sum2, buf)    {ADLER_DO8(sum1, sum2, buf, 0); ADLER_DO8(sum1, sum2, buf, 8);}

Z_FORCEINLINE static void adler32_copy_align(uint32_t *Z_RESTRICT adler, uint8_t *dst, const uint8_t *buf, size_t len,
                                             uint32_t *Z_RESTRICT sum2, const int MAX_LEN, const int COPY) {
    Z_UNUSED(MAX_LEN);
    if (len & 1) {
        if (COPY) {
            *dst = *buf;
            dst += 1;
        }
        ADLER_DO1(*adler, *sum2, buf, 0);
        buf += 1;
    }
    if (len & 2) {
        if (COPY) {
            memcpy(dst, buf, 2);
            dst += 2;
        }
        ADLER_DO2(*adler, *sum2, buf, 0);
        buf += 2;
    }
    while (len >= 4) {
        if (COPY) {
            memcpy(dst, buf, 4);
            dst += 4;
        }
        len -= 4;
        ADLER_DO4(*adler, *sum2, buf, 0);
        buf += 4;
    }
}

#if OPTIMAL_CMP >= 64
/* SWAR scalar adler32 for 64-bit platforms with fast unaligned access. Splits bytes
 * into even/odd lanes packed as 4x16-bit in uint64_t, with prefix sums for s2.
 * Reduction uses multiply-and-shift with positional weight constants.
 *
 * Technique pioneered by Michael Niedermayer <michaelni@gmx.at>.
 * Max chunk: 23 iterations * 8 bytes = 184 (255*23 = 5865 < 65535). */
#define ADLER32_SWAR_MAX_BYTES   (23 * 8)
#define ADLER32_SWAR_EVEN_MASK   0x00FF00FF00FF00FFULL
#define ADLER32_SWAR_HSUM        0x1000100010001ULL

Z_FORCEINLINE static void adler32_swar(uint32_t *adler, uint8_t *dst, const uint8_t *buf, size_t len,
                                       uint32_t *sum2, const int COPY) {
    uint64_t sum_even = 0, sum_odd = 0, prefix_even = 0, prefix_odd = 0;

    *sum2 += *adler * (uint32_t)len;

    while (len >= 16) {
        uint64_t v0, v1;
        memcpy(&v0, buf, sizeof(v0));
        memcpy(&v1, buf + 8, sizeof(v1));
        if (COPY) {
            memcpy(dst, &v0, sizeof(v0));
            memcpy(dst + 8, &v1, sizeof(v1));
            dst += 16;
        }

        prefix_even += sum_even;
        prefix_odd += sum_odd;
        sum_even +=  v0       & ADLER32_SWAR_EVEN_MASK;
        sum_odd  += (v0 >> 8) & ADLER32_SWAR_EVEN_MASK;

        prefix_even += sum_even;
        prefix_odd += sum_odd;
        sum_even +=  v1       & ADLER32_SWAR_EVEN_MASK;
        sum_odd  += (v1 >> 8) & ADLER32_SWAR_EVEN_MASK;

        buf += 16;
        len -= 16;
    }

    /* Handle remaining 8 bytes if present */
    if (len >= 8) {
        uint64_t v;
        memcpy(&v, buf, sizeof(v));
        if (COPY)
            memcpy(dst, &v, sizeof(v));

        prefix_even += sum_even;
        prefix_odd += sum_odd;
        sum_even +=  v       & ADLER32_SWAR_EVEN_MASK;
        sum_odd  += (v >> 8) & ADLER32_SWAR_EVEN_MASK;
    }

    /* Horizontal sum of 4x16-bit lanes for s1 */
    *adler += (uint32_t)(((sum_even + sum_odd) * ADLER32_SWAR_HSUM) >> 48);

    /* Widen prefix sums to 32-bit pairs and horizontal sum for s2 */
    uint64_t pe_lo = prefix_even & 0xFFFF0000FFFFULL;
    uint64_t pe_hi = (prefix_even >> 16) & 0xFFFF0000FFFFULL;
    uint64_t po_lo = prefix_odd & 0xFFFF0000FFFFULL;
    uint64_t po_hi = (prefix_odd >> 16) & 0xFFFF0000FFFFULL;

    *sum2 += (uint32_t)(((pe_lo + po_lo + pe_hi + po_hi) * 0x800000008ULL) >> 32);

    /* Positional weights [8,7,6,5,4,3,2,1] per 8-byte group for s2.
     * On big-endian the even mask captures odd-index memory bytes (b1,b3,b5,b7)
     * so HSUM (+1 per odd-index byte) must be applied to sum_even, not sum_odd. */
#if BYTE_ORDER == LITTLE_ENDIAN
    *sum2 += 2 * (uint32_t)((sum_even * 0x4000300020001ULL) >> 48)
           +     (uint32_t)((sum_odd  * ADLER32_SWAR_HSUM) >> 48)
           + 2 * (uint32_t)((sum_odd  * 0x3000200010000ULL) >> 48);
#else
    *sum2 += 2 * (uint32_t)((sum_even * 0x0000100020003ULL) >> 48)
           +     (uint32_t)((sum_even * ADLER32_SWAR_HSUM) >> 48)
           + 2 * (uint32_t)((sum_odd  * 0x1000200030004ULL) >> 48);
#endif
}

#endif

Z_FORCEINLINE static uint32_t adler32_copy_tail(uint32_t adler, uint8_t *dst, const uint8_t *buf, size_t len,
                                                uint32_t sum2, const int REBASE, const int MAX_LEN, const int COPY) {
    if (len) {
#if OPTIMAL_CMP >= 64
        Z_UNUSED(MAX_LEN);
        /* Process using packed 64-bit arithmetic */
        while (len >= 8) {
            size_t chunk = MIN(ALIGN_DOWN(len, 8), ADLER32_SWAR_MAX_BYTES);
            adler32_swar(&adler, dst, buf, chunk, &sum2, COPY);
            buf += chunk;
            if (COPY)
                dst += chunk;
            len -= chunk;
        }
#else
        /* DO16 loop for large remainders only (scalar, risc-v). */
        if (MAX_LEN >= 32) {
            while (len >= 16) {
                if (COPY) {
                    memcpy(dst, buf, 16);
                    dst += 16;
                }
                len -= 16;
                ADLER_DO16(adler, sum2, buf);
                buf += 16;
            }
        }
#endif
        /* DO4 loop avoids GCC x86 register pressure from hoisted DO8/DO16 loads. */
        while (len >= 4) {
            if (COPY) {
                memcpy(dst, buf, 4);
                dst += 4;
            }
            len -= 4;
            ADLER_DO4(adler, sum2, buf, 0);
            buf += 4;
        }
        if (len & 2) {
            if (COPY) {
                memcpy(dst, buf, 2);
                dst += 2;
            }
            ADLER_DO2(adler, sum2, buf, 0);
            buf += 2;
        }
        if (len & 1) {
            if (COPY)
                *dst = *buf;
            ADLER_DO1(adler, sum2, buf, 0);
        }
    }
    if (REBASE) {
        adler %= BASE;
        sum2 %= BASE;
    }
    /* D = B * 65536 + A, see: https://en.wikipedia.org/wiki/Adler-32. */
    return adler | (sum2 << 16);
}

#endif /* ADLER32_P_H */
