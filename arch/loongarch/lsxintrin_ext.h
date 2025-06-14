/* lsxintrin_ext.h
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef LSXINTRIN_EXT_H
#define LSXINTRIN_EXT_H

#include <lsxintrin.h>


static inline __m128i lsx_sad_bu(__m128i a, __m128i b) {
    __m128i tmp = __lsx_vabsd_bu(a, b);
    tmp = __lsx_vhaddw_hu_bu(tmp, tmp);
    tmp = __lsx_vhaddw_wu_hu(tmp, tmp);
    return __lsx_vhaddw_du_wu(tmp, tmp);
}

static inline int lsx_movemask_b(__m128i v) {
    return __lsx_vpickve2gr_w(__lsx_vmskltz_b(v), 0);
}

static inline __m128i lsx_shuffle_b(__m128i a, __m128i b) {
    /* most significant bit is set - negative 8-bit integer */
    __m128i msb_mask = __lsx_vslti_b(b, 0);

    /* shuffle, clear msb in indices vector b */
    __m128i dst = __lsx_vshuf_b(a, a, __lsx_vandi_b(b, 0xF));

    /* invert and apply mask - clear dst-element if b-msb is set */
    return __lsx_vand_v(dst, __lsx_vnor_v(msb_mask, msb_mask));
}

#endif // include guard LSXINTRIN_EXT_H
