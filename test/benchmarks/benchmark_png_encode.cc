#include <stdio.h>
#include <assert.h>
#include <benchmark/benchmark.h>
#include "benchmark_png_shared.h"
#include "test/test_data_p.h"

#define IMWIDTH 1024
#define IMHEIGHT 1024

class png_encode: public benchmark::Fixture {
protected:
    png_dat outpng;

    /* Backing this on the heap is a more realistic benchmark */
    uint8_t *input_img_buf = NULL;
    uint8_t *realistic_img_buf = NULL;

public:
    void SetUp(const ::benchmark::State&) {
        input_img_buf = gen_test_data(TEST_DATA_STRIPED_RGB, IMWIDTH * IMHEIGHT * 3);
        realistic_img_buf = gen_test_data(TEST_DATA_REALISTIC_RGB, IMWIDTH * IMHEIGHT * 3);
        outpng.buf = (uint8_t*)malloc(IMWIDTH * IMHEIGHT * 3);
        /* Using malloc rather than zng_alloc so that we can call realloc.
         * IMWIDTH * IMHEIGHT is likely to be more than enough bytes, though,
         * given that a simple run length encoding already pretty much can
         * reduce to this */
        outpng.len = 0;
        outpng.buf_rem = IMWIDTH * IMHEIGHT * 3;
        assert(input_img_buf != NULL);
        assert(realistic_img_buf != NULL);
        assert(outpng.buf != NULL);
    }

    /* State in this circumstance will convey the compression level */
    void Bench(benchmark::State &state, uint8_t *img_buf, int32_t filters) {
        for (auto _ : state) {
            encode_png((png_bytep)img_buf, &outpng, state.range(0), filters, IMWIDTH, IMHEIGHT);
            outpng.buf_rem = outpng.len;
            outpng.len = 0;
        }
    }

    void TearDown(const ::benchmark::State &) {
        free(input_img_buf);
        free(realistic_img_buf);
        free(outpng.buf);
    }
};

BENCHMARK_DEFINE_F(png_encode, encode_compressible)(benchmark::State &state) {
    Bench(state, input_img_buf, PNG_FILTER_NONE);
}
BENCHMARK_REGISTER_F(png_encode, encode_compressible)->DenseRange(0, 9, 1)->Unit(benchmark::kMicrosecond);

/* libpng's default write path, filtered rows select Z_FILTERED in deflate. */
BENCHMARK_DEFINE_F(png_encode, encode_compressible_filtered)(benchmark::State &state) {
    Bench(state, realistic_img_buf, PNG_ALL_FILTERS);
}
BENCHMARK_REGISTER_F(png_encode, encode_compressible_filtered)->DenseRange(0, 9, 1)->Unit(benchmark::kMicrosecond);
