/* test_inflate_overlap.cc - Test inflate of overlapped matches at distances near the chunk size */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

#define UNCOMPR_SIZE (64 * 1024)

class inflate_overlap : public ::testing::TestWithParam<unsigned> {};

TEST_P(inflate_overlap, roundtrip) {
    unsigned period = GetParam();
    int err;

    uint8_t *uncompr = (uint8_t *)malloc(UNCOMPR_SIZE);
    ASSERT_NE(uncompr, nullptr);
    for (unsigned i = 0; i < period; i++)
        uncompr[i] = (uint8_t)(i * 37 + 11);
    for (unsigned i = period; i < UNCOMPR_SIZE; i++)
        uncompr[i] = uncompr[i - period];

    size_t compr_size = UNCOMPR_SIZE + 1024;
    uint8_t *compr = (uint8_t *)malloc(compr_size);
    ASSERT_NE(compr, nullptr);
    uint8_t *decompr = (uint8_t *)malloc(UNCOMPR_SIZE);
    ASSERT_NE(decompr, nullptr);

    PREFIX3(stream) c_stream;
    memset(&c_stream, 0, sizeof(c_stream));
    err = PREFIX(deflateInit2)(&c_stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
                               MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);
    c_stream.next_in = (z_const uint8_t *)uncompr;
    c_stream.avail_in = UNCOMPR_SIZE;
    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)compr_size;
    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    ASSERT_EQ(err, Z_STREAM_END);
    compr_size = c_stream.total_out;
    err = PREFIX(deflateEnd)(&c_stream);
    ASSERT_EQ(err, Z_OK);

    PREFIX3(stream) d_stream;
    memset(&d_stream, 0, sizeof(d_stream));
    err = PREFIX(inflateInit2)(&d_stream, -MAX_WBITS);
    ASSERT_EQ(err, Z_OK);
    d_stream.next_in = compr;
    d_stream.avail_in = (uint32_t)compr_size;
    d_stream.next_out = decompr;
    d_stream.avail_out = UNCOMPR_SIZE;
    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    ASSERT_EQ(err, Z_STREAM_END);
    ASSERT_EQ(d_stream.total_out, (z_uintmax_t)UNCOMPR_SIZE);
    err = PREFIX(inflateEnd)(&d_stream);
    ASSERT_EQ(err, Z_OK);

    EXPECT_EQ(memcmp(uncompr, decompr, UNCOMPR_SIZE), 0);

    free(uncompr);
    free(compr);
    free(decompr);
}

/* Cover every distance around the 16, 32, and 64 byte chunk sizes and their doubles */
INSTANTIATE_TEST_SUITE_P(periods, inflate_overlap, ::testing::Range(1u, 131u));
