/* benchmark_crc32_copy.cc -- benchmark for crc32 implementations with copying
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

class crc32_copy: public benchmark::Fixture {
protected:
    uint32_t *testdata;
    uint8_t *dstbuf;

public:
    void SetUp(const ::benchmark::State&) {
        testdata = (uint32_t *)malloc(BUFSIZE);
        dstbuf = (uint8_t *)malloc(BUFSIZE);
        assert((testdata != NULL) && (dstbuf != NULL));

        for (uint32_t i = 0; i < BUFSIZE/sizeof(uint32_t); i++) {
            testdata[i] = rand();
        }
    }

    void Bench(benchmark::State& state, crc32_copy_func crc32_copy) {
        int misalign = 0;
        uint32_t crc = 0;

        // Benchmark the CRC32 copy operation
        for (auto _ : state) {
            crc = crc32_copy(crc, dstbuf + misalign, (const unsigned char*)testdata + misalign, (size_t)state.range(0));
            misalign++;
            if (misalign > 14)
                misalign = 0;
        }

        // Prevent the result from being optimized away
        benchmark::DoNotOptimize(crc);
    }

    void TearDown(const ::benchmark::State&) {
        free(testdata);
        free(dstbuf);
    }
};

#define BENCHMARK_CRC32_COPY(name, copyfunc, support_flag) \
    BENCHMARK_DEFINE_F(crc32_copy, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, copyfunc); \
    } \
    BENCHMARK_REGISTER_F(crc32_copy, name)->Arg(16)->Arg(48)->Arg(192)->Arg(512)->Arg(4<<10)->Arg(16<<10)->Arg(32<<10);

// Base test
BENCHMARK_CRC32_COPY(braid, crc32_copy_braid, 1);

#ifdef DISABLE_RUNTIME_CPU_DETECTION
    // Native
    BENCHMARK_CRC32_COPY(native, native_crc32_copy, 1)
#else
    // Optimized functions
#  ifndef WITHOUT_CHORBA
    BENCHMARK_CRC32_COPY(chorba, crc32_copy_chorba, 1)
#  endif
#  ifndef WITHOUT_CHORBA_SSE
#    ifdef X86_SSE2
    BENCHMARK_CRC32_COPY(chorba_sse2, crc32_copy_chorba_sse2, test_cpu_features.x86.has_sse2);
#    endif
#    ifdef X86_SSE41
    BENCHMARK_CRC32_COPY(chorba_sse41, crc32_copy_chorba_sse41, test_cpu_features.x86.has_sse41);
#    endif
#  endif
#  ifdef ARM_CRC32
    BENCHMARK_CRC32_COPY(armv8, crc32_copy_armv8, test_cpu_features.arm.has_crc32)
#  endif
#  ifdef ARM_PMULL_EOR3
    BENCHMARK_CRC32_COPY(armv8_pmull_eor3, crc32_copy_armv8_pmull_eor3, test_cpu_features.arm.has_crc32 && test_cpu_features.arm.has_pmull && test_cpu_features.arm.has_eor3)
#  endif
#  ifdef LOONGARCH_CRC
    BENCHMARK_CRC32_COPY(loongarch, crc32_copy_loongarch64, test_cpu_features.loongarch.has_crc)
#  endif
#  ifdef POWER8_VSX_CRC32
    BENCHMARK_CRC32_COPY(power8, crc32_copy_power8, test_cpu_features.power.has_arch_2_07)
#  endif
#  ifdef RISCV_CRC32_ZBC
    BENCHMARK_CRC32_COPY(riscv, crc32_copy_riscv64_zbc, test_cpu_features.riscv.has_zbc)
#  endif
#  ifdef S390_CRC32_VX
    BENCHMARK_CRC32_COPY(vx, crc32_copy_s390_vx, test_cpu_features.s390.has_vx)
#  endif
#  ifdef X86_PCLMULQDQ_CRC
    BENCHMARK_CRC32_COPY(pclmulqdq, crc32_copy_pclmulqdq, test_cpu_features.x86.has_pclmulqdq)
#  endif
#  ifdef X86_VPCLMULQDQ_CRC
    BENCHMARK_CRC32_COPY(vpclmulqdq, crc32_copy_vpclmulqdq, (test_cpu_features.x86.has_pclmulqdq && test_cpu_features.x86.has_avx512_common && test_cpu_features.x86.has_vpclmulqdq))
#  endif

#endif
