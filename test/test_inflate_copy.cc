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

TEST(inflate, copy_and_decompress) {
    int err;
    unsigned char compr[256], uncompr[256];
    z_size_t compr_len, uncompr_len;
    PREFIX3(stream) c_stream, d_stream, d_copy;
    const char test_data[] = "zlib-ng inflateCopy test data for pointer bounds guard";

    /* compress */
    memset(&c_stream, 0, sizeof(c_stream));
    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    ASSERT_EQ(err, Z_OK);
    c_stream.next_in = (const unsigned char *)test_data;
    c_stream.avail_in = (unsigned int)strlen(test_data);
    c_stream.next_out = compr;
    c_stream.avail_out = sizeof(compr);
    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    ASSERT_EQ(err, Z_STREAM_END);
    compr_len = c_stream.total_out;
    err = PREFIX(deflateEnd)(&c_stream);
    ASSERT_EQ(err, Z_OK);

    /* init inflate and copy */
    memset(&d_stream, 0, sizeof(d_stream));
    err = PREFIX(inflateInit)(&d_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateCopy)(&d_copy, &d_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateEnd)(&d_stream);
    ASSERT_EQ(err, Z_OK);

    /* decompress through copied stream */
    d_copy.next_in = compr;
    d_copy.avail_in = (unsigned int)compr_len;
    d_copy.next_out = uncompr;
    d_copy.avail_out = sizeof(uncompr);
    err = PREFIX(inflate)(&d_copy, Z_FINISH);
    ASSERT_EQ(err, Z_STREAM_END);
    uncompr_len = d_copy.total_out;
    err = PREFIX(inflateEnd)(&d_copy);
    ASSERT_EQ(err, Z_OK);

    /* verify data matches */
    ASSERT_EQ(uncompr_len, strlen(test_data));
    ASSERT_EQ(memcmp(test_data, uncompr, uncompr_len), 0);
}
