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

            // Prevent the result from being optimized away
            benchmark::DoNotOptimize(hash);
        }
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


// Adler32 + memcpy benchmarks for reference
#ifdef HASH_BASELINE
#define MEMCPY_NAME(name) name##_memcpy
#define BENCHMARK_ADLER32_MEMCPY_MISALIGNED(name, hashfunc, support_flag) \
    BENCHMARK_DEFINE_F(adler32_copy, MEMCPY_NAME(name))(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, [](uint32_t init_sum, unsigned char *dst, \
                        const uint8_t *buf, size_t len) -> uint32_t { \
            memcpy(dst, buf, (size_t)len); \
            return hashfunc(init_sum, buf, len); \
        }, 0); \
    } \
    BENCHMARK_REGISTER_F(adler32_copy, MEMCPY_NAME(name))->Arg(32)->Arg(512)->Arg(8<<10)->Arg(32<<10)->Arg(64<<10);

#define MEMCPY_ALIGNED_NAME(name) name##_memcpy_aligned
#define BENCHMARK_ADLER32_MEMCPY_ALIGNED(name, hashfunc, support_flag) \
    BENCHMARK_DEFINE_F(adler32_copy, MEMCPY_ALIGNED_NAME(name))(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, [](uint32_t init_sum, unsigned char *dst, \
                        const uint8_t *buf, size_t len) -> uint32_t { \
            memcpy(dst, buf, (size_t)len); \
            return hashfunc(init_sum, buf, len); \
        }, 1); \
    } \
    BENCHMARK_REGISTER_F(adler32_copy, MEMCPY_ALIGNED_NAME(name))->Arg(32)->Arg(512)->Arg(8<<10)->Arg(32<<10)->Arg(64<<10);
#endif


// Queue both misaligned and aligned for each benchmark
#define BENCHMARK_ADLER32_COPY_ONLY(name, copyfunc, support_flag) \
    BENCHMARK_ADLER32_COPY_MISALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_ADLER32_COPY_ALIGNED(name, copyfunc, support_flag);

// Optionally also benchmark using memcpy with normal hash function for baseline
#ifdef HASH_BASELINE
#define BENCHMARK_ADLER32_COPY(name, hashfunc, copyfunc, support_flag) \
    BENCHMARK_ADLER32_COPY_MISALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_ADLER32_COPY_ALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_ADLER32_MEMCPY_MISALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_ADLER32_MEMCPY_ALIGNED(name, copyfunc, support_flag);
#else
#define BENCHMARK_ADLER32_COPY(name, hashfunc, copyfunc, support_flag) \
    BENCHMARK_ADLER32_COPY_ONLY(name, copyfunc, support_flag)
#endif

BENCHMARK_ADLER32_COPY(c, adler32_c, adler32_copy_c, 1);

#ifdef DISABLE_RUNTIME_CPU_DETECTION
BENCHMARK_ADLER32_COPY(native, native_adler32, native_adler32_copy, 1);
#else

#ifdef ARM_NEON
BENCHMARK_ADLER32_COPY(neon, adler32_neon, adler32_copy_neon, test_cpu_features.arm.has_neon);
#endif

#ifdef PPC_VMX
BENCHMARK_ADLER32_COPY(vmx, adler32_vmx, adler32_copy_vmx, test_cpu_features.power.has_altivec);
#endif
#ifdef POWER8_VSX
BENCHMARK_ADLER32_COPY(power8, adler32_power8, adler32_copy_power8, test_cpu_features.power.has_arch_2_07);
#endif

#ifdef RISCV_RVV
BENCHMARK_ADLER32_COPY(rvv, adler32_rvv, adler32_copy_rvv, test_cpu_features.riscv.has_rvv);
#endif

#ifdef X86_SSSE3
BENCHMARK_ADLER32_COPY(ssse3, adler32_ssse3, adler32_copy_ssse3, test_cpu_features.x86.has_ssse3);
#endif
#ifdef X86_SSE42
// There is no adler32_sse42, so only test the copy variant
BENCHMARK_ADLER32_COPY_ONLY(sse42, adler32_copy_sse42, test_cpu_features.x86.has_sse42);
#endif
#ifdef X86_AVX2
BENCHMARK_ADLER32_COPY(avx2, adler32_avx, adler32_copy_avx2, test_cpu_features.x86.has_avx2);
#endif
#ifdef X86_AVX512
BENCHMARK_ADLER32_COPY(avx512, adler32_avx512, adler32_copy_avx512, test_cpu_features.x86.has_avx512_common);
#endif
#ifdef X86_AVX512VNNI
BENCHMARK_ADLER32_COPY(avx512_vnni, adler32_avx512_vnni, adler32_copy_avx512_vnni, test_cpu_features.x86.has_avx512vnni);
#endif

#ifdef LOONGARCH_LSX
BENCHMARK_ADLER32_COPY(lsx, adler32_lsx, adler32_copy_lsx, test_cpu_features.loongarch.has_lsx);
#endif
#ifdef LOONGARCH_LASX
BENCHMARK_ADLER32_COPY(lasx, adler32_lasx, adler32_copy_lasx, test_cpu_features.loongarch.has_lasx);
#endif

#endif
