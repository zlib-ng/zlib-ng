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
#  include "compressible_data_p.h"
}

#define MAX_SIZE (1024 * 1024)
#define NUM_TESTS 6

class inflate_bench: public benchmark::Fixture {
private:
    uint8_t *inbuff;
    uint8_t *outbuff;
    uint8_t *compressed_buff[NUM_TESTS];
    z_uintmax_t compressed_sizes[NUM_TESTS];
    uint32_t sizes[NUM_TESTS] = {1, 64, 1024, 16384, 128*1024, 1024*1024};

public:
    void SetUp(::benchmark::State& state) {
        int err;
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


        // Compress data into different buffers
        for (int i = 0; i < NUM_TESTS; ++i) {
            compressed_buff[i] = (uint8_t *)malloc(sizes[i] + 64);
            if (compressed_buff[i] == NULL) {
                state.SkipWithError("malloc failed");
                return;
            }

            strm.avail_in = sizes[i];                   // Size of the input buffer
            strm.next_in = (z_const uint8_t *)inbuff;   // Pointer to the input buffer
            strm.next_out = compressed_buff[i];         // Pointer to the output buffer
            strm.avail_out = sizes[i] + 64;             // Maximum size of the output buffer

            err = PREFIX(deflate)(&strm, Z_FINISH);     // Perform compression
            if (err != Z_STREAM_END ) {
                state.SkipWithError("deflate did not return Z_STREAM_END");
                PREFIX(deflateEnd)(&strm);
                return;
            }

            compressed_sizes[i] = strm.total_out;       // Total compressed size

            err = PREFIX(deflateReset)(&strm);                // Reset Deflate state
            if (err != Z_OK) {
                state.SkipWithError("deflateReset did not return Z_OK");
                return;
            }
        }

        err = PREFIX(deflateEnd)(&strm);                // Clean up the deflate stream
        if (err != Z_OK) {
            state.SkipWithError("deflateEnd did not return Z_OK");
            return;
        }
    }

    void Bench(benchmark::State& state) {
        int err;
        int index = 0;
        while (sizes[index] != (uint32_t)state.range(0)) ++index;

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

            strm.avail_in = (uint32_t)compressed_sizes[index];  // Size of the input
            strm.next_in = compressed_buff[index];              // Pointer to the compressed data
            strm.avail_out = MAX_SIZE;                          // Max size for output
            strm.next_out = outbuff;                            // Output buffer

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

    void TearDown(const ::benchmark::State&) {
        free(inbuff);
        free(outbuff);

        for (int i = 0; i < NUM_TESTS; ++i) {
            free(compressed_buff[i]);
        }
    }
};

#define BENCHMARK_INFLATE(name) \
    BENCHMARK_DEFINE_F(inflate_bench, name)(benchmark::State& state) { \
        Bench(state); \
    } \
    BENCHMARK_REGISTER_F(inflate_bench, name)->Arg(1)->Arg(64)->Arg(1024)->Arg(16<<10)->Arg(128<<10)->Arg(1024<<10);

BENCHMARK_INFLATE(inflate_nocrc);
