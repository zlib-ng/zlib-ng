/* test_inflate_copy.cc - Test copying inflate stream */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <string.h>

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

TEST(inflate, shared_gzip_header_extra_bounds) {
    /* Nothing may be written to extra once the lengths disagree, so 4 GiB of real space is not needed. */
    uint8_t extra[16];
    memset(extra, 0xaa, sizeof(extra));

    PREFIX(gz_header) header;
    memset(&header, 0, sizeof(header));
    header.extra = extra;
    header.extra_max = UINT32_MAX;

    PREFIX3(stream) first, second;
    memset(&first, 0, sizeof(first));
    ASSERT_EQ(PREFIX(inflateInit2)(&first, MAX_WBITS + 16), Z_OK);
    ASSERT_EQ(PREFIX(inflateGetHeader)(&first, &header), Z_OK);

    /* Copy the stream after the fixed part of the gzip header, then give each copy its own XLEN. */
    uint8_t fixed_header[10] = {0x1f, 0x8b, 8, 4, 0, 0, 0, 0, 0, 3};
    uint8_t first_xlen[2] = {100, 0};
    uint8_t second_xlen[2] = {10, 0};
    uint8_t output[16];
    first.next_in = fixed_header;
    first.avail_in = sizeof(fixed_header);
    first.next_out = output;
    first.avail_out = sizeof(output);
    ASSERT_EQ(PREFIX(inflate)(&first, Z_NO_FLUSH), Z_OK);
    ASSERT_EQ(first.avail_in, 0U);

    ASSERT_EQ(PREFIX(inflateCopy)(&second, &first), Z_OK);

    first.next_in = first_xlen;
    first.avail_in = sizeof(first_xlen);
    ASSERT_EQ(PREFIX(inflate)(&first, Z_NO_FLUSH), Z_OK);
    ASSERT_EQ(header.extra_len, 100U);

    second.next_in = second_xlen;
    second.avail_in = sizeof(second_xlen);
    second.next_out = output;
    second.avail_out = sizeof(output);
    ASSERT_EQ(PREFIX(inflate)(&second, Z_NO_FLUSH), Z_OK);
    ASSERT_EQ(header.extra_len, 10U);

    /* Resuming first with 100 payload bytes sees state->length (100) > extra_len (10) and writes nothing. */
    uint8_t payload[100];
    memset(payload, 'C', sizeof(payload));
    first.next_in = payload;
    first.avail_in = sizeof(payload);
    EXPECT_EQ(PREFIX(inflate)(&first, Z_NO_FLUSH), Z_OK);
    EXPECT_EQ(first.avail_in, 0U);
    for (size_t idx = 0; idx < sizeof(extra); idx++) {
        EXPECT_EQ(extra[idx], 0xaa);
    }

    EXPECT_EQ(PREFIX(inflateEnd)(&second), Z_OK);
    EXPECT_EQ(PREFIX(inflateEnd)(&first), Z_OK);
}
