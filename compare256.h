#include <stdint.h>

#ifdef X86_FEATURES
#  include "fallback_builtins.h"
#endif

/* compare256 */
typedef uint32_t (*compare256_func)(const uint8_t *src0, const uint8_t *src1);

extern uint32_t compare256_c(const uint8_t *src0, const uint8_t *src1);
#define COMPARE256 compare256_c
#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
extern uint32_t compare256_unaligned_16(const uint8_t *src0, const uint8_t *src1);
#  undef COMPARE256
#  define COMPARE256 compare256_unaligned_16
#  ifdef HAVE_BUILTIN_CTZ
extern uint32_t compare256_unaligned_32(const uint8_t *src0, const uint8_t *src1);
#    undef COMPARE256
#    define COMPARE256 compare256_unaligned_32
#  endif
#  if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
extern uint32_t compare256_unaligned_64(const uint8_t *src0, const uint8_t *src1);
#    undef COMPARE256
#    define COMPARE256 compare256_unaligned_64
#  endif
#endif

#if defined(ARM_NEON) && defined(HAVE_BUILTIN_CTZLL)
extern uint32_t compare256_neon(const uint8_t *src0, const uint8_t *src1);
#  undef COMPARE256
#  define COMPARE256 compare256_neon
#endif
#ifdef POWER9
extern uint32_t compare256_power9(const uint8_t *src0, const uint8_t *src1);
#  undef COMPARE256
#  define COMPARE256 compare256_power9
#endif
#ifdef RISCV_RVV
extern uint32_t compare256_rvv(const uint8_t *src0, const uint8_t *src1);
#  undef COMPARE256
#  define COMPARE256 compare256_rvv
#endif
#if defined(X86_SSE2) && defined(HAVE_BUILTIN_CTZ)
extern uint32_t compare256_sse2(const uint8_t *src0, const uint8_t *src1);
#  undef COMPARE256
#  define COMPARE256 compare256_sse2
#endif
#if defined(X86_AVX2) && defined(HAVE_BUILTIN_CTZ)
extern uint32_t compare256_avx2(const uint8_t *src0, const uint8_t *src1);
#  undef COMPARE256
#  define COMPARE256 compare256_avx2
#endif

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#  undef COMPARE256
#  define COMPARE256 functable.compare256
#endif
