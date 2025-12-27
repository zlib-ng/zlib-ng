/* lasxintrin_ext.h
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef LASXINTRIN_EXT_H
#define LASXINTRIN_EXT_H

#include <lsxintrin.h>
#include <lasxintrin.h>


static inline __m256i lasx_zext_128(__m128i src) {
#ifdef __loongarch_asx_sx_conv
    return __lasx_insert_128_lo(__lasx_xvldi(0), src);
#else
    __m256i dest = __lasx_xvldi(0);
    __asm__ volatile ("xvpermi.q %u0,%u2,0x30\n" : "=f"(dest) : "0"(dest), "f"(src));
    return dest;
#endif
}

#ifndef __loongarch_asx_sx_conv
static inline __m256i __lasx_concat_128(__m128i lo, __m128i hi) {
    __m256i dest;
    __asm__ volatile ("xvpermi.q %u0,%u2,0x02\n" : "=f"(dest) : "0"(lo), "f"(hi));
    return dest;
}
#endif

static inline __m256i lasx_broadcast_128(__m128i in) {
    return __lasx_concat_128(in, in);
}

static inline __m256i lasx_sad_bu(__m256i a, __m256i b) {
    __m256i tmp = __lasx_xvabsd_bu(a, b);
    tmp = __lasx_xvhaddw_hu_bu(tmp, tmp);
    tmp = __lasx_xvhaddw_wu_hu(tmp, tmp);
    return __lasx_xvhaddw_du_wu(tmp, tmp);
}

static inline __m256i lasx_maddubs_w_h(__m256i a, __m256i b) {
    return __lasx_xvsadd_h(__lasx_xvmulwod_h_bu_b(a, b), __lasx_xvmulwev_h_bu_b(a, b));
}

static inline __m256i lasx_madd_w_h(__m256i a, __m256i b) {
    return __lasx_xvmaddwod_w_h(__lasx_xvmulwev_w_h(a, b), a, b);
}

static inline int lasx_movemask_b(__m256i v) {
    v = __lasx_xvmskltz_b(v);
    return __lasx_xvpickve2gr_w(v, 0) | (__lasx_xvpickve2gr_w(v, 4) << 16);
}

/* See: lsx_shuffle_b */
static inline __m256i lasx_shuffle_b(__m256i a, __m256i b) {
    __m256i msb_mask = __lasx_xvslti_b(b, 0);
    __m256i dst = __lasx_xvshuf_b(a, a, __lasx_xvandi_b(b, 0xF));
    return __lasx_xvand_v(dst, __lasx_xvnor_v(msb_mask, msb_mask));
}

#endif // include guard LASXINTRIN_EXT_H
