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

#include "benchmark_data_types.h"

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
        /* deflateBound does not cover the empty stored blocks that sync flushing emits,
         * so reserve room for one marker per smallest chunk. */
        outbuff_size = PREFIX(deflateBound)(NULL, MAX_SIZE) + (MAX_SIZE / 1024 + 1) * 16;
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

    void Bench(benchmark::State& state, int window_bits, int strategy, int sync) {
        int err;
        size_t size, chunk;
        int level = (int)state.range(1);

        /* Sync variants compress MAX_SIZE in Args(chunk)-sized pieces with Z_SYNC_FLUSH
         * between them. Whole-buffer variants are the degenerate case of one chunk. */
        if (sync) {
            size = MAX_SIZE;
            chunk = (size_t)state.range(0);
        } else {
            size = (size_t)state.range(0);
            chunk = size;
        }

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

            strm.next_out = outbuff;
            strm.avail_out = (uint32_t)outbuff_size;

            size_t offset = 0;
            while (offset < size) {
                size_t len = size - offset < chunk ? size - offset : chunk;
                strm.next_in = (z_const uint8_t *)inbuff + offset;
                strm.avail_in = (uint32_t)len;
                offset += len;

                err = PREFIX(deflate)(&strm, offset < size ? Z_SYNC_FLUSH : Z_FINISH);
                if (err != Z_OK && err != Z_STREAM_END) {
                    state.SkipWithError("deflate did not return Z_OK");
                    PREFIX(deflateEnd)(&strm);
                    return;
                }
            }
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

    void Dispatch(benchmark::State& state, enum test_data_type data_type, int window_bits, int strategy, int sync) {
        DoSetUp(state, data_type);
        if (state.skipped())
            return;
        Bench(state, window_bits, strategy, sync);
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

/* Non-text data types use a reduced size/level ladder to keep the benchmark
   count down; the text variants keep the full ladders. */
#define DEFLATE_DATA_ARGS \
    ->Args({131072, 3})->Args({131072, 6})->Args({131072, 9}) \
    ->Args({1048576, 3})->Args({1048576, 6})->Args({1048576, 9})

/* Sync-flush variants use Args(chunk, level), two cadences at the default level */
#define DEFLATE_SYNC_ARGS \
    ->Args({1024, 6})->Args({4096, 6})

#define DEFLATE_VARIANT(variant, data, wbits, strategy, sync, dt) \
    BENCHMARK_DEFINE_F(deflate_bench, variant##_##data)(benchmark::State& state) { \
        Dispatch(state, dt, wbits, strategy, sync); \
    }

#define DEFLATE_ALL_DATA(variant, wbits, strategy, sync) \
    DEFLATE_VARIANT(variant, text,          wbits, strategy, sync, TEST_DATA_TEXT); \
    DEFLATE_VARIANT(variant, short_match,   wbits, strategy, sync, TEST_DATA_SHORT_MATCH); \
    DEFLATE_VARIANT(variant, dna,           wbits, strategy, sync, TEST_DATA_DNA); \
    DEFLATE_VARIANT(variant, random,        wbits, strategy, sync, TEST_DATA_RANDOM); \
    DEFLATE_VARIANT(variant, literals,      wbits, strategy, sync, TEST_DATA_LITERALS); \
    DEFLATE_VARIANT(variant, mixed,         wbits, strategy, sync, TEST_DATA_MIXED); \
    DEFLATE_VARIANT(variant, realistic_rgb, wbits, strategy, sync, TEST_DATA_REALISTIC_RGB); \
    DEFLATE_VARIANT(variant, striped_rgb,   wbits, strategy, sync, TEST_DATA_STRIPED_RGB)

/* Parameterized deflate with zlib wrapping (includes adler32 checksum) */
DEFLATE_ALL_DATA(level,      MAX_WBITS,  Z_DEFAULT_STRATEGY, 0);
/* Parameterized raw deflate without checksum */
DEFLATE_ALL_DATA(nocrc,      -MAX_WBITS, Z_DEFAULT_STRATEGY, 0);
/* Parameterized deflate with filtered strategy */
DEFLATE_ALL_DATA(filtered,   MAX_WBITS,  Z_FILTERED, 0);
/* Parameterized deflate with Huffman-only strategy */
DEFLATE_ALL_DATA(huffman,    MAX_WBITS,  Z_HUFFMAN_ONLY, 0);
/* Parameterized deflate with RLE strategy */
DEFLATE_ALL_DATA(rle,        MAX_WBITS,  Z_RLE, 0);
/* Parameterized deflate with fixed Huffman codes */
DEFLATE_ALL_DATA(fixed,      MAX_WBITS,  Z_FIXED, 0);
/* Parameterized deflate with periodic Z_SYNC_FLUSH, cutting many small blocks so
   tree construction is a much larger share of the work than whole-buffer runs show */
DEFLATE_ALL_DATA(sync_flush, MAX_WBITS,  Z_DEFAULT_STRATEGY, 1);

/* Registered at runtime for the data types selected by --benchmark_data_types */
#define DEFLATE_REGISTER(variant, data, dt, args_macro) \
    if (mask & (1u << (dt))) \
        ::benchmark::internal::RegisterBenchmarkInternal( \
            ::benchmark::internal::make_unique<deflate_bench_##variant##_##data##_Benchmark>()) \
            ->Name("deflate_bench/" #variant "/" #data) args_macro

#define DEFLATE_REGISTER_ALL_DATA(variant, text_args_macro, data_args_macro) \
    DEFLATE_REGISTER(variant, text,          TEST_DATA_TEXT,          text_args_macro); \
    DEFLATE_REGISTER(variant, short_match,   TEST_DATA_SHORT_MATCH,   data_args_macro); \
    DEFLATE_REGISTER(variant, dna,           TEST_DATA_DNA,           data_args_macro); \
    DEFLATE_REGISTER(variant, random,        TEST_DATA_RANDOM,        data_args_macro); \
    DEFLATE_REGISTER(variant, literals,      TEST_DATA_LITERALS,      data_args_macro); \
    DEFLATE_REGISTER(variant, mixed,         TEST_DATA_MIXED,         data_args_macro); \
    DEFLATE_REGISTER(variant, realistic_rgb, TEST_DATA_REALISTIC_RGB, data_args_macro); \
    DEFLATE_REGISTER(variant, striped_rgb,   TEST_DATA_STRIPED_RGB,   data_args_macro)

static void deflate_register_data_types(uint32_t mask) {
    DEFLATE_REGISTER_ALL_DATA(level,      DEFLATE_ARGS,          DEFLATE_DATA_ARGS);
    DEFLATE_REGISTER_ALL_DATA(nocrc,      DEFLATE_ARGS,          DEFLATE_DATA_ARGS);
    DEFLATE_REGISTER_ALL_DATA(filtered,   DEFLATE_STRATEGY_ARGS, DEFLATE_DATA_ARGS);
    DEFLATE_REGISTER_ALL_DATA(huffman,    DEFLATE_STRATEGY_ARGS, DEFLATE_DATA_ARGS);
    DEFLATE_REGISTER_ALL_DATA(rle,        DEFLATE_STRATEGY_ARGS, DEFLATE_DATA_ARGS);
    DEFLATE_REGISTER_ALL_DATA(fixed,      DEFLATE_STRATEGY_ARGS, DEFLATE_DATA_ARGS);
    DEFLATE_REGISTER_ALL_DATA(sync_flush, DEFLATE_SYNC_ARGS,     DEFLATE_SYNC_ARGS);
}

static int deflate_data_types = benchmark_data_types_hook(deflate_register_data_types);
