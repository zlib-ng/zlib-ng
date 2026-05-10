/* benchmark_chunkmemset.cc -- benchmark chunkmemset_safe variants
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil_p.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
}

#define BUFSIZE        8192
#define BENCH_MAX_DIST 64

typedef uint8_t* (*chunkmemset_safe_func)(uint8_t *out, uint8_t *from, size_t len, size_t left);

class chunkmemset: public benchmark::Fixture {
private:
    uint8_t *buf;

public:
    void SetUp(::benchmark::State& state) {
        buf = (uint8_t *)zng_alloc_aligned(BUFSIZE, 64);
        if (buf == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }
        for (size_t i = 0; i < BUFSIZE; i++) {
            buf[i] = (uint8_t)(i & 0xFF);
        }
    }

    void TearDown(const ::benchmark::State&) {
        zng_free_aligned(buf);
    }

    void Bench(benchmark::State& state, chunkmemset_safe_func chunkmemset_safe) {
        size_t dist = (size_t)state.range(0);
        size_t len  = (size_t)state.range(1);
        uint8_t *out  = buf + BENCH_MAX_DIST;
        uint8_t *from = out - dist;
        size_t left = BUFSIZE - BENCH_MAX_DIST;

        for (auto _ : state) {
            uint8_t *result = chunkmemset_safe(out, from, len, left);
            benchmark::DoNotOptimize(result);
        }
    }
};

#define CHUNKMEMSET_ARGS                                       \
        ArgNames({"dist", "len"})                              \
        ->Args({1,  32})    /* memset path */                  \
        ->Args({1,  258})                                      \
        ->Args({2,  16})    /* fast vdupq, small */            \
        ->Args({2,  64})    /* fast vdupq, main loop */        \
        ->Args({2,  258})                                      \
        ->Args({3,  16})    /* magazine (TBL/PERMUTE) small */ \
        ->Args({3,  64})                                       \
        ->Args({3,  258})                                      \
        ->Args({4,  16})    /* fast vdupq u32 */               \
        ->Args({4,  258})                                      \
        ->Args({6,  16})    /* magazine, half-chunk on AVX2 */ \
        ->Args({6,  258})                                      \
        ->Args({8,  16})    /* fast vdupq u64 */               \
        ->Args({8,  258})                                      \
        ->Args({15, 258})   /* magazine, dist near chunk */    \
        ->Args({16, 258})   /* dist == chunk_t boundary */     \
        ->Args({32, 64})    /* chunkcopy direct */             \
        ->Args({32, 258})                                      \
        ->Args({8,  3})     /* dist >= len bypass (no fold) */ \
        ->Args({15, 8})     /* dist >= len bypass */

#define BENCHMARK_CHUNKMEMSET(name, fn, support_flag)                          \
    BENCHMARK_DEFINE_F(chunkmemset, name)(benchmark::State& state) {           \
        if (!(support_flag)) {                                                 \
            state.SkipWithError("CPU does not support " #name);                \
        }                                                                      \
        Bench(state, fn);                                                      \
    }                                                                          \
    BENCHMARK_REGISTER_F(chunkmemset, name)->CHUNKMEMSET_ARGS;

#ifdef CHUNKSET_FALLBACK
BENCHMARK_CHUNKMEMSET(c, chunkmemset_safe_c, 1);
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
BENCHMARK_CHUNKMEMSET(native, native_chunkmemset_safe, 1);
#else

#ifdef ARM_NEON
BENCHMARK_CHUNKMEMSET(neon, chunkmemset_safe_neon, test_cpu_features.arm.has_neon);
#endif

#ifdef X86_SSE2
BENCHMARK_CHUNKMEMSET(sse2, chunkmemset_safe_sse2, test_cpu_features.x86.has_sse2);
#endif
#ifdef X86_SSSE3
BENCHMARK_CHUNKMEMSET(ssse3, chunkmemset_safe_ssse3, test_cpu_features.x86.has_ssse3);
#endif
#ifdef X86_AVX2
BENCHMARK_CHUNKMEMSET(avx2, chunkmemset_safe_avx2, test_cpu_features.x86.has_avx2);
#endif
#ifdef X86_AVX512
BENCHMARK_CHUNKMEMSET(avx512, chunkmemset_safe_avx512, test_cpu_features.x86.has_avx512_common);
#endif

#ifdef POWER8_VSX
BENCHMARK_CHUNKMEMSET(power8, chunkmemset_safe_power8, test_cpu_features.power.has_arch_2_07);
#endif

#ifdef RISCV_RVV
BENCHMARK_CHUNKMEMSET(rvv, chunkmemset_safe_rvv, test_cpu_features.riscv.has_rvv);
#endif

#ifdef LOONGARCH_LSX
BENCHMARK_CHUNKMEMSET(lsx, chunkmemset_safe_lsx, test_cpu_features.loongarch.has_lsx);
#endif
#ifdef LOONGARCH_LASX
BENCHMARK_CHUNKMEMSET(lasx, chunkmemset_safe_lasx, test_cpu_features.loongarch.has_lasx);
#endif

#endif
