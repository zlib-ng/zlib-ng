#ifndef FALLBACK_BUILTINS_H
#define FALLBACK_BUILTINS_H

#if defined(_MSC_VER) && !defined(__clang__)
#  include <intrin.h>
#endif

/* Provide fallback for compilers that don't support __has_builtin */
#ifndef __has_builtin
#  define __has_builtin(x) 0
#endif

/* Count trailing zeros (CTZ) functions with portable fallback.
 *
 * Predicate: Input must be non-zero. The result is undefined for zero input because
 * __builtin_ctz, BSF, and TZCNT all have undefined/different behavior for zero. TZCNT
 * returns operand size for zero, BSF leaves destination undefined, and __builtin_ctz
 * is explicitly undefined per GCC/Clang docs. */

Z_FORCEINLINE static uint32_t zng_ctz32(uint32_t value) {
    Assert(value != 0, "Invalid input value: 0");
#if __has_builtin(__builtin_ctz)
    return (uint32_t)__builtin_ctz(value);
#elif defined(_MSC_VER) && !defined(__clang__)
#  if defined(X86_FEATURES) && !(_MSC_VER < 1700)
    /* tzcnt falls back to bsf on cpus without BMI1, and is equal or faster on all x86 cpus. */
    return (uint32_t)_tzcnt_u32(value);
#  else
    unsigned long trailing_zero;
    _BitScanForward(&trailing_zero, value);
    return (uint32_t)trailing_zero;
#  endif
#else
    /* De Bruijn CTZ for 32-bit values */
    static const uint8_t debruijn_ctz32[32] = {
        0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
        31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };
    uint32_t lsb = value & (~value + 1u);
    return debruijn_ctz32[(lsb * 0x077CB531U) >> 27];
#endif
}

Z_FORCEINLINE static uint32_t zng_ctz64(uint64_t value) {
    Assert(value != 0, "Invalid input value: 0");
#if __has_builtin(__builtin_ctzll)
    return (uint32_t)__builtin_ctzll(value);
#elif defined(_MSC_VER) && !defined(__clang__) && defined(ARCH_64BIT)
#  if defined(X86_FEATURES) && !(_MSC_VER < 1700)
    /* tzcnt falls back to bsf on cpus without BMI1, and is equal or faster on all x86 cpus. */
    return (uint32_t)_tzcnt_u64(value);
#  else
    unsigned long trailing_zero;
    _BitScanForward64(&trailing_zero, value);
    return (uint32_t)trailing_zero;
#  endif
#else
    /* De Bruijn CTZ for 64-bit values */
    static const uint8_t debruijn_ctz64[64] = {
        63, 0, 1, 52, 2, 6, 53, 26, 3, 37, 40, 7, 33, 54, 47, 27,
        61, 4, 38, 45, 43, 41, 21, 8, 23, 34, 58, 55, 48, 17, 28, 10,
        62, 51, 5, 25, 36, 39, 32, 46, 60, 44, 42, 20, 22, 57, 16, 9,
        50, 24, 35, 31, 59, 19, 56, 15, 49, 30, 18, 14, 29, 13, 12, 11
    };
    uint64_t lsb = value & (~value + 1ull);
    return debruijn_ctz64[(lsb * 0x045FBAC7992A70DAULL) >> 58];
#endif
}

Z_FORCEINLINE static uint16_t zng_bitreverse16(uint16_t value) {
#if __has_builtin(__builtin_bitreverse16)
    return (uint16_t)__builtin_bitreverse16(value);
#elif defined(ARCH_ARM) && defined(ARCH_64BIT) && !defined(_MSC_VER)
    /* ARM bit reversal for 16-bit values using rbit instruction */
    uint32_t res;
#    if __has_builtin(__builtin_rbit)
    res = __builtin_rbit((uint32_t)value);
#    else
    __asm__ volatile("rbit %w0, %w1" : "=r"(res) : "r"((uint32_t)value));
#    endif
    return (uint16_t)(res >> 16);
#elif defined(ARCH_LOONGARCH)
    /* LoongArch bit reversal for 16-bit values */
    uint32_t res;
    __asm__ volatile("bitrev.w %0, %1" : "=r"(res) : "r"(value));
    return (uint16_t)(res >> 16);
#else
    /* Bit reversal for 8-bit values using multiplication method */
#  define bitrev8(value) \
    (uint8_t)((((uint8_t)(value) * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32)
    /* General purpose bit reversal for 16-bit values */
    return ((bitrev8(value >> 8) | (uint16_t)bitrev8(value) << 8));
#  undef bitrev8
#endif
}

#endif // include guard FALLBACK_BUILTINS_H
