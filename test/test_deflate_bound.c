/* test_deflate_bound.c - Test deflateBound() with small buffers */

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

int main() {
    PREFIX3(stream) c_stream;
    int estimate_len = 0;
    unsigned char *out_buf = NULL;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));

    c_stream.avail_in = hello_len;
    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_out = 0;
    c_stream.next_out = out_buf;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    /* calculate actual output length and update structure */
    estimate_len = PREFIX(deflateBound)(&c_stream, hello_len);
    out_buf = malloc(estimate_len);

    if (out_buf != NULL) {
        /* update zlib configuration */
        c_stream.avail_out = estimate_len;
        c_stream.next_out = out_buf;

        /* do the compression */
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) {
            printf("deflateBound(): OK\n");
        } else {
            CHECK_ERR(err, "deflate");
        }
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    free(out_buf);
    return 0;
}
