#ifndef SLIDE_HASH_H_
#define SLIDE_HASH_H_

#include "deflate.h"

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#endif

/* slide_hash */
typedef void (*slide_hash_func)(deflate_state *s);

extern void slide_hash_c(deflate_state *s);

#if defined(ARM_SIMD)
extern void slide_hash_armv6(deflate_state *s);
#  define native_slide_hash slide_hash_armv6
#endif
#if defined(ARM_NEON)
extern void slide_hash_neon(deflate_state *s);
#  define native_slide_hash slide_hash_neon
#endif
#if defined(PPC_VMX)
extern void slide_hash_vmx(deflate_state *s);
#  define native_slide_hash slide_hash_vmx
#endif
#if defined(POWER8_VSX)
extern void slide_hash_power8(deflate_state *s);
#  undef native_slide_hash
#  define native_slide_hash slide_hash_power8
#endif
#if defined(RISCV_RVV)
extern void slide_hash_rvv(deflate_state *s);
#  define native_slide_hash slide_hash_rvv
#endif
#ifdef X86_SSE2
extern void slide_hash_sse2(deflate_state* s);
#  define native_slide_hash slide_hash_sse2
#endif
#ifdef X86_AVX2
extern void slide_hash_avx2(deflate_state *s);
#  undef native_slide_hash
#  define native_slide_hash slide_hash_avx2
#endif

#ifndef native_slide_hash
#  define native_slide_hash slide_hash_c
#endif

#endif
