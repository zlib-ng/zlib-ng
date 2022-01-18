#ifndef FALLBACK_BUILTINS_H
#define FALLBACK_BUILTINS_H

#if defined(_MSC_VER) && !defined(__clang__)
#if defined(_M_IX86) || defined(_M_AMD64) || defined(_M_IA64) ||  defined(_M_ARM) || defined(_M_ARM64)

#include <intrin.h>
#ifdef X86_FEATURES
#  include "arch/x86/x86.h"
#endif

/* This is not a general purpose replacement for __builtin_ctz. The function expects that value is != 0
 * Because of that assumption trailing_zero is not initialized and the return value of _BitScanForward is not checked
 */
static __forceinline unsigned long __builtin_ctz(uint32_t value) {
#ifdef X86_FEATURES
    if (x86_cpu_has_tzcnt)
        return _tzcnt_u32(value);
#endif
    unsigned long trailing_zero;
    _BitScanForward(&trailing_zero, value);
    return trailing_zero;
}
#define HAVE_BUILTIN_CTZ

#ifdef _M_AMD64
/* This is not a general purpose replacement for __builtin_ctzll. The function expects that value is != 0
 * Because of that assumption trailing_zero is not initialized and the return value of _BitScanForward64 is not checked
 */
static __forceinline unsigned long long __builtin_ctzll(uint64_t value) {
#ifdef X86_FEATURES
    if (x86_cpu_has_tzcnt)
        return _tzcnt_u64(value);
#endif
    unsigned long trailing_zero;
    _BitScanForward64(&trailing_zero, value);
    return trailing_zero;
}
#define HAVE_BUILTIN_CTZLL
#endif

#endif
#endif

/* Unfortunately GCC didn't support these things until version 10 */
#ifdef __AVX2__
#include <immintrin.h>

#if (!defined(__clang__) && defined(__GNUC__) && __GNUC__ < 10)
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
#endif // gcc version 10 test

#endif // __AVX2__
#endif // include guard FALLBACK_BUILTINS_H 
