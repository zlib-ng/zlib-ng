/* lasxintrin_ext.h
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef LASXINTRIN_EXT_H
#define LASXINTRIN_EXT_H

#include <lsxintrin.h>
#include <lasxintrin.h>


static inline __m256i lasx_sad_bu(__m256i a, __m256i b) {
    __m256i tmp = __lasx_xvabsd_bu(a, b);
    tmp = __lasx_xvhaddw_hu_bu(tmp, tmp);
    tmp = __lasx_xvhaddw_wu_hu(tmp, tmp);
    return __lasx_xvhaddw_du_wu(tmp, tmp);
}

static inline int lasx_movemask_b(__m256i v) {
    v = __lasx_xvmskltz_b(v);
    return __lasx_xvpickve2gr_w(v, 0) | (__lasx_xvpickve2gr_w(v, 4) << 16);
}

static inline __m256i lasx_castsi128_si256(__m128i v)
{
    return (__m256i) { v[0], v[1], 0, 0 };
}

static inline __m256i lasx_inserti128_si256(__m256i a, __m128i b, const int imm8) {
    if (imm8 == 0)
        return __lasx_xvpermi_q(a, lasx_castsi128_si256(b), 0x30);
    else
        return __lasx_xvpermi_q(a, lasx_castsi128_si256(b), 0x02);
}

static inline __m256i lasx_zextsi128_si256(__m128i v) {
    return (__m256i) { v[0], v[1], 0, 0 };
    /* return lasx_inserti128_si256(__lasx_xvreplgr2vr_w(0), v, 0); */
}

/* See: lsx_shuffle_b */
static inline __m256i lasx_shuffle_b(__m256i a, __m256i b) {
    __m256i msb_mask = __lasx_xvslti_b(b, 0);
    __m256i dst = __lasx_xvshuf_b(a, a, __lasx_xvandi_b(b, 0xF));
    return __lasx_xvand_v(dst, __lasx_xvnor_v(msb_mask, msb_mask));
}

#endif // include guard LASXINTRIN_EXT_H
