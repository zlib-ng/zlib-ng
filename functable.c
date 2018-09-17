/* functable.c -- Choose relevant optimized functions at runtime
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "functable.h"
#include "deflate.h"
#include "deflate_p.h"

#include "gzendian.h"

#if defined(X86_CPUID)
# include "arch/x86/x86.h"
#elif (defined(__arm__) || defined(__aarch64__) || defined(_M_ARM))
extern int arm_has_crc32();
extern int arm_has_neon();
#endif


/* insert_string */
#ifdef X86_SSE4_2_CRC_HASH
extern Pos insert_string_sse(deflate_state *const s, const Pos str, unsigned int count);
#elif defined(ARM_ACLE_CRC_HASH)
extern Pos insert_string_acle(deflate_state *const s, const Pos str, unsigned int count);
#endif

/* fill_window */
#ifdef X86_SSE2_FILL_WINDOW
extern void fill_window_sse(deflate_state *s);
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM)
extern void fill_window_arm(deflate_state *s);
#endif

/* adler32 */
extern uint32_t adler32_c(uint32_t adler, const unsigned char *buf, size_t len);
#if ((defined(__ARM_NEON__) || defined(__ARM_NEON)) && defined(ARM_NEON_ADLER32))
extern uint32_t adler32_neon(uint32_t adler, const unsigned char *buf, size_t len);
#endif

ZLIB_INTERNAL uint32_t crc32_generic(uint32_t, const unsigned char *, uint64_t);

#ifdef __ARM_FEATURE_CRC32
extern uint32_t crc32_acle(uint32_t, const unsigned char *, uint64_t);
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
extern uint32_t crc32_little(uint32_t, const unsigned char *, uint64_t);
#elif BYTE_ORDER == BIG_ENDIAN
extern uint32_t crc32_big(uint32_t, const unsigned char *, uint64_t);
#endif

/* stub definitions */
ZLIB_INTERNAL Pos insert_string_stub(deflate_state *const s, const Pos str, unsigned int count);
ZLIB_INTERNAL void fill_window_stub(deflate_state *s);
ZLIB_INTERNAL uint32_t adler32_stub(uint32_t adler, const unsigned char *buf, size_t len);
ZLIB_INTERNAL uint32_t crc32_stub(uint32_t crc, const unsigned char *buf, uint64_t len);

/* functable init */
ZLIB_INTERNAL __thread struct functable_s functable = {fill_window_stub,insert_string_stub,adler32_stub,crc32_stub};


/* stub functions */
ZLIB_INTERNAL Pos insert_string_stub(deflate_state *const s, const Pos str, unsigned int count) {
    // Initialize default
    functable.insert_string=&insert_string_c;

    #ifdef X86_SSE4_2_CRC_HASH
    if (x86_cpu_has_sse42)
        functable.insert_string=&insert_string_sse;
    #elif defined(__ARM_FEATURE_CRC32) && defined(ARM_ACLE_CRC_HASH)
    if (arm_has_crc32())
        functable.insert_string=&insert_string_acle;
    #endif

    return functable.insert_string(s, str, count);
}

ZLIB_INTERNAL void fill_window_stub(deflate_state *s) {
    // Initialize default
    functable.fill_window=&fill_window_c;

    #ifdef X86_SSE2_FILL_WINDOW
    # ifndef X86_NOCHECK_SSE2
    if (x86_cpu_has_sse2)
    # endif
        functable.fill_window=&fill_window_sse;
    #elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM)
        functable.fill_window=&fill_window_arm;
    #endif

    functable.fill_window(s);
}

ZLIB_INTERNAL uint32_t adler32_stub(uint32_t adler, const unsigned char *buf, size_t len) {
    // Initialize default
    functable.adler32=&adler32_c;

    #if ((defined(__ARM_NEON__) || defined(__ARM_NEON)) && defined(ARM_NEON_ADLER32))
    if (arm_has_neon())
        functable.adler32=&adler32_neon;
    #endif

    return functable.adler32(adler, buf, len);
}

ZLIB_INTERNAL uint32_t crc32_stub(uint32_t crc, const unsigned char *buf, uint64_t len) {


   Assert(sizeof(uint64_t) >= sizeof(size_t),
          "crc32_z takes size_t but internally we have a uint64_t len");
/* return a function pointer for optimized arches here after a capability test */

#ifdef DYNAMIC_CRC_TABLE
    if (crc_table_empty)
        make_crc_table();
#endif /* DYNAMIC_CRC_TABLE */

    if (sizeof(void *) == sizeof(ptrdiff_t)) {
#if BYTE_ORDER == LITTLE_ENDIAN
      functable.crc32=crc32_little;
#  if __ARM_FEATURE_CRC32 && defined(ARM_ACLE_CRC_HASH)
      if (arm_has_crc32())
        functable.crc32=crc32_acle;
#  endif
#elif BYTE_ORDER == BIG_ENDIAN
        functable.crc32=crc32_big;
#else
#  error No endian defined
#endif
    } else {
        functable.crc32=crc32_generic;
    }

    return functable.crc32(crc, buf, len);
}
