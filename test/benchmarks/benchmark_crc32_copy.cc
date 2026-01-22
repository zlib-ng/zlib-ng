/* benchmark_crc32_copy.cc -- benchmark for crc32 implementations with copying
 * Copyright (C) 2025 Hans Kristian Rosbach
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

class crc32_copy: public benchmark::Fixture {
protected:
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

    // Benchmark CRC32_copy, with rolling buffer misalignment for consistent results
    void Bench(benchmark::State& state, crc32_copy_func crc32_copy, const int DO_ALIGNED) {
        int misalign = 0;
        uint32_t hash = 0;

        for (auto _ : state) {
            hash = crc32_copy(hash, dstbuf + misalign, (const unsigned char*)testdata + misalign, (size_t)state.range(0));
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
#define BENCHMARK_CRC32_COPY_MISALIGNED(name, copyfunc, support_flag) \
    BENCHMARK_DEFINE_F(crc32_copy, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, copyfunc, 0); \
    } \
    BENCHMARK_REGISTER_F(crc32_copy, name)->Arg(32)->Arg(512)->Arg(8<<10)->Arg(32<<10)->Arg(64<<10);

// Aligned
#define ALIGNED_NAME(name) name##_aligned
#define BENCHMARK_CRC32_COPY_ALIGNED(name, copyfunc, support_flag) \
    BENCHMARK_DEFINE_F(crc32_copy, ALIGNED_NAME(name))(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, copyfunc, 1); \
    } \
    BENCHMARK_REGISTER_F(crc32_copy, ALIGNED_NAME(name))->Arg(32)->Arg(512)->Arg(8<<10)->Arg(32<<10)->Arg(64<<10);

// CRC32 + memcpy benchmarks for reference
#ifdef HASH_BASELINE
#define MEMCPY_NAME(name) name##_memcpy
#define BENCHMARK_CRC32_MEMCPY_MISALIGNED(name, hashfunc, support_flag) \
    BENCHMARK_DEFINE_F(crc32_copy, MEMCPY_NAME(name))(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
	Bench(state, [](uint32_t init_sum, unsigned char *dst, \
                        const uint8_t *buf, size_t len) -> uint32_t { \
            memcpy(dst, buf, (size_t)len); \
            return hashfunc(init_sum, buf, len); \
        }, 0); \
    } \
    BENCHMARK_REGISTER_F(crc32_copy, MEMCPY_NAME(name))->Arg(32)->Arg(512)->Arg(8<<10)->Arg(32<<10)->Arg(64<<10);

#define MEMCPY_ALIGNED_NAME(name) name##_memcpy_aligned
#define BENCHMARK_CRC32_MEMCPY_ALIGNED(name, hashfunc, support_flag) \
    BENCHMARK_DEFINE_F(crc32_copy, MEMCPY_ALIGNED_NAME(name))(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
	Bench(state, [](uint32_t init_sum, unsigned char *dst, \
                        const uint8_t *buf, size_t len) -> uint32_t { \
            memcpy(dst, buf, (size_t)len); \
            return hashfunc(init_sum, buf, len); \
        }, 1); \
    } \
    BENCHMARK_REGISTER_F(crc32_copy, MEMCPY_ALIGNED_NAME(name))->Arg(32)->Arg(512)->Arg(8<<10)->Arg(32<<10)->Arg(64<<10);
#endif


// Queue both misaligned and aligned for each benchmark
#define BENCHMARK_CRC32_COPY_ONLY(name, copyfunc, support_flag) \
    BENCHMARK_CRC32_COPY_MISALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_CRC32_COPY_ALIGNED(name, copyfunc, support_flag);

// Optionally also benchmark using memcpy with normal hash function for baseline
#ifdef HASH_BASELINE
#define BENCHMARK_CRC32_COPY(name, hashfunc, copyfunc, support_flag) \
    BENCHMARK_CRC32_COPY_MISALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_CRC32_COPY_ALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_CRC32_MEMCPY_MISALIGNED(name, copyfunc, support_flag); \
    BENCHMARK_CRC32_MEMCPY_ALIGNED(name, copyfunc, support_flag);
#else
#define BENCHMARK_CRC32_COPY(name, hashfunc, copyfunc, support_flag) \
    BENCHMARK_CRC32_COPY_ONLY(name, copyfunc, support_flag)
#endif

// Base test
BENCHMARK_CRC32_COPY(braid, crc32_braid, crc32_copy_braid, 1);

#ifdef DISABLE_RUNTIME_CPU_DETECTION
    // Native
    BENCHMARK_CRC32_COPY(native, native_crc32, native_crc32_copy, 1)
#else
    // Optimized functions
#  ifndef WITHOUT_CHORBA
    BENCHMARK_CRC32_COPY(chorba, crc32_chorba, crc32_copy_chorba, 1)
#  endif
#  ifndef WITHOUT_CHORBA_SSE
#    ifdef X86_SSE2
    BENCHMARK_CRC32_COPY(chorba_sse2, crc32_chorba_sse2, crc32_copy_chorba_sse2, test_cpu_features.x86.has_sse2);
#    endif
#    ifdef X86_SSE41
    BENCHMARK_CRC32_COPY(chorba_sse41, crc32_chorba_sse41, crc32_copy_chorba_sse41, test_cpu_features.x86.has_sse41);
#    endif
#  endif
#  ifdef ARM_CRC32
    BENCHMARK_CRC32_COPY(armv8, crc32_armv8, crc32_copy_armv8, test_cpu_features.arm.has_crc32)
#  endif
#  ifdef ARM_PMULL_EOR3
    BENCHMARK_CRC32_COPY(armv8_pmull_eor3, crc32_armv8_pmull_eor3, crc32_copy_armv8_pmull_eor3, test_cpu_features.arm.has_crc32 && test_cpu_features.arm.has_pmull && test_cpu_features.arm.has_eor3)
#  endif
#  ifdef LOONGARCH_CRC
    BENCHMARK_CRC32_COPY(loongarch, crc32_loongarch64, crc32_copy_loongarch64, test_cpu_features.loongarch.has_crc)
#  endif
#  ifdef POWER8_VSX_CRC32
    BENCHMARK_CRC32_COPY(power8, crc32_power8, crc32_copy_power8, test_cpu_features.power.has_arch_2_07)
#  endif
#  ifdef RISCV_CRC32_ZBC
    BENCHMARK_CRC32_COPY(riscv, crc32_riscv, crc32_copy_riscv64_zbc, test_cpu_features.riscv.has_zbc)
#  endif
#  ifdef S390_CRC32_VX
    BENCHMARK_CRC32_COPY(vx, crc32_s390_vx, crc32_copy_s390_vx, test_cpu_features.s390.has_vx)
#  endif
#  ifdef X86_PCLMULQDQ_CRC
    BENCHMARK_CRC32_COPY(pclmulqdq, crc32_pclmulqdq, crc32_copy_pclmulqdq, test_cpu_features.x86.has_pclmulqdq)
#  endif
#  ifdef X86_VPCLMULQDQ_CRC
    BENCHMARK_CRC32_COPY(vpclmulqdq, crc32_vpclmulqdq, crc32_copy_vpclmulqdq, (test_cpu_features.x86.has_pclmulqdq && test_cpu_features.x86.has_avx512_common && test_cpu_features.x86.has_vpclmulqdq))
#  endif

#endif
