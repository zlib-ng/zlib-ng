/* Generated by fuzzing - test block_open handling in deflate_quick(). */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_shared.h"

int main() {
    PREFIX3(stream) strm;
    int err;

    memset(&strm, 0, sizeof(strm));
    err = PREFIX(deflateInit2)(&strm, 1, Z_DEFLATED, -15, 1, Z_FILTERED);
    CHECK_ERR(err, "deflateInit2");

    z_const unsigned char next_in[494] =
            "\x1d\x1d\x00\x00\x00\x4a\x4a\x4a\xaf\xaf\xaf\xaf\x4a\x4a\x4a\x4a"
            "\x3f\x3e\xaf\xff\xff\xff\x11\xff\xff\xff\xff\xdf\x00\x00\x00\x01"
            "\x3f\x7d\x00\x50\x00\x00\xc8\x01\x2b\x60\xc8\x00\x24\x06\xff\xff"
            "\x4a\x4e\x4a\x7d\xc8\x01\xf1\x2b\x28\xb2\xb2\x60\x25\xc8\x06\x00"
            "\x00\x00\x31\x00\x01\xb2\xb2\xb2\xff\xff\xfd\xb2\xb2\x40\xff\x7d"
            "\x3b\x34\x3e\xff\xff\x4a\x4a\x01\xf1\xff\x02\xff\x3f\xff\x02\xff"
            "\xff\xff\xbf\x0a\xff\x00\x01\x3f\xb3\xff\x26\x00\x00\x13\x00\xc8"
            "\x3e\x3e\x3e\x4a\x76\x4a\x4a\x2e\x7d\x3e\x3e\x3e\x3e\x1d\x1d\x1d"
            "\xfe\xea\xef\x80\x01\x00\x00\x40\x00\x00\xba\x00\x06\xfa\xb9\x11"
            "\xbf\x98\xee\x45\x7e\x04\x00\xff\xff\xff\x67\xc3\xc3\xc3\xc3\x00"
            "\x1d\x1d\xe1\xe3\x00\xc3\x1d\x98\x1d\x1d\x1d\x1d\x1d\x00\x00\x00"
            "\x02\x00\x00\x00\xe8\x00\x00\x1d\x1d\x1d\xfa\x1e\x12\xff\xff\xff"
            "\x00\x01\xa7\xff\xff\xff\x1d\x1d\x1d\x63\xff\xff\xff\x1f\x00\x00"
            "\x10\x40\x00\x00\xad\xff\xff\x3f\x51\x00\xf8\xff\xff\x8a\x01\x05"
            "\x00\x00\x03\x00\x00\xff\x00\x00\x00\x05\x40\x1f\x08\x0a\x00\xff"
            "\xff\x01\x00\x12\x00\x00\x01\x00\x3f\x40\x1d\x1d\x1d\x1d\x1d\x1d"
            "\x21\x00\x1d\x00\x00\x00\xe4\x00\x00\x00\x07\x00\x00\xe6\xe6\x34"
            "\xe6\xe6\xe6\xe6\xff\x2b\xee\x1d\x1d\x1d\x93\x1d\x1d\x1d\xee\x2b"
            "\xee\x01\x81\x1d\x00\x00\x58\x00\x00\x01\x14\x00\x1b\x00\x00\x2c"
            "\x00\x00\x00\xdb\x00\x45\x7e\x00\x00\x00\xfb\xbd\x00\x06\x21\xd3"
            "\x00\xff\xff\xff\xff\xff\x00\x49\x49\xc9\x49\x3d\x00\x34\x01\x00"
            "\x00\x6a\x2b\x00\x00\x50\x40\xf0\xf0\xf0\xf0\xa3\xa3\xa3\xa3\xf0"
            "\xf0\x06\xfa\xa9\x01\x10\xbf\x98\x9d\x2b\xee\x2d\x21\x01\xdb\x00"
            "\x45\x10\x00\x00\x7e\x00\x00\xe7\x00\xff\xff\x00\xf6\x00\x00\x00"
            "\xf9\x00\x00\x00\x11\x00\x00\x00\xe2\x00\x00\x00\x2d\x00\x00\x00"
            "\x2f\x00\x3f\x54\x1d\x1d\x1d\x4c\x4c\x4c\x4c\x2a\x4c\x4c\x10\xff"
            "\xff\x1a\x00\x00\x01\xff\x00\xff\xf9\x00\x3f\x53\xcc\xcc\xcc\xcc"
            "\x6e\x00\x00\x01\xf8\xff\xff\xff\x49\x04\x2c\x01\x00\x1d\x00\x07"
            "\x01\xff\x00\x00\x00\xf8\xff\x09\x00\x27\x00\x08\x21\x1c\x00\x00"
            "\x00\x00\x1d\x05\x00\x00\x00\x2c\x53\x3f\x00\x01\x00\x00\xe6\xff"
            "\xff\xff\x6a\x2b\xee\xe6\x6a\x2b\xee\x2b\xee\xee\x2b\xee";
    strm.next_in = next_in;
    unsigned char next_out[1116];
    strm.next_out = next_out;

    strm.avail_in = sizeof(next_in);
    while (1) {
        strm.avail_out = (uint32_t)(next_out + sizeof(next_out) - strm.next_out);
        if (strm.avail_out > 38)
            strm.avail_out = 38;
        err = PREFIX(deflate)(&strm, Z_FINISH);
        if (err == Z_STREAM_END)
            break;
        CHECK_ERR(err, "deflate");
    }
    uint32_t compressed_size = (uint32_t)(strm.next_out - next_out);

    err = PREFIX(deflateEnd)(&strm);
    CHECK_ERR(err, "deflateEnd");

    memset(&strm, 0, sizeof(strm));
    err = PREFIX(inflateInit2)(&strm, -15);
    CHECK_ERR(err, "inflateInit2");

    strm.next_in = next_out;
    strm.avail_in = compressed_size;
    unsigned char uncompressed[sizeof(next_in)];
    strm.next_out = uncompressed;
    strm.avail_out = sizeof(uncompressed);

    err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    if (err != Z_STREAM_END)
        error("inflate() failed with code %d\n", err);

    err = PREFIX(inflateEnd)(&strm);
    CHECK_ERR(err, "inflateEnd");

    if (memcmp(uncompressed, next_in, sizeof(uncompressed)) != 0)
        error("Uncompressed data differs from the original\n");
    else
        printf("deflate_quick block open: OK\n");
    return 0;
    return 0;
}
