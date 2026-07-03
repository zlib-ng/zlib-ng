/* adler32_avx512_vnni.c -- compute the Adler-32 checksum of a data stream
 * Based on Brian Bockelman's AVX2 version
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_AVX512VNNI

#include "zbuild.h"
#include "adler32_p.h"
#include "arch_functions.h"
#include <immintrin.h>
#include "x86_intrins.h"
#include "adler32_avx512_p.h"
#include "adler32_avx2_p.h"

Z_INTERNAL uint32_t adler32_avx512_vnni(uint32_t adler, const uint8_t *src, size_t len) {
    uint32_t adler0, adler1;
    adler1 = (adler >> 16) & 0xffff;
    adler0 = adler & 0xffff;

rem_peel:
    if (len < 32)
        return adler32_ssse3(adler, src, len);

    if (len < 64)
        return adler32_avx2(adler, src, len);

    const __m512i dot2v = _mm512_set_epi8(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                          20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
                                          38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
                                          56, 57, 58, 59, 60, 61, 62, 63, 64);

    const __m512i zero = _mm512_setzero_si512();
    __m512i vs1, vs2_0;

    while (len >= 64) {
        vs1 = _mm512_zextsi128_si512(_mm_cvtsi32_si128(adler0));
        vs2_0 = _mm512_zextsi128_si512(_mm_cvtsi32_si128(adler1));
        size_t k = ALIGN_DOWN(MIN(len, NMAX), 64);
        len -= k;
        __m512i vs1_0 = vs1;
        __m512i vs3 = _mm512_setzero_si512();

        __m512i vs2_1 = _mm512_setzero_si512();
        __m512i vs2_2 = _mm512_setzero_si512();
        __m512i vs2_3 = _mm512_setzero_si512();

        __m512i vbuf0, vbuf1, vbuf2, vbuf3;

        /* Remainder peeling for 256-byte alignment boundaries */
        while (k % 256) {
            vbuf0 = _mm512_loadu_si512((__m512i*)src);
            src += 64;
            k -= 64;

            __m512i vs1_sad = _mm512_sad_epu8(vbuf0, zero);
            vs1 = _mm512_add_epi32(vs1, vs1_sad);
            vs3 = _mm512_add_epi32(vs3, vs1_0);
            vs2_0 = _mm512_dpbusd_epi32(vs2_0, vbuf0, dot2v);
            vs1_0 = vs1;
        }

        /* Manually unrolled by 4 to maximize ILP with separate accumulators */
        while (k >= 256) {
            vbuf0 = _mm512_loadu_si512((__m512i*)src);
            vbuf1 = _mm512_loadu_si512((__m512i*)(src + 64));
            vbuf2 = _mm512_loadu_si512((__m512i*)(src + 128));
            vbuf3 = _mm512_loadu_si512((__m512i*)(src + 192));
            src += 256;
            k -= 256;

            // Chunk 0
            __m512i vs1_sad0 = _mm512_sad_epu8(vbuf0, zero);
            vs1 = _mm512_add_epi32(vs1, vs1_sad0);
            vs3 = _mm512_add_epi32(vs3, vs1_0);
            vs2_0 = _mm512_dpbusd_epi32(vs2_0, vbuf0, dot2v);

            // Chunk 1
            vs3 = _mm512_add_epi32(vs3, vs1);
            __m512i vs1_sad1 = _mm512_sad_epu8(vbuf1, zero);
            vs1 = _mm512_add_epi32(vs1, vs1_sad1);
            vs2_1 = _mm512_dpbusd_epi32(vs2_1, vbuf1, dot2v);

            // Chunk 2
            vs3 = _mm512_add_epi32(vs3, vs1);
            __m512i vs1_sad2 = _mm512_sad_epu8(vbuf2, zero);
            vs1 = _mm512_add_epi32(vs1, vs1_sad2);
            vs2_2 = _mm512_dpbusd_epi32(vs2_2, vbuf2, dot2v);

            // Chunk 3
            vs3 = _mm512_add_epi32(vs3, vs1);
            __m512i vs1_sad3 = _mm512_sad_epu8(vbuf3, zero);
            vs1 = _mm512_add_epi32(vs1, vs1_sad3);
            vs2_3 = _mm512_dpbusd_epi32(vs2_3, vbuf3, dot2v);

            vs1_0 = vs1;
        }

        /* Scale baseline accumulators by 64 bytes. 128 rolls over the sign bit
         * and the dot product instruction forces the multiplicand to be interpreted
         * as signed prior to the accumulation */
        vs3 = _mm512_slli_epi32(vs3, 6);
        vs2_0 = _mm512_add_epi32(vs2_0, vs3);

        vs2_0 = _mm512_add_epi32(vs2_0, vs2_1);
        vs2_2 = _mm512_add_epi32(vs2_2, vs2_3);
        vs2_0 = _mm512_add_epi32(vs2_0, vs2_2);

        adler0 = partial_hsum(vs1) % BASE;
        adler1 = _mm512_reduce_add_epu32(vs2_0) % BASE;
    }

    adler = adler0 | (adler1 << 16);

    /* Process tail (len < 64). */
    if (len) {
        goto rem_peel;
    }

    return adler;
}

