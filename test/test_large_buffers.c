/* test_large_buffers.c - Test deflate() and inflate() with large buffers */

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
#include <time.h>

#include "test_shared.h"

#define COMPR_BUFFER_SIZE (48 * 1024)
#define UNCOMPR_BUFFER_SIZE (32 * 1024)
#define UNCOMPR_RAND_SIZE (8 * 1024)

int main() {
    PREFIX3(stream) c_stream, d_stream;
    uint8_t *compr, *uncompr;
    uint32_t compr_len, uncompr_len;
    int32_t i;
    time_t now;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&d_stream, 0, sizeof(d_stream));

    compr = calloc(1, COMPR_BUFFER_SIZE);
    uncompr = calloc(1, UNCOMPR_BUFFER_SIZE);

    if (compr == NULL || uncompr == NULL)
        error("out of memory\n");

    compr_len = COMPR_BUFFER_SIZE;
    uncompr_len = UNCOMPR_BUFFER_SIZE;

    srand((unsigned)time(&now));
    for (i = 0; i < UNCOMPR_RAND_SIZE; i++)
        uncompr[i] = (uint8_t)(rand() % 256);

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_out = compr;
    c_stream.avail_out = compr_len;
    c_stream.next_in = uncompr;
    c_stream.avail_in = uncompr_len;

    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");
    if (c_stream.avail_in != 0)
        error("deflate not greedy\n");

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END)
        error("deflate should report Z_STREAM_END\n");

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    d_stream.next_in  = compr;
    d_stream.avail_in = compr_len;
    d_stream.next_out = uncompr;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    for (;;) {
        d_stream.next_out = uncompr;            /* discard the output */
        d_stream.avail_out = uncompr_len;
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "large inflate");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (d_stream.total_out != uncompr_len)
        error("bad large inflate\n");
    else
        printf("large_inflate(): OK\n");

    free(compr);
    free(uncompr);
    return 0;
}
