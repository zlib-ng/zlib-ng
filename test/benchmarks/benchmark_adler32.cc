/* benchmark_adler32.cc -- benchmark adler32 variants
 * Copyright (C) 2022 Nathan Moinvaziri, Adam Stylinski
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
}

#define BUFSIZE ((4 * 1024 * 1024) + 64)

class adler32: public benchmark::Fixture {
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

    // Benchmark Adler32, with rolling buffer misalignment for consistent results
    void Bench(benchmark::State& state, adler32_func adler32, const int DO_ALIGNED) {
        int misalign = 0;
        uint32_t hash = 0;

        for (auto _ : state) {
            hash = adler32(hash, (const unsigned char*)testdata + misalign, (size_t)state.range(0));
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

#define BENCHMARK_ADLER32_MISALIGNED(name, hashfunc, support_flag) \
    BENCHMARK_DEFINE_F(adler32, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, hashfunc, 0); \
    } \
    BENCHMARK_REGISTER_F(adler32, name)->Arg(1)->Arg(8)->Arg(12)->Arg(16)->Arg(32)->Arg(64)->Arg(512)->Arg(4<<10)->Arg(32<<10)->Arg(256<<10)->Arg(4096<<10);

// Aligned
#define ALIGNED_NAME(name) name##_aligned
#define BENCHMARK_ADLER32_ALIGNED(name, hashfunc, support_flag) \
    BENCHMARK_DEFINE_F(adler32, ALIGNED_NAME(name))(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, hashfunc, 1); \
    } \
    BENCHMARK_REGISTER_F(adler32, ALIGNED_NAME(name))->Arg(8)->Arg(12)->Arg(16)->Arg(32)->Arg(64)->Arg(512)->Arg(4<<10)->Arg(32<<10)->Arg(256<<10)->Arg(4096<<10);

// Queue both misaligned and aligned for each benchmark
#define BENCHMARK_ADLER32(name, hashfunc, support_flag) \
    BENCHMARK_ADLER32_MISALIGNED(name, hashfunc, support_flag); \
    BENCHMARK_ADLER32_ALIGNED(name, hashfunc, support_flag);

BENCHMARK_ADLER32(c, adler32_c, 1);

#ifdef DISABLE_RUNTIME_CPU_DETECTION
BENCHMARK_ADLER32(native, native_adler32, 1);
#else

#ifdef ARM_NEON
BENCHMARK_ADLER32(neon, adler32_neon, test_cpu_features.arm.has_neon);
#endif

#ifdef PPC_VMX
BENCHMARK_ADLER32(vmx, adler32_vmx, test_cpu_features.power.has_altivec);
#endif
#ifdef POWER8_VSX
BENCHMARK_ADLER32(power8, adler32_power8, test_cpu_features.power.has_arch_2_07);
#endif

#ifdef RISCV_RVV
BENCHMARK_ADLER32(rvv, adler32_rvv, test_cpu_features.riscv.has_rvv);
#endif

#ifdef X86_SSSE3
BENCHMARK_ADLER32(ssse3, adler32_ssse3, test_cpu_features.x86.has_ssse3);
#endif
#ifdef X86_AVX2
BENCHMARK_ADLER32(avx2, adler32_avx2, test_cpu_features.x86.has_avx2);
#endif
#ifdef X86_AVX512
BENCHMARK_ADLER32(avx512, adler32_avx512, test_cpu_features.x86.has_avx512_common);
#endif
#ifdef X86_AVX512VNNI
BENCHMARK_ADLER32(avx512_vnni, adler32_avx512_vnni, test_cpu_features.x86.has_avx512vnni);
#endif

#ifdef LOONGARCH_LSX
BENCHMARK_ADLER32(lsx, adler32_lsx, test_cpu_features.loongarch.has_lsx);
#endif
#ifdef LOONGARCH_LASX
BENCHMARK_ADLER32(lasx, adler32_lasx, test_cpu_features.loongarch.has_lasx);
#endif

#endif
