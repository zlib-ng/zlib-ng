/* test_deflate_pending.c - Test deflatePending() with small buffers */

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
    uint8_t compr[128];
    z_size_t compr_len = sizeof(compr);
    int *bits = calloc(256, 1);
    unsigned *ped = calloc(256, 1);
    int err;

    memset(&c_stream, 0, sizeof(c_stream));

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != hello_len && c_stream.total_out < compr_len) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflatePending)(&c_stream, ped, bits);
    CHECK_ERR(err, "deflatePending");

    if (*bits >= 0 && *bits <= 7) {
        printf("deflatePending(): OK\n");
    } else {
        printf("deflatePending(): error\n");
    }

    /* Finish the stream, still forcing small buffers: */
    for (;;) {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    free(bits);
    free(ped);
    return 0;
}
