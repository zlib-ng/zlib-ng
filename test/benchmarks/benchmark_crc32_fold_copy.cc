/* benchmark_crc32_fold_copy.cc -- benchmark for crc32 implementations doing folded copying
 * Copyright (C) 2025 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <benchmark/benchmark.h>
#include <assert.h>

extern "C" {
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
}

#define BUFSIZE (32768 + 16 + 16)

// We have no function that gives us direct access to these, so we have a local implementation for benchmarks
static void crc32_fold_copy_braid(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_braid(crc->value, src, len);
    memcpy(dst, src, len);
}
#ifndef WITHOUT_CHORBA
static void crc32_fold_copy_chorba(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_chorba(crc->value, src, len);
    memcpy(dst, src, len);
}
#endif
#ifndef WITHOUT_CHORBA_SSE
#  ifdef X86_SSE2
    static void crc32_fold_copy_chorba_sse2(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
        crc->value = crc32_chorba_sse2(crc->value, src, len);
        memcpy(dst, src, len);
    }
#  endif
#  ifdef X86_SSE41
    static void crc32_fold_copy_chorba_sse41(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
        crc->value = crc32_chorba_sse41(crc->value, src, len);
        memcpy(dst, src, len);
    }
#  endif
#endif

class crc32_fc: public benchmark::Fixture {
protected:
    uint32_t *testdata;
    uint8_t *dstbuf;
    uint32_t crc;

public:
    void SetUp(const ::benchmark::State& state) {
        testdata = (uint32_t *)malloc(BUFSIZE);
        dstbuf = (uint8_t *)malloc(BUFSIZE);
        assert((testdata != NULL) && (dstbuf != NULL));

        for (uint32_t i = 0; i < BUFSIZE/sizeof(uint32_t); i++) {
            testdata[i] = rand();
        }
    }

    void Bench(benchmark::State& state, crc32_fold_reset_func fold_reset, crc32_fold_copy_func fold_copy,
               crc32_fold_final_func fold_final) {
        ALIGNED_(16) crc32_fold crc_st;
        int misalign = 0;
        // Prepare an initial crc state
        fold_reset(&crc_st);
        crc = 0;

        // Benchmark the CRC32 fold copy operation
        for (auto _ : state) {
            fold_copy(&crc_st, dstbuf + misalign, (const unsigned char*)testdata + misalign, (size_t)state.range(0));
            misalign++;
            if (misalign > 14)
                misalign = 0;
        }

        // Finalize the CRC32 calculation
        crc = fold_final(&crc_st);

        // Prevent the result from being optimized away
        benchmark::DoNotOptimize(crc);
    }

    void TearDown(const ::benchmark::State& state) {
        free(testdata);
        free(dstbuf);
    }
};

#define BENCHMARK_CRC32_FOLD(name, resfunc, copyfunc, finfunc, support_flag) \
    BENCHMARK_DEFINE_F(crc32_fc, name)(benchmark::State& state) { \
        if (!support_flag) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, resfunc, copyfunc, finfunc); \
    } \
    BENCHMARK_REGISTER_F(crc32_fc, name)->Arg(16)->Arg(48)->Arg(192)->Arg(512)->Arg(4<<10)->Arg(16<<10)->Arg(32<<10);

// Generic
BENCHMARK_CRC32_FOLD(braid_c, crc32_fold_reset_c, crc32_fold_copy_braid, crc32_fold_final_c, 1)

#ifdef DISABLE_RUNTIME_CPU_DETECTION
    // Native
    BENCHMARK_CRC32_FOLD(native, native_crc32_fold_reset, native_crc32_fold_copy, native_crc32_fold_final, 1)
#else

    // Optimized functions
#  ifndef WITHOUT_CHORBA
    BENCHMARK_CRC32_FOLD(chorba_c, crc32_fold_reset_c, crc32_fold_copy_chorba, crc32_fold_final_c, 1)
#  endif
#  ifdef ARM_CRC32
    BENCHMARK_CRC32_FOLD(armv8, crc32_fold_reset_c, crc32_fold_copy_armv8, crc32_fold_final_c, test_cpu_features.arm.has_crc32)
#  endif
#  ifndef WITHOUT_CHORBA_SSE
#    ifdef X86_SSE2
        BENCHMARK_CRC32_FOLD(chorba_sse2, crc32_fold_reset_c, crc32_fold_copy_chorba_sse2, crc32_fold_final_c, test_cpu_features.x86.has_sse2)
#    endif
#    ifdef X86_SSE41
        BENCHMARK_CRC32_FOLD(chorba_sse41, crc32_fold_reset_c, crc32_fold_copy_chorba_sse41, crc32_fold_final_c, test_cpu_features.x86.has_sse41)
#     endif
#  endif
#  ifdef X86_PCLMULQDQ_CRC
    BENCHMARK_CRC32_FOLD(pclmulqdq, crc32_fold_pclmulqdq_reset, crc32_fold_pclmulqdq_copy, crc32_fold_pclmulqdq_final, test_cpu_features.x86.has_pclmulqdq)
#  endif
#  ifdef X86_VPCLMULQDQ_CRC
    BENCHMARK_CRC32_FOLD(vpclmulqdq, crc32_fold_pclmulqdq_reset, crc32_fold_vpclmulqdq_copy, crc32_fold_pclmulqdq_final, (test_cpu_features.x86.has_pclmulqdq && test_cpu_features.x86.has_avx512_common && test_cpu_features.x86.has_vpclmulqdq))
#  endif
#  ifdef LOONGARCH_CRC
    BENCHMARK_CRC32_FOLD(loongarch64, crc32_fold_reset_c, crc32_fold_copy_loongarch64, crc32_fold_final_c, test_cpu_features.loongarch.has_crc)
#  endif

#endif
