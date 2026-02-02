/* benchmark_compare256_rle.cc -- benchmark compare256_rle variants
 * Copyright (C) 2022 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "compare256_rle.h"
}

#define MAX_COMPARE_SIZE (256 + 64)

class compare256_rle: public benchmark::Fixture {
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

    // Benchmark compare256_rle, with rolling buffer misalignment for consistent results
    void Bench(benchmark::State& state, compare256_rle_func compare256_rle) {
        int misalign = 0;
        int32_t match_len = (int32_t)state.range(0) - 1;
        uint32_t len = 0;

        for (auto _ : state) {
            str2[match_len + misalign] = 0;   // Set new match limit

            len = compare256_rle((const uint8_t *)str1 + misalign, (const uint8_t *)str2 + misalign);

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

#define BENCHMARK_COMPARE256_RLE(name, comparefunc, support_flag) \
    BENCHMARK_DEFINE_F(compare256_rle, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, comparefunc); \
    } \
    BENCHMARK_REGISTER_F(compare256_rle, name)->Arg(1)->Arg(10)->Arg(40)->Arg(80)->Arg(100)->Arg(175)->Arg(256);;

BENCHMARK_COMPARE256_RLE(8, compare256_rle_8, 1);
BENCHMARK_COMPARE256_RLE(64, compare256_rle_64, 1);
