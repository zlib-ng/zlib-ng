/* lasxintrin_ext.h
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef LASXINTRIN_EXT_H
#define LASXINTRIN_EXT_H

#include <lsxintrin.h>
#include <lasxintrin.h>


#ifdef __clang__
#  define LA_VREGS_PREFIX "$vr"
#  define LA_XREGS_PREFIX "$xr"
#else /* GCC */
#  define LA_VREGS_PREFIX "$f"
#  define LA_XREGS_PREFIX "$f"
#endif
#define LA_ALL_REGS "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31"

static inline __m256i lasx_zextsi128_si256(__m128i in) {
    __m256i out = __lasx_xvldi(0);
    __asm__ volatile (
        ".irp i," LA_ALL_REGS                  "\n\t"
        " .ifc %[out], " LA_XREGS_PREFIX"\\i    \n\t"
        "  .irp j," LA_ALL_REGS                "\n\t"
        "   .ifc %[in], " LA_VREGS_PREFIX "\\j  \n\t"
        "    xvpermi.q $xr\\i, $xr\\j, 0x20     \n\t"
        "   .endif                              \n\t"
        "  .endr                                \n\t"
        " .endif                                \n\t"
        ".endr                                  \n\t"
        : [out] "+f" (out) : [in] "f" (in)
    );
    return out;
}

static inline __m256i lasx_set_si128(__m128i inhi, __m128i inlo) {
    __m256i out;
    __asm__ volatile (
        ".irp i," LA_ALL_REGS                  "\n\t"
        " .ifc %[hi], " LA_VREGS_PREFIX "\\i    \n\t"
        "  .irp j," LA_ALL_REGS                "\n\t"
        "   .ifc %[lo], " LA_VREGS_PREFIX "\\j  \n\t"
        "    xvpermi.q $xr\\i, $xr\\j, 0x20     \n\t"
        "   .endif                              \n\t"
        "  .endr                                \n\t"
        " .endif                                \n\t"
        ".endr                                  \n\t"
        ".ifnc %[out], %[hi]                    \n\t"
        ".irp i," LA_ALL_REGS                  "\n\t"
        " .ifc %[out], " LA_XREGS_PREFIX "\\i   \n\t"
        "  .irp j," LA_ALL_REGS                "\n\t"
        "   .ifc %[hi], " LA_VREGS_PREFIX "\\j  \n\t"
        "    xvori.b $xr\\i, $xr\\j, 0          \n\t"
        "   .endif                              \n\t"
        "  .endr                                \n\t"
        " .endif                                \n\t"
        ".endr                                  \n\t"
        ".endif                                 \n\t"
        : [out] "=f" (out), [hi] "+f" (inhi)
        : [lo] "f" (inlo)
    );
    return out;
}

static inline __m256i lasx_broadcastsi128_si256(__m128i in) {
    return lasx_set_si128(in, in);
}

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

/* See: lsx_shuffle_b */
static inline __m256i lasx_shuffle_b(__m256i a, __m256i b) {
    __m256i msb_mask = __lasx_xvslti_b(b, 0);
    __m256i dst = __lasx_xvshuf_b(a, a, __lasx_xvandi_b(b, 0xF));
    return __lasx_xvand_v(dst, __lasx_xvnor_v(msb_mask, msb_mask));
}

#endif // include guard LASXINTRIN_EXT_H
