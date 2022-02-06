/* test_deflate_bound.cc - Test deflateBound() with small buffers */

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

#include "test_shared.h"

#include <gtest/gtest.h>

TEST(deflate, bound) {
    PREFIX3(stream) c_stream;
    int estimate_len = 0;
    uint8_t *out_buf = NULL;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));

    c_stream.avail_in = hello_len;
    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_out = 0;
    c_stream.next_out = out_buf;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    EXPECT_EQ(err, Z_OK);

    /* calculate actual output length and update structure */
    estimate_len = PREFIX(deflateBound)(&c_stream, hello_len);
    out_buf = (uint8_t *)malloc(estimate_len);

    if (out_buf != NULL) {
        /* update zlib configuration */
        c_stream.avail_out = estimate_len;
        c_stream.next_out = out_buf;

        /* do the compression */
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        EXPECT_EQ(err, Z_STREAM_END);
    }

    err = PREFIX(deflateEnd)(&c_stream);
    EXPECT_EQ(err, Z_OK);

    free(out_buf);
}
