/* adler32_lasx.c -- compute the Adler-32 checksum of a data stream, based on Intel AVX2 implementation
 * Copyright (C) 1995-2011 Mark Adler
 * Copyright (C) 2022 Adam Stylinski
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * Authors:
 *   Brian Bockelman <bockelman@gmail.com>
 *   Adam Stylinski <kungfujesus06@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef LOONGARCH_LASX

#include "zbuild.h"
#include "adler32_p.h"

#include <lasxintrin.h>
#include "lasxintrin_ext.h"


/* 32 bit horizontal sum */
static inline uint32_t hsum256(__m256i x) {
    __m256i sum1 = __lasx_xvadd_w(x, __lasx_xvbsrl_v(x, 8));
    __m256i sum2 = __lasx_xvadd_w(sum1, __lasx_xvpermi_d(sum1, 0x2));
    __m256i sum3 = __lasx_xvadd_w(sum2, __lasx_xvbsrl_v(sum2, 4));
    return (uint32_t)__lasx_xvpickve2gr_wu(sum3, 0);
}

static inline uint32_t partial_hsum256(__m256i x) {
    __m256i sum1 = __lasx_xvadd_w(x, __lasx_xvbsrl_v(x, 8));
    __m256i sum2 = __lasx_xvadd_w(sum1, __lasx_xvpermi_d(sum1, 0x2));
    return (uint32_t)__lasx_xvpickve2gr_wu(sum2, 0);
}

extern uint32_t adler32_fold_copy_lsx(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
extern uint32_t adler32_lsx(uint32_t adler, const uint8_t *src, size_t len);

static inline uint32_t adler32_fold_copy_impl(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len, const int COPY) {
    if (src == NULL) return 1L;
    if (len == 0) return adler;

    uint32_t adler0, adler1;
    adler1 = (adler >> 16) & 0xffff;
    adler0 = adler & 0xffff;

rem_peel:
    if (len < 16) {
        if (COPY) {
            return adler32_copy_len_16(adler0, src, dst, len, adler1);
        } else {
            return adler32_len_16(adler0, src, len, adler1);
        }
    } else if (len < 32) {
        if (COPY) {
            return adler32_fold_copy_lsx(adler, dst, src, len);
        } else {
            return adler32_lsx(adler, src, len);
        }
    }

    __m256i vs1, vs2;

    const __m256i dot2v = (__m256i)((v32i8){ 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
                                             14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 });
    const __m256i dot3v = __lasx_xvreplgr2vr_h(1);
    const __m256i zero = __lasx_xvldi(0);

    while (len >= 32) {
        vs1 = __lasx_xvinsgr2vr_w(zero, adler0, 0);
        vs2 = __lasx_xvinsgr2vr_w(zero, adler1, 0);

        __m256i vs1_0 = vs1;
        __m256i vs3 = __lasx_xvldi(0);

        size_t k = MIN(len, NMAX);
        k -= k % 32;
        len -= k;

        while (k >= 32) {
            /*
               vs1 = adler + sum(c[i])
               vs2 = sum2 + 32 vs1 + sum( (32-i+1) c[i] )
            */
            __m256i vbuf = __lasx_xvld(src, 0);
            src += 32;
            k -= 32;

            __m256i vs1_sad = lasx_sad_bu(vbuf, zero); // Sum of abs diff, resulting in 2 x int32's

            if (COPY) {
                __lasx_xvst(vbuf, dst, 0);
                dst += 32;
            }

            vs1 = __lasx_xvadd_w(vs1, vs1_sad);
            vs3 = __lasx_xvadd_w(vs3, vs1_0);
            __m256i v_short_sum2 = __lasx_xvsadd_h(__lasx_xvmulwod_h_bu_b(vbuf, dot2v), __lasx_xvmulwev_h_bu_b(vbuf, dot2v)); // sum 32 uint8s to 16 shorts
            __m256i vsum2 = __lasx_xvmaddwod_w_h(__lasx_xvmulwev_w_h(v_short_sum2, dot3v), v_short_sum2, dot3v); // sum 16 shorts to 8 uint32s
            vs2 = __lasx_xvadd_w(vsum2, vs2);
            vs1_0 = vs1;
        }

        /* Defer the multiplication with 32 to outside of the loop */
        vs3 = __lasx_xvslli_w(vs3, 5);
        vs2 = __lasx_xvadd_w(vs2, vs3);

        adler0 = partial_hsum256(vs1) % BASE;
        adler1 = hsum256(vs2) % BASE;
    }

    adler = adler0 | (adler1 << 16);

    if (len) {
        goto rem_peel;
    }

    return adler;
}

Z_INTERNAL uint32_t adler32_lasx(uint32_t adler, const uint8_t *src, size_t len) {
    return adler32_fold_copy_impl(adler, NULL, src, len, 0);
}

Z_INTERNAL uint32_t adler32_fold_copy_lasx(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
    return adler32_fold_copy_impl(adler, dst, src, len, 1);
}

#endif
