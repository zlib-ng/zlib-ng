/* adler32_vmx.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Copyright (C) 2017-2021 Mika T. Lindqvist <postmaster@raasu.org>
 * Copyright (C) 2021 Adam Stylinski <kungfujesus06@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef PPC_VMX_ADLER32
#include <altivec.h>
#include "zutil.h"
#include "adler32_p.h"

#define vmx_zero()  (vec_splat_u32(0))

static inline void vmx_handle_head_or_tail(uint32_t *pair, const unsigned char *buf, size_t len) {
    unsigned int i;
    for (i = 0; i < len; ++i) {
        pair[0] += buf[i];
        pair[1] += pair[0];
    }
}

static void vmx_accum32(uint32_t *s, const unsigned char *buf, size_t len) {
    const vector unsigned char t0 = {16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1};
    /* As silly and inefficient as it seems, creating 1 permutation vector to permute
     * a 2 element vector from a single load + a subsequent shift is just barely faster
     * than doing 2 indexed insertions into zero initialized vectors from unaligned memory. */
    const vector unsigned char s0_perm = {0, 1, 2, 3, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
    const vector unsigned char shift_vec = vec_sl(vec_splat_u8(8), vec_splat_u8(2));
    vector unsigned int  adacc, s2acc;
    vector unsigned int pair_vec = vec_ld(0, s);
    adacc = vec_perm(pair_vec, pair_vec, s0_perm);
    s2acc = vec_slo(pair_vec, shift_vec);
    
    vector unsigned int s3acc = vmx_zero();
    vector unsigned int s2acc_0 = s3acc;
    vector unsigned int adacc_0 = adacc;

    int num_iter = len / 2;
    int rem = len & 1;

    for (int i = 0; i < num_iter; ++i) {
        vector unsigned char d0 = vec_ld(0, buf);
        vector unsigned char d1 = vec_ld(16, buf);

        adacc = vec_sum4s(d0, adacc);
        s3acc = vec_add(s3acc, adacc_0);
        s2acc = vec_msum(t0, d0, s2acc);

        s3acc = vec_add(s3acc, adacc);
        adacc = vec_sum4s(d1, adacc);
        s2acc_0 = vec_msum(t0, d1, s2acc_0);
        adacc_0 = adacc;

        buf += 32;
    }

    if (rem) {
        vector unsigned char d0 = vec_ld(0, buf);
        s3acc = vec_add(s3acc, adacc);
        s2acc = vec_msum(t0, d0, s2acc);
        adacc = vec_sum4s(d0, adacc);
    }

    s2acc = vec_add(s2acc, s2acc_0);
    s3acc = vec_sl(s3acc, vec_splat_u32(4));
    s2acc = vec_add(s2acc, s3acc);

    adacc = vec_add(adacc, vec_sld(adacc, adacc, 8));
    s2acc = vec_add(s2acc, vec_sld(s2acc, s2acc, 8));
    adacc = vec_add(adacc, vec_sld(adacc, adacc, 4));
    s2acc = vec_add(s2acc, vec_sld(s2acc, s2acc, 4));

    vec_ste(adacc, 0, s);
    vec_ste(s2acc, 0, s+1);
}

uint32_t adler32_vmx(uint32_t adler, const unsigned char *buf, size_t len) {
    uint32_t sum2;
    uint32_t pair[16] ALIGNED_(16);
    memset(&pair[2], 0, 14);
    int n = NMAX;
    unsigned int done = 0, i;

    /* Split Adler-32 into component sums, it can be supplied by
     * the caller sites (e.g. in a PNG file).
     */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;
    pair[0] = adler;
    pair[1] = sum2;

    /* in case user likes doing a byte at a time, keep it fast */
    if (UNLIKELY(len == 1))
        return adler32_len_1(adler, buf, sum2);

    /* initial Adler-32 value (deferred check for len == 1 speed) */
    if (UNLIKELY(buf == NULL))
        return 1L;

    /* in case short lengths are provided, keep it somewhat fast */
    if (UNLIKELY(len < 16))
        return adler32_len_16(adler, buf, len, sum2);

    // Align buffer
    unsigned int al = 0;
    if ((uintptr_t)buf & 0xf) {
        al = 16-((uintptr_t)buf & 0xf);
        if (al > len) {
            al=len;
        }
        vmx_handle_head_or_tail(pair, buf, al);

        done += al;
        /* Rather than rebasing, we can reduce the max sums for the
         * first round only */
        n -= al;
    }
    for (i = al; i < len; i += n) {
        int remaining = (int)(len-i);
        n = MIN(remaining, (i == al) ? n : NMAX);

        if (n < 16)
            break;

        vmx_accum32(pair, buf + i, n / 16);
        pair[0] %= BASE;
        pair[1] %= BASE;

        done += (n / 16) * 16;
    }

    /* Handle the tail elements. */
    if (done < len) {
        vmx_handle_head_or_tail(pair, (buf + done), len - done);
        pair[0] %= BASE;
        pair[1] %= BASE;
    }

    /* D = B * 65536 + A, see: https://en.wikipedia.org/wiki/Adler-32. */
    return (pair[1] << 16) | pair[0];
}
#endif
