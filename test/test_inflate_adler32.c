/* GH-1066 - inflate small amount of data and validate with adler32 checksum. */

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

const char* original = "The quick brown fox jumped over the lazy dog";

z_const unsigned char compressed[] = {
    0x78, 0x9c, 0x0b, 0xc9, 0x48, 0x55, 0x28, 0x2c, 0xcd, 0x4c, 0xce, 0x56, 0x48,
    0x2a, 0xca, 0x2f, 0xcf, 0x53, 0x48, 0xcb, 0xaf, 0x50, 0xc8, 0x2a, 0xcd, 0x2d,
    0x48, 0x4d, 0x51, 0xc8, 0x2f, 0x4b, 0x2d, 0x52, 0x28, 0xc9, 0x48, 0x55, 0xc8,
    0x49, 0xac, 0xaa, 0x54, 0x48, 0xc9, 0x4f, 0x07, 0x00, 0x6b, 0x93, 0x10, 0x30
};

int main(){
    unsigned char uncompressed[1024];
    PREFIX3(stream) strm;

    memset(&strm, 0, sizeof(strm));

    int err = PREFIX(inflateInit2)(&strm, 32 + MAX_WBITS);
    CHECK_ERR(err, "inflateInit2");

    strm.next_in = compressed;
    strm.avail_in = sizeof(compressed);
    strm.next_out = uncompressed;
    strm.avail_out = sizeof(uncompressed);

    err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    if (err != Z_STREAM_END) {
        error("inflate() failed with code %d\n", err);
    }

    err = PREFIX(inflateEnd)(&strm);
    CHECK_ERR(err, "inflateEnd");

    if (memcmp(uncompressed, original, MIN(strm.total_out, strlen(original))) != 0)
        error("expected output does not match\n");
    else
        printf("inflate adler32: OK\n");
    return 0;
}
