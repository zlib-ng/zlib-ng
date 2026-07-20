/* test_inflate_back.cc - Test inflateBack() with safe mode match bailout
 * Verifies that inflateBack() correctly handles the MATCH state when
 * inflate_fast() bails out due to insufficient output space.
 */

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

/* Callback to provide all input at once */
static z_uint32_t pull_all(void *desc, z_const unsigned char **buf) {
    PREFIX3(stream) *strm = (PREFIX3(stream) *)desc;
    if (strm->avail_in == 0)
        return 0;
    *buf = strm->next_in;
    unsigned avail = strm->avail_in;
    strm->avail_in = 0;
    return avail;
}

struct output_state {
    uint8_t *buf;
    size_t size;
    size_t pos;
};

/* Callback to collect all output */
static z_int32_t push_all(void *desc, unsigned char *buf, z_uint32_t len) {
    struct output_state *out = (struct output_state *)desc;
    if (out->pos + len > out->size)
        return 1;  /* overflow */
    memcpy(out->buf + out->pos, buf, len);
    out->pos += len;
    return 0;
}

class inflate_back : public ::testing::Test {
protected:
    static const int wbits = 9;
    static const uint32_t wsize = 1U << wbits;
    static const uint32_t data_size = wsize * 64;

    uint8_t *window = nullptr;
    uint8_t *decompressed = nullptr;
    uint8_t *original = nullptr;
    uint8_t *compressed = nullptr;

    void SetUp() override {
        window = (uint8_t *)malloc(wsize);
        decompressed = (uint8_t *)malloc(data_size);
        original = (uint8_t *)malloc(data_size);
        ASSERT_NE(window, nullptr);
        ASSERT_NE(decompressed, nullptr);
        ASSERT_NE(original, nullptr);

        /* Generate data that triggers MATCH bailout */
        /* 5-byte repeating pattern -> len=258 dist=5 tokens */
        for (uint32_t i = 0; i < data_size; i++)
            original[i] = (uint8_t)(i % 5);
    }

    void TearDown() override {
        free(compressed);
        free(original);
        free(decompressed);
        free(window);
    }
};

/* Test that inflateBack() handles the MATCH state when inflate_fast() bails
 * out due to insufficient output space. */
TEST_F(inflate_back, match_state) {
    int err;

    /* Compress with raw deflate (no zlib/gzip header) */
    PREFIX3(stream) c_strm;
    memset(&c_strm, 0, sizeof(c_strm));
    err = PREFIX(deflateInit2)(&c_strm, Z_BEST_COMPRESSION, Z_DEFLATED,
                               -wbits, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);

    z_uintmax_t comp_bound = PREFIX(deflateBound)(&c_strm, data_size);
    compressed = (uint8_t *)malloc(comp_bound);
    ASSERT_NE(compressed, nullptr);

    c_strm.next_in = original;
    c_strm.avail_in = data_size;
    c_strm.next_out = compressed;
    c_strm.avail_out = (uint32_t)comp_bound;

    err = PREFIX(deflate)(&c_strm, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);
    uint32_t compressed_size = (uint32_t)c_strm.total_out;
    PREFIX(deflateEnd)(&c_strm);

    /* Decompress with inflateBack() */
    PREFIX3(stream) d_strm;
    memset(&d_strm, 0, sizeof(d_strm));

    err = PREFIX(inflateBackInit)(&d_strm, wbits, window);
    ASSERT_EQ(err, Z_OK);

    d_strm.next_in = compressed;
    d_strm.avail_in = compressed_size;

    struct output_state out_state;
    out_state.buf = decompressed;
    out_state.size = data_size;
    out_state.pos = 0;

    err = PREFIX(inflateBack)(&d_strm, pull_all, &d_strm, push_all, &out_state);
    EXPECT_EQ(err, Z_STREAM_END);

    PREFIX(inflateBackEnd)(&d_strm);

    /* Verify decompressed output matches original */
    EXPECT_EQ(out_state.pos, (size_t)data_size);
    EXPECT_EQ(memcmp(decompressed, original, data_size), 0);
}

/* Test that inflateBack() rejects a distance that reaches past its window when
 * inflate_fast() bails out to the MATCH state, instead of copying from before
 * the caller-supplied window. */
TEST_F(inflate_back, match_state_invalid_distance) {
    int err;
    /* Prefix length is chosen so the repeat below starts late in a window pass,
     * leaving fewer than 258 bytes of window space and forcing inflate_fast()
     * into its MATCH bailout. */
    const uint32_t small_wbits = 8;
    const uint32_t small_wsize = 1U << small_wbits;
    const uint32_t prefix = small_wsize + 144;
    const uint32_t src_size = prefix + 258 + 42;

    /* Unpredictable prefix so the first long-distance match is the one below,
     * then a 258-byte repeat of the start, giving dist > wsize. */
    uint32_t seed = 0x12345678;
    for (uint32_t i = 0; i < src_size; i++) {
        seed = seed * 1103515245 + 12345;
        original[i] = (uint8_t)(seed >> 16);
    }
    memcpy(original + prefix, original, 258);

    /* Compress with a full-size window so the encoder may use the long distance */
    PREFIX3(stream) c_strm;
    memset(&c_strm, 0, sizeof(c_strm));
    err = PREFIX(deflateInit2)(&c_strm, Z_BEST_COMPRESSION, Z_DEFLATED,
                               -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);

    z_uintmax_t comp_bound = PREFIX(deflateBound)(&c_strm, src_size);
    compressed = (uint8_t *)malloc(comp_bound);
    ASSERT_NE(compressed, nullptr);

    c_strm.next_in = original;
    c_strm.avail_in = src_size;
    c_strm.next_out = compressed;
    c_strm.avail_out = (uint32_t)comp_bound;

    err = PREFIX(deflate)(&c_strm, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);
    uint32_t compressed_size = (uint32_t)c_strm.total_out;
    PREFIX(deflateEnd)(&c_strm);

    /* Decompress with a window too small for that distance */
    PREFIX3(stream) d_strm;
    memset(&d_strm, 0, sizeof(d_strm));

    uint8_t *small_window = (uint8_t *)malloc(small_wsize);
    ASSERT_NE(small_window, nullptr);

    err = PREFIX(inflateBackInit)(&d_strm, small_wbits, small_window);
    ASSERT_EQ(err, Z_OK);

    d_strm.next_in = compressed;
    d_strm.avail_in = compressed_size;

    struct output_state out_state;
    out_state.buf = decompressed;
    out_state.size = data_size;
    out_state.pos = 0;

    err = PREFIX(inflateBack)(&d_strm, pull_all, &d_strm, push_all, &out_state);
    EXPECT_EQ(err, Z_DATA_ERROR);

    PREFIX(inflateBackEnd)(&d_strm);
    free(small_window);
}
