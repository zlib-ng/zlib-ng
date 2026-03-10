/* benchmark_deflate.cc -- benchmark deflate() with various levels and raw mode
 * Copyright (C) 2026 Nathan Moinvaziri
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
#  include "test/compressible_data_p.h"
}

#define MAX_SIZE (1024 * 1024)

/* Parameterized deflate benchmark: Args(size, level) */
class deflate_bench: public benchmark::Fixture {
private:
    uint8_t *inbuff = nullptr;
    uint8_t *outbuff = nullptr;
    z_uintmax_t outbuff_size = 0;

public:
    void SetUp(::benchmark::State& state) {
        outbuff_size = PREFIX(deflateBound)(NULL, MAX_SIZE);
        outbuff = (uint8_t *)malloc(outbuff_size);
        if (outbuff == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }

        inbuff = gen_compressible_data(MAX_SIZE);
        if (inbuff == NULL) {
            free(outbuff);
            outbuff = NULL;
            state.SkipWithError("gen_compressible_data() failed");
            return;
        }
    }

    void Bench(benchmark::State& state, int window_bits, int strategy = Z_DEFAULT_STRATEGY) {
        int err;
        size_t size = (size_t)state.range(0);
        int level = (int)state.range(1);

        PREFIX3(stream) strm;
        strm.zalloc = NULL;
        strm.zfree = NULL;
        strm.opaque = NULL;
        strm.total_in = 0;
        strm.total_out = 0;
        strm.next_out = NULL;
        strm.avail_out = 0;

        err = PREFIX(deflateInit2)(&strm, level, Z_DEFLATED, window_bits, MAX_MEM_LEVEL, strategy);
        if (err != Z_OK) {
            state.SkipWithError("deflateInit2 did not return Z_OK");
            return;
        }

        for (auto _ : state) {
            err = PREFIX(deflateReset)(&strm);
            if (err != Z_OK) {
                state.SkipWithError("deflateReset did not return Z_OK");
                PREFIX(deflateEnd)(&strm);
                return;
            }

            strm.avail_in = (uint32_t)size;
            strm.next_in = (z_const uint8_t *)inbuff;
            strm.next_out = outbuff;
            strm.avail_out = (uint32_t)outbuff_size;

            err = PREFIX(deflate)(&strm, Z_FINISH);
            if (err != Z_STREAM_END) {
                state.SkipWithError("deflate did not return Z_STREAM_END");
                PREFIX(deflateEnd)(&strm);
                return;
            }
        }

        err = PREFIX(deflateEnd)(&strm);
        if (err != Z_OK) {
            state.SkipWithError("deflateEnd did not return Z_OK");
            return;
        }
    }

    void TearDown(const ::benchmark::State&) {
        free(inbuff);
        free(outbuff);
    }
};

#define BENCHMARK_DEFLATE_ARGS \
    ->Args({1024, 1})->Args({1024, 3})->Args({1024, 6})->Args({1024, 9}) \
    ->Args({16384, 1})->Args({16384, 3})->Args({16384, 6})->Args({16384, 9}) \
    ->Args({131072, 1})->Args({131072, 3})->Args({131072, 6})->Args({131072, 9}) \
    ->Args({1048576, 1})->Args({1048576, 3})->Args({1048576, 6})->Args({1048576, 9})

/* Parameterized deflate with zlib wrapping (includes adler32 checksum) */
BENCHMARK_DEFINE_F(deflate_bench, deflate_level)(benchmark::State& state) {
    Bench(state, MAX_WBITS);
}
BENCHMARK_REGISTER_F(deflate_bench, deflate_level) BENCHMARK_DEFLATE_ARGS;

/* Parameterized raw deflate without checksum */
BENCHMARK_DEFINE_F(deflate_bench, deflate_nocrc)(benchmark::State& state) {
    Bench(state, -MAX_WBITS);
}
BENCHMARK_REGISTER_F(deflate_bench, deflate_nocrc) BENCHMARK_DEFLATE_ARGS;

/* Strategy benchmarks use fewer size/level combos to keep test count reasonable */
#define BENCHMARK_DEFLATE_STRATEGY_ARGS \
    ->Args({1024, 1})->Args({1024, 6})->Args({1024, 9}) \
    ->Args({1048576, 1})->Args({1048576, 6})->Args({1048576, 9})

/* Parameterized deflate with filtered strategy */
BENCHMARK_DEFINE_F(deflate_bench, deflate_filtered)(benchmark::State& state) {
    Bench(state, MAX_WBITS, Z_FILTERED);
}
BENCHMARK_REGISTER_F(deflate_bench, deflate_filtered) BENCHMARK_DEFLATE_STRATEGY_ARGS;

/* Parameterized deflate with Huffman-only strategy */
BENCHMARK_DEFINE_F(deflate_bench, deflate_huffman)(benchmark::State& state) {
    Bench(state, MAX_WBITS, Z_HUFFMAN_ONLY);
}
BENCHMARK_REGISTER_F(deflate_bench, deflate_huffman) BENCHMARK_DEFLATE_STRATEGY_ARGS;

/* Parameterized deflate with RLE strategy */
BENCHMARK_DEFINE_F(deflate_bench, deflate_rle)(benchmark::State& state) {
    Bench(state, MAX_WBITS, Z_RLE);
}
BENCHMARK_REGISTER_F(deflate_bench, deflate_rle) BENCHMARK_DEFLATE_STRATEGY_ARGS;

/* Parameterized deflate with fixed Huffman codes */
BENCHMARK_DEFINE_F(deflate_bench, deflate_fixed)(benchmark::State& state) {
    Bench(state, MAX_WBITS, Z_FIXED);
}
BENCHMARK_REGISTER_F(deflate_bench, deflate_fixed) BENCHMARK_DEFLATE_STRATEGY_ARGS;
