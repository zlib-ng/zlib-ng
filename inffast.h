#include <stdint.h>

#include "zbuild.h"

#ifdef ZLIB_COMPAT
typedef struct z_stream_s z_stream;
#else
typedef struct zng_stream_s zng_stream;
#endif

/* inflate fast loop */
extern void inflate_fast_c(PREFIX3(stream) *strm, uint32_t start);
#ifdef X86_SSE2
extern void inflate_fast_sse2(PREFIX3(stream) *strm, uint32_t start);
#endif
#ifdef X86_SSSE3
extern void inflate_fast_ssse3(PREFIX3(stream) *strm, uint32_t start);
#endif
#ifdef X86_AVX2
extern void inflate_fast_avx2(PREFIX3(stream) *strm, uint32_t start);
#endif
#ifdef ARM_NEON
extern void inflate_fast_neon(PREFIX3(stream) *strm, uint32_t start);
#endif
#ifdef POWER8_VSX
extern void inflate_fast_power8(PREFIX3(stream) *strm, uint32_t start);
#endif
#ifdef RISCV_RVV
extern void inflate_fast_rvv(PREFIX3(stream) *strm, uint32_t start);
#endif