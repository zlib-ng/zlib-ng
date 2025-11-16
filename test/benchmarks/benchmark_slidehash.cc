/* benchmark_slidehash.cc -- benchmark slide_hash variants
 * Copyright (C) 2022 Adam Stylinski, Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <limits.h>

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil_p.h"
#  include "deflate.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
}

#define MAX_RANDOM_INTS 32768

class slide_hash: public benchmark::Fixture {
private:
    uint16_t *l0;
    uint16_t *l1;
    deflate_state *s_g;

public:
    /**
     * @brief Prepare the benchmark fixture by allocating and initializing working data.
     *
     * Allocates two 64-byte-aligned arrays of `uint16_t` (one of size HASH_SIZE, one of size MAX_RANDOM_INTS),
     * fills them with pseudorandom `uint16_t` values, allocates a `deflate_state` structure, and sets
     * its `head` and `prev` pointers to the allocated arrays.
     *
     * @param state Benchmark-provided state object from Google Benchmark (supplied by the framework).
     */
    void SetUp(const ::benchmark::State& state) {
        l0 = (uint16_t *)zng_alloc_aligned(HASH_SIZE * sizeof(uint16_t), 64);

        for (uint32_t i = 0; i < HASH_SIZE; i++) {
            l0[i] = (uint16_t)rand();
        }

        l1 = (uint16_t *)zng_alloc_aligned(MAX_RANDOM_INTS * sizeof(uint16_t), 64);

        for (int32_t i = 0; i < MAX_RANDOM_INTS; i++) {
            l1[i] = (uint16_t)rand();
        }

        deflate_state *s = (deflate_state*)malloc(sizeof(deflate_state));
        s->head = l0;
        s->prev = l1;
        s_g = s;
    }

    void Bench(benchmark::State& state, slide_hash_func slide_hash) {
        s_g->w_size = (uint32_t)state.range(0);

        for (auto _ : state) {
            slide_hash(s_g);
            benchmark::DoNotOptimize(s_g);
        }
    }

    void TearDown(const ::benchmark::State& state) {
        zng_free_aligned(l0);
        zng_free_aligned(l1);
        free(s_g);
    }
};

#define BENCHMARK_SLIDEHASH(name, fptr, support_flag) \
    BENCHMARK_DEFINE_F(slide_hash, name)(benchmark::State& state) { \
        if (!support_flag) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, fptr); \
    } \
    BENCHMARK_REGISTER_F(slide_hash, name)->RangeMultiplier(2)->Range(512, MAX_RANDOM_INTS);

#if defined(WITH_ALL_FALLBACKS) || !(defined(__x86_64__) || defined(_M_X64))
BENCHMARK_SLIDEHASH(c, slide_hash_c, 1);
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
BENCHMARK_SLIDEHASH(native, native_slide_hash, 1);
#else

#ifdef ARM_SIMD
BENCHMARK_SLIDEHASH(armv6, slide_hash_armv6, test_cpu_features.arm.has_simd);
#endif
#ifdef ARM_NEON
BENCHMARK_SLIDEHASH(neon, slide_hash_neon, test_cpu_features.arm.has_neon);
#endif
#ifdef POWER8_VSX
BENCHMARK_SLIDEHASH(power8, slide_hash_power8, test_cpu_features.power.has_arch_2_07);
#endif
#ifdef PPC_VMX
BENCHMARK_SLIDEHASH(vmx, slide_hash_vmx, test_cpu_features.power.has_altivec);
#endif
#ifdef RISCV_RVV
BENCHMARK_SLIDEHASH(rvv, slide_hash_rvv, test_cpu_features.riscv.has_rvv);
#endif
#ifdef X86_SSE2
BENCHMARK_SLIDEHASH(sse2, slide_hash_sse2, test_cpu_features.x86.has_sse2);
#endif
#ifdef X86_AVX2
BENCHMARK_SLIDEHASH(avx2, slide_hash_avx2, test_cpu_features.x86.has_avx2);
#endif
#ifdef LOONGARCH_LSX
BENCHMARK_SLIDEHASH(lsx, slide_hash_lsx, test_cpu_features.loongarch.has_lsx);
#endif
#ifdef LOONGARCH_LASX
BENCHMARK_SLIDEHASH(lasx, slide_hash_lasx, test_cpu_features.loongarch.has_lasx);
#endif

#endif