/* functable.c -- Choose relevant optimized functions at runtime
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "zendian.h"
#include "crc32_braid_p.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"
#include "cpu_features.h"

static void init_functable(void) {
    struct functable_s ft;

    cpu_check_features();

    // Generic code
    ft.adler32 = &adler32_c;
    ft.adler32_fold_copy = &adler32_fold_copy_c;
    ft.chunkmemset_safe = &chunkmemset_safe_c;
    ft.chunksize = &chunksize_c;
    ft.crc32 = &PREFIX(crc32_braid);
    ft.crc32_fold = &crc32_fold_c;
    ft.crc32_fold_copy = &crc32_fold_copy_c;
    ft.crc32_fold_final = &crc32_fold_final_c;
    ft.crc32_fold_reset = &crc32_fold_reset_c;
    ft.inflate_fast = &inflate_fast_c;
    ft.insert_string = &insert_string_c;
    ft.quick_insert_string = &quick_insert_string_c;
    ft.slide_hash = &slide_hash_c;
    ft.update_hash = &update_hash_c;

#ifdef UNALIGNED_OK
#  if defined(UNALIGNED64_OK) && defined(HAVE_BUILTIN_CTZLL)
    ft.longest_match = &longest_match_unaligned_64;
    ft.longest_match_slow = &longest_match_slow_unaligned_64;
    ft.compare256 = &compare256_unaligned_64;
#  elif defined(HAVE_BUILTIN_CTZ)
    ft.longest_match = &longest_match_unaligned_32;
    ft.longest_match_slow = &longest_match_slow_unaligned_32;
    ft.compare256 = &compare256_unaligned_32;
#  else
    ft.longest_match = &longest_match_unaligned_16;
    ft.longest_match_slow = &longest_match_slow_unaligned_16;
    ft.compare256 = &compare256_unaligned_16;
#  endif
#else
    ft.longest_match = &longest_match_c;
    ft.longest_match_slow = &longest_match_slow_c;
    ft.compare256 = &compare256_c;
#endif


    // Select arch-optimized functions

    // X86 - SSE2
#ifdef X86_SSE2
#  if !defined(__x86_64__) && !defined(_M_X64) && !defined(X86_NOCHECK_SSE2)
    if (x86_cpu_has_sse2)
#  endif
    {
        ft.chunkmemset_safe = &chunkmemset_safe_sse2;
        ft.chunksize = &chunksize_sse2;
        ft.inflate_fast = &inflate_fast_sse2;
        ft.slide_hash = &slide_hash_sse2;
#  ifdef HAVE_BUILTIN_CTZ
        ft.compare256 = &compare256_sse2;
        ft.longest_match = &longest_match_sse2;
        ft.longest_match_slow = &longest_match_slow_sse2;
#  endif
    }
#endif
    // X86 - SSSE3
#ifdef X86_SSSE3
    if (x86_cpu_has_ssse3)
        ft.adler32 = &adler32_ssse3;
#endif
    // X86 - SSE4
#if defined(X86_SSE41) && defined(X86_SSE2)
    if (x86_cpu_has_sse41) {
        ft.chunkmemset_safe = &chunkmemset_safe_sse41;
        ft.inflate_fast = &inflate_fast_sse41;
    }
#endif
#ifdef X86_SSE42
    if (x86_cpu_has_sse42) {
        ft.adler32_fold_copy = &adler32_fold_copy_sse42;
        ft.insert_string = &insert_string_sse4;
        ft.quick_insert_string = &quick_insert_string_sse4;
        ft.update_hash = &update_hash_sse4;
    }
#endif
    // X86 - PCLMUL
#ifdef X86_PCLMULQDQ_CRC
    if (x86_cpu_has_pclmulqdq) {
        ft.crc32 = &crc32_pclmulqdq;
        ft.crc32_fold = &crc32_fold_pclmulqdq;
        ft.crc32_fold_copy = &crc32_fold_pclmulqdq_copy;
        ft.crc32_fold_final = &crc32_fold_pclmulqdq_final;
        ft.crc32_fold_reset = &crc32_fold_pclmulqdq_reset;
    }
#endif
    // X86 - AVX
#ifdef X86_AVX2
    if (x86_cpu_has_avx2) {
        ft.adler32 = &adler32_avx2;
        ft.adler32_fold_copy = &adler32_fold_copy_avx2;
        ft.chunkmemset_safe = &chunkmemset_safe_avx;
        ft.chunksize = &chunksize_avx;
        ft.inflate_fast = &inflate_fast_avx;
        ft.slide_hash = &slide_hash_avx2;
#  ifdef HAVE_BUILTIN_CTZ
        ft.compare256 = &compare256_avx2;
        ft.longest_match = &longest_match_avx2;
        ft.longest_match_slow = &longest_match_slow_avx2;
#  endif
    }
#endif
#ifdef X86_AVX512
    if (x86_cpu_has_avx512) {
        ft.adler32 = &adler32_avx512;
        ft.adler32_fold_copy = &adler32_fold_copy_avx512;
    }
#endif
#ifdef X86_AVX512VNNI
    if (x86_cpu_has_avx512vnni) {
        ft.adler32 = &adler32_avx512_vnni;
        ft.adler32_fold_copy = &adler32_fold_copy_avx512_vnni;
    }
#endif


    // ARM - NEON
#ifdef ARM_NEON
#  ifndef ARM_NOCHECK_NEON
    if (arm_cpu_has_neon)
#  endif
    {
        ft.adler32 = &adler32_neon;
        ft.chunkmemset_safe = &chunkmemset_safe_neon;
        ft.chunksize = &chunksize_neon;
        ft.inflate_fast = &inflate_fast_neon;
        ft.slide_hash = &slide_hash_neon;
#  ifdef HAVE_BUILTIN_CTZLL
        ft.compare256 = &compare256_neon;
        ft.longest_match = &longest_match_neon;
        ft.longest_match_slow = &longest_match_slow_neon;
#  endif
    }
#endif
    // ARM - ACLE
#ifdef ARM_ACLE
    if (arm_cpu_has_crc32) {
        ft.crc32 = &crc32_acle;
        ft.insert_string = &insert_string_acle;
        ft.quick_insert_string = &quick_insert_string_acle;
        ft.update_hash = &update_hash_acle;
    }
#endif


    // Power - VMX
#ifdef PPC_VMX
    if (power_cpu_has_altivec) {
        ft.adler32 = &adler32_vmx;
        ft.slide_hash = &slide_hash_vmx;
    }
#endif
    // Power8 - VSX
#ifdef POWER8_VSX
    if (power_cpu_has_arch_2_07) {
        ft.adler32 = &adler32_power8;
        ft.chunkmemset_safe = &chunkmemset_safe_power8;
        ft.chunksize = &chunksize_power8;
        ft.inflate_fast = &inflate_fast_power8;
        ft.slide_hash = &slide_hash_power8;
    }
#endif
#ifdef POWER8_VSX_CRC32
    if (power_cpu_has_arch_2_07)
        ft.crc32 = &crc32_power8;
#endif
    // Power9
#ifdef POWER9
    if (power_cpu_has_arch_3_00) {
        ft.compare256 = &compare256_power9;
        ft.longest_match = &longest_match_power9;
        ft.longest_match_slow = &longest_match_slow_power9;
    }
#endif


    // S390
#ifdef S390_CRC32_VX
    if (PREFIX(s390_cpu_has_vx))
        ft.crc32 = &PREFIX(s390_crc32_vx);
#endif

    // Assign function pointers individually for atomic operation
    functable.adler32 = ft.adler32;
    functable.adler32_fold_copy = ft.adler32_fold_copy;
    functable.chunkmemset_safe = ft.chunkmemset_safe;
    functable.chunksize = ft.chunksize;
    functable.compare256 = ft.compare256;
    functable.crc32 = ft.crc32;
    functable.crc32_fold = ft.crc32_fold;
    functable.crc32_fold_copy = ft.crc32_fold_copy;
    functable.crc32_fold_final = ft.crc32_fold_final;
    functable.crc32_fold_reset = ft.crc32_fold_reset;
    functable.inflate_fast = ft.inflate_fast;
    functable.insert_string = ft.insert_string;
    functable.longest_match = ft.longest_match;
    functable.longest_match_slow = ft.longest_match_slow;
    functable.quick_insert_string = ft.quick_insert_string;
    functable.slide_hash = ft.slide_hash;
    functable.update_hash = ft.update_hash;
}

/* stub functions */
static uint32_t adler32_stub(uint32_t adler, const uint8_t* buf, size_t len) {
    init_functable();
    return functable.adler32(adler, buf, len);
}

