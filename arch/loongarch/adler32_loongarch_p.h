/* adler32_loongarch_p.h -- LoongArch optimized adler32 utility functions and tail processing
 * Copyright (C) 1995-2011 Mark Adler
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ADLER32_LOONGARCH_P_H
#define ADLER32_LOONGARCH_P_H

#include "adler32_p.h"
#ifdef LOONGARCH_LSX
#  include <lsxintrin.h>
#  include "lsxintrin_ext.h"
#endif
#ifdef LOONGARCH_LASX
#  include <lasxintrin.h>
#  include "lasxintrin_ext.h"
#endif

#ifdef LOONGARCH_LSX
/* 128-bit horizontal sum (full: 4 lanes) */
static inline uint32_t hsum128(__m128i x) {
    __m128i sum1 = __lsx_vilvh_d(x, x);
    __m128i sum2 = __lsx_vadd_w(x, sum1);
    __m128i sum3 = __lsx_vshuf4i_w(sum2, 0x01);
    __m128i sum4 = __lsx_vadd_w(sum2, sum3);
    return __lsx_vpickve2gr_w(sum4, 0);
}

/* 128-bit partial horizontal sum (2 lanes) */
static inline uint32_t partial_hsum128(__m128i x) {
    __m128i second_int = __lsx_vbsrl_v(x, 8);
    __m128i sum = __lsx_vadd_w(x, second_int);
    return __lsx_vpickve2gr_w(sum, 0);
}
#endif /* LOONGARCH_LSX */

#ifdef LOONGARCH_LASX
/* 256-bit horizontal sum (full: 8 lanes) */
static inline uint32_t hsum256(__m256i x) {
    __m256i sum1 = __lasx_xvadd_w(x, __lasx_xvbsrl_v(x, 8));
    __m256i sum2 = __lasx_xvadd_w(sum1, __lasx_xvpermi_d(sum1, 0x2));
    __m256i sum3 = __lasx_xvadd_w(sum2, __lasx_xvbsrl_v(sum2, 4));
    return (uint32_t)__lasx_xvpickve2gr_wu(sum3, 0);
}

/* 256-bit partial horizontal sum */
static inline uint32_t partial_hsum256(__m256i x) {
    __m256i sum1 = __lasx_xvadd_w(x, __lasx_xvbsrl_v(x, 8));
    __m256i sum2 = __lasx_xvadd_w(sum1, __lasx_xvpermi_d(sum1, 0x2));
    return (uint32_t)__lasx_xvpickve2gr_wu(sum2, 0);
}
#endif /* LOONGARCH_LASX */

Z_FORCEINLINE static uint32_t adler32_copy_tail_loongarch(uint32_t adler, uint8_t *dst, const uint8_t *buf, size_t len,
                                                          uint32_t sum2, const int MAX_LEN, const int COPY) {
#ifdef LOONGARCH_LASX
    if (MAX_LEN >= 32 && len >= 32) {
        const __m256i dot2v = (__m256i)((v32i8){ 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17,
                                                 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 });
        const __m256i dot3v = __lasx_xvreplgr2vr_h(1);
        const __m256i zero = __lasx_xvldi(0);

        __m256i vbuf = __lasx_xvld(buf, 0);

        if (COPY) {
            __lasx_xvst(vbuf, dst, 0);
            dst += 32;
        }

        __m256i vs1 = __lasx_xvinsgr2vr_w(zero, adler, 0);
        __m256i vs2 = __lasx_xvinsgr2vr_w(zero, sum2, 0);

        /* s2 += 32 * s1 */
        vs2 = __lasx_xvadd_w(vs2, __lasx_xvslli_w(vs1, 5));

        /* s1 += sum of all bytes */
        vs1 = __lasx_xvadd_w(vs1, lasx_sad_bu(vbuf, zero));

        /* s2 += weighted byte sum: buf[0]*32 + buf[1]*31 + ... + buf[31]*1 */
        __m256i v_short_sum = lasx_maddubs_w_h(vbuf, dot2v);
        vs2 = __lasx_xvadd_w(vs2, lasx_madd_w_h(v_short_sum, dot3v));

        adler = partial_hsum256(vs1);
        sum2 = hsum256(vs2);

        buf += 32;
        len -= 32;
    }
#endif

#ifdef LOONGARCH_LSX
    if (MAX_LEN >= 16 && len >= 16) {
        const __m128i dot2v = (__m128i)((v16i8){ 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 });
        const __m128i dot3v = __lsx_vreplgr2vr_h(1);
        const __m128i zero = __lsx_vldi(0);

        __m128i vbuf = __lsx_vld(buf, 0);

        if (COPY) {
            __lsx_vst(vbuf, dst, 0);
            dst += 16;
        }

        __m128i vs1 = __lsx_vinsgr2vr_w(zero, adler, 0);
        __m128i vs2 = __lsx_vinsgr2vr_w(zero, sum2, 0);

        /* s2 += 16 * s1 */
        vs2 = __lsx_vadd_w(vs2, __lsx_vslli_w(vs1, 4));

        /* s1 += sum of all bytes */
        vs1 = __lsx_vadd_w(vs1, lsx_sad_bu(vbuf, zero));

        /* s2 += weighted byte sum: buf[0]*16 + buf[1]*15 + ... + buf[15]*1 */
        __m128i v_short_sum = __lsx_vsadd_h(__lsx_vmulwev_h_bu_b(vbuf, dot2v), __lsx_vmulwod_h_bu_b(vbuf, dot2v));
        __m128i vsum2 = __lsx_vmaddwod_w_h(__lsx_vmulwev_w_h(v_short_sum, dot3v), v_short_sum, dot3v);
        vs2 = __lsx_vadd_w(vs2, vsum2);

        adler = partial_hsum128(vs1);
        sum2 = hsum128(vs2);

        buf += 16;
        len -= 16;
    }
#endif

    /* Process tail (len < 16). */
    return adler32_copy_tail(adler, dst, buf, len, sum2, 1, 15, COPY);
}

#endif /* ADLER32_LOONGARCH_P_H */
