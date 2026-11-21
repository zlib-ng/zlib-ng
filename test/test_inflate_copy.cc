/* test_inflate_copy.cc - Test copying inflate stream */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include "test_shared.h"

#include <gtest/gtest.h>

TEST(inflate, copy_back_and_forth) {
    PREFIX3(stream) d1_stream, d2_stream;
    int err;

    memset(&d1_stream, 0, sizeof(d1_stream));
    err = PREFIX(inflateInit2)(&d1_stream, MAX_WBITS + 14);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateCopy)(&d2_stream, &d1_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateEnd)(&d1_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateCopy)(&d1_stream, &d2_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateEnd)(&d1_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateEnd)(&d2_stream);
    ASSERT_EQ(err, Z_OK);
}

/* Copy a stream that is stopped inside a block and let the copy finish the decode.
   Z_FIXED leaves the fixed code tables in use, Z_DEFAULT_STRATEGY the dynamic ones, so
   both table provenances reach inflateCopy(). */
static void copy_mid_block(int strategy) {
    PREFIX3(stream) c_stream, d_stream, d_copy;
    uint8_t plain[4096], compr[8192], uncompr[4096];
    int err;

    for (size_t i = 0; i < sizeof(plain); i++)
        plain[i] = (uint8_t)(i % 251);

    memset(&c_stream, 0, sizeof(c_stream));
    err = PREFIX(deflateInit2)(&c_stream, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, strategy);
    ASSERT_EQ(err, Z_OK);
    c_stream.next_in = plain;
    c_stream.avail_in = (uint32_t)sizeof(plain);
    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)sizeof(compr);
    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    ASSERT_EQ(err, Z_STREAM_END);
    uint32_t compr_len = (uint32_t)(sizeof(compr) - c_stream.avail_out);
    err = PREFIX(deflateEnd)(&c_stream);
    ASSERT_EQ(err, Z_OK);

    memset(&d_stream, 0, sizeof(d_stream));
    err = PREFIX(inflateInit2)(&d_stream, -MAX_WBITS);
    ASSERT_EQ(err, Z_OK);
    d_stream.next_in = compr;
    d_stream.avail_in = compr_len;
    d_stream.next_out = uncompr;
    d_stream.avail_out = 64;            /* stop inside the block */
    err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
    ASSERT_EQ(err, Z_OK);
    ASSERT_EQ(d_stream.avail_out, 0u);

    memset(&d_copy, 0, sizeof(d_copy));
    err = PREFIX(inflateCopy)(&d_copy, &d_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateEnd)(&d_stream);
    ASSERT_EQ(err, Z_OK);

    /* The copy owns the rest of the decode. */
    d_copy.next_out = uncompr + 64;
    d_copy.avail_out = (uint32_t)(sizeof(uncompr) - 64);
    err = PREFIX(inflate)(&d_copy, Z_FINISH);
    ASSERT_EQ(err, Z_STREAM_END);
    EXPECT_EQ(d_copy.avail_out, 0u);
    EXPECT_EQ(memcmp(uncompr, plain, sizeof(plain)), 0);
    err = PREFIX(inflateEnd)(&d_copy);
    ASSERT_EQ(err, Z_OK);
}

TEST(inflate, copy_mid_block_fixed_tables) {
    copy_mid_block(Z_FIXED);
}

TEST(inflate, copy_mid_block_dynamic_tables) {
    copy_mid_block(Z_DEFAULT_STRATEGY);
}