/* Use 256-bit vectors when copying because 512-bit variant is slower for numerous architectural reasons */
Z_INTERNAL uint32_t adler32_copy_avx512_vnni(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
    uint32_t adler0, adler1;
    adler1 = (adler >> 16) & 0xffff;
    adler0 = adler & 0xffff;

rem_peel_copy:
    if (len < 32) {
       __mmask32 storemask = _bzhi_u32(0xFFFFFFFF, (unsigned)len);
        __m256i copy_vec = _mm256_maskz_loadu_epi8(storemask, src);
        _mm256_mask_storeu_epi8(dst, storemask, copy_vec);

        return adler32_ssse3(adler, src, len);
    }

    const __m256i dot2v = _mm256_set_epi8(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                          20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32);

    const __m256i zero = _mm256_setzero_si256();
    __m256i vs1, vs2_0;

    while (len >= 32) {
        vs1 = _mm256_zextsi128_si256(_mm_cvtsi32_si128(adler0));
        vs2_0 = _mm256_zextsi128_si256(_mm_cvtsi32_si128(adler1));

        size_t k = ALIGN_DOWN(MIN(len, NMAX), 32);
        len -= k;

        __m256i vs1_0 = vs1;
        __m256i vs3 = _mm256_setzero_si256();

        __m256i vs2_1 = _mm256_setzero_si256();
        __m256i vs2_2 = _mm256_setzero_si256();
        __m256i vs2_3 = _mm256_setzero_si256();

        __m256i vbuf0, vbuf1, vbuf2, vbuf3;

        while (k % 128) {
            vbuf0 = _mm256_loadu_si256((__m256i*)src);
            _mm256_storeu_si256((__m256i*)dst, vbuf0);
            dst += 32;
            src += 32;
            k -= 32;

            __m256i vs1_sad = _mm256_sad_epu8(vbuf0, zero);
            vs1 = _mm256_add_epi32(vs1, vs1_sad);
            vs3 = _mm256_add_epi32(vs3, vs1_0);
            vs2_0 = _mm256_dpbusd_epi32(vs2_0, vbuf0, dot2v);
            vs1_0 = vs1;
        }

        /* Manually unrolled by 4 blocks (128 bytes total per iteration) */
        while (k >= 128) {
            vbuf0 = _mm256_loadu_si256((__m256i*)src);
            vbuf1 = _mm256_loadu_si256((__m256i*)(src + 32));
            vbuf2 = _mm256_loadu_si256((__m256i*)(src + 64));
            vbuf3 = _mm256_loadu_si256((__m256i*)(src + 96));
            src += 128;
            k -= 128;

            _mm256_storeu_si256((__m256i*)dst, vbuf0);
            _mm256_storeu_si256((__m256i*)(dst + 32), vbuf1);
            _mm256_storeu_si256((__m256i*)(dst + 64), vbuf2);
            _mm256_storeu_si256((__m256i*)(dst + 96), vbuf3);
            dst += 128;

            __m256i vs1_sad0 = _mm256_sad_epu8(vbuf0, zero);
            vs1 = _mm256_add_epi32(vs1, vs1_sad0);
            vs3 = _mm256_add_epi32(vs3, vs1_0);
            vs2_0 = _mm256_dpbusd_epi32(vs2_0, vbuf0, dot2v);

            vs3 = _mm256_add_epi32(vs3, vs1);
            __m256i vs1_sad1 = _mm256_sad_epu8(vbuf1, zero);
            vs1 = _mm256_add_epi32(vs1, vs1_sad1);
            vs2_1 = _mm256_dpbusd_epi32(vs2_1, vbuf1, dot2v);

            vs3 = _mm256_add_epi32(vs3, vs1);
            __m256i vs1_sad2 = _mm256_sad_epu8(vbuf2, zero);
            vs1 = _mm256_add_epi32(vs1, vs1_sad2);
            vs2_2 = _mm256_dpbusd_epi32(vs2_2, vbuf2, dot2v);

            vs3 = _mm256_add_epi32(vs3, vs1);
            __m256i vs1_sad3 = _mm256_sad_epu8(vbuf3, zero);
            vs1 = _mm256_add_epi32(vs1, vs1_sad3);
            vs2_3 = _mm256_dpbusd_epi32(vs2_3, vbuf3, dot2v);

            vs1_0 = vs1;
        }

        vs3 = _mm256_slli_epi32(vs3, 5);
        vs2_0 = _mm256_add_epi32(vs2_0, vs3);

        vs2_0 = _mm256_add_epi32(vs2_0, vs2_1);
        vs2_2 = _mm256_add_epi32(vs2_2, vs2_3);
        vs2_0 = _mm256_add_epi32(vs2_0, vs2_2);

        adler0 = partial_hsum256(vs1) % BASE;
        adler1 = hsum256(vs2_0) % BASE;
    }

    adler = adler0 | (adler1 << 16);

    /* Process tail (len < 32). */
    if (len) {
        goto rem_peel_copy;
    }

    return adler;
}

#endif
