/* functable.c -- Choose relevant optimized functions at runtime
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zendian.h"
#include "deflate.h"
#include "deflate_p.h"

#include "functable.h"
/* insert_string */
extern Pos insert_string_c(deflate_state *const s, const Pos str, unsigned int count);
#ifdef X86_SSE42_CRC_HASH
extern Pos insert_string_sse4(deflate_state *const s, const Pos str, unsigned int count);
#elif defined(ARM_ACLE_CRC_HASH)
extern Pos insert_string_acle(deflate_state *const s, const Pos str, unsigned int count);
#endif

/* quick_insert_string */
extern Pos quick_insert_string_c(deflate_state *const s, const Pos str);
#ifdef X86_SSE42_CRC_HASH
extern Pos quick_insert_string_sse4(deflate_state *const s, const Pos str);
#elif defined(ARM_ACLE_CRC_HASH)
extern Pos quick_insert_string_acle(deflate_state *const s, const Pos str);
#endif
/* slide_hash */
#ifdef X86_SSE2
void slide_hash_sse2(deflate_state *s);
#elif defined(ARM_NEON_SLIDEHASH)
void slide_hash_neon(deflate_state *s);
#endif
#ifdef X86_AVX2
void slide_hash_avx2(deflate_state *s);
#endif

/* adler32 */
extern uint32_t adler32_c(uint32_t adler, const unsigned char *buf, size_t len);
#if (defined(__ARM_NEON__) || defined(__ARM_NEON)) && defined(ARM_NEON_ADLER32)
extern uint32_t adler32_neon(uint32_t adler, const unsigned char *buf, size_t len);
#endif

/* CRC32 */
ZLIB_INTERNAL uint32_t crc32_generic(uint32_t, const unsigned char *, uint64_t);

#ifdef DYNAMIC_CRC_TABLE
extern volatile int crc_table_empty;
extern void make_crc_table(void);
#endif

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
ZLIB_INTERNAL Pos quick_insert_string_stub(deflate_state *const s, const Pos str);
ZLIB_INTERNAL uint32_t adler32_stub(uint32_t adler, const unsigned char *buf, size_t len);
ZLIB_INTERNAL uint32_t crc32_stub(uint32_t crc, const unsigned char *buf, uint64_t len);
ZLIB_INTERNAL void slide_hash_stub(deflate_state *s);

/* functable init */
ZLIB_INTERNAL __thread struct functable_s functable = {
    insert_string_stub,
    quick_insert_string_stub,
    adler32_stub,
    crc32_stub,
    slide_hash_stub
};

ZLIB_INTERNAL void cpu_check_features(void)
{
    static int features_checked = 0;
    if (features_checked)
        return;
#ifdef X86_CPUID
    x86_check_features();
#elif ARM_CPUID
    arm_check_features();
#endif
    features_checked = 1;
}

/* stub functions */
ZLIB_INTERNAL Pos insert_string_stub(deflate_state *const s, const Pos str, unsigned int count) {
    // Initialize default

    functable.insert_string = &insert_string_c;
    cpu_check_features();

#ifdef X86_SSE42_CRC_HASH
    if (x86_cpu_has_sse42)
        functable.insert_string = &insert_string_sse4;
#elif defined(__ARM_FEATURE_CRC32) && defined(ARM_ACLE_CRC_HASH)
    if (arm_cpu_has_crc32)
        functable.insert_string = &insert_string_acle;
#endif

    return functable.insert_string(s, str, count);
}

ZLIB_INTERNAL Pos quick_insert_string_stub(deflate_state *const s, const Pos str) {
    functable.quick_insert_string = &quick_insert_string_c;

#ifdef X86_SSE42_CRC_HASH
    if (x86_cpu_has_sse42)
        functable.quick_insert_string = &quick_insert_string_sse4;
#elif defined(__ARM_FEATURE_CRC32) && defined(ARM_ACLE_CRC_HASH)
    if (arm_cpu_has_crc32)
        functable.quick_insert_string = &quick_insert_string_acle;
#endif

    return functable.quick_insert_string(s, str);
}

ZLIB_INTERNAL void slide_hash_stub(deflate_state *s) {

    functable.slide_hash = &slide_hash_c;
    cpu_check_features();

#ifdef X86_SSE2
#  if !defined(__x86_64__) && !defined(_M_X64) && !defined(X86_NOCHECK_SSE2)
    if (x86_cpu_has_sse2)
#  endif
        functable.slide_hash = &slide_hash_sse2;
#elif defined(ARM_NEON_SLIDEHASH)
    if (arm_cpu_has_neon)
        functable.slide_hash = &slide_hash_neon;
#endif
#ifdef X86_AVX2
    if (x86_cpu_has_avx2)
        functable.slide_hash = &slide_hash_avx2;
#endif

    functable.slide_hash(s);
}

ZLIB_INTERNAL uint32_t adler32_stub(uint32_t adler, const unsigned char *buf, size_t len) {
    // Initialize default
    functable.adler32 = &adler32_c;
    cpu_check_features();

#if (defined(__ARM_NEON__) || defined(__ARM_NEON)) && defined(ARM_NEON_ADLER32)
    if (arm_cpu_has_neon)
        functable.adler32 = &adler32_neon;
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
    cpu_check_features();

    if (sizeof(void *) == sizeof(ptrdiff_t)) {
#if BYTE_ORDER == LITTLE_ENDIAN
        functable.crc32 = crc32_little;
#  if defined(__ARM_FEATURE_CRC32) && defined(ARM_ACLE_CRC_HASH)
        if (arm_cpu_has_crc32)
            functable.crc32 = crc32_acle;
#  endif
#elif BYTE_ORDER == BIG_ENDIAN
        functable.crc32 = crc32_big;
#else
#  error No endian defined
#endif
    } else {
        functable.crc32 = crc32_generic;
    }

    return functable.crc32(crc, buf, len);
}
