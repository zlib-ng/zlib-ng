/* adler32_avx512.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Adam Stylinski <kungfujesus06@gmail.com>
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../adler32_p.h"
#include "../../cpu_features.h"
#include "../../fallback_builtins.h"

#include <immintrin.h>

#ifdef X86_AVX512_ADLER32
static inline uint32_t partial_hsum(__m512i x) {
    /* We need a permutation vector to extract every other integer. The
     * rest are going to be zeros. Marking this const so the compiler stands
     * a better chance of keeping this resident in a register through entire 
     * loop execution. We certainly have enough zmm registers (32) */
    const __m512i perm_vec = _mm512_setr_epi32(0, 2, 4, 6, 8, 10, 12, 14,
                                               1, 1, 1, 1, 1,  1,  1,  1);

    __m512i non_zero = _mm512_permutexvar_epi32(perm_vec, x);

    /* From here, it's a simple 256 bit wide reduction sum */
    __m256i non_zero_avx = _mm512_castsi512_si256(non_zero);
    
    /* See Agner Fog's vectorclass for a decent reference. Essentially, phadd is
     * pretty slow, much slower than the longer instruction sequence below */
    __m128i sum1  = _mm_add_epi32(_mm256_extracti128_si256(non_zero_avx, 1),
                                  _mm256_castsi256_si128(non_zero_avx));
    __m128i sum2  = _mm_add_epi32(sum1,_mm_unpackhi_epi64(sum1, sum1));
    __m128i sum3  = _mm_add_epi32(sum2,_mm_shuffle_epi32(sum2, 1));
    return (uint32_t)_mm_cvtsi128_si32(sum3);
}

Z_INTERNAL uint32_t adler32_avx512(uint32_t adler, const unsigned char *buf, size_t len) {
    uint32_t sum2;

    /* For impossibly tiny sizes, use the smaller width versions. We still need
     * to check for compile time support for these but they are likely there */
#ifdef X86_SSE41_ADLER32 
    if (len < 32) 
        return adler32_sse41(adler, buf, len);
#endif

#ifdef X86_AVX2_ADLER32
    if (len < 64)
        return adler32_avx2(adler, buf, len);
#endif

     /* split Adler-32 into component sums */
    sum2 = (adler >> 16) & 0xffff;
    adler &= 0xffff;

    /* Only capture these corner cases if we didn't compile with SSE41 and AVX2 support
     * This should make for shorter compiled code */
#if !defined(X86_AVX2_ADLER32) && !defined(X86_SSE41_ADLER32)
    /* in case user likes doing a byte at a time, keep it fast */
    if (UNLIKELY(len == 1))
        return adler32_len_1(adler, buf, sum2);

    /* initial Adler-32 value (deferred check for len == 1 speed) */
    if (UNLIKELY(buf == NULL))
        return 1L;

    /* in case short lengths are provided, keep it somewhat fast */
    if (UNLIKELY(len < 16))
        return adler32_len_16(adler, buf, len, sum2);
#endif

    __m512i vs1 = _mm512_zextsi128_si512(_mm_cvtsi32_si128(adler));
    __m512i vs2 = _mm512_zextsi128_si512(_mm_cvtsi32_si128(sum2));

    const __m512i dot2v = _mm512_set_epi8(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                                          20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
                                          38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55,
                                          56, 57, 58, 59, 60, 61, 62, 63, 64);
    const __m512i dot3v = _mm512_set1_epi16(1);
    const __m512i zero = _mm512_setzero_si512();

    while (len >= 64) {
        __m512i vs1_0 = vs1;
        __m512i vs3 = _mm512_setzero_si512();

        int k = (len < NMAX ? (int)len : NMAX);
        k -= k % 64;
        len -= k;

        while (k >= 64) {
            /*
               vs1 = adler + sum(c[i])
               vs2 = sum2 + 64 vs1 + sum( (64-i+1) c[i] )
            */
            __m512i vbuf = _mm512_loadu_si512(buf);
            buf += 64;
            k -= 64;

            __m512i vs1_sad = _mm512_sad_epu8(vbuf, zero);
            __m512i v_short_sum2 = _mm512_maddubs_epi16(vbuf, dot2v);
            vs1 = _mm512_add_epi32(vs1_sad, vs1);
            vs3 = _mm512_add_epi32(vs3, vs1_0);
            __m512i vsum2 = _mm512_madd_epi16(v_short_sum2, dot3v);
            vs2 = _mm512_add_epi32(vsum2, vs2);
            vs1_0 = vs1;
        }

        vs3 = _mm512_slli_epi32(vs3, 6);
        vs2 = _mm512_add_epi32(vs2, vs3);

        adler = partial_hsum(vs1) % BASE;
        vs1 = _mm512_zextsi128_si512(_mm_cvtsi32_si128(adler));
        sum2 = _mm512_reduce_add_epi32(vs2) % BASE;
        vs2 = _mm512_zextsi128_si512(_mm_cvtsi32_si128(sum2));
    }

    /* Process tail (len < 64). */
    return adler32_len_16(adler, buf, len, sum2);
}

#endif
