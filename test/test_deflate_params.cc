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
#include "test_data_p.h"

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

/* Level 0 fills the window buffer, which is larger than 2 * w_size for small windows. The next
 * matching level must refill from that position without reading past the buffer. */
static void deflate_params_from_stored(int window_bits, int to_level) {
    const uint32_t len = 128 * 1024;
    const uint32_t compr_size = 2 * len;

    uint8_t *uncompr = (uint8_t *)calloc(1, len);
    uint8_t *compr = (uint8_t *)calloc(1, compr_size);
    uint8_t *check = (uint8_t *)calloc(1, len);
    ASSERT_TRUE(uncompr != NULL && compr != NULL && check != NULL);

    fill_random(uncompr, len);

    PREFIX3(stream) c_stream;
    memset(&c_stream, 0, sizeof(c_stream));
    EXPECT_EQ(PREFIX(deflateInit2)(&c_stream, 0, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY), Z_OK);

    /* A small output buffer keeps level 0 from copying straight through, so input fills the
     * window */
    c_stream.next_out = compr;
    c_stream.avail_out = 16;
    c_stream.next_in = uncompr;
    c_stream.avail_in = len;
    EXPECT_EQ(PREFIX(deflate)(&c_stream, Z_NO_FLUSH), Z_OK);

    /* Hold back the rest so the switch leaves level 0's data in the window */
    uint32_t remaining = c_stream.avail_in;
    c_stream.avail_in = 0;
    c_stream.avail_out = compr_size - (uint32_t)(c_stream.next_out - compr);
    EXPECT_EQ(PREFIX(deflateParams)(&c_stream, to_level, Z_DEFAULT_STRATEGY), Z_OK);

    c_stream.avail_in = remaining;
    EXPECT_EQ(PREFIX(deflate)(&c_stream, Z_FINISH), Z_STREAM_END);

    uint32_t compr_len = (uint32_t)(c_stream.next_out - compr);
    EXPECT_EQ(PREFIX(deflateEnd)(&c_stream), Z_OK);

    PREFIX3(stream) d_stream;
    memset(&d_stream, 0, sizeof(d_stream));
    EXPECT_EQ(PREFIX(inflateInit2)(&d_stream, window_bits), Z_OK);
    d_stream.next_in = compr;
    d_stream.avail_in = compr_len;
    d_stream.next_out = check;
    d_stream.avail_out = len;
    EXPECT_EQ(PREFIX(inflate)(&d_stream, Z_FINISH), Z_STREAM_END);
    EXPECT_EQ(d_stream.total_out, len);
    EXPECT_EQ(memcmp(uncompr, check, len), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&d_stream), Z_OK);

    free(uncompr);
    free(compr);
    free(check);
}

TEST(deflate, params_from_stored) {
    for (int window_bits = 9; window_bits <= 15; window_bits++) {
        for (int level : {1, 2, 6, 9}) {
            SCOPED_TRACE(testing::Message() << "window bits " << window_bits << ", level " << level);
            deflate_params_from_stored(window_bits, level);
        }
    }
}

/* Switching from a faster strategy (levels 2-6) to deflate_slow (levels 7-8) after
 * deflateReset() must rebuild prev[] so longest_match_slow() does not follow stale
 * links left by the earlier stream. */
static void deflate_params_to_slow_after_reset(int from_level, int to_level) {
    const uint32_t total = 64 * 1024;
    const uint32_t split = 16 * 1024;
    const uint32_t out_size = 2 * total;

    uint8_t *input = gen_text_data(total);
    uint8_t *fresh_out = (uint8_t *)malloc(out_size);
    uint8_t *reused_out = (uint8_t *)malloc(out_size);
    uint8_t *decoded = (uint8_t *)malloc(total);
    ASSERT_TRUE(input != NULL && fresh_out != NULL && reused_out != NULL && decoded != NULL);

    auto compress_switch = [&](PREFIX3(stream) *strm, uint8_t *out, uint32_t *out_len) {
        strm->next_in = input;
        strm->avail_in = split;
        strm->next_out = out;
        strm->avail_out = out_size;
        EXPECT_EQ(PREFIX(deflate)(strm, Z_NO_FLUSH), Z_OK);
        EXPECT_EQ(strm->avail_in, 0u);

        EXPECT_EQ(PREFIX(deflateParams)(strm, to_level, Z_DEFAULT_STRATEGY), Z_OK);

        strm->next_in = input + split;
        strm->avail_in = total - split;
        EXPECT_EQ(PREFIX(deflate)(strm, Z_FINISH), Z_STREAM_END);
        *out_len = (uint32_t)(strm->next_out - out);
    };

    PREFIX3(stream) fresh = {};
    ASSERT_EQ(PREFIX(deflateInit2)(&fresh, from_level, Z_DEFLATED, MAX_WBITS, 8,
                                   Z_DEFAULT_STRATEGY), Z_OK);
    uint32_t fresh_len = 0;
    compress_switch(&fresh, fresh_out, &fresh_len);
    EXPECT_EQ(PREFIX(deflateEnd)(&fresh), Z_OK);

    PREFIX3(stream) reused = {};
    ASSERT_EQ(PREFIX(deflateInit2)(&reused, from_level, Z_DEFLATED, MAX_WBITS, 8,
                                   Z_DEFAULT_STRATEGY), Z_OK);
    reused.next_in = input;
    reused.avail_in = total;
    reused.next_out = reused_out;
    reused.avail_out = out_size;
    EXPECT_EQ(PREFIX(deflate)(&reused, Z_FINISH), Z_STREAM_END);

    EXPECT_EQ(PREFIX(deflateReset)(&reused), Z_OK);
    uint32_t reused_len = 0;
    compress_switch(&reused, reused_out, &reused_len);
    EXPECT_EQ(PREFIX(deflateEnd)(&reused), Z_OK);

    EXPECT_EQ(reused_len, fresh_len);
    if (reused_len == fresh_len)
        EXPECT_EQ(memcmp(reused_out, fresh_out, fresh_len), 0);

    PREFIX3(stream) decoder = {};
    ASSERT_EQ(PREFIX(inflateInit)(&decoder), Z_OK);
    decoder.next_in = reused_out;
    decoder.avail_in = reused_len;
    decoder.next_out = decoded;
    decoder.avail_out = total;
    EXPECT_EQ(PREFIX(inflate)(&decoder, Z_FINISH), Z_STREAM_END);
    EXPECT_EQ(decoder.total_out, total);
    EXPECT_EQ(memcmp(decoded, input, total), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&decoder), Z_OK);

    free(input);
    free(fresh_out);
    free(reused_out);
    free(decoded);
}

TEST(deflate, params_to_slow_after_reset) {
    for (int from_level : {2, 3, 4, 5, 6}) {
        for (int to_level : {7, 8}) {
            SCOPED_TRACE(testing::Message() << "from level " << from_level
                                            << " to level " << to_level);
            deflate_params_to_slow_after_reset(from_level, to_level);
        }
    }
}
