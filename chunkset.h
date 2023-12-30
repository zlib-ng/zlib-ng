#ifndef CHUNKSET_H_
#define CHUNKSET_H_

#include "zbuild.h"

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#endif

/* memory chunking */
extern uint32_t chunksize_c(void);
extern uint8_t* chunkmemset_safe_c(uint8_t *out, unsigned dist, unsigned len, unsigned left);

#ifdef ARM_NEON
extern uint32_t chunksize_neon(void);
extern uint8_t* chunkmemset_safe_neon(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  define native_chunksize chunksize_neon
#  define native_chunkmemset_safe chunkmemset_safe_neon
#endif
#ifdef POWER8_VSX
extern uint32_t chunksize_power8(void);
extern uint8_t* chunkmemset_safe_power8(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  define native_chunksize chunksize_power8
#  define native_chunkmemset_safe chunkmemset_safe_power8
#endif
#ifdef RISCV_RVV
extern uint32_t chunksize_rvv(void);
extern uint8_t* chunkmemset_safe_rvv(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  define native_chunksize chunksize_rvv
#  define native_chunkmemset_safe chunkmemset_safe_rvv
#endif
#ifdef X86_SSE2
extern uint32_t chunksize_sse2(void);
extern uint8_t* chunkmemset_safe_sse2(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  define native_chunksize chunksize_sse2
#  define native_chunkmemset_safe chunkmemset_safe_sse2
#endif
#ifdef X86_SSSE3
extern uint32_t chunksize_ssse3(void);
extern uint8_t* chunkmemset_safe_ssse3(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  undef native_chunksize
#  define native_chunksize chunksize_ssse3
#  undef native_chunkmemset_safe
#  define native_chunkmemset_safe chunkmemset_safe_ssse3
#endif
#ifdef X86_AVX2
extern uint32_t chunksize_avx2(void);
extern uint8_t* chunkmemset_safe_avx2(uint8_t *out, unsigned dist, unsigned len, unsigned left);
#  undef native_chunksize
#  define native_chunksize chunksize_avx2
#  undef native_chunkmemset_safe
#  define native_chunkmemset_safe chunkmemset_safe_avx2
#endif

#ifndef native_chunksize
#  define native_chunksize chunksize_c
#endif
#ifndef native_chunkmemset_safe
#  define native_chunkmemset_safe chunkmemset_safe_c
#endif

#endif
