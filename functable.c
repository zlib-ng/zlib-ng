/* functable.c -- Choose relevant optimized functions at runtime
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef DISABLE_RUNTIME_CPU_DETECTION

#include "zbuild.h"

#if defined(_MSC_VER)
#  include <intrin.h>
#endif

#include "functable.h"
#include "cpu_features.h"
#include "arch_functions.h"

/* Platform has pointer size atomic store */
#if defined(__GNUC__) || defined(__clang__)
#  define FUNCTABLE_ASSIGN(VAR, FUNC_NAME) \
    __atomic_store(&(functable.FUNC_NAME), &(VAR.FUNC_NAME), __ATOMIC_SEQ_CST)
#  define FUNCTABLE_BARRIER() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#elif defined(_MSC_VER)
#  define FUNCTABLE_ASSIGN(VAR, FUNC_NAME) \
    _InterlockedExchangePointer((void * volatile *)&(functable.FUNC_NAME), (void *)(VAR.FUNC_NAME))
#  if defined(_M_ARM) || defined(_M_ARM64)
#    define FUNCTABLE_BARRIER() do { \
    _ReadWriteBarrier();  \
    __dmb(0xB); /* _ARM_BARRIER_ISH */ \
    _ReadWriteBarrier(); \
} while (0)
#  else
#    define FUNCTABLE_BARRIER() _ReadWriteBarrier()
#  endif
#else
#  warning Unable to detect atomic intrinsic support.
#  define FUNCTABLE_ASSIGN(VAR, FUNC_NAME) \
    *((void * volatile *)&(functable.FUNC_NAME)) = (void *)(VAR.FUNC_NAME)
#  define FUNCTABLE_BARRIER() do { /* Empty */ } while (0)
#endif

/* Verify all pointers are valid before assigning, return 1 on failure
 * This allows inflateinit/deflateinit functions to gracefully return Z_VERSION_ERROR
 * if functable initialization fails.
 */
#define FUNCTABLE_VERIFY_ASSIGN(VAR, FUNC_NAME) \
    if (!VAR.FUNC_NAME) { \
        fprintf(stderr, "Zlib-ng functable failed initialization!\n"); \
        return 1; \
    } \
    FUNCTABLE_ASSIGN(VAR, FUNC_NAME);

/* Functable init & abort on failure.
 * Abort is needed because some stub functions are reachable without first
 * calling any inflateinit/deflateinit functions, and have no error propagation.
 */
#define FUNCTABLE_INIT_ABORT \
    if (init_functable()) { \
        fprintf(stderr, "Zlib-ng functable failed initialization!\n"); \
        abort(); \
    };

// Empty stub, used when functable has already been initialized
static int force_init_empty(void) {
    return 0;
}

/* Functable initialization.
 * Selects the best available optimized functions appropriate for the runtime cpu.
 */
