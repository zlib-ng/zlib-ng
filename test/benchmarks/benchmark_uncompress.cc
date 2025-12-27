/* benchmark_uncompress.cc -- benchmark uncompress()
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

#define MAX_SIZE (1024 * 1024)
#define NUM_TESTS 6

class uncompress_bench: public benchmark::Fixture {
private:
    uint8_t *inbuff;
    uint8_t *outbuff;
    uint8_t *compressed_buff[NUM_TESTS];
    z_uintmax_t compressed_sizes[NUM_TESTS];
    uint32_t sizes[NUM_TESTS] = {1, 64, 1024, 16384, 128*1024, 1024*1024};

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

        // Compress data into different buffers
        for (int i = 0; i < NUM_TESTS; ++i) {
            compressed_buff[i] = (uint8_t *)zng_alloc(sizes[i] + 64);
            assert(compressed_buff[i] != NULL);

            z_uintmax_t compressed_size = sizes[i] + 64;
            int err = PREFIX(compress2)(compressed_buff[i], &compressed_size, inbuff, sizes[i], Z_BEST_COMPRESSION);
            if (err != Z_OK) {
                fprintf(stderr, "compress() failed with error %d\n", err);
                abort();
            }
            compressed_sizes[i] = compressed_size;
        }
    }

    void Bench(benchmark::State& state) {
        int err;

        for (auto _ : state) {
            int index = 0;
            while (sizes[index] != (uint32_t)state.range(0)) ++index;

            z_uintmax_t out_size = MAX_SIZE;
            err = PREFIX(uncompress)(outbuff, &out_size, compressed_buff[index], compressed_sizes[index]);
            if (err != Z_OK) {
                fprintf(stderr, "uncompress() failed with error %d\n", err);
                abort();
            }
        }
    }

    void TearDown(const ::benchmark::State&) {
        free(inbuff);
        free(outbuff);

        for (int i = 0; i < NUM_TESTS; ++i) {
            zng_free(compressed_buff[i]);
        }
    }
};

#define BENCHMARK_UNCOMPRESS(name) \
    BENCHMARK_DEFINE_F(uncompress_bench, name)(benchmark::State& state) { \
        Bench(state); \
    } \
    BENCHMARK_REGISTER_F(uncompress_bench, name)->Arg(1)->Arg(64)->Arg(1024)->Arg(16<<10)->Arg(128<<10)->Arg(1024<<10);

BENCHMARK_UNCOMPRESS(uncompress_bench);
