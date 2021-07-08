/* adler32_vmx.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Copyright (C) 2017-2021 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef PPC_VMX_ADLER32
#include <altivec.h>
#include "zutil.h"
#include "adler32_p.h"

#define vmx_zero()  (vec_splat_u32(0))

vector unsigned short vec_hadduh(vector unsigned char a) {
    vector unsigned char vmx_one = vec_splat_u8(1);
    return vec_add(vec_mulo(a, vmx_one), vec_mule(a, vmx_one));
}

vector unsigned int vec_hadduw(vector unsigned short a) {
    vector unsigned short vmx_one = vec_splat_u16(1);
    return vec_add(vec_mulo(a, vmx_one), vec_mule(a, vmx_one));
}

static inline void vmx_handle_head_or_tail(uint32_t *pair, const unsigned char *buf, size_t len) {
    unsigned int i;
    for (i = 0; i < len; ++i) {
        pair[0] += buf[i];
        pair[1] += pair[0];
    }
}

static void vmx_accum32(uint32_t *s, const unsigned char *buf, size_t len) {
    static const uint8_t tc0[16] ALIGNED_(16) = {16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1};

    vector unsigned char t0 = vec_ld(0, tc0);
    vector unsigned int  adacc, s2acc;
    adacc = vec_insert(s[0], vmx_zero(), 0);
    s2acc = vec_insert(s[1], vmx_zero(), 0);

    while (len > 0) {
        vector unsigned char d0 = vec_ld(0, buf);
        vector unsigned short sum2;
        sum2  = vec_add(vec_mulo(t0, d0), vec_mule(t0, d0));
        s2acc = vec_add(s2acc, vec_sl(adacc, vec_splat_u32(4)));
        s2acc = vec_add(s2acc, vec_hadduw(sum2));
        adacc = vec_add(adacc, vec_hadduw(vec_hadduh(d0)));
        buf += 16;
        len--;
    }

    s[0] = vec_extract(adacc, 0) + vec_extract(adacc, 1) + vec_extract(adacc, 2) + vec_extract(adacc, 3); /* Horizontal add */
    s[1] = vec_extract(s2acc, 0) + vec_extract(s2acc, 1) + vec_extract(s2acc, 2) + vec_extract(s2acc, 3); /* Horizontal add */
}

uint32_t adler32_vmx(uint32_t adler, const unsigned char *buf, size_t len) {
    uint32_t sum2;
    uint32_t pair[2];
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
        pair[0] %= BASE;
        pair[1] %= BASE;

        done += al;
    }
    for (i = al; i < len; i += n) {
        if ((i + n) > len)
            n = (int)(len - i);

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
