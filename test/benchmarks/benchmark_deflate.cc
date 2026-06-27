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
#  include "test/test_data_p.h"
}

#define MAX_SIZE (1024 * 1024)

/* Parameterized deflate benchmark: Args(size, level) */
class deflate_bench: public benchmark::Fixture {
private:
    uint8_t *inbuff = nullptr;
    uint8_t *outbuff = nullptr;
    z_uintmax_t outbuff_size = 0;

public:
    /* Real setup runs from Dispatch() so each variant can pass its data type. */
    void SetUp(::benchmark::State&) {}

    void DoSetUp(::benchmark::State& state, enum test_data_type data_type) {
        outbuff_size = PREFIX(deflateBound)(NULL, MAX_SIZE);
        outbuff = (uint8_t *)malloc(outbuff_size);
        if (outbuff == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }

        inbuff = gen_test_data(data_type, MAX_SIZE);
        if (inbuff == NULL) {
            free(outbuff);
            outbuff = NULL;
            state.SkipWithError("gen_test_data() failed");
            return;
        }
    }

    void Bench(benchmark::State& state, int window_bits, int strategy) {
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

        state.counters["compressed"] = benchmark::Counter(double(strm.total_out));
        state.counters["ratio"] = benchmark::Counter(double(size) / double(strm.total_out));
    }

    void Dispatch(benchmark::State& state, enum test_data_type data_type, int window_bits, int strategy) {
        DoSetUp(state, data_type);
        if (state.skipped())
            return;
        Bench(state, window_bits, strategy);
    }

    void TearDown(const ::benchmark::State&) {
        free(inbuff);
        free(outbuff);
    }
};

#define DEFLATE_ARGS \
    ->Args({1024, 1})->Args({1024, 3})->Args({1024, 6})->Args({1024, 9}) \
    ->Args({16384, 1})->Args({16384, 3})->Args({16384, 6})->Args({16384, 9}) \
    ->Args({131072, 1})->Args({131072, 3})->Args({131072, 6})->Args({131072, 9}) \
    ->Args({1048576, 1})->Args({1048576, 3})->Args({1048576, 6})->Args({1048576, 9})

/* Strategy benchmarks use fewer size/level combos to keep test count reasonable */
#define DEFLATE_STRATEGY_ARGS \
    ->Args({1024, 1})->Args({1024, 6})->Args({1024, 9}) \
    ->Args({1048576, 1})->Args({1048576, 6})->Args({1048576, 9})

#define DEFLATE_VARIANT(variant, data, wbits, strategy, dt, args_macro) \
    BENCHMARK_DEFINE_F(deflate_bench, variant##_##data)(benchmark::State& state) { \
        Dispatch(state, dt, wbits, strategy); \
    } \
    BENCHMARK_REGISTER_F(deflate_bench, variant##_##data) \
        ->Name("deflate_bench/" #variant "/" #data) args_macro

#define DEFLATE_ALL_DATA(variant, wbits, strategy, args_macro) \
    DEFLATE_VARIANT(variant, text,          wbits, strategy, TEST_DATA_TEXT,          args_macro); \
    DEFLATE_VARIANT(variant, short_match,   wbits, strategy, TEST_DATA_SHORT_MATCH,   args_macro); \
    DEFLATE_VARIANT(variant, dna,           wbits, strategy, TEST_DATA_DNA,           args_macro); \
    DEFLATE_VARIANT(variant, random,        wbits, strategy, TEST_DATA_RANDOM,        args_macro); \
    DEFLATE_VARIANT(variant, realistic_rgb, wbits, strategy, TEST_DATA_REALISTIC_RGB, args_macro); \
    DEFLATE_VARIANT(variant, striped_rgb,   wbits, strategy, TEST_DATA_STRIPED_RGB,   args_macro)

/* Parameterized deflate with zlib wrapping (includes adler32 checksum) */
DEFLATE_ALL_DATA(level,    MAX_WBITS,  Z_DEFAULT_STRATEGY, DEFLATE_ARGS);
/* Parameterized raw deflate without checksum */
DEFLATE_ALL_DATA(nocrc,    -MAX_WBITS, Z_DEFAULT_STRATEGY, DEFLATE_ARGS);
/* Parameterized deflate with filtered strategy */
DEFLATE_ALL_DATA(filtered, MAX_WBITS,  Z_FILTERED,         DEFLATE_STRATEGY_ARGS);
/* Parameterized deflate with Huffman-only strategy */
DEFLATE_ALL_DATA(huffman,  MAX_WBITS,  Z_HUFFMAN_ONLY,     DEFLATE_STRATEGY_ARGS);
/* Parameterized deflate with RLE strategy */
DEFLATE_ALL_DATA(rle,      MAX_WBITS,  Z_RLE,              DEFLATE_STRATEGY_ARGS);
/* Parameterized deflate with fixed Huffman codes */
DEFLATE_ALL_DATA(fixed,    MAX_WBITS,  Z_FIXED,            DEFLATE_STRATEGY_ARGS);
