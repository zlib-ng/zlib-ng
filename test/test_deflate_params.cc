/* test_deflate_params.cc - Test deflate() with dynamic change of compression level */

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
#include <inttypes.h>

#include "deflate.h"

#include <gtest/gtest.h>

#include "test_shared.h"

#define COMPR_BUFFER_SIZE (48 * 1024)
#define UNCOMPR_BUFFER_SIZE (64 * 1024)
#define UNCOMPR_RAND_SIZE (8 * 1024)

/* Fill a buffer with deterministic, effectively incompressible bytes. */
static void fill_random(uint8_t *buf, uint32_t len) {
    uint32_t r = 0xa5a5a5a5u;
    for (uint32_t i = 0; i < len; i++) {
        r = r * 1664525u + 1013904223u;
        buf[i] = (uint8_t)(r >> 24);
    }
}

TEST(deflate, params) {
    PREFIX3(stream) c_stream, d_stream;
    uint8_t *compr, *uncompr;
    uint32_t compr_len, uncompr_len;
    uint32_t diff;
    int err;
#ifndef ZLIB_COMPAT
    int level = -1;
    int strategy = -1;
    zng_deflate_param_value params[2];

    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &level;
    params[0].size = sizeof(level);

    params[1].param = Z_DEFLATE_STRATEGY;
    params[1].buf = &strategy;
    params[1].size = sizeof(strategy);
#endif

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&d_stream, 0, sizeof(d_stream));

    compr = (uint8_t *)calloc(1, COMPR_BUFFER_SIZE);
    ASSERT_TRUE(compr != NULL);
    uncompr = (uint8_t *)calloc(1, UNCOMPR_BUFFER_SIZE);
    ASSERT_TRUE(uncompr != NULL);

    compr_len = COMPR_BUFFER_SIZE;
    uncompr_len = UNCOMPR_BUFFER_SIZE;

    fill_random(uncompr, UNCOMPR_RAND_SIZE);

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_SPEED);
    EXPECT_EQ(err, Z_OK);

    c_stream.next_out = compr;
    c_stream.avail_out = compr_len;
    c_stream.next_in = uncompr;
    c_stream.avail_in = uncompr_len;

    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    EXPECT_EQ(err, Z_OK);
    EXPECT_EQ(c_stream.avail_in, 0);

    /* Feed in already compressed data and switch to no compression: */
#ifndef ZLIB_COMPAT
    zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
    EXPECT_EQ(level, Z_BEST_SPEED);
    EXPECT_EQ(strategy, Z_DEFAULT_STRATEGY);

    level = Z_NO_COMPRESSION;
    strategy = Z_DEFAULT_STRATEGY;
    zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
#else
    PREFIX(deflateParams)(&c_stream, Z_NO_COMPRESSION, Z_DEFAULT_STRATEGY);
#endif

    c_stream.next_in = compr;
    diff = (unsigned int)(c_stream.next_out - compr);
    c_stream.avail_in = diff;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    EXPECT_EQ(err, Z_OK);

    /* Switch back to compressing mode: */
#ifndef ZLIB_COMPAT
    level = -1;
    strategy = -1;
    zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
    EXPECT_EQ(level, Z_NO_COMPRESSION);
    EXPECT_EQ(strategy, Z_DEFAULT_STRATEGY);

    level = Z_BEST_COMPRESSION;
    strategy = Z_FILTERED;
    zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
#else
    PREFIX(deflateParams)(&c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
#endif

    c_stream.next_in = uncompr;
    c_stream.avail_in = (unsigned int)uncompr_len;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    EXPECT_EQ(err, Z_OK);

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);

    err = PREFIX(deflateEnd)(&c_stream);
    EXPECT_EQ(err, Z_OK);

    d_stream.next_in  = compr;
    d_stream.avail_in = (unsigned int)compr_len;

    err = PREFIX(inflateInit)(&d_stream);
    EXPECT_EQ(err, Z_OK);

    do {
        d_stream.next_out = uncompr;            /* discard the output */
        d_stream.avail_out = uncompr_len;
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END)
            break;
        EXPECT_EQ(err, Z_OK);
    } while (err == Z_OK);

    err = PREFIX(inflateEnd)(&d_stream);
    EXPECT_EQ(err, Z_OK);

    EXPECT_EQ(d_stream.total_out, (2 * uncompr_len) + diff);

    free(compr);
    free(uncompr);
}

