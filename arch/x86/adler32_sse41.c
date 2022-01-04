/* adler32_sse41.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#include "../../adler32_p.h"

#ifdef X86_SSE41_ADLER32

#include <immintrin.h>

static inline uint32_t partial_hsum(__m128i x) {
    __m128i second_int = _mm_bsrli_si128(x, 8);
    __m128i sum = _mm_add_epi32(x, second_int);
    return _mm_cvtsi128_si32(sum);
}

static inline uint32_t hsum(__m128i x) {
    __m128i sum1 = _mm_unpackhi_epi64(x, x);
    __m128i sum2 = _mm_add_epi32(x, sum1);
    __m128i sum3 = _mm_shuffle_epi32(sum2, 0x01);
    __m128i sum4 = _mm_add_epi32(sum2, sum3);
    return _mm_cvtsi128_si32(sum4);
}

Z_INTERNAL uint32_t adler32_sse41(uint32_t adler, const unsigned char *buf, size_t len) {
    uint32_t sum2;

     /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    /* in case user likes doing a byte at a time, keep it fast */
    if (UNLIKELY(len == 1))
        return adler32_len_1(adler, buf, sum2);

    /* initial Adler-32 value (deferred check for len == 1 speed) */
    if (UNLIKELY(buf == NULL))
        return 1L;

    /* in case short lengths are provided, keep it somewhat fast */
    if (UNLIKELY(len < 16))
        return adler32_len_16(adler, buf, len, sum2);

    const __m128i dot2v = _mm_setr_epi8(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
    const __m128i dot3v = _mm_set1_epi16(1);
    const __m128i zero = _mm_setzero_si128();

    __m128i vs1 = _mm_cvtsi32_si128(adler);
    __m128i vs2 = _mm_cvtsi32_si128(sum2);

    while (len >= 16) {
       __m128i vs1_0 = vs1;
       __m128i vs3 = _mm_setzero_si128();

       int k = (len < NMAX ? (int)len : NMAX);
       k -= k % 16;
       len -= k;

       /* Aligned version of the loop */
       if (((uintptr_t)buf & 15) == 0) {
           while (k >= 16) {
               /*
                  vs1 = adler + sum(c[i])
                  vs2 = sum2 + 16 vs1 + sum( (16-i+1) c[i] )
               */
               __m128i vbuf = _mm_load_si128((__m128i*)buf);
               buf += 16;
               k -= 16;

               __m128i v_sad_sum1 = _mm_sad_epu8(vbuf, zero);
               vs1 = _mm_add_epi32(v_sad_sum1, vs1);
               vs3 = _mm_add_epi32(vs1_0, vs3);
               __m128i v_short_sum2 = _mm_maddubs_epi16(vbuf, dot2v);
               __m128i vsum2 = _mm_madd_epi16(v_short_sum2, dot3v);
               vs2 = _mm_add_epi32(vsum2, vs2);
               vs1_0 = vs1;
           }
       } else {
           while (k >= 16) {
               __m128i vbuf = _mm_loadu_si128((__m128i*)buf);
               buf += 16;
               k -= 16;

               __m128i v_sad_sum1 = _mm_sad_epu8(vbuf, zero);
               vs1 = _mm_add_epi32(v_sad_sum1, vs1);
               vs3 = _mm_add_epi32(vs1_0, vs3);
               __m128i v_short_sum2 = _mm_maddubs_epi16(vbuf, dot2v);
               __m128i vsum2 = _mm_madd_epi16(v_short_sum2, dot3v);
               vs2 = _mm_add_epi32(vsum2, vs2);
               vs1_0 = vs1;
           }
       }

       vs3 = _mm_slli_epi32(vs3, 4);
       vs2 = _mm_add_epi32(vs2, vs3);

       /* We don't actually need to do a full horizontal sum, since psadbw is actually doing
        * a partial reduction sum implicitly and only summing to integers in vector positions
        * 0 and 2. This saves us some contention on the shuffle port(s) */
       adler = partial_hsum(vs1) % BASE;
       sum2 = hsum(vs2) % BASE;

       vs1 = _mm_cvtsi32_si128(adler);
       vs2 = _mm_cvtsi32_si128(sum2);
    }

    /* Process tail (len < 16).  */
    return adler32_len_16(adler, buf, len, sum2);
}

#endif
