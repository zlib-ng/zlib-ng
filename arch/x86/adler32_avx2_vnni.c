/* adler32_avx2_vnni.c -- compute the Adler-32 checksum of a data stream
 * Based on Brian Bockelman's AVX2 version
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_AVX2VNNI

#include "zbuild.h"
#include "adler32_p.h"
#include "arch_functions.h"
#include <immintrin.h>
#include "x86_intrins.h"
#include "adler32_avx2_p.h"

Z_INTERNAL uint32_t adler32_avx2_vnni(uint32_t adler, const uint8_t *src, size_t len) {
    uint32_t adler0, adler1;
    adler1 = (adler >> 16) & 0xffff;
    adler0 = adler & 0xffff;

rempeel:
    if (len < 16) {
        return adler32_copy_tail(adler0, NULL, src, len, adler1, 1, 15, 0);
    } else if (len < 32) {
        return adler32_ssse3(adler, src, len);
    }

    const __m256i dot2v = _mm256_setr_epi8(64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47,
                                           46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33);
    const __m256i dot2v_0 = _mm256_setr_epi8(32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
                                             14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);

    const __m256i zero = _mm256_setzero_si256();
    /* Just like the avx512 version but we need to split over 2 registers */
    __m256i vs1a, vs2a, vs2_0a;
    __m256i vs1b, vs2b, vs2_0b;

    while (len >= 32) {
        /* Set the bottommost bytes */
        vs1a = _mm256_zextsi128_si256(_mm_cvtsi32_si128(adler0));
        vs2a = _mm256_zextsi128_si256(_mm_cvtsi32_si128(adler1));

        vs1b = zero;
        vs2b = zero;

        __m256i vs1_0a = vs1a;
        __m256i vs1_0b = vs1b;
        __m256i vs3 = zero;
        __m256i vs3b = zero;
        vs2_0a = zero;
        vs2_0b = zero;

        size_t k = ALIGN_DOWN(MIN(len, NMAX), 32);
        len -= k;

        /* We might get a tad bit more ILP here if we sum to a second register in the loop */
        __m256i vbuf0, vbuf1, vbuf2, vbuf3;

        /* Manually unrolled this loop by 4 for a decent amount of ILP */
        while (k >= 128) {
            /*
               vs1 = adler + sum(c[i])
               vs2 = sum2 + 64 vs1 + sum( (64-i+1) c[i] )
            */
            vbuf0 = _mm256_loadu_si256((__m256i*)src);
            vbuf1 = _mm256_loadu_si256((__m256i*)(src + 32));
            vbuf2 = _mm256_loadu_si256((__m256i*)(src + 64));
            vbuf3 = _mm256_loadu_si256((__m256i*)(src + 96));
            src += 128;
            k -= 128;

            __m256i vs1_sada = _mm256_sad_epu8(vbuf0, zero);
            __m256i vs1_sadb = _mm256_sad_epu8(vbuf1, zero);

            vs1a = _mm256_add_epi32(vs1a, vs1_sada);
            vs1b = _mm256_add_epi32(vs1b, vs1_sadb);

            vs3 = _mm256_add_epi32(vs3, vs1_0a);
            vs3b = _mm256_add_epi32(vs3b, vs1_0b);

            vs2a = _mm256_dpbusd_epi32(vs2a, vbuf0, dot2v);
            vs2b = _mm256_dpbusd_epi32(vs2b, vbuf1, dot2v_0);

            vs3 = _mm256_add_epi32(vs3, vs1a);
            vs3b = _mm256_add_epi32(vs3b, vs1b);

            vs1_sada = _mm256_sad_epu8(vbuf2, zero);
            vs1_sadb = _mm256_sad_epu8(vbuf3, zero);

            vs1a = _mm256_add_epi32(vs1a, vs1_sada);
            vs1b = _mm256_add_epi32(vs1b, vs1_sadb);

            vs2_0a = _mm256_dpbusd_epi32(vs2_0a, vbuf2, dot2v);
            vs2_0b = _mm256_dpbusd_epi32(vs2_0b, vbuf3, dot2v_0);

            vs1_0a = vs1a;
            vs1_0b = vs1b;
        }

        vs3 = _mm256_add_epi32(vs3, vs3b);
        vs3 = _mm256_slli_epi32(vs3, 6);
        vs2_0a = _mm256_add_epi32(vs2_0a, vs2_0b);
        vs2a = _mm256_add_epi32(vs2a, vs3);
        vs2a = _mm256_add_epi32(vs2a, vs2_0a);
        vs2a = _mm256_add_epi32(vs2a, vs2b);
        vs1a = _mm256_add_epi32(vs1a, vs1b);
        vs1_0a = _mm256_add_epi32(vs1_0a, vs1_0b);

        vs3 = zero;


        while (k >= 32) {
            __m256i vbuf = _mm256_loadu_si256((__m256i*)src);
            src += 32;
            k -= 32;

            __m256i vs1_sad = _mm256_sad_epu8(vbuf, zero); // Sum of abs diff, resulting in 2 x int32's

            vs1a = _mm256_add_epi32(vs1a, vs1_sad);
            vs3 = _mm256_add_epi32(vs3, vs1_0a);
            vs2a = _mm256_dpbusd_epi32(vs2a, vbuf, dot2v_0);
            vs1_0a = vs1a;
        }

        vs3 = _mm256_slli_epi32(vs3, 5);
        vs2a = _mm256_add_epi32(vs2a, vs3);

        adler0 = partial_hsum256(vs1a) % BASE;
        adler1 = hsum256(vs2a) % BASE;
    }

    adler = adler0 | (adler1 << 16);

    if (len) {
        goto rempeel;
    }

    return adler;
}

