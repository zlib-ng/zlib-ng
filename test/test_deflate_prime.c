/* test_deflate_prime.c - Test deflatePrime() wrapping gzip around deflate stream */

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
    uint32_t crc = 0;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&d_stream, 0, sizeof(d_stream));

    /* Raw deflate windowBits is -15 */
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    CHECK_ERR(err, "deflateInit2");

    /* Gzip magic number */
    err = PREFIX(deflatePrime)(&c_stream, 16, 0x8b1f);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip compression method (deflate) */
    err = PREFIX(deflatePrime)(&c_stream, 8, 0x08);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip flags (one byte, using two odd bit calls) */
    err = PREFIX(deflatePrime)(&c_stream, 3, 0x0);
    CHECK_ERR(err, "deflatePrime");
    err = PREFIX(deflatePrime)(&c_stream, 5, 0x0);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip modified time */
    err = PREFIX(deflatePrime)(&c_stream, 32, 0x0);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip extra flags */
    err = PREFIX(deflatePrime)(&c_stream, 8, 0x0);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip operating system */
    err = PREFIX(deflatePrime)(&c_stream, 8, 255);
    CHECK_ERR(err, "deflatePrime");

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (uint32_t)hello_len;
    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)compr_len;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END)
        CHECK_ERR(err, "deflate");

    /* Gzip uncompressed data crc32 */
    crc = PREFIX(crc32)(0, (const uint8_t *)hello, (uint32_t)hello_len);
    err = PREFIX(deflatePrime)(&c_stream, 32, crc);
    CHECK_ERR(err, "deflatePrime");
    /* Gzip uncompressed data length */
    err = PREFIX(deflatePrime)(&c_stream, 32, (uint32_t)hello_len);
    CHECK_ERR(err, "deflatePrime");

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    d_stream.next_in  = compr;
    d_stream.avail_in = (uint32_t)c_stream.total_out;
    d_stream.next_out = uncompr;
    d_stream.avail_out = (uint32_t)uncompr_len;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    /* Inflate with gzip header */
    err = PREFIX(inflateInit2)(&d_stream, MAX_WBITS + 32);
    CHECK_ERR(err, "inflateInit");

    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    if (err != Z_BUF_ERROR) {
        CHECK_ERR(err, "inflate");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (strcmp((const char *)uncompr, hello) != 0)
        error("bad deflatePrime\n");
    printf("deflatePrime(): OK\n");
    return 0;
}
