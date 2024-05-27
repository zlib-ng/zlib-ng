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

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

static void __attribute__((constructor)) init_functable(void) {
    struct functable_s ft;
    struct cpu_features cf;

    cpu_check_features(&cf);

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

#if defined(UNALIGNED_OK) && BYTE_ORDER == LITTLE_ENDIAN
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
    if (cf.x86.has_sse2)
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
    if (cf.x86.has_ssse3) {
        ft.adler32 = &adler32_ssse3;
#  ifdef X86_SSE2
        ft.chunkmemset_safe = &chunkmemset_safe_ssse3;
        ft.inflate_fast = &inflate_fast_ssse3;
#  endif
    }
#endif
    // X86 - SSE4.2
#ifdef X86_SSE42
    if (cf.x86.has_sse42) {
        ft.adler32_fold_copy = &adler32_fold_copy_sse42;
        ft.insert_string = &insert_string_sse42;
        ft.quick_insert_string = &quick_insert_string_sse42;
        ft.update_hash = &update_hash_sse42;
    }
#endif
    // X86 - PCLMUL
#ifdef X86_PCLMULQDQ_CRC
    if (cf.x86.has_pclmulqdq) {
        ft.crc32 = &crc32_pclmulqdq;
        ft.crc32_fold = &crc32_fold_pclmulqdq;
        ft.crc32_fold_copy = &crc32_fold_pclmulqdq_copy;
        ft.crc32_fold_final = &crc32_fold_pclmulqdq_final;
        ft.crc32_fold_reset = &crc32_fold_pclmulqdq_reset;
    }
#endif
    // X86 - AVX
#ifdef X86_AVX2
    if (cf.x86.has_avx2) {
        ft.adler32 = &adler32_avx2;
        ft.adler32_fold_copy = &adler32_fold_copy_avx2;
        ft.chunkmemset_safe = &chunkmemset_safe_avx2;
        ft.chunksize = &chunksize_avx2;
        ft.inflate_fast = &inflate_fast_avx2;
        ft.slide_hash = &slide_hash_avx2;
#  ifdef HAVE_BUILTIN_CTZ
        ft.compare256 = &compare256_avx2;
        ft.longest_match = &longest_match_avx2;
        ft.longest_match_slow = &longest_match_slow_avx2;
#  endif
    }
#endif
#ifdef X86_AVX512
    if (cf.x86.has_avx512) {
        ft.adler32 = &adler32_avx512;
        ft.adler32_fold_copy = &adler32_fold_copy_avx512;
    }
#endif
#ifdef X86_AVX512VNNI
    if (cf.x86.has_avx512vnni) {
        ft.adler32 = &adler32_avx512_vnni;
        ft.adler32_fold_copy = &adler32_fold_copy_avx512_vnni;
    }
#endif
    // X86 - VPCLMULQDQ
#if defined(X86_PCLMULQDQ_CRC) && defined(X86_VPCLMULQDQ_CRC)
    if (cf.x86.has_pclmulqdq && cf.x86.has_avx512 && cf.x86.has_vpclmulqdq) {
        ft.crc32 = &crc32_vpclmulqdq;
        ft.crc32_fold = &crc32_fold_vpclmulqdq;
        ft.crc32_fold_copy = &crc32_fold_vpclmulqdq_copy;
        ft.crc32_fold_final = &crc32_fold_vpclmulqdq_final;
        ft.crc32_fold_reset = &crc32_fold_vpclmulqdq_reset;
    }
#endif


    // ARM - SIMD
#ifdef ARM_SIMD
#  ifndef ARM_NOCHECK_SIMD
    if (cf.arm.has_simd)
#  endif
    {
        ft.slide_hash = &slide_hash_armv6;
    }
#endif
    // ARM - NEON
#ifdef ARM_NEON
#  ifndef ARM_NOCHECK_NEON
    if (cf.arm.has_neon)
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
    if (cf.arm.has_crc32) {
        ft.crc32 = &crc32_acle;
        ft.insert_string = &insert_string_acle;
        ft.quick_insert_string = &quick_insert_string_acle;
        ft.update_hash = &update_hash_acle;
    }
#endif


    // Power - VMX
#ifdef PPC_VMX
    if (cf.power.has_altivec) {
        ft.adler32 = &adler32_vmx;
        ft.slide_hash = &slide_hash_vmx;
    }
#endif
    // Power8 - VSX
#ifdef POWER8_VSX
    if (cf.power.has_arch_2_07) {
        ft.adler32 = &adler32_power8;
        ft.chunkmemset_safe = &chunkmemset_safe_power8;
        ft.chunksize = &chunksize_power8;
        ft.inflate_fast = &inflate_fast_power8;
        ft.slide_hash = &slide_hash_power8;
    }
#endif
#ifdef POWER8_VSX_CRC32
    if (cf.power.has_arch_2_07)
        ft.crc32 = &crc32_power8;
#endif
    // Power9
#ifdef POWER9
    if (cf.power.has_arch_3_00) {
        ft.compare256 = &compare256_power9;
        ft.longest_match = &longest_match_power9;
        ft.longest_match_slow = &longest_match_slow_power9;
    }
#endif


    // RISCV - RVV
#ifdef RISCV_RVV
    if (cf.riscv.has_rvv) {
        ft.adler32 = &adler32_rvv;
        ft.adler32_fold_copy = &adler32_fold_copy_rvv;
        ft.chunkmemset_safe = &chunkmemset_safe_rvv;
        ft.chunksize = &chunksize_rvv;
        ft.compare256 = &compare256_rvv;
        ft.inflate_fast = &inflate_fast_rvv;
        ft.longest_match = &longest_match_rvv;
        ft.longest_match_slow = &longest_match_slow_rvv;
        ft.slide_hash = &slide_hash_rvv;
    }
#endif


    // S390
#ifdef S390_CRC32_VX
    if (cf.s390.has_vx)
        ft.crc32 = crc32_s390_vx;
#endif

    functable = ft;
}

Z_INTERNAL void dummy_linker_glue(void) {}

/* functable init */
Z_INTERNAL struct functable_s functable;