static int init_functable(void) {
    struct functable_s ft;
    struct cpu_features cf;

    memset(&ft, 0, sizeof(struct functable_s));
    cpu_check_features(&cf);
    ft.force_init = &force_init_empty;

    // Set up generic C code fallbacks
#ifndef WITH_ALL_FALLBACKS
#  if (defined(__x86_64__) || defined(_M_X64)) && defined(X86_SSE2)
    // x86_64 always has SSE2, so we can use SSE2 functions as fallbacks where available.
    ft.adler32 = &adler32_c;
    ft.adler32_copy = &adler32_copy_c;
    ft.crc32 = &crc32_braid;
    ft.crc32_copy = &crc32_copy_braid;
#    ifndef HAVE_BUILTIN_CTZ
    ft.longest_match = &longest_match_c;
    ft.longest_match_slow = &longest_match_slow_c;
    ft.compare256 = &compare256_c;
#    endif
#  endif
#else // WITH_ALL_FALLBACKS
    ft.adler32 = &adler32_c;
    ft.adler32_copy = &adler32_copy_c;
    ft.chunkmemset_safe = &chunkmemset_safe_c;
    ft.crc32 = &crc32_braid;
    ft.crc32_copy = &crc32_copy_braid;
    ft.inflate_fast = &inflate_fast_c;
    ft.slide_hash = &slide_hash_c;
    ft.longest_match = &longest_match_c;
    ft.longest_match_slow = &longest_match_slow_c;
    ft.compare256 = &compare256_c;
#endif

    // Select arch-optimized functions
#ifdef WITH_OPTIM

    // Chorba generic C fallback
#ifndef WITHOUT_CHORBA
    ft.crc32 = &crc32_chorba;
    ft.crc32_copy = &crc32_copy_chorba;
#endif

    // X86 - SSE2
#ifdef X86_SSE2
#  if !defined(__x86_64__) && !defined(_M_X64)
    if (cf.x86.has_sse2)
#  endif
    {
        ft.chunkmemset_safe = &chunkmemset_safe_sse2;
#  if !defined(WITHOUT_CHORBA_SSE)
        ft.crc32 = &crc32_chorba_sse2;
        ft.crc32_copy = &crc32_copy_chorba_sse2;
#  endif
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
        ft.adler32_copy = &adler32_copy_ssse3;
        ft.chunkmemset_safe = &chunkmemset_safe_ssse3;
        ft.inflate_fast = &inflate_fast_ssse3;
    }
#endif

    // X86 - SSE4.1
#if defined(X86_SSE41) && !defined(WITHOUT_CHORBA_SSE)
    if (cf.x86.has_sse41) {
        ft.crc32 = &crc32_chorba_sse41;
        ft.crc32_copy = &crc32_copy_chorba_sse41;
    }
#endif

    // X86 - SSE4.2
#ifdef X86_SSE42
    if (cf.x86.has_sse42) {
        ft.adler32_copy = &adler32_copy_sse42;
    }
#endif
    // X86 - PCLMUL
#ifdef X86_PCLMULQDQ_CRC
    if (cf.x86.has_pclmulqdq) {
        ft.crc32 = &crc32_pclmulqdq;
        ft.crc32_copy = &crc32_copy_pclmulqdq;
    }
#endif
    // X86 - AVX
#ifdef X86_AVX2
    /* BMI2 support is all but implicit with AVX2 but let's sanity check this just in case. Enabling BMI2 allows for
     * flagless shifts, resulting in fewer flag stalls for the pipeline, and allows us to set destination registers
     * for the shift results as an operand, eliminating several register-register moves when the original value needs
     * to remain intact. They also allow for a count operand that isn't the CL register, avoiding contention there */
    if (cf.x86.has_avx2 && cf.x86.has_bmi2) {
        ft.adler32 = &adler32_avx2;
        ft.adler32_copy = &adler32_copy_avx2;
        ft.chunkmemset_safe = &chunkmemset_safe_avx2;
        ft.inflate_fast = &inflate_fast_avx2;
        ft.slide_hash = &slide_hash_avx2;
#  ifdef HAVE_BUILTIN_CTZ
        ft.compare256 = &compare256_avx2;
        ft.longest_match = &longest_match_avx2;
        ft.longest_match_slow = &longest_match_slow_avx2;
#  endif
    }
#endif
    // X86 - AVX512 (F,DQ,BW,Vl)
#ifdef X86_AVX512
    if (cf.x86.has_avx512_common) {
        ft.adler32 = &adler32_avx512;
        ft.adler32_copy = &adler32_copy_avx512;
        ft.chunkmemset_safe = &chunkmemset_safe_avx512;
        ft.inflate_fast = &inflate_fast_avx512;
#  ifdef HAVE_BUILTIN_CTZLL
        ft.compare256 = &compare256_avx512;
        ft.longest_match = &longest_match_avx512;
        ft.longest_match_slow = &longest_match_slow_avx512;
#  endif
    }
#endif
#ifdef X86_AVX512VNNI
    if (cf.x86.has_avx512vnni) {
        ft.adler32 = &adler32_avx512_vnni;
        ft.adler32_copy = &adler32_copy_avx512_vnni;
    }
#endif
    // X86 - VPCLMULQDQ
#ifdef X86_VPCLMULQDQ_CRC
    if (cf.x86.has_pclmulqdq && cf.x86.has_avx512_common && cf.x86.has_vpclmulqdq) {
        ft.crc32 = &crc32_vpclmulqdq;
        ft.crc32_copy = &crc32_copy_vpclmulqdq;
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
        ft.adler32_copy = &adler32_copy_neon;
        ft.chunkmemset_safe = &chunkmemset_safe_neon;
        ft.inflate_fast = &inflate_fast_neon;
        ft.slide_hash = &slide_hash_neon;
#  ifdef HAVE_BUILTIN_CTZLL
        ft.compare256 = &compare256_neon;
        ft.longest_match = &longest_match_neon;
        ft.longest_match_slow = &longest_match_slow_neon;
#  endif
    }
#endif
    // ARM - CRC32
#ifdef ARM_CRC32
    if (cf.arm.has_crc32) {
        ft.crc32 = &crc32_armv8;
        ft.crc32_copy = &crc32_copy_armv8;
    }
#endif
    // ARM - PMULL EOR3
#ifdef ARM_PMULL_EOR3
    if (cf.arm.has_crc32 && cf.arm.has_pmull && cf.arm.has_eor3 && cf.arm.has_fast_pmull) {
        ft.crc32 = &crc32_armv8_pmull_eor3;
        ft.crc32_copy = &crc32_copy_armv8_pmull_eor3;
    }
#endif

    // Power - VMX
#ifdef PPC_VMX
    if (cf.power.has_altivec) {
        ft.adler32 = &adler32_vmx;
        ft.adler32_copy = &adler32_copy_vmx;
        ft.slide_hash = &slide_hash_vmx;
    }
#endif
    // Power8 - VSX
#ifdef POWER8_VSX
    if (cf.power.has_arch_2_07) {
        ft.adler32 = &adler32_power8;
        ft.adler32_copy = &adler32_copy_power8;
        ft.chunkmemset_safe = &chunkmemset_safe_power8;
        ft.inflate_fast = &inflate_fast_power8;
        ft.slide_hash = &slide_hash_power8;
    }
#endif
#ifdef POWER8_VSX_CRC32
    if (cf.power.has_arch_2_07) {
        ft.crc32 = &crc32_power8;
        ft.crc32_copy = &crc32_copy_power8;
    }
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
        ft.adler32_copy = &adler32_copy_rvv;
        ft.chunkmemset_safe = &chunkmemset_safe_rvv;
        ft.compare256 = &compare256_rvv;
        ft.inflate_fast = &inflate_fast_rvv;
        ft.longest_match = &longest_match_rvv;
        ft.longest_match_slow = &longest_match_slow_rvv;
        ft.slide_hash = &slide_hash_rvv;
    }
