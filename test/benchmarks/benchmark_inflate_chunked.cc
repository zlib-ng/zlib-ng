/* benchmark_inflate_chunked.cc -- benchmark inflate() with various output buffer sizes
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

#define TOTAL_SIZE (256 * 1024)

class inflate_chunked: public benchmark::Fixture {
private:
    uint8_t *inbuff = nullptr;
    uint8_t *outbuff = nullptr;
    uint8_t *compressed = nullptr;
    z_uintmax_t compressed_size;

public:
    void SetUp(::benchmark::State& state) {
        int err;
        outbuff = (uint8_t *)malloc(TOTAL_SIZE);
        if (outbuff == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }

        inbuff = gen_compressible_data(TOTAL_SIZE);
        if (inbuff == NULL) {
            free(outbuff);
            outbuff = NULL;
            state.SkipWithError("gen_compressible_data() failed");
            return;
        }

        PREFIX3(stream) strm;
        strm.zalloc = NULL;
        strm.zfree = NULL;
        strm.opaque = NULL;
        strm.total_in = 0;
        strm.total_out = 0;

        err = PREFIX(deflateInit2)(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, -15, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
        if (err != Z_OK) {
            state.SkipWithError("deflateInit2 failed");
            return;
        }

        compressed_size = PREFIX(deflateBound)(&strm, TOTAL_SIZE);
        compressed = (uint8_t *)malloc(compressed_size);
        if (compressed == NULL) {
            state.SkipWithError("malloc failed");
            PREFIX(deflateEnd)(&strm);
            return;
        }

        strm.avail_in = TOTAL_SIZE;
        strm.next_in = (z_const uint8_t *)inbuff;
        strm.next_out = compressed;
        strm.avail_out = (uint32_t)compressed_size;

        err = PREFIX(deflate)(&strm, Z_FINISH);
        if (err != Z_STREAM_END) {
            state.SkipWithError("deflate did not return Z_STREAM_END");
            PREFIX(deflateEnd)(&strm);
            return;
        }

        compressed_size = strm.total_out;
        PREFIX(deflateEnd)(&strm);
    }

    void Bench(benchmark::State& state) {
        int err;
        uint32_t max_chunk_size = (uint32_t)state.range(0);

        PREFIX3(stream) strm;
        strm.zalloc = NULL;
        strm.zfree = NULL;
        strm.opaque = NULL;
        strm.next_in = NULL;
        strm.avail_in = 0;

        err = PREFIX(inflateInit2)(&strm, -15);
        if (err != Z_OK) {
            state.SkipWithError("inflateInit2 failed");
            return;
        }

        for (auto _ : state) {
            err = PREFIX(inflateReset)(&strm);
            if (err != Z_OK) {
                state.SkipWithError("inflateReset failed");
                return;
            }

            strm.avail_in = (uint32_t)compressed_size;
            strm.next_in = compressed;

            /* Inflate chunk-by-chunk */
            uint8_t *out_pos = outbuff;
            uint32_t bytes_left = TOTAL_SIZE;

            while (bytes_left > 0) {
                uint32_t chunk_size = MIN(bytes_left, max_chunk_size);
                strm.next_out = out_pos;
                strm.avail_out = chunk_size;

                err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
                if (err == Z_STREAM_END)
                    break;
                if (err != Z_OK) {
                    state.SkipWithError("inflate failed");
                    PREFIX(inflateEnd)(&strm);
                    return;
                }

                uint32_t processed = chunk_size - strm.avail_out;
                if (processed == 0) {
                    state.SkipWithError("inflate made no progress");
                    PREFIX(inflateEnd)(&strm);
                    return;
                }
                out_pos += processed;
                bytes_left -= processed;
            }

            if (err != Z_STREAM_END || strm.total_out != TOTAL_SIZE) {
                state.SkipWithError("inflate did not produce the full output");
                PREFIX(inflateEnd)(&strm);
                return;
            }
        }

        PREFIX(inflateEnd)(&strm);
    }

    void TearDown(const ::benchmark::State&) {
        free(inbuff);
        free(outbuff);
        free(compressed);
    }
};

BENCHMARK_DEFINE_F(inflate_chunked, small_output_buf)(benchmark::State& state) {
    Bench(state);
}
/* Arg values are avail_out per inflate() call, simulating PNG row widths */
BENCHMARK_REGISTER_F(inflate_chunked, small_output_buf)
    ->Arg(64)->Arg(128)->Arg(256)->Arg(512)
    ->Arg(1024)->Arg(2048)->Arg(4096)->Arg(16384)
    ->Arg(65536)->Arg(131072)->Arg(262144);
