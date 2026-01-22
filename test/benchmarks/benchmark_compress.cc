/* benchmark_compress.cc -- benchmark compress()
 * Copyright (C) 2024-2025 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdio.h>
#include <assert.h>
#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil_p.h"
#  if defined(ZLIB_COMPAT)
#    include "zlib.h"
#  else
#    include "zlib-ng.h"
#  endif
#  include "compressible_data_p.h"
}

#define MAX_SIZE (64 * 1024)

class compress_bench: public benchmark::Fixture {
private:
    uint8_t *inbuff;
    uint8_t *outbuff;

public:
    void SetUp(::benchmark::State& state) {
        outbuff = (uint8_t *)malloc(MAX_SIZE + 16);
        if (outbuff == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }

        // Initialize input buffer with highly compressible data, interspersed
        // with small amounts of random data and 3-byte matches.
        inbuff = gen_compressible_data(MAX_SIZE);
        if (inbuff == NULL) {
            free(outbuff);
            outbuff = NULL;
            state.SkipWithError("gen_compressible_data() failed");
            return;
        }
    }

    void Bench(benchmark::State& state) {
        int err = 0;

        for (auto _ : state) {
            z_uintmax_t compressed_size = MAX_SIZE + 16;
            err = PREFIX(compress)(outbuff, &compressed_size, inbuff, (size_t)state.range(0));
            if (err != Z_OK) {
                fprintf(stderr, "compress() failed with error %d\n", err);
                abort();
            }

            // Prevent the result from being optimized away
            benchmark::DoNotOptimize(err);
        }
    }

    void TearDown(const ::benchmark::State&) {
        free(inbuff);
        free(outbuff);
    }
};

#define BENCHMARK_COMPRESS(name) \
    BENCHMARK_DEFINE_F(compress_bench, name)(benchmark::State& state) { \
        Bench(state); \
    } \
    BENCHMARK_REGISTER_F(compress_bench, name)->Arg(1)->Arg(16)->Arg(48)->Arg(256)->Arg(1<<10)->Arg(4<<10)->Arg(16<<10)->Arg(64<<10);

BENCHMARK_COMPRESS(compress_bench);
