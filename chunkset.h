#include <stdint.h>

/* memory chunking */
extern uint32_t chunksize_c(void);
extern uint8_t* chunkmemset_safe_c(uint8_t *out, unsigned dist, unsigned len, unsigned left);

#ifdef ARM_NEON
extern uint32_t chunksize_neon(void);
extern uint8_t* chunkmemset_safe_neon(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  define CHUNKSIZE chunksize_neon
#  define CHUNKMEMSET_SAFE chunkmemset_safe_neon
#endif
#ifdef POWER8_VSX
extern uint32_t chunksize_power8(void);
extern uint8_t* chunkmemset_safe_power8(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  define CHUNKSIZE chunksize_power8
#  define CHUNKMEMSET_SAFE chunkmemset_safe_power8
#endif
#ifdef RISCV_RVV
extern uint32_t chunksize_rvv(void);
extern uint8_t* chunkmemset_safe_rvv(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  define CHUNKSIZE chunksize_rvv
#  define CHUNKMEMSET_SAFE chunkmemset_safe_rvv
#endif
#ifdef X86_SSE2
extern uint32_t chunksize_sse2(void);
extern uint8_t* chunkmemset_safe_sse2(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  define CHUNKSIZE chunksize_sse2
#  define CHUNKMEMSET_SAFE chunkmemset_safe_sse2
#endif
#ifdef X86_SSSE3
extern uint8_t* chunkmemset_safe_ssse3(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  undef CHUNKMEMSET_SAFE
#  define CHUNKMEMSET_SAFE chunkmemset_safe_ssse3
#endif
#ifdef X86_AVX2
extern uint32_t chunksize_avx2(void);
extern uint8_t* chunkmemset_safe_avx2(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  undef CHUNKSIZE
#  define CHUNKSIZE chunksize_avx2
#  undef CHUNKMEMSET_SAFE
#  define CHUNKMEMSET_SAFE chunkmemset_safe_avx2
#endif

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#  undef CHUNKSIZE
#  define CHUNKSIZE functable.chunksize
#  undef CHUNKMEMSET_SAFE
#  define CHUNKMEMSET_SAFE functable.chunkmemset_safe
#else
#  ifndef CHUNKSIZE
#    define CHUNKSIZE chunksize_c
#  endif
#  ifndef CHUNKMEMSET_SAFE
#    define CHUNKMEMSET_SAFE chunkmemset_safe_c
#  endif
#endif