/* There's a data dependency bubble to pop here to maximize throughput but I haven't been able to quite factor
 * out exactly what gets us there. I'm leaving this here as this is the closest I managed to get. For performance
 * cores on Raptor Lake, the avx2 copy function is faster. For E-cores, this function is faster. There's
 * likely some sophisticated fusion happening on Raptor Cove that is just not happening on Gracemont */

#if 0
Z_INTERNAL uint32_t adler32_copy_avx2_vnni(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len) {
    uint32_t adler0, adler1;
    adler1 = (adler >> 16) & 0xffff;
    adler0 = adler & 0xffff;

rem_peel_copy:
    if (len < 16) {
        return adler32_copy_tail(adler0, dst, src, len, adler1, 1, 15, 1);
    } else if (len < 32) {
        return adler32_copy_sse42(adler, dst, src, len);
    }

    const __m256i dot2v = _mm256_setr_epi8(64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47,
                                           46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33);
    const __m256i dot2v_0 = _mm256_setr_epi8(32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
                                             14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);

    const __m256i zero = _mm256_setzero_si256();
    __m256i vs1, vs2, vs2_0, vs3_0;

    while (len >= 32) {
        vs1 = _mm256_zextsi128_si256(_mm_cvtsi32_si128(adler0));
        vs2 = _mm256_zextsi128_si256(_mm_cvtsi32_si128(adler1));
        __m256i vs1_0 = vs1;
        __m256i vs1_1 = zero;
        __m256i vs3 = zero;
        vs2_0 = vs3;
        vs3_0 = vs3;

        size_t k = MIN(len, NMAX);
        k -= k % 32;
        len -= k;

        /* We might get a tad bit more ILP here if we sum to a second register in the loop */
        __m256i vbuf0, vbuf1;

        /* Manually unrolled this loop by 2 for a decent amount of ILP */
        while (k >= 64) {
            /*
               vs1 = adler + sum(c[i])
               vs2 = sum2 + 64 vs1 + sum( (64-i+1) c[i] )
            */
            vbuf0 = _mm256_loadu_si256((__m256i*)src);
            vbuf1 = _mm256_loadu_si256((__m256i*)(src + 32));
            _mm256_storeu_si256((__m256i*)dst, vbuf0);
            _mm256_storeu_si256((__m256i*)(dst + 32), vbuf1);
            src += 64;
            dst += 64;
            k -= 64;

            __m256i vs1_sad = _mm256_sad_epu8(vbuf0, zero);
            __m256i vs1_sad2 = _mm256_sad_epu8(vbuf1, zero);

            /* multiply-add, resulting in 16 ints. Fuse with sum stage from prior versions, as we now have the dp
             * instructions to eliminate them */
            vs2 = _mm256_dpbusd_epi32(vs2, vbuf0, dot2v);
            vs2_0 = _mm256_dpbusd_epi32(vs2_0, vbuf1, dot2v_0);

            vs1_0 = _mm256_add_epi32(vs1_0, vs1_sad);
            vs1_1 = _mm256_add_epi32(vs1_1, vs1_sad2);

            vs3_0 = _mm256_add_epi32(vs3_0, vs1_1);
            vs3 = _mm256_add_epi32(vs3, vs1_0);
            vs1_0 = vs1;
        }

        vs2 = _mm256_add_epi32(vs2_0, vs2);
        vs3 = _mm256_slli_epi32(vs3, 6);
        vs2 = _mm256_add_epi32(vs2, vs3);
        vs2 = _mm256_add_epi32(vs3_0, vs2);
        vs3 = _mm256_setzero_si256();

        while (k >= 32) {
            __m256i vbuf = _mm256_loadu_si256((__m256i*)src);
            _mm256_storeu_si256((__m256i*)dst, vbuf);
            src += 32;
            dst += 32;
            k -= 32;

            __m256i vs1_sad = _mm256_sad_epu8(vbuf, zero); // Sum of abs diff, resulting in 2 x int32's

            vs1 = _mm256_add_epi32(vs1, vs1_sad);
            vs3 = _mm256_add_epi32(vs3, vs1_0);
            vs2 = _mm256_dpbusd_epi32(vs2, vbuf, dot2v_0);
            vs1_0 = vs1;
        }

        vs3 = _mm256_slli_epi32(vs3, 5);
        vs2 = _mm256_add_epi32(vs2, vs3);

        adler0 = partial_hsum256(vs1) % BASE;
        adler1 = hsum256(vs2) % BASE;
    }

    adler = adler0 | (adler1 << 16);

    /* Process tail (len < 64). */
    if (len) {
        goto rem_peel_copy;
    }

    return adler;
}
#endif

#endif
