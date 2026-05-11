#include <stdio.h>
#include <assert.h>
#include <benchmark/benchmark.h>
#include "benchmark_png_shared.h"
#include "test/test_data_p.h"

#define IMWIDTH 1024
#define IMHEIGHT 1024

class png_encode: public benchmark::Fixture {
private:
    png_dat outpng;

    /* Backing this on the heap is a more realistic benchmark */
    uint8_t *input_img_buf = NULL;

public:
    void SetUp(const ::benchmark::State&) {
        input_img_buf = gen_test_data(TEST_DATA_STRIPED_RGB, IMWIDTH * IMHEIGHT * 3);
        outpng.buf = (uint8_t*)malloc(IMWIDTH * IMHEIGHT * 3);
        /* Using malloc rather than zng_alloc so that we can call realloc.
         * IMWIDTH * IMHEIGHT is likely to be more than enough bytes, though,
         * given that a simple run length encoding already pretty much can
         * reduce to this */
        outpng.len = 0;
        outpng.buf_rem = IMWIDTH * IMHEIGHT * 3;
        assert(input_img_buf != NULL);
        assert(outpng.buf != NULL);
    }

    /* State in this circumstance will convey the compression level */
    void Bench(benchmark::State &state) {
        for (auto _ : state) {
            encode_png((png_bytep)input_img_buf, &outpng, state.range(0), IMWIDTH, IMHEIGHT);
            outpng.buf_rem = outpng.len;
            outpng.len = 0;
        }
    }

    void TearDown(const ::benchmark::State &) {
        free(input_img_buf);
        free(outpng.buf);
    }
};

BENCHMARK_DEFINE_F(png_encode, encode_compressible)(benchmark::State &state) {
    Bench(state);
}
BENCHMARK_REGISTER_F(png_encode, encode_compressible)->DenseRange(0, 9, 1)->Unit(benchmark::kMicrosecond);
