#ifndef COMPARE258_SSE_H_
#define COMPARE258_SSE_H_

/* compare258_sse.c -- SSE4.2 static version of compare258
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Authors:
 *  Wajdi Feghali   <wajdi.k.feghali@intel.com>
 *  Jim Guilford    <james.guilford@intel.com>
 *  Vinodh Gopal    <vinodh.gopal@intel.com>
 *     Erdinc Ozturk   <erdinc.ozturk@intel.com>
 *  Jim Kukunas     <james.t.kukunas@linux.intel.com>
 *
 * Portions are Copyright (C) 2016 12Sided Technology, LLC.
 * Author:
 *  Phil Vachon     <pvachon@12sidedtech.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#ifdef X86_SSE42_CMP_STR

#include <immintrin.h>
#ifdef _MSC_VER
#  include <nmmintrin.h>
#endif

// UNALIGNED_OK, SSE4.2 instrinic comparison
static inline int32_t compare258_unaligned_sse_static(const unsigned char *src0, const unsigned char *src1) {
#ifdef _MSC_VER
    register const unsigned char *src0start = src0;
    register const unsigned char *src0end = src0 + 258; // (258 - 2) % 16 = 0

    if (*(uint16_t *)src0 != *(uint16_t *)src1)
        return (*src0 == *src1);

    src0 += 2, src1 += 2;

    if (*src0 != *src1)
        return 2;
    if (src0[1] != src1[1])
        return 3;

    do {
        #define mode _SIDD_UBYTE_OPS | _SIDD_CMP_EQUAL_EACH | _SIDD_NEGATIVE_POLARITY
        __m128i xmm_src0, xmm_src1;
        int ret;

        xmm_src0 = _mm_loadu_si128((__m128i *)src0);
        xmm_src1 = _mm_loadu_si128((__m128i *)src1);
        ret = _mm_cmpestri(xmm_src0, 16, xmm_src1, 16, mode);
        if (_mm_cmpestrc(xmm_src0, 16, xmm_src1, 16, mode)) {
            return (int32_t)(src0 - src0start + ret);
        }
        src0 += 16, src1 += 16;

        xmm_src0 = _mm_loadu_si128((__m128i *)src0);
        xmm_src1 = _mm_loadu_si128((__m128i *)src1);
        ret = _mm_cmpestri(xmm_src0, 16, xmm_src1, 16, mode);
        if (_mm_cmpestrc(xmm_src0, 16, xmm_src1, 16, mode)) {
            return (int32_t)(src0 - src0start + ret);
        }
        src0 += 16, src1 += 16;
    } while (src0 < src0end);

    return (int32_t)(src0 - src0start);
#else
    uintptr_t ax, dx, cx;
    __m128i xmm_src0;

    ax = 16;
    dx = 16;
    /* Set cx to something, otherwise gcc thinks it's used
       uninitalised */
    cx = 0;

    __asm__ __volatile__ (
    "1:"
        "movdqu     -16(%[src0], %[ax]), %[xmm_src0]\n\t"
        "pcmpestri  $0x18, -16(%[src1], %[ax]), %[xmm_src0]\n\t"
        "jc         2f\n\t"
        "add        $16, %[ax]\n\t"

        "movdqu     -16(%[src0], %[ax]), %[xmm_src0]\n\t"
        "pcmpestri  $0x18, -16(%[src1], %[ax]), %[xmm_src0]\n\t"
        "jc         2f\n\t"
        "add        $16, %[ax]\n\t"

        "cmp        $256 + 16, %[ax]\n\t"
        "jb         1b\n\t"

#  if !defined(__x86_64__)
        "movzwl     -16(%[src0], %[ax]), %[dx]\n\t"
#  else
        "movzwq     -16(%[src0], %[ax]), %[dx]\n\t"
#  endif
        "xorw       -16(%[src1], %[ax]), %%dx\n\t"
        "jnz        3f\n\t"

        "add        $2, %[ax]\n\t"
        "jmp        4f\n\t"
    "3:\n\t"
        "rep; bsf   %[dx], %[cx]\n\t"
        "shr        $3, %[cx]\n\t"
    "2:"
        "add        %[cx], %[ax]\n\t"
    "4:"
    : [ax] "+a" (ax),
      [cx] "+c" (cx),
      [dx] "+d" (dx),
      [xmm_src0] "=x" (xmm_src0)
    : [src0] "r" (src0),
      [src1] "r" (src1)
    : "cc"
    );
    return (int32_t)(ax - 16);
#endif
}

#endif

#endif
