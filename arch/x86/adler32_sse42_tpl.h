/* adler32_ssse3_tpl.h -- adler32 ssse3 vectorized function templates
 * Copyright (C) 2022 Adam Stylinski
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ADLER32_SSE42_TPL_H_
#define ADLER32_SSE42_TPL_H_

#include "../../zbuild.h"
#include <immintrin.h>
#include "../../adler32_fold.h"
#include "../../adler32_p.h"
#include "adler32_ssse3_p.h"

#ifdef COPY
Z_INTERNAL void adler32_fold_copy_sse42(adler32_fold *adler, uint8_t *dst, const uint8_t *src, size_t len) {
#else
Z_INTERNAL void adler32_fold_sse42(adler32_fold *adler, const uint8_t *src, size_t len) {
#endif

    uint32_t adler0, adler1;
    adler1 = (adler->nsums >> 16) & 0xffff;
    adler0 = adler->nsums & 0xffff; 

    if (len < 16) {
rem_peel:
#ifdef COPY
       adler->nsums = adler32_copy_len_16(adler0, src, dst, len, adler1);
#else
       adler->nsums = adler32_len_16(adler0, src, len, adler1);
#endif
       return;
    }

    __m128i vbuf, vbuf_0;
    __m128i vs1_0, vs3, vs1, vs2, vs2_0, v_sad_sum1, v_short_sum2, v_short_sum2_0,
            v_sad_sum2, vsum2, vsum2_0;
    __m128i zero = _mm_setzero_si128();
    const __m128i dot2v = _mm_setr_epi8(32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17);
    const __m128i dot2v_0 = _mm_setr_epi8(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
    const __m128i dot3v = _mm_set1_epi16(1);
    size_t k;

    while (len >= 16) {

        k = MIN(len, NMAX);
        k -= k % 16;
        len -= k;

        vs1 = _mm_cvtsi32_si128(adler0);
        vs2 = _mm_cvtsi32_si128(adler1);

        vs3 = _mm_setzero_si128();
        vs2_0 = _mm_setzero_si128();
        vs1_0 = vs1;

        while (k >= 32) {
            /*
               vs1 = adler + sum(c[i])
               vs2 = sum2 + 16 vs1 + sum( (16-i+1) c[i] )
            */
            vbuf = _mm_loadu_si128((__m128i*)src);
            vbuf_0 = _mm_loadu_si128((__m128i*)(src + 16));
            src += 32;
            k -= 32;

            v_sad_sum1 = _mm_sad_epu8(vbuf, zero);
            v_sad_sum2 = _mm_sad_epu8(vbuf_0, zero);
#ifdef COPY
            _mm_storeu_si128((__m128i*)dst, vbuf);
            _mm_storeu_si128((__m128i*)(dst + 16), vbuf_0);
            dst += 32;
#endif
            v_short_sum2 = _mm_maddubs_epi16(vbuf, dot2v);
            v_short_sum2_0 = _mm_maddubs_epi16(vbuf_0, dot2v_0);

            vs1 = _mm_add_epi32(v_sad_sum1, vs1);
            vs3 = _mm_add_epi32(vs1_0, vs3);

            vsum2 = _mm_madd_epi16(v_short_sum2, dot3v);
            vsum2_0 = _mm_madd_epi16(v_short_sum2_0, dot3v);
            vs1 = _mm_add_epi32(v_sad_sum2, vs1);
            vs2 = _mm_add_epi32(vsum2, vs2);
            vs2_0 = _mm_add_epi32(vsum2_0, vs2_0);
            vs1_0 = vs1;
        }

        vs2 = _mm_add_epi32(vs2_0, vs2);
        vs3 = _mm_slli_epi32(vs3, 5);
        vs2 = _mm_add_epi32(vs3, vs2);
        vs3 = _mm_setzero_si128();

        while (k >= 16) {
            /*
               vs1 = adler + sum(c[i])
               vs2 = sum2 + 16 vs1 + sum( (16-i+1) c[i] )
            */
            vbuf = _mm_loadu_si128((__m128i*)src);
            src += 16;
            k -= 16;

            v_sad_sum1 = _mm_sad_epu8(vbuf, zero);
            v_short_sum2 = _mm_maddubs_epi16(vbuf, dot2v_0);

            vs1 = _mm_add_epi32(v_sad_sum1, vs1);
            vs3 = _mm_add_epi32(vs1_0, vs3);
            vsum2 = _mm_madd_epi16(v_short_sum2, dot3v);
            vs2 = _mm_add_epi32(vsum2, vs2);
            vs1_0 = vs1;

#ifdef COPY
            _mm_storeu_si128((__m128i*)dst, vbuf);
            dst += 16;
#endif
        }

        vs3 = _mm_slli_epi32(vs3, 4);
        vs2 = _mm_add_epi32(vs2, vs3);

        adler0 = partial_hsum(vs1) % BASE;
        adler1 = hsum(vs2) % BASE;
    }

    /* If this is true, there's fewer than 16 elements remaining */
    if (len) {
        goto rem_peel;
    }

    adler->nsums = adler0 | (adler1 << 16);
}

#endif
