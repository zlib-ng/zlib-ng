#include <stdio.h>
#include <assert.h>
#include <benchmark/benchmark.h>

#include "benchmark_png_shared.h"

/* Decode PNGs through libpng at various image widths. libpng calls inflate() with
 * avail_out equal to one row (width * 3 bytes for RGB). Narrow images produce rows
 * below the 260-byte inflate_fast threshold, exercising the slow path on every row. */
class png_decode_small: public benchmark::Fixture {
protected:
    png_dat encoded = {NULL, 0, 0};
    uint8_t *output_buf = NULL;
    uint32_t img_width = 0;
    uint32_t img_height = 0;

public:
    void SetUp(const ::benchmark::State& state) {
        img_width = (uint32_t)state.range(0);
        img_height = (1024 * 1024) / img_width;

        size_t num_pixels = (size_t)img_width * img_height;

        output_buf = (uint8_t *)malloc(num_pixels * 3);
        assert(output_buf != NULL);
        init_realistic(output_buf, img_width, img_height);

        encoded = {NULL, 0, 0};
        encode_png(output_buf, &encoded, 9, img_width, img_height);
    }

    void Bench(benchmark::State &state) {
        size_t buf_size = (size_t)img_width * img_height * 3;
        for (auto _ : state) {
            png_parse_dat in = { encoded.buf };
            uint32_t w, h;
            decode_png(&in, (png_bytepp)&output_buf, buf_size, w, h);
        }
    }

    void TearDown(const ::benchmark::State &) {
        free(output_buf);
        free(encoded.buf);
    }
};

BENCHMARK_DEFINE_F(png_decode_small, png_decode_small)(benchmark::State &state) {
    Bench(state);
}
BENCHMARK_REGISTER_F(png_decode_small, png_decode_small)
    /* width in pixels: row bytes = width * 3
     *   32 ->  96 bytes/row (well below 260)
     *   64 -> 192 bytes/row (below 260)
     *   86 -> 258 bytes/row (just below 260)
     *  128 -> 384 bytes/row (above 260, but tail in slow path)
     *  256 -> 768 bytes/row
     * 1024 -> 3072 bytes/row (reference, minimal slow-path impact) */
    ->Arg(32)->Arg(64)->Arg(86)->Arg(128)->Arg(256)->Arg(1024)
    ->Unit(benchmark::kMicrosecond);