#endif

    // RISCV - ZBC
#ifdef RISCV_CRC32_ZBC
    if (cf.riscv.has_zbc) {
        ft.crc32 = &crc32_riscv64_zbc;
        ft.crc32_copy = &crc32_copy_riscv64_zbc;
    }
#endif

    // S390
#ifdef S390_CRC32_VX
    if (cf.s390.has_vx) {
        ft.crc32 = crc32_s390_vx;
        ft.crc32_copy = crc32_copy_s390_vx;
    }
#endif

    // LOONGARCH
#ifdef LOONGARCH_CRC
    if (cf.loongarch.has_crc) {
        ft.crc32 = crc32_loongarch64;
        ft.crc32_copy = crc32_copy_loongarch64;
    }
#endif
#ifdef LOONGARCH_LSX
    if (cf.loongarch.has_lsx) {
        ft.adler32 = &adler32_lsx;
        ft.adler32_copy = &adler32_copy_lsx;
        ft.slide_hash = slide_hash_lsx;
#  ifdef HAVE_BUILTIN_CTZ
        ft.compare256 = &compare256_lsx;
        ft.longest_match = &longest_match_lsx;
        ft.longest_match_slow = &longest_match_slow_lsx;
#  endif
        ft.chunkmemset_safe = &chunkmemset_safe_lsx;
        ft.inflate_fast = &inflate_fast_lsx;
    }
