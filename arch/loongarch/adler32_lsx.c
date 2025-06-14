/* adler32_lsx.c -- compute the Adler-32 checksum of a data stream, based on Intel SSE4.2 implementation
 * Copyright (C) 1995-2011 Mark Adler
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * Authors:
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "adler32_p.h"

#ifdef LOONGARCH_LSX

#include <lsxintrin.h>
#include "lsxintrin_ext.h"

static inline uint32_t partial_hsum(__m128i x) {
    __m128i second_int = __lsx_vbsrl_v(x, 8);
    __m128i sum = __lsx_vadd_w(x, second_int);
    return __lsx_vpickve2gr_w(sum, 0);
}

static inline uint32_t hsum(__m128i x) {
    __m128i sum1 = __lsx_vilvh_d(x, x);
    __m128i sum2 = __lsx_vadd_w(x, sum1);
    __m128i sum3 = __lsx_vshuf4i_w(sum2, 0x01);
    __m128i sum4 = __lsx_vadd_w(sum2, sum3);
    return __lsx_vpickve2gr_w(sum4, 0);
}

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
    }

    __m128i vbuf, vbuf_0;
    __m128i vs1_0, vs3, vs1, vs2, vs2_0, v_sad_sum1, v_short_sum2, v_short_sum2_0,
            v_sad_sum2, vsum2, vsum2_0;
    __m128i zero = __lsx_vldi(0);
    const __m128i dot2v = (__m128i)((v16i8){ 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17 });
    const __m128i dot2v_0 = (__m128i)((v16i8){ 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 });
    const __m128i dot3v = __lsx_vreplgr2vr_h(1);
    size_t k;

    while (len >= 16) {

        k = MIN(len, NMAX);
        k -= k % 16;
        len -= k;

        vs1 = __lsx_vinsgr2vr_w(zero, adler0, 0);
        vs2 = __lsx_vinsgr2vr_w(zero, adler1, 0);

        vs3 = __lsx_vldi(0);
        vs2_0 = __lsx_vldi(0);
        vs1_0 = vs1;

        while (k >= 32) {
            /*
               vs1 = adler + sum(c[i])
               vs2 = sum2 + 16 vs1 + sum( (16-i+1) c[i] )
            */
            vbuf = __lsx_vld(src, 0);
            vbuf_0 = __lsx_vld(src, 16);
            src += 32;
            k -= 32;

            v_sad_sum1 = lsx_sad_bu(vbuf, zero);
            v_sad_sum2 = lsx_sad_bu(vbuf_0, zero);

            if (COPY) {
                __lsx_vst(vbuf, dst, 0);
                __lsx_vst(vbuf_0, dst, 16);
                dst += 32;
            }

            v_short_sum2 = __lsx_vsadd_h(__lsx_vmulwev_h_bu_b(vbuf, dot2v), __lsx_vmulwod_h_bu_b(vbuf, dot2v));
            v_short_sum2_0 = __lsx_vsadd_h(__lsx_vmulwev_h_bu_b(vbuf_0, dot2v_0), __lsx_vmulwod_h_bu_b(vbuf_0, dot2v_0));

            vs1 = __lsx_vadd_w(v_sad_sum1, vs1);
            vs3 = __lsx_vadd_w(vs1_0, vs3);

            vsum2 = __lsx_vmaddwod_w_h(__lsx_vmulwev_w_h(v_short_sum2, dot3v), v_short_sum2, dot3v);
            vsum2_0 = __lsx_vmaddwod_w_h(__lsx_vmulwev_w_h(v_short_sum2_0, dot3v), v_short_sum2_0, dot3v);
            vs1 = __lsx_vadd_w(v_sad_sum2, vs1);
            vs2 = __lsx_vadd_w(vsum2, vs2);
            vs2_0 = __lsx_vadd_w(vsum2_0, vs2_0);
            vs1_0 = vs1;
        }

        vs2 = __lsx_vadd_w(vs2_0, vs2);
        vs3 = __lsx_vslli_w(vs3, 5);
        vs2 = __lsx_vadd_w(vs3, vs2);
        vs3 = __lsx_vldi(0);

        while (k >= 16) {
            /*
               vs1 = adler + sum(c[i])
               vs2 = sum2 + 16 vs1 + sum( (16-i+1) c[i] )
            */
            vbuf = __lsx_vld(src, 0);
            src += 16;
            k -= 16;

            v_sad_sum1 = lsx_sad_bu(vbuf, zero);
            v_short_sum2 = __lsx_vsadd_h(__lsx_vmulwev_h_bu_b(vbuf, dot2v_0), __lsx_vmulwod_h_bu_b(vbuf, dot2v_0));

            vs1 = __lsx_vadd_w(v_sad_sum1, vs1);
            vs3 = __lsx_vadd_w(vs1_0, vs3);
            vsum2 = __lsx_vmaddwod_w_h(__lsx_vmulwev_w_h(v_short_sum2, dot3v), v_short_sum2, dot3v);
            vs2 = __lsx_vadd_w(vsum2, vs2);
            vs1_0 = vs1;

            if (COPY) {
                __lsx_vst(vbuf, dst, 0);
                dst += 16;
            }
        }

        vs3 = __lsx_vslli_w(vs3, 4);
        vs2 = __lsx_vadd_w(vs2, vs3);

        adler0 = partial_hsum(vs1) % BASE;
        adler1 = hsum(vs2) % BASE;
    }

    /* If this is true, there's fewer than 16 elements remaining */
    if (len) {
        goto rem_peel;
    }

    return adler0 | (adler1 << 16);
}

Z_INTERNAL uint32_t adler32_lsx(uint32_t adler, const uint8_t *src, size_t len) {
    return adler32_fold_copy_impl(adler, NULL, src, len, 0);
}

Z_INTERNAL uint32_t adler32_fold_copy_lsx(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
    return adler32_fold_copy_impl(adler, dst, src, len, 1);
}

#endif
