/* test_deflate_copy.c - Test deflateCopy() with small buffers */

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

#include "deflate.h"

#include "test_shared.h"

int main() {
    PREFIX3(stream) c_stream, c_stream_copy;
    uint8_t compr[128];
    z_size_t compr_len = sizeof(compr);
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&c_stream_copy, 0, sizeof(c_stream_copy));

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != hello_len && c_stream.total_out < compr_len) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflateCopy)(&c_stream_copy, &c_stream);
    CHECK_ERR(err, "deflate_copy");

    if (c_stream.state->status == c_stream_copy.state->status) {
        printf("deflate_copy(): OK\n");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd original");

    err = PREFIX(deflateEnd)(&c_stream_copy);
    CHECK_ERR(err, "deflateEnd copy");
    return 0;
}
