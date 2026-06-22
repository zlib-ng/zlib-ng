/* adler32_msa.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Copyright (C) 2017-2026 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef MIPS_MSA
#include "zbuild.h"
#include "zendian.h"
#include "adler32_p.h"

#include <msa.h>

#define msa_zero()  (__msa_fill_w(0))

static Z_FORCEINLINE void msa_accum32(uint32_t *s, const unsigned char *buf, size_t len) {
    static const uint8_t tc0[16] ALIGNED_(16) = {16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1};
    v16u8 t0 = (v16u8) __msa_ld_b(tc0, 0);
    v4u32 adacc, s2acc;
    adacc = (v4u32) __msa_insert_w(msa_zero(), 0, s[0]);
    s2acc = (v4u32) __msa_insert_w(msa_zero(), 0, s[1]);
    while (len > 0) {
        v16u8 d0 = (v16u8) __msa_ld_b(buf, 0);
        v8u16 sum2 = __msa_dotp_u_h(t0, d0);
        s2acc = (v4u32) __msa_addv_w((v4i32) s2acc, __msa_slli_w((v4i32) adacc, 4));
        s2acc = (v4u32) __msa_addv_w((v4i32) s2acc, (v4i32) __msa_hadd_u_w(sum2, sum2));
        v8u16 d0h = __msa_hadd_u_h(d0, d0);
        adacc = (v4u32) __msa_addv_w((v4i32) adacc, (v4i32) __msa_hadd_u_w(d0h, d0h));
        buf += 16;
        len--;
    }
    v2u64 adacc2 = __msa_hadd_u_d(adacc, adacc);
    v2u64 s2acc2 = __msa_hadd_u_d(s2acc, s2acc);
    s[0] = (uint32_t) ((__msa_copy_u_d((v2i64) adacc2, 0) + __msa_copy_u_d((v2i64) adacc2, 1)) % BASE); /* Horizontal add and modulo */
    s[1] = (uint32_t) ((__msa_copy_u_d((v2i64) s2acc2, 0) + __msa_copy_u_d((v2i64) s2acc2, 1)) % BASE); /* Horizontal add and modulo */
}

uint32_t adler32_msa(uint32_t adler, const unsigned char *buf, size_t len) {
    uint32_t sum2;
    uint32_t pair[2];
    /* Split Adler-32 into component sums, it can be supplied by
     * the caller sites (e.g. in a PNG file).     */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;
    /* in case user likes doing a byte at a time, keep it fast */
    if (UNLIKELY(len == 1))
        return adler32_copy_tail(adler, NULL, buf, 1, sum2, 1, 1, 0);
    /* in case short lengths are provided, keep it somewhat fast */
    if (UNLIKELY(len < 16))
        return adler32_copy_tail(adler, NULL, buf, len, sum2, 1, 15, 0);

    pair[0] = adler;
    pair[1] = sum2;

    // Align buffer
    size_t align_diff = MIN(ALIGN_DIFF(buf, 16), len);
    size_t n = NMAX;
    if (align_diff) {
        adler32_copy_align(&pair[0], NULL, buf, align_diff, &pair[1], 15, 0);

        buf += align_diff;
        len -= align_diff;
        n -= align_diff;
    }

    while (len >= 16) {
        n = MIN(len, n);

        msa_accum32(pair, buf, n / 16);

        size_t k = (n / 16) * 16;
        buf += k;
        len -= k;
        n = NMAX;
    }
    /* Process tail (len < 16).  */
    return adler32_copy_tail(pair[0], NULL, buf, len, pair[1], len != 0 || align_diff, 15, 0);
}

/* MSA stores can have higher latency than optimized memcpy */
Z_INTERNAL uint32_t adler32_copy_msa(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
    adler = adler32_msa(adler, src, len);
    memcpy(dst, src, len);
    return adler;
}
#endif
