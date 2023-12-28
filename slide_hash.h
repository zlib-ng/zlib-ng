#include "deflate.h"

/* slide_hash */
typedef void (*slide_hash_func)(deflate_state *s);

#ifdef X86_SSE2
extern void slide_hash_sse2(deflate_state *s);
#endif
#if defined(ARM_SIMD)
extern void slide_hash_armv6(deflate_state *s);
#endif
#if defined(ARM_NEON)
extern void slide_hash_neon(deflate_state *s);
#endif
#if defined(PPC_VMX)
extern void slide_hash_vmx(deflate_state *s);
#endif
#if defined(POWER8_VSX)
extern void slide_hash_power8(deflate_state *s);
#endif
#if defined(RISCV_RVV)
extern void slide_hash_rvv(deflate_state *s);
#endif
#ifdef X86_AVX2
extern void slide_hash_avx2(deflate_state *s);
#endif