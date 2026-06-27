/* benchmark_inflate.cc -- benchmark inflate() without crc32/adler32
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
#  include "test/test_data_p.h"
}

#define MAX_SIZE (1024 * 1024)
class inflate_bench: public benchmark::Fixture {
private:
    uint8_t *inbuff = nullptr;
    uint8_t *outbuff = nullptr;
    uint8_t *compressed_buff = nullptr;
    z_uintmax_t compressed_size = 0;

public:
    /* Real setup runs from Run() so each variant can pass its data type
       without the type needing to live in state.range. */
    void SetUp(::benchmark::State&) {}

    void DoSetUp(::benchmark::State& state, enum test_data_type data_type) {
        int err;
        uint32_t size = (uint32_t)state.range(0);
        outbuff = (uint8_t *)malloc(MAX_SIZE + 16);
        if (outbuff == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }

        // Initialize input buffer with the selected type of test data
        inbuff = gen_test_data(data_type, MAX_SIZE);
        if (inbuff == NULL) {
            free(outbuff);
            outbuff = NULL;
            state.SkipWithError("input data generator failed");
            return;
        }

        // Initialize Deflate state
        PREFIX3(stream) strm;
        strm.zalloc = NULL;
        strm.zfree = NULL;
        strm.opaque = NULL;
        strm.total_in = 0;
        strm.total_out = 0;
        strm.next_out = NULL;
        strm.avail_out = 0;

        err = PREFIX(deflateInit2)(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, -15, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
        if (err != Z_OK) {
            state.SkipWithError("deflateInit2 did not return Z_OK");
            return;
        }

        // Compress the size being benchmarked
        size_t buf_cap = (size_t)PREFIX(deflateBound)(&strm, size);
        compressed_buff = (uint8_t *)malloc(buf_cap);
        if (compressed_buff == NULL) {
            state.SkipWithError("malloc failed");
            PREFIX(deflateEnd)(&strm);
            return;
        }

        strm.avail_in = size;                       // Size of the input buffer
        strm.next_in = (z_const uint8_t *)inbuff;   // Pointer to the input buffer
        strm.next_out = compressed_buff;            // Pointer to the output buffer
        strm.avail_out = (uint32_t)buf_cap;         // Maximum size of the output buffer

        err = PREFIX(deflate)(&strm, Z_FINISH);     // Perform compression
        if (err != Z_STREAM_END) {
            state.SkipWithError("deflate did not return Z_STREAM_END");
            PREFIX(deflateEnd)(&strm);
            return;
        }

        compressed_size = strm.total_out;           // Total compressed size

        err = PREFIX(deflateEnd)(&strm);            // Clean up the deflate stream
        if (err != Z_OK) {
            state.SkipWithError("deflateEnd did not return Z_OK");
            return;
        }
    }

    void Bench(benchmark::State& state) {
        int err;

        // Initialize the inflate stream
        PREFIX3(stream) strm;
        strm.zalloc = NULL;
        strm.zfree = NULL;
        strm.opaque = NULL;
        strm.next_in = NULL;
        strm.avail_in = 0;

        err = PREFIX(inflateInit2)(&strm, -15);  // Initialize the inflate state, no crc/adler
        if (err != Z_OK) {
            state.SkipWithError("inflateInit did not return Z_OK");
            return;
        }

        for (auto _ : state) {
            // Perform reset, avoids benchmarking inflateInit and inflateEnd
            err = PREFIX(inflateReset)(&strm);
            if (err != Z_OK) {
                state.SkipWithError("inflateReset did not return Z_OK");
                return;
            }

            strm.avail_in = (uint32_t)compressed_size;  // Size of the input
            strm.next_in = compressed_buff;             // Pointer to the compressed data
            strm.avail_out = MAX_SIZE;                  // Max size for output
            strm.next_out = outbuff;                    // Output buffer

            // Perform decompression
            err = PREFIX(inflate)(&strm, Z_FINISH);
            if (err != Z_STREAM_END) {
                state.SkipWithError("inflate did not return Z_STREAM_END");
                PREFIX(inflateEnd)(&strm);
                return;
            }
        }

        // Finalize the inflation process
        err = PREFIX(inflateEnd)(&strm);
        if (err != Z_OK) {
            state.SkipWithError("inflateEnd did not return Z_OK");
            return;
        }
    }

    void Dispatch(benchmark::State& state, enum test_data_type data_type) {
        DoSetUp(state, data_type);
        if (state.skipped())
            return;
        Bench(state);
    }

    void TearDown(const ::benchmark::State&) {
        free(inbuff);
        free(outbuff);
        free(compressed_buff);
    }
};

#define INFLATE_SIZES_ARGS \
    ->Arg(1)->Arg(64)->Arg(1024)->Arg(16<<10)->Arg(128<<10)->Arg(1024<<10)

#define INFLATE_VARIANT(name, dt) \
    BENCHMARK_DEFINE_F(inflate_bench, name)(benchmark::State& state) { Dispatch(state, dt); } \
    BENCHMARK_REGISTER_F(inflate_bench, name)->Name("inflate_bench/nocrc/" #name) INFLATE_SIZES_ARGS

INFLATE_VARIANT(text,           TEST_DATA_TEXT);
INFLATE_VARIANT(short_match,    TEST_DATA_SHORT_MATCH);
INFLATE_VARIANT(dna,            TEST_DATA_DNA);
INFLATE_VARIANT(random,         TEST_DATA_RANDOM);
INFLATE_VARIANT(realistic_rgb,  TEST_DATA_REALISTIC_RGB);
INFLATE_VARIANT(striped_rgb,    TEST_DATA_STRIPED_RGB);
