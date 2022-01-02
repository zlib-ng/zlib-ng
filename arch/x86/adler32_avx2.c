/* adler32_avx2.c -- compute the Adler-32 checksum of a data stream
 * Copyright (C) 1995-2011 Mark Adler
 * Authors:
 *   Brian Bockelman <bockelman@gmail.com>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zbuild.h"
#include "../../zutil.h"

#include "../../adler32_p.h"

#include <immintrin.h>

#ifdef X86_AVX2_ADLER32

/* 64 bit horizontal sum, adapted from Agner Fog's vector library. */
static inline uint64_t hsum(__m256i x) {
    __m256i sum1 = _mm256_shuffle_epi32(x, 0x0E);
    __m256i sum2 = _mm256_add_epi64(x, sum1);
    __m128i sum3 = _mm256_extracti128_si256(sum2, 1);
#if defined(__x86_64__) || defined(_M_X64)
    return _mm_cvtsi128_si64(_mm_add_epi64(_mm256_castsi256_si128(sum2), sum3));
#else
    __m128i ret_vec = _mm_add_epi64(_mm256_castsi256_si128(sum2), sum3);
    uint64_t ret_val;
    _mm_storel_epi64((__m128i*)&ret_val, ret_vec);
    return ret_val;
#endif
}

Z_INTERNAL uint32_t adler32_avx2(uint32_t adler, const unsigned char *buf, size_t len) {
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

    const __m256i vs_mask = _mm256_setr_epi32(0, 0, 0, 0, 0, 0, 0, -1);
    __m256i vs1 = _mm256_set1_epi32(adler);
    __m256i vs2 = _mm256_set1_epi32(sum2);
    vs1 = _mm256_and_si256(vs1, vs_mask);
    vs2 = _mm256_and_si256(vs2, vs_mask);

    const __m256i dot1v = _mm256_set1_epi8(1);
    const __m256i dot2v = _mm256_setr_epi8(32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15,
                                           14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
    const __m256i dot3v = _mm256_set1_epi16(1);

    while (len >= 32) {
       __m256i vs1_0 = vs1;

       int k = (len < NMAX ? (int)len : NMAX);
       k -= k % 32;
       len -= k;

       while (k >= 32) {
           /*
              vs1 = adler + sum(c[i])
              vs2 = sum2 + 32 vs1 + sum( (32-i+1) c[i] )
           */
           __m256i vbuf = _mm256_loadu_si256((__m256i*)buf);
           buf += 32;
           k -= 32;

           __m256i v_short_sum1 = _mm256_maddubs_epi16(vbuf, dot1v); // multiply-add, resulting in 8 shorts.
           __m256i vsum1 = _mm256_madd_epi16(v_short_sum1, dot3v);   // sum 8 shorts to 4 int32_t;
           __m256i v_short_sum2 = _mm256_maddubs_epi16(vbuf, dot2v);
           vs1 = _mm256_add_epi32(vsum1, vs1);
           __m256i vsum2 = _mm256_madd_epi16(v_short_sum2, dot3v);
           vs1_0 = _mm256_slli_epi32(vs1_0, 5);
           vsum2 = _mm256_add_epi32(vsum2, vs2);
           vs2   = _mm256_add_epi32(vsum2, vs1_0);
           vs1_0 = vs1;
       }

       /* The compiler is generating the following sequence for this integer modulus
        * when done the scalar way, in GPRs:
        
        adler = (s1_unpack[0] % BASE) + (s1_unpack[1] % BASE) + (s1_unpack[2] % BASE) + (s1_unpack[3] % BASE) +
                (s1_unpack[4] % BASE) + (s1_unpack[5] % BASE) + (s1_unpack[6] % BASE) + (s1_unpack[7] % BASE);

        mov    $0x80078071,%edi // move magic constant into 32 bit register %edi
        ...
        vmovd  %xmm1,%esi // move vector lane 0 to 32 bit register %esi
        mov    %rsi,%rax  // zero-extend this value to 64 bit precision in %rax
        imul   %rdi,%rsi // do a signed multiplication with magic constant and vector element 
        shr    $0x2f,%rsi // shift right by 47
        imul   $0xfff1,%esi,%esi // do a signed multiplication with value truncated to 32 bits with 0xfff1 
        sub    %esi,%eax // subtract lower 32 bits of original vector value from modified one above
        ...
        // repeats for each element with vpextract instructions

        This is tricky with AVX2 for a number of reasons:
            1.) There's no 64 bit multiplication instruction, but there is a sequence to get there
            2.) There's ways to extend vectors to 64 bit precision, but no simple way to truncate
                back down to 32 bit precision later (there is in AVX512) 
            3.) Full width integer multiplications aren't cheap

        We can, however, cast up to 64 bit precision on all 8 integers at once, and do a relatively
        cheap sequence for horizontal sums. Then, we simply do the integer modulus on the resulting
        64 bit GPR, on a scalar value
        */

 
        /* Will translate to nops */
        __m128i s1lo = _mm256_castsi256_si128(vs1);
        __m128i s2lo = _mm256_castsi256_si128(vs2);

        /* Requires vextracti128 */
        __m128i s1hi = _mm256_extracti128_si256(vs1, 1);
        __m128i s2hi = _mm256_extracti128_si256(vs2, 1);
        
        /* Convert up to 64 bit precision to prevent overflow */
        __m256i s1lo256 = _mm256_cvtepi32_epi64(s1lo);
        __m256i s1hi256 = _mm256_cvtepi32_epi64(s1hi);
        __m256i s2lo256 = _mm256_cvtepi32_epi64(s2lo);
        __m256i s2hi256 = _mm256_cvtepi32_epi64(s2hi);

        /* Sum vectors in existing lanes */
        __m256i s1_sum = _mm256_add_epi64(s1lo256, s1hi256);
        __m256i s2_sum = _mm256_add_epi64(s2lo256, s2hi256);
        
        /* In AVX2-land, this trip through GPRs will probably be unvoidable, as there's no cheap and easy
         * conversion from 64 bit integer to 32 bit. This casting to 32 bit is cheap through GPRs 
         * (just register aliasing), and safe, as our base is significantly smaller than UINT32_MAX */
        adler = (uint32_t)(hsum(s1_sum) % BASE);
        sum2 = (uint32_t)(hsum(s2_sum) % BASE);

        vs1 = _mm256_set1_epi32(adler);
        vs2 = _mm256_set1_epi32(sum2);

        vs1 = _mm256_and_si256(vs1, vs_mask);
        vs2 = _mm256_and_si256(vs2, vs_mask);
    }

    /* Process tail (len < 16).  */
    return adler32_len_16(adler, buf, len, sum2);
}

#endif
