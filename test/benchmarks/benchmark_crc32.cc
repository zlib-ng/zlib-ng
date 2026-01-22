/* benchmark_crc32.cc -- benchmark crc32 variants
 * Copyright (C) 2022 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
}

#define BUFSIZE ((4 * 1024 * 1024) + 64)

class crc32: public benchmark::Fixture {
private:
    uint32_t *testdata;

public:
    void SetUp(::benchmark::State& state) {
        testdata = (uint32_t *)zng_alloc_aligned(BUFSIZE, 64);
        if (testdata == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }

        for (uint32_t i = 0; i < BUFSIZE/sizeof(uint32_t); i++) {
            testdata[i] = rand();
        }
    }

    // Benchmark CRC32, with rolling buffer misalignment for consistent results
    void Bench(benchmark::State& state, crc32_func crc32, const int DO_ALIGNED) {
        int misalign = 0;
        uint32_t hash = 0;

        for (auto _ : state) {
            hash = crc32(hash, (const unsigned char *)testdata + misalign, (size_t)state.range(0));
            if (misalign >= 63)
                misalign = 0;
            else
                misalign += (DO_ALIGNED) ? 16 : 1;

            // Prevent the result from being optimized away
            benchmark::DoNotOptimize(hash);
        }
    }

    void TearDown(const ::benchmark::State&) {
        zng_free_aligned(testdata);
    }
};

#define BENCHMARK_CRC32_MISALIGNED(name, hashfunc, support_flag) \
    BENCHMARK_DEFINE_F(crc32, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, hashfunc, 0); \
    } \
    BENCHMARK_REGISTER_F(crc32, name)->Arg(1)->Arg(8)->Arg(12)->Arg(16)->Arg(32)->Arg(64)->Arg(512)->Arg(4<<10)->Arg(32<<10)->Arg(256<<10)->Arg(4096<<10);

// Aligned
#define ALIGNED_NAME(name) name##_aligned
#define BENCHMARK_CRC32_ALIGNED(name, hashfunc, support_flag) \
    BENCHMARK_DEFINE_F(crc32, ALIGNED_NAME(name))(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, hashfunc, 1); \
    } \
    BENCHMARK_REGISTER_F(crc32, ALIGNED_NAME(name))->Arg(8)->Arg(12)->Arg(16)->Arg(32)->Arg(64)->Arg(512)->Arg(4<<10)->Arg(32<<10)->Arg(256<<10)->Arg(4096<<10);

// Queue both misaligned and aligned for each benchmark
#define BENCHMARK_CRC32(name, hashfunc, support_flag) \
    BENCHMARK_CRC32_MISALIGNED(name, hashfunc, support_flag); \
    BENCHMARK_CRC32_ALIGNED(name, hashfunc, support_flag);

BENCHMARK_CRC32(braid, crc32_braid, 1);

#ifdef DISABLE_RUNTIME_CPU_DETECTION
BENCHMARK_CRC32(native, native_crc32, 1);
#else

#ifndef WITHOUT_CHORBA
BENCHMARK_CRC32(chorba_c, crc32_chorba, 1);
#endif
#ifndef WITHOUT_CHORBA_SSE
#   ifdef X86_SSE2
    BENCHMARK_CRC32(chorba_sse2, crc32_chorba_sse2, test_cpu_features.x86.has_sse2);
#   endif
#   ifdef X86_SSE41
    BENCHMARK_CRC32(chorba_sse41, crc32_chorba_sse41, test_cpu_features.x86.has_sse41);
#   endif
#endif
#ifdef ARM_CRC32
BENCHMARK_CRC32(armv8, crc32_armv8, test_cpu_features.arm.has_crc32);
#endif
#ifdef ARM_PMULL_EOR3
BENCHMARK_CRC32(armv8_pmull_eor3, crc32_armv8_pmull_eor3, test_cpu_features.arm.has_crc32 && test_cpu_features.arm.has_pmull && test_cpu_features.arm.has_eor3);
#endif
#ifdef RISCV_CRC32_ZBC
BENCHMARK_CRC32(riscv, crc32_riscv64_zbc, test_cpu_features.riscv.has_zbc);
#endif
#ifdef POWER8_VSX_CRC32
BENCHMARK_CRC32(power8, crc32_power8, test_cpu_features.power.has_arch_2_07);
#endif
#ifdef S390_CRC32_VX
BENCHMARK_CRC32(vx, crc32_s390_vx, test_cpu_features.s390.has_vx);
#endif
#ifdef X86_PCLMULQDQ_CRC
BENCHMARK_CRC32(pclmulqdq, crc32_pclmulqdq, test_cpu_features.x86.has_pclmulqdq);
#endif
#ifdef X86_VPCLMULQDQ_CRC
BENCHMARK_CRC32(vpclmulqdq, crc32_vpclmulqdq, (test_cpu_features.x86.has_pclmulqdq && test_cpu_features.x86.has_avx512_common && test_cpu_features.x86.has_vpclmulqdq));
#endif
#ifdef LOONGARCH_CRC
BENCHMARK_CRC32(loongarch64, crc32_loongarch64, test_cpu_features.loongarch.has_crc);
#endif

#endif
