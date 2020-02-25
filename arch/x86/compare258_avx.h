#ifndef COMPARE258_AVX_H_
#define COMPARE258_AVX_H_

/* compare258_avx.c -- AVX2 static version of compare258
 * Copyright Mika T. Lindqvist  <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#include "fallback_builtins.h"

#if defined(X86_AVX2) && defined(HAVE_BUILTIN_CTZ)

#include <immintrin.h>
#ifdef _MSC_VER
#  include <nmmintrin.h>
#endif

// UNALIGNED_OK, AVX2 instrinic comparison
static inline int32_t compare258_unaligned_avx_static(const unsigned char *src0, const unsigned char *src1) {
    register const unsigned char *src0start = src0;
    register const unsigned char *src0end = src0 + 258;
    __m256i ymm_zero = _mm256_set1_epi8(0);

    if (*(uint16_t *)src0 != *(uint16_t *)src1)
        return (*src0 == *src1);

    src0 += 2, src1 += 2;
    if (*src0 != *src1)
        return 2;
    if (src0[1] != src1[1])
        return 3;

    do {
        __m256i ymm_src0, ymm_src1, ymm_xor;
        ymm_src0 = _mm256_loadu_si256((__m256i*)src0);
        ymm_src1 = _mm256_loadu_si256((__m256i*)src1);
        ymm_xor = _mm256_xor_si256(ymm_src0, ymm_src1); // identical bytes = 00
        ymm_xor = _mm256_cmpeq_epi8(ymm_xor, ymm_zero); // non-identical bytes = 00, identical bytes = FF
        uint32_t mask = _mm256_movemask_epi8(ymm_xor); 
        if (mask != 0xFFFFFFFF) {
            int match_byte = __builtin_ctz(~mask); // Invert bits so identical = 0
            return (int32_t)(src0 - src0start + match_byte);
        }

        src0 += 32, src1 += 32;
    } while (src0 < src0end);

    return (int32_t)(src0 - src0start);
}
#endif

#endif
