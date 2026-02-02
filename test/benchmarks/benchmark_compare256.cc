/* benchmark_compare256.cc -- benchmark compare256 variants
 * Copyright (C) 2022 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
}

#define MAX_COMPARE_SIZE (256 + 64)

class compare256: public benchmark::Fixture {
private:
    uint8_t *str1;
    uint8_t *str2;

public:
    void SetUp(::benchmark::State& state) {
        str1 = (uint8_t *)malloc(MAX_COMPARE_SIZE);
        str2 = (uint8_t *)malloc(MAX_COMPARE_SIZE);
        if (str1 == NULL || str2 == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }

        memset(str1, 'a', MAX_COMPARE_SIZE);
        memset(str2, 'a', MAX_COMPARE_SIZE);
    }

    // Benchmark compare256, with rolling buffer misalignment for consistent results
    void Bench(benchmark::State& state, compare256_func compare256) {
        int misalign = 0;
        int32_t match_len = (int32_t)state.range(0) - 1;
        uint32_t len = 0;

        for (auto _ : state) {
            str2[match_len + misalign] = 0;   // Set new match limit

            len = compare256((const uint8_t *)str1 + misalign, (const uint8_t *)str2 + misalign);

            str2[match_len + misalign] = 'a'; // Reset match limit

            if (misalign >= 63)
                misalign = 0;
            else
                misalign++;

            // Prevent the result from being optimized away
            benchmark::DoNotOptimize(len);
        }
    }

    void TearDown(const ::benchmark::State&) {
        free(str1);
        free(str2);
    }
};

#define BENCHMARK_COMPARE256(name, comparefunc, support_flag) \
    BENCHMARK_DEFINE_F(compare256, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, comparefunc); \
    } \
    BENCHMARK_REGISTER_F(compare256, name)->Arg(1)->Arg(10)->Arg(40)->Arg(80)->Arg(100)->Arg(175)->Arg(256);

#ifdef DISABLE_RUNTIME_CPU_DETECTION
BENCHMARK_COMPARE256(native, native_compare256, 1);
#else

#ifdef WITH_ALL_FALLBACKS
BENCHMARK_COMPARE256(8, compare256_8, 1);
BENCHMARK_COMPARE256(64, compare256_64, 1);
#endif

#ifdef X86_SSE2
BENCHMARK_COMPARE256(sse2, compare256_sse2, test_cpu_features.x86.has_sse2);
#endif
#ifdef X86_AVX2
BENCHMARK_COMPARE256(avx2, compare256_avx2, test_cpu_features.x86.has_avx2);
#endif
#ifdef X86_AVX512
BENCHMARK_COMPARE256(avx512, compare256_avx512, test_cpu_features.x86.has_avx512_common);
#endif
#ifdef ARM_NEON
BENCHMARK_COMPARE256(neon, compare256_neon, test_cpu_features.arm.has_neon);
#endif
#ifdef POWER9
BENCHMARK_COMPARE256(power9, compare256_power9, test_cpu_features.power.has_arch_3_00);
#endif
#ifdef RISCV_RVV
BENCHMARK_COMPARE256(rvv, compare256_rvv, test_cpu_features.riscv.has_rvv);
#endif
#ifdef LOONGARCH_LSX
BENCHMARK_COMPARE256(lsx, compare256_lsx, test_cpu_features.loongarch.has_lsx);
#endif
#ifdef LOONGARCH_LASX
BENCHMARK_COMPARE256(lasx, compare256_lasx, test_cpu_features.loongarch.has_lasx);
#endif

#endif
