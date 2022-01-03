/* test_small_buffers.c - Test deflate() and inflate() with small buffers */

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
    PREFIX3(stream) c_stream, d_stream;
    uint8_t compr[128], uncompr[128];
    z_size_t compr_len = sizeof(compr), uncompr_len = sizeof(uncompr);
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&d_stream, 0, sizeof(d_stream));

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in  = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != hello_len && c_stream.total_out < compr_len) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }
    /* Finish the stream, still forcing small buffers */
    for (;;) {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    strcpy((char*)uncompr, "garbage");

    d_stream.next_in  = compr;
    d_stream.next_out = uncompr;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    while (d_stream.total_out < uncompr_len && d_stream.total_in < compr_len) {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "inflate");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (strcmp((char*)uncompr, hello))
        error("bad inflate\n");
    else
        printf("inflate(): %s\n", (char *)uncompr);
    return 0;
}
