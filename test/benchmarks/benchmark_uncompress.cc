/* benchmark_uncompress.cc -- benchmark uncompress()
 * Copyright (C) 2024 Hans Kristian Rosbach
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
}

#define MAX_SIZE (1024 * 1024)
#define NUM_TESTS 6

class uncompress_bench: public benchmark::Fixture {
private:
    size_t maxlen;
    uint8_t *inbuff;
    uint8_t *outbuff;
    uint8_t *compressed_buff[NUM_TESTS];
    uLong compressed_sizes[NUM_TESTS];
    int64_t sizes[NUM_TESTS] = {1, 64, 1024, 16384, 128*1024, 1024*1024};

public:
    void SetUp(const ::benchmark::State& state) {
        const char teststr[42] = "Hello hello World broken Test tast mello.";
        maxlen = MAX_SIZE;

        inbuff = (uint8_t *)zng_alloc(MAX_SIZE + 1);
        assert(inbuff != NULL);

        outbuff = (uint8_t *)zng_alloc(MAX_SIZE + 1);
        assert(outbuff != NULL);

        // Initialize input buffer
        int pos = 0;
        for (int32_t i = 0; i < MAX_SIZE - 42 ; i+=42){
           pos += sprintf((char *)inbuff+pos, "%s", teststr);
        }

        // Compress data into different buffers
        for (size_t i = 0; i < NUM_TESTS; ++i) {
            compressed_buff[i] = (uint8_t *)zng_alloc(MAX_SIZE + 1);
            assert(compressed_buff[i] != NULL);

            z_uintmax_t compressed_size = maxlen;
            int err = PREFIX(compress)(compressed_buff[i], &compressed_size, inbuff, sizes[i]);
            if (err != Z_OK) {
                fprintf(stderr, "Compression failed with error %d\n", err);
                abort();
            }
            compressed_sizes[i] = compressed_size;
        }
    }

    void Bench(benchmark::State& state) {
        int err = 0;

        for (auto _ : state) {
            int index = 0;
            while (sizes[index] != state.range(0)) ++index;

            z_uintmax_t out_size = maxlen;
            err = PREFIX(uncompress)(outbuff, &out_size, compressed_buff[index], compressed_sizes[index]);
        }

        benchmark::DoNotOptimize(err);
    }

    void TearDown(const ::benchmark::State& state) {
        zng_free(inbuff);
        zng_free(outbuff);

        for (size_t i = 0; i < NUM_TESTS; ++i) {
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
