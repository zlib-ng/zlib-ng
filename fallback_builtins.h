#ifndef FALLBACK_BUILTINS_H
#define FALLBACK_BUILTINS_H

#if defined(_MSC_VER) && !defined(__clang__)
#if defined(_M_IX86) || defined(_M_AMD64) || defined(_M_IA64) ||  defined(_M_ARM) || defined(_M_ARM64)

#include <intrin.h>
#ifdef X86_FEATURES
#  include "arch/x86/x86_features.h"
#endif

/* This is not a general purpose replacement for __builtin_ctz. The function expects that value is != 0.
 * Because of that assumption trailing_zero is not initialized and the return value is not checked.
 * Tzcnt and bsf give identical results except when input value is 0, therefore this can not be allowed.
 * If tzcnt instruction is not supported, the cpu will itself execute bsf instead.
 * Performance tzcnt/bsf is identical on Intel cpu, tzcnt is faster than bsf on AMD cpu.
 */
static __forceinline int __builtin_ctz(unsigned int value) {
    Assert(value != 0, "Invalid input value: 0");
# if defined(X86_FEATURES) && !(_MSC_VER < 1700)
    return (int)_tzcnt_u32(value);
# else
    unsigned long trailing_zero;
    _BitScanForward(&trailing_zero, value);
    return (int)trailing_zero;
# endif
}
#define HAVE_BUILTIN_CTZ

#ifdef _M_AMD64
/* This is not a general purpose replacement for __builtin_ctzll. The function expects that value is != 0.
 * Because of that assumption trailing_zero is not initialized and the return value is not checked.
 */
static __forceinline int __builtin_ctzll(unsigned long long value) {
    Assert(value != 0, "Invalid input value: 0");
# if defined(X86_FEATURES) && !(_MSC_VER < 1700)
    return (int)_tzcnt_u64(value);
# else
    unsigned long trailing_zero;
    _BitScanForward64(&trailing_zero, value);
    return (int)trailing_zero;
# endif
}
#define HAVE_BUILTIN_CTZLL
#endif // Microsoft AMD64

#endif // Microsoft AMD64/IA64/x86/ARM/ARM64 test
#endif // _MSC_VER & !clang

/* Unfortunately GCC didn't support these things until version 10.
 * Similarly, AppleClang didn't support them in Xcode 9.2 but did in 9.3.
 */
#ifdef __AVX2__
#include <immintrin.h>

#if (!defined(__clang__) && defined(__GNUC__) && __GNUC__ < 10) \
    || (defined(__apple_build_version__) && __apple_build_version__ < 9020039)
static inline __m256i _mm256_zextsi128_si256(__m128i a) {
    __m128i r;
    __asm__ volatile ("vmovdqa %1,%0" : "=x" (r) : "x" (a));
    return _mm256_castsi128_si256(r);
}

#ifdef __AVX512F__
static inline __m512i _mm512_zextsi128_si512(__m128i a) {
    __m128i r;
    __asm__ volatile ("vmovdqa %1,%0" : "=x" (r) : "x" (a));
    return _mm512_castsi128_si512(r);
}
#endif // __AVX512F__
#endif // gcc/AppleClang version test

#endif // __AVX2__

/* Missing zero-extension AVX and AVX512 intrinsics.
 * Fixed in Microsoft Visual Studio 2017 version 15.7
 * https://developercommunity.visualstudio.com/t/missing-zero-extension-avx-and-avx512-intrinsics/175737
 */
#if defined(_MSC_VER) && _MSC_VER < 1914
#ifdef __AVX2__
static inline __m256i _mm256_zextsi128_si256(__m128i a) {
    return _mm256_inserti128_si256(_mm256_setzero_si256(), a, 0);
}
#endif // __AVX2__

#ifdef __AVX512F__
static inline __m512i _mm512_zextsi128_si512(__m128i a) {
    return _mm512_inserti32x4(_mm512_setzero_si512(), a, 0);
}
#endif // __AVX512F__
#endif // defined(_MSC_VER) && _MSC_VER < 1914

#endif // include guard FALLBACK_BUILTINS_H