/* deflateParams must preserve match history when the params change mid-stream.
 * The second half of the input is an exact copy of the first and sits within
 * the 32K window, so preserved history turns it into cross-boundary matches.
 * If the chains were dropped at the switch the copy is re-emitted and the
 * stream nearly doubles. */
static void deflate_params_history(int from_level, int from_strategy, int to_level, int to_strategy) {
    const uint32_t half = 16 * 1024;
    const uint32_t len = 2 * half;

    uint8_t *uncompr = (uint8_t *)calloc(1, len);
    uint8_t *compr = (uint8_t *)calloc(1, 2 * len);
    uint8_t *check = (uint8_t *)calloc(1, len);
    ASSERT_TRUE(uncompr != NULL && compr != NULL && check != NULL);

    fill_random(uncompr, half);
    memcpy(uncompr + half, uncompr, half);

    PREFIX3(stream) c_stream;
    memset(&c_stream, 0, sizeof(c_stream));
    EXPECT_EQ(PREFIX(deflateInit2)(&c_stream, from_level, Z_DEFLATED, 15, 8, from_strategy), Z_OK);

    c_stream.next_out = compr;
    c_stream.avail_out = 2 * len;
    c_stream.next_in = uncompr;
    c_stream.avail_in = half;
    EXPECT_EQ(PREFIX(deflate)(&c_stream, Z_NO_FLUSH), Z_OK);
    EXPECT_EQ(c_stream.avail_in, 0u);

    EXPECT_EQ(PREFIX(deflateParams)(&c_stream, to_level, to_strategy), Z_OK);

    c_stream.next_in = uncompr + half;
    c_stream.avail_in = half;
    EXPECT_EQ(PREFIX(deflate)(&c_stream, Z_FINISH), Z_STREAM_END);

    uint32_t compr_len = (uint32_t)(c_stream.next_out - compr);
    EXPECT_EQ(PREFIX(deflateEnd)(&c_stream), Z_OK);

    PREFIX3(stream) d_stream;
    memset(&d_stream, 0, sizeof(d_stream));
    EXPECT_EQ(PREFIX(inflateInit)(&d_stream), Z_OK);
    d_stream.next_in = compr;
    d_stream.avail_in = compr_len;
    d_stream.next_out = check;
    d_stream.avail_out = len;
    EXPECT_EQ(PREFIX(inflate)(&d_stream, Z_FINISH), Z_STREAM_END);
    EXPECT_EQ(d_stream.total_out, len);
    EXPECT_EQ(memcmp(uncompr, check, len), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&d_stream), Z_OK);

    EXPECT_LT(compr_len, half + half / 2);

    free(uncompr);
    free(compr);
    free(check);
}

TEST(deflate, params_history_level9_down) {
    deflate_params_history(9, Z_DEFAULT_STRATEGY, 6, Z_DEFAULT_STRATEGY);
}

TEST(deflate, params_history_level9_up) {
    deflate_params_history(6, Z_DEFAULT_STRATEGY, 9, Z_DEFAULT_STRATEGY);
}

/* Switching from a hashless strategy (Z_RLE/Z_HUFFMAN_ONLY) back to a hashing one
 * at the same level must also rebuild, since those strategies never insert. */
TEST(deflate, params_history_rle) {
    deflate_params_history(6, Z_RLE, 6, Z_DEFAULT_STRATEGY);
}

TEST(deflate, params_history_huffman) {
    deflate_params_history(6, Z_HUFFMAN_ONLY, 6, Z_DEFAULT_STRATEGY);
}
