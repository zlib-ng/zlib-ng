/* benchmark_adler32_copy.cc -- benchmark adler32 (elided copy) variants
 * Copyright (C) 2022 Nathan Moinvaziri, Adam Stylinski
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
}

// Hash copy functions are used on strm->next_in buffers, we process
// 512-32k sizes (x2 for initial fill) at a time if enough data is available.
#define BUFSIZE (65536 + 64)

class adler32_copy: public benchmark::Fixture {
private:
    uint32_t *testdata;
    uint8_t *dstbuf;

public:
    void SetUp(::benchmark::State& state) {
        testdata = (uint32_t *)zng_alloc_aligned(BUFSIZE, 64);
        dstbuf = (uint8_t *)zng_alloc_aligned(BUFSIZE, 64);
        if (testdata == NULL || dstbuf == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }

        for (uint32_t i = 0; i < BUFSIZE/sizeof(uint32_t); i++) {
            testdata[i] = rand();
        }
    }

    // Benchmark Adler32_copy, with rolling buffer misalignment for consistent results
    void Bench(benchmark::State& state, adler32_copy_func adler32_copy, const int DO_ALIGNED) {
        int misalign = 0;
        uint32_t hash = 0;

        for (auto _ : state) {
            hash = adler32_copy(hash, dstbuf + misalign, (const unsigned char*)testdata + misalign, (size_t)state.range(0));
            if (misalign >= 63)
                misalign = 0;
            else
                misalign += (DO_ALIGNED) ? 16 : 1;
        }

        // Prevent the result from being optimized away
        benchmark::DoNotOptimize(hash);
    }

    void TearDown(const ::benchmark::State&) {
        zng_free_aligned(testdata);
        zng_free_aligned(dstbuf);
    }
};

// Misaligned
#define BENCHMARK_ADLER32_COPY_MISALIGNED(name, copyfunc, support_flag) \
    BENCHMARK_DEFINE_F(adler32_copy, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, copyfunc, 0); \
    } \
    BENCHMARK_REGISTER_F(adler32_copy, name)->Arg(32)->Arg(512)->Arg(8<<10)->Arg(32<<10)->Arg(64<<10);

// Aligned
#define ALIGNED_NAME(name) name##_aligned
#define BENCHMARK_ADLER32_COPY_ALIGNED(name, copyfunc, support_flag) \
    BENCHMARK_DEFINE_F(adler32_copy, ALIGNED_NAME(name))(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, copyfunc, 1); \
    } \
    BENCHMARK_REGISTER_F(adler32_copy, ALIGNED_NAME(name))->Arg(32)->Arg(512)->Arg(8<<10)->Arg(32<<10)->Arg(64<<10);

// Queue both misaligned and aligned for each benchmark
#define BENCHMARK_ADLER32_COPY(name, copyfunc, support_flag) \
    BENCHMARK_ADLER32_COPY_MISALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_ADLER32_COPY_ALIGNED(name, copyfunc, support_flag);

// Adler32 + memcpy benchmark for reference
#define BENCHMARK_ADLER32_BASELINE_COPY(name, copyfunc, support_flag) \
    BENCHMARK_DEFINE_F(adler32_copy, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, [](uint32_t init_sum, unsigned char *dst, \
                        const uint8_t *buf, size_t len) -> uint32_t { \
            memcpy(dst, buf, (size_t)len); \
            return copyfunc(init_sum, buf, len); \
        }, 1); \
    } \
    BENCHMARK_REGISTER_F(adler32_copy, name)->Arg(3)->Arg(16)->Arg(48)->Arg(192)->Arg(512)->Arg(4<<10)->Arg(16<<10)->Arg(32<<10)->Arg(64<<10);

BENCHMARK_ADLER32_BASELINE_COPY(c, adler32_c, 1);

#ifdef DISABLE_RUNTIME_CPU_DETECTION
BENCHMARK_ADLER32_BASELINE_COPY(native, native_adler32, 1);
#else

#ifdef ARM_NEON
/* If we inline this copy for neon, the function would go here */
BENCHMARK_ADLER32_COPY(neon, adler32_copy_neon, test_cpu_features.arm.has_neon);
BENCHMARK_ADLER32_BASELINE_COPY(neon_copy_baseline, adler32_neon, test_cpu_features.arm.has_neon);
#endif

#ifdef PPC_VMX
BENCHMARK_ADLER32_COPY(vmx, adler32_copy_vmx, test_cpu_features.power.has_altivec);
#endif
#ifdef POWER8_VSX
BENCHMARK_ADLER32_COPY(power8, adler32_copy_power8, test_cpu_features.power.has_arch_2_07);
#endif

#ifdef RISCV_RVV
//BENCHMARK_ADLER32_COPY(rvv, adler32_rvv, test_cpu_features.riscv.has_rvv);
BENCHMARK_ADLER32_BASELINE_COPY(rvv, adler32_rvv, test_cpu_features.riscv.has_rvv);
#endif
#ifdef X86_SSSE3
BENCHMARK_ADLER32_COPY(ssse3, adler32_copy_ssse3, test_cpu_features.x86.has_ssse3);
#endif
#ifdef X86_SSE42
BENCHMARK_ADLER32_BASELINE_COPY(sse42_baseline, adler32_ssse3, test_cpu_features.x86.has_ssse3);
BENCHMARK_ADLER32_COPY(sse42, adler32_copy_sse42, test_cpu_features.x86.has_sse42);
#endif
#ifdef X86_AVX2
BENCHMARK_ADLER32_BASELINE_COPY(avx2_baseline, adler32_avx2, test_cpu_features.x86.has_avx2);
BENCHMARK_ADLER32_COPY(avx2, adler32_copy_avx2, test_cpu_features.x86.has_avx2);
#endif
#ifdef X86_AVX512
BENCHMARK_ADLER32_BASELINE_COPY(avx512_baseline, adler32_avx512, test_cpu_features.x86.has_avx512_common);
BENCHMARK_ADLER32_COPY(avx512, adler32_copy_avx512, test_cpu_features.x86.has_avx512_common);
#endif
#ifdef X86_AVX512VNNI
BENCHMARK_ADLER32_BASELINE_COPY(avx512_vnni_baseline, adler32_avx512_vnni, test_cpu_features.x86.has_avx512vnni);
BENCHMARK_ADLER32_COPY(avx512_vnni, adler32_copy_avx512_vnni, test_cpu_features.x86.has_avx512vnni);
#endif

#ifdef LOONGARCH_LSX
BENCHMARK_ADLER32_BASELINE_COPY(lsx_baseline, adler32_lsx, test_cpu_features.loongarch.has_lsx);
BENCHMARK_ADLER32_COPY(lsx, adler32_copy_lsx, test_cpu_features.loongarch.has_lsx);
#endif
#ifdef LOONGARCH_LASX
BENCHMARK_ADLER32_BASELINE_COPY(lasx_baseline, adler32_lasx, test_cpu_features.loongarch.has_lasx);
BENCHMARK_ADLER32_COPY(lasx, adler32_copy_lasx, test_cpu_features.loongarch.has_lasx);
#endif

#endif
