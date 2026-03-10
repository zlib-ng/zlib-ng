/* test_deflate.cc -- Test deflate/inflate round-trip with various levels, sizes, and strategies */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

#include "compressible_data_p.h"
#include "test_shared.h"

#define MAX_SIZE (1024 * 1024)

class deflate_variant : public testing::TestWithParam<std::tuple<size_t, int, int, int>> {
public:
    static void SetUpTestSuite() {
        inbuf = gen_compressible_data(MAX_SIZE);
        ASSERT_TRUE(inbuf != NULL);
    }
    static void TearDownTestSuite() {
        free(inbuf);
        inbuf = NULL;
    }
    void SetUp() override {
        compbuf = NULL;
        decompbuf = NULL;
    }
    void TearDown() override {
        free(compbuf);
        free(decompbuf);
    }

    static uint8_t *inbuf;
    uint8_t *compbuf;
    uint8_t *decompbuf;
};

uint8_t *deflate_variant::inbuf = NULL;

TEST_P(deflate_variant, round_trip) {
    size_t size = std::get<0>(GetParam());
    int level = std::get<1>(GetParam());
    int window_bits = std::get<2>(GetParam());
    int strategy = std::get<3>(GetParam());
    PREFIX3(stream) c_stream, d_stream;
    int err;

    /* Allocate output buffer */
    unsigned long compbuf_size = PREFIX(deflateBound)(NULL, (unsigned long)size);
    compbuf = (uint8_t *)malloc((size_t)compbuf_size);
    ASSERT_TRUE(compbuf != NULL);

    /* Compress */
    memset(&c_stream, 0, sizeof(c_stream));
    err = PREFIX(deflateInit2)(&c_stream, level, Z_DEFLATED,
                               window_bits, MAX_MEM_LEVEL, strategy);
    ASSERT_EQ(err, Z_OK) << "deflateInit2 failed";

    c_stream.next_in = inbuf;
    c_stream.avail_in = (uint32_t)size;
    c_stream.next_out = compbuf;
    c_stream.avail_out = (uint32_t)compbuf_size;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    ASSERT_EQ(err, Z_STREAM_END) <<
        "size: " << size << "\n" <<
        "level: " << level << "\n" <<
        "window_bits: " << window_bits << "\n" <<
        "strategy: " << strategy;

    size_t comp_size = (size_t)c_stream.total_out;

    err = PREFIX(deflateEnd)(&c_stream);
    EXPECT_EQ(err, Z_OK);

    /* Decompress and verify round-trip */
    decompbuf = (uint8_t *)malloc(size);
    ASSERT_TRUE(decompbuf != NULL);

    memset(&d_stream, 0, sizeof(d_stream));
    err = PREFIX(inflateInit2)(&d_stream, window_bits);
    ASSERT_EQ(err, Z_OK) << "inflateInit2 failed";

    d_stream.next_in = compbuf;
    d_stream.avail_in = (uint32_t)comp_size;
    d_stream.next_out = decompbuf;
    d_stream.avail_out = (uint32_t)size;

    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    ASSERT_EQ(err, Z_STREAM_END) <<
        "size: " << size << "\n" <<
        "level: " << level << "\n" <<
        "window_bits: " << window_bits << "\n" <<
        "strategy: " << strategy;

    EXPECT_EQ(d_stream.total_in, comp_size);
    EXPECT_EQ(d_stream.total_out, size);
    EXPECT_EQ(memcmp(inbuf, decompbuf, size), 0) << "round-trip data mismatch";

    err = PREFIX(inflateEnd)(&d_stream);
    EXPECT_EQ(err, Z_OK);
}

static std::string deflate_test_name(const testing::TestParamInfo<deflate_variant::ParamType> &info) {
    size_t size = std::get<0>(info.param);
    int level = std::get<1>(info.param);
    int window_bits = std::get<2>(info.param);
    int strategy = std::get<3>(info.param);

    char size_str[32];
    if (size >= 1048576 && size % 1048576 == 0)
        snprintf(size_str, sizeof(size_str), "%zuM", size / 1048576);
    else if (size >= 1024 && size % 1024 == 0)
        snprintf(size_str, sizeof(size_str), "%zuK", size / 1024);
    else
        snprintf(size_str, sizeof(size_str), "%zu", size);

    const char *wrap_str = (window_bits < 0) ? "raw" : "zlib";

    const char *strategy_str;
    switch (strategy) {
        case Z_FILTERED:     strategy_str = "filtered"; break;
        case Z_HUFFMAN_ONLY: strategy_str = "huffman"; break;
        case Z_RLE:          strategy_str = "rle"; break;
        case Z_FIXED:        strategy_str = "fixed"; break;
        default:             strategy_str = "default"; break;
    }

    char name[64];
    snprintf(name, sizeof(name), "%s_level%d_%s_%s", size_str, level, wrap_str, strategy_str);
    return name;
}

INSTANTIATE_TEST_SUITE_P(deflate, deflate_variant,
    testing::Combine(
        testing::Values(1024, 16384, 131072, 1048576),          /* size */
        testing::Values(1, 3, 6, 9),                            /* level */
        testing::Values(MAX_WBITS, -MAX_WBITS),                 /* window_bits */
        testing::Values(Z_DEFAULT_STRATEGY, Z_FILTERED,
                        Z_HUFFMAN_ONLY, Z_RLE, Z_FIXED)         /* strategy */
    ),
    deflate_test_name);
