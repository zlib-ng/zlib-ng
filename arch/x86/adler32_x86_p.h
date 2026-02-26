/* adler32_x86_p.h -- x86 optimized short-length adler32 tail processing
 * Copyright (C) 1995-2011 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ADLER32_X86_P_H
#define ADLER32_X86_P_H

#include <immintrin.h>

#include "adler32_p.h"
#include "adler32_ssse3_p.h"
#if defined(X86_AVX2) || defined(X86_AVX512VNNI)
#  include "adler32_avx2_p.h"
#endif

Z_FORCEINLINE static uint32_t adler32_copy_tail_x86(uint32_t adler, uint8_t *dst, const uint8_t *buf, size_t len,
                                                    uint32_t sum2, const int MAX_LEN, const int COPY) {
#if defined(X86_AVX2) || defined(X86_AVX512VNNI)
    if (MAX_LEN >= 32 && len >= 32) {
        const __m256i dot2v = _mm256_setr_epi8(32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17,
                                               16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        const __m256i dot3v = _mm256_set1_epi16(1);
        const __m256i zero = _mm256_setzero_si256();

        __m256i vbuf = _mm256_loadu_si256((__m256i*)buf);

        if (COPY) {
            _mm256_storeu_si256((__m256i*)dst, vbuf);
            dst += 32;
        }

        __m256i vs1 = _mm256_zextsi128_si256(_mm_cvtsi32_si128(adler));
        __m256i vs2 = _mm256_zextsi128_si256(_mm_cvtsi32_si128(sum2));

        /* s2 += 32 * s1 */
        vs2 = _mm256_add_epi32(vs2, _mm256_slli_epi32(vs1, 5));

        /* s1 += sum of all bytes */
        vs1 = _mm256_add_epi32(vs1, _mm256_sad_epu8(vbuf, zero));

        /* s2 += weighted byte sum: buf[0]*32 + buf[1]*31 + ... + buf[31]*1 */
        __m256i v_short_sum = _mm256_maddubs_epi16(vbuf, dot2v);
        vs2 = _mm256_add_epi32(vs2, _mm256_madd_epi16(v_short_sum, dot3v));

        adler = partial_hsum256(vs1);
        sum2 = hsum256(vs2);

        buf += 32;
        len -= 32;
    }
#endif

#ifdef X86_SSSE3
    if (MAX_LEN >= 16 && len >= 16) {
        const __m128i dot2v = _mm_setr_epi8(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
        const __m128i dot3v = _mm_set1_epi16(1);
        const __m128i zero = _mm_setzero_si128();

        __m128i vbuf = _mm_loadu_si128((__m128i*)buf);

        if (COPY) {
            _mm_storeu_si128((__m128i*)dst, vbuf);
            dst += 16;
        }

        __m128i vs1 = _mm_cvtsi32_si128(adler);
        __m128i vs2 = _mm_cvtsi32_si128(sum2);

        /* s2 += 16 * s1 */
        vs2 = _mm_add_epi32(vs2, _mm_slli_epi32(vs1, 4));

        /* s1 += sum of all bytes */
        vs1 = _mm_add_epi32(vs1, _mm_sad_epu8(vbuf, zero));

        /* s2 += weighted byte sum: buf[0]*16 + buf[1]*15 + ... + buf[15]*1 */
        __m128i v_short_sum = _mm_maddubs_epi16(vbuf, dot2v);
        vs2 = _mm_add_epi32(vs2, _mm_madd_epi16(v_short_sum, dot3v));

        adler = partial_hsum(vs1);
        sum2 = hsum(vs2);

        buf += 16;
        len -= 16;
    }
#endif

    /* Process tail (len < 16). */
    return adler32_copy_tail(adler, dst, buf, len, sum2, 1, 15, COPY);
}

#endif /* ADLER32_X86_P_H */
