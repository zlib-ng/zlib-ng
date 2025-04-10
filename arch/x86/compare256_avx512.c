/* compare256_avx512.c -- AVX512 version of compare256
 * Copyright (C) 2025 Hans Kristian Rosbach
 * Based on AVX2 implementation by Mika T. Lindqvist
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zmemory.h"
#include "deflate.h"
#include "fallback_builtins.h"

#if defined(X86_AVX512) && defined(HAVE_BUILTIN_CTZLL)

#include <immintrin.h>
#ifdef _MSC_VER
#  include <nmmintrin.h>
#endif

static inline uint32_t compare256_avx512_static(const uint8_t *src0, const uint8_t *src1) {
    __m512i zmm_src0_4, zmm_src1_4;
    __m512i zmm_src0_3, zmm_src1_3;
    __m512i zmm_src0_2, zmm_src1_2;
    __m512i zmm_src0_1, zmm_src1_1;
    __m128i xmm_src0_0, xmm_src1_0;
    uint64_t mask_1, mask_2, mask_3, mask_4;
    uint32_t mask_0;

    // First do a 16byte round before increasing to 64bytes, this reduces the
    // penalty for the short matches, and those are usually the most common ones.
    // This requires us to overlap on the last round, giving a small penalty
    // on matches of 192+ bytes (Still faster than AVX2 though).

    // 16 bytes
    xmm_src0_0 = _mm_loadu_si128((__m128i*)src0);
    xmm_src1_0 = _mm_loadu_si128((__m128i*)src1);
    mask_0 = (uint32_t)_mm_cmpeq_epu8_mask(xmm_src0_0, xmm_src1_0); // zero-extended to use __builtin_ctz
    if (mask_0 != 0x0000FFFF) {
        // There is potential for using __builtin_ctzg/__builtin_ctzs/_tzcnt_u16/__tzcnt_u16 here
        uint32_t match_byte = (uint32_t)__builtin_ctz(~mask_0); /* Invert bits so identical = 0 */
        return match_byte;
    }

    // 64 bytes
    zmm_src0_1 = _mm512_loadu_si512((__m512i*)(src0 + 16));
    zmm_src1_1 = _mm512_loadu_si512((__m512i*)(src1 + 16));
    mask_1 = _mm512_cmpeq_epu8_mask(zmm_src0_1, zmm_src1_1);
    if (mask_1 != 0xFFFFFFFFFFFFFFFF) {
        uint32_t match_byte = (uint32_t)__builtin_ctzll(~mask_1);
        return 16 + match_byte;
    }

    // 64 bytes
    zmm_src0_2 = _mm512_loadu_si512((__m512i*)(src0 + 80));
    zmm_src1_2 = _mm512_loadu_si512((__m512i*)(src1 + 80));
    mask_2 = _mm512_cmpeq_epu8_mask(zmm_src0_2, zmm_src1_2);
    if (mask_2 != 0xFFFFFFFFFFFFFFFF) {
        uint32_t match_byte = (uint32_t)__builtin_ctzll(~mask_2);
        return 80 + match_byte;
    }

    // 64 bytes
    zmm_src0_3 = _mm512_loadu_si512((__m512i*)(src0 + 144));
    zmm_src1_3 = _mm512_loadu_si512((__m512i*)(src1 + 144));
    mask_3 = _mm512_cmpeq_epu8_mask(zmm_src0_3, zmm_src1_3);
    if (mask_3 != 0xFFFFFFFFFFFFFFFF) {
        uint32_t match_byte = (uint32_t)__builtin_ctzll(~mask_3);
        return 144 + match_byte;
    }

    // 64 bytes (overlaps the previous 16 bytes for fast tail processing)
    zmm_src0_4 = _mm512_loadu_si512((__m512i*)(src0 + 192));
    zmm_src1_4 = _mm512_loadu_si512((__m512i*)(src1 + 192));
    mask_4 = _mm512_cmpeq_epu8_mask(zmm_src0_4, zmm_src1_4);
    if (mask_4 != 0xFFFFFFFFFFFFFFFF) {
        uint32_t match_byte = (uint32_t)__builtin_ctzll(~mask_4);
        return 192 + match_byte;
    }

    return 256;
}

Z_INTERNAL uint32_t compare256_avx512(const uint8_t *src0, const uint8_t *src1) {
    return compare256_avx512_static(src0, src1);
}

#define LONGEST_MATCH       longest_match_avx512
#define COMPARE256          compare256_avx512_static

#include "match_tpl.h"

#define LONGEST_MATCH_SLOW
#define LONGEST_MATCH       longest_match_slow_avx512
#define COMPARE256          compare256_avx512_static

#include "match_tpl.h"

#endif
