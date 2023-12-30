#ifndef INFFAST_H_
#define INFFAST_H_

#include "zbuild.h"

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#endif

#ifdef ZLIB_COMPAT
typedef struct z_stream_s z_stream;
#else
typedef struct zng_stream_s zng_stream;
#endif

/* inflate fast loop */
extern void inflate_fast_c(PREFIX3(stream) *strm, uint32_t start);
#ifdef ARM_NEON
extern void inflate_fast_neon(PREFIX3(stream) *strm, uint32_t start);
#  define native_inflate_fast inflate_fast_neon
#endif
#ifdef POWER8_VSX
extern void inflate_fast_power8(PREFIX3(stream) *strm, uint32_t start);
#  define native_inflate_fast inflate_fast_power8
#endif
#ifdef RISCV_RVV
extern void inflate_fast_rvv(PREFIX3(stream) *strm, uint32_t start);
#  define native_inflate_fast inflate_fast_rvv
#endif
#ifdef X86_SSE2
extern void inflate_fast_sse2(PREFIX3(stream) *strm, uint32_t start);
#  define native_inflate_fast inflate_fast_sse2
#endif
#ifdef X86_SSSE3
extern void inflate_fast_ssse3(PREFIX3(stream) *strm, uint32_t start);
#  undef native_inflate_fast
#  define native_inflate_fast inflate_fast_ssse3
#endif
#ifdef X86_AVX2
extern void inflate_fast_avx2(PREFIX3(stream) *strm, uint32_t start);
#  undef native_inflate_fast
#  define native_inflate_fast inflate_fast_avx2
#endif

#ifndef native_inflate_fast
#  define native_inflate_fast inflate_fast_c
#endif

#endif