static uint32_t adler32_fold_copy_stub(uint32_t adler, uint8_t* dst, const uint8_t* src, size_t len) {
    init_functable();
    return functable.adler32_fold_copy(adler, dst, src, len);
}

static uint8_t* chunkmemset_safe_stub(uint8_t* out, unsigned dist, unsigned len, unsigned left) {
    init_functable();
    return functable.chunkmemset_safe(out, dist, len, left);
}

static uint32_t chunksize_stub(void) {
    init_functable();
    return functable.chunksize();
}

static uint32_t compare256_stub(const uint8_t* src0, const uint8_t* src1) {
    init_functable();
    return functable.compare256(src0, src1);
}

static uint32_t crc32_stub(uint32_t crc, const uint8_t* buf, size_t len) {
    init_functable();
    return functable.crc32(crc, buf, len);
}

static void crc32_fold_stub(crc32_fold* crc, const uint8_t* src, size_t len, uint32_t init_crc) {
    init_functable();
    functable.crc32_fold(crc, src, len, init_crc);
}

static void crc32_fold_copy_stub(crc32_fold* crc, uint8_t* dst, const uint8_t* src, size_t len) {
    init_functable();
    functable.crc32_fold_copy(crc, dst, src, len);
}

static uint32_t crc32_fold_final_stub(crc32_fold* crc) {
    init_functable();
    return functable.crc32_fold_final(crc);
}

