#ifndef FALLBACK_BUILTINS_H
#define FALLBACK_BUILTINS_H

/* Provide fallback for compilers that don't support __has_builtin */
#  ifndef __has_builtin
#    define __has_builtin(x) 0
#  endif

#if defined(_MSC_VER) && !defined(__clang__)

#include <intrin.h>

/* This is not a general purpose replacement for __builtin_ctz. The function expects that value is != 0.
 * Because of that assumption trailing_zero is not initialized and the return value is not checked.
 * Tzcnt and bsf give identical results except when input value is 0, therefore this can not be allowed.
 * If tzcnt instruction is not supported, the cpu will itself execute bsf instead.
 * Performance tzcnt/bsf is identical on Intel cpu, tzcnt is faster than bsf on AMD cpu.
 */
Z_FORCEINLINE static int __builtin_ctz(unsigned int value) {
    Assert(value != 0, "Invalid input value: 0");
#  if defined(X86_FEATURES) && !(_MSC_VER < 1700)
    return (int)_tzcnt_u32(value);
#  else
    unsigned long trailing_zero;
    _BitScanForward(&trailing_zero, value);
    return (int)trailing_zero;
#  endif
}
#  define HAVE_BUILTIN_CTZ

#  ifdef ARCH_64BIT
/* This is not a general purpose replacement for __builtin_ctzll. The function expects that value is != 0.
 * Because of that assumption trailing_zero is not initialized and the return value is not checked.
 */
Z_FORCEINLINE static int __builtin_ctzll(unsigned long long value) {
    Assert(value != 0, "Invalid input value: 0");
#  if defined(X86_FEATURES) && !(_MSC_VER < 1700)
    return (int)_tzcnt_u64(value);
#  else
    unsigned long trailing_zero;
    _BitScanForward64(&trailing_zero, value);
    return (int)trailing_zero;
#  endif
}
#  define HAVE_BUILTIN_CTZLL
#  endif // ARCH_64BIT

#endif // _MSC_VER && !__clang__

#if !__has_builtin(__builtin_bitreverse16)

#  if defined(ARCH_ARM) && defined(ARCH_64BIT) && !defined(_MSC_VER)
/* ARM bit reversal for 16-bit values using rbit instruction */
Z_FORCEINLINE static uint16_t __builtin_bitreverse16(uint16_t value) {
    uint32_t res;
#    if __has_builtin(__builtin_rbit)
    res = __builtin_rbit((uint32_t)value);
#    else
    __asm__ volatile("rbit %w0, %w1" : "=r"(res) : "r"((uint32_t)value));
#    endif
    return (uint16_t)(res >> 16);
}
#  elif defined(ARCH_LOONGARCH)
/* LoongArch bit reversal for 16-bit values */
Z_FORCEINLINE static uint16_t __builtin_bitreverse16(uint16_t value) {
    uint32_t res;
    __asm__ volatile("bitrev.w %0, %1" : "=r"(res) : "r"(value));
    return (uint16_t)(res >> 16);
}
#  else
/* Bit reversal for 8-bit values using multiplication method */
#    define bitrev8(value) \
    (uint8_t)((((uint8_t)(value) * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32)

/* General purpose bit reversal for 16-bit values */
Z_FORCEINLINE static uint16_t __builtin_bitreverse16(uint16_t value) {
    return ((bitrev8(value >> 8) | (uint16_t)bitrev8(value) << 8));
}
#    undef bitrev8
#  endif

#endif // __builtin_bitreverse16

#endif // include guard FALLBACK_BUILTINS_H