#endif
#ifdef LOONGARCH_LASX
    if (cf.loongarch.has_lasx) {
        ft.adler32 = &adler32_lasx;
        ft.adler32_copy = &adler32_copy_lasx;
        ft.slide_hash = slide_hash_lasx;
#  ifdef HAVE_BUILTIN_CTZ
        ft.compare256 = &compare256_lasx;
        ft.longest_match = &longest_match_lasx;
        ft.longest_match_slow = &longest_match_slow_lasx;
#  endif
        ft.chunkmemset_safe = &chunkmemset_safe_lasx;
        ft.inflate_fast = &inflate_fast_lasx;
    }
#endif

#endif // WITH_OPTIM

    // Assign function pointers individually for atomic operation
    FUNCTABLE_ASSIGN(ft, force_init);
    FUNCTABLE_VERIFY_ASSIGN(ft, adler32);
    FUNCTABLE_VERIFY_ASSIGN(ft, adler32_copy);
    FUNCTABLE_VERIFY_ASSIGN(ft, chunkmemset_safe);
    FUNCTABLE_VERIFY_ASSIGN(ft, compare256);
    FUNCTABLE_VERIFY_ASSIGN(ft, crc32);
    FUNCTABLE_VERIFY_ASSIGN(ft, crc32_copy);
    FUNCTABLE_VERIFY_ASSIGN(ft, inflate_fast);
    FUNCTABLE_VERIFY_ASSIGN(ft, longest_match);
    FUNCTABLE_VERIFY_ASSIGN(ft, longest_match_slow);
    FUNCTABLE_VERIFY_ASSIGN(ft, slide_hash);

    // Memory barrier for weak memory order CPUs
    FUNCTABLE_BARRIER();

    return Z_OK;
}

/* stub functions */
static int force_init_stub(void) {
    return init_functable();
}

static uint32_t adler32_stub(uint32_t adler, const uint8_t* buf, size_t len) {
    FUNCTABLE_INIT_ABORT;
    return functable.adler32(adler, buf, len);
}

static uint32_t adler32_copy_stub(uint32_t adler, uint8_t* dst, const uint8_t* src, size_t len) {
    FUNCTABLE_INIT_ABORT;
    return functable.adler32_copy(adler, dst, src, len);
}

static uint8_t* chunkmemset_safe_stub(uint8_t* out, uint8_t *from, unsigned len, unsigned left) {
    FUNCTABLE_INIT_ABORT;
    return functable.chunkmemset_safe(out, from, len, left);
}

static uint32_t compare256_stub(const uint8_t* src0, const uint8_t* src1) {
    FUNCTABLE_INIT_ABORT;
    return functable.compare256(src0, src1);
}

static uint32_t crc32_stub(uint32_t crc, const uint8_t* buf, size_t len) {
    FUNCTABLE_INIT_ABORT;
    return functable.crc32(crc, buf, len);
}

static uint32_t crc32_copy_stub(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    FUNCTABLE_INIT_ABORT;
    return functable.crc32_copy(crc, dst, src, len);
}

static void inflate_fast_stub(PREFIX3(stream) *strm, uint32_t start) {
    FUNCTABLE_INIT_ABORT;
    functable.inflate_fast(strm, start);
}

static uint32_t longest_match_stub(deflate_state* const s, uint32_t cur_match) {
    FUNCTABLE_INIT_ABORT;
    return functable.longest_match(s, cur_match);
}

static uint32_t longest_match_slow_stub(deflate_state* const s, uint32_t cur_match) {
    FUNCTABLE_INIT_ABORT;
    return functable.longest_match_slow(s, cur_match);
}

static void slide_hash_stub(deflate_state* s) {
    FUNCTABLE_INIT_ABORT;
    functable.slide_hash(s);
}

/* functable init */
Z_INTERNAL struct functable_s functable = {
    force_init_stub,
    adler32_stub,
    adler32_copy_stub,
    chunkmemset_safe_stub,
    compare256_stub,
    crc32_stub,
    crc32_copy_stub,
    inflate_fast_stub,
    longest_match_stub,
    longest_match_slow_stub,
    slide_hash_stub,
};

#endif