static uint32_t crc32_fold_reset_stub(crc32_fold* crc) {
    init_functable();
    return functable.crc32_fold_reset(crc);
}

static void inflate_fast_stub(PREFIX3(stream) *strm, uint32_t start) {
    init_functable();
    functable.inflate_fast(strm, start);
}

static void insert_string_stub(deflate_state* const s, uint32_t str, uint32_t count) {
    init_functable();
    functable.insert_string(s, str, count);
}

static uint32_t longest_match_stub(deflate_state* const s, Pos cur_match) {
    init_functable();
    return functable.longest_match(s, cur_match);
}

static uint32_t longest_match_slow_stub(deflate_state* const s, Pos cur_match) {
    init_functable();
    return functable.longest_match_slow(s, cur_match);
}

static Pos quick_insert_string_stub(deflate_state* const s, const uint32_t str) {
    init_functable();
    return functable.quick_insert_string(s, str);
}

static void slide_hash_stub(deflate_state* s) {
    init_functable();
    functable.slide_hash(s);
}

static uint32_t update_hash_stub(deflate_state* const s, uint32_t h, uint32_t val) {
    init_functable();
    return functable.update_hash(s, h, val);
}

/* functable init */
Z_INTERNAL Z_TLS struct functable_s functable = {
    adler32_stub,
    adler32_fold_copy_stub,
    chunkmemset_safe_stub,
    chunksize_stub,
    compare256_stub,
    crc32_stub,
    crc32_fold_stub,
    crc32_fold_copy_stub,
    crc32_fold_final_stub,
    crc32_fold_reset_stub,
    inflate_fast_stub,
    insert_string_stub,
    longest_match_stub,
    longest_match_slow_stub,
    quick_insert_string_stub,
    slide_hash_stub,
    update_hash_stub
};
