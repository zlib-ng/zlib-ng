/* test_deflate_dict.c - Test deflateGetDictionary() with small buffers */

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
    uint32_t compr_len = sizeof(compr);
    unsigned char *dict_new = NULL;
    unsigned int *dict_len;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_out = compr;
    c_stream.avail_out = compr_len;

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (uint32_t)hello_len;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);

    if (err != Z_STREAM_END)
        error("deflate should report Z_STREAM_END\n");

    dict_new = calloc(256, 1);
    dict_len = (unsigned int *)calloc(4, 1);
    if (dict_new == NULL || dict_len == NULL)
        error("out of memory\n");

    err = PREFIX(deflateGetDictionary)(&c_stream, dict_new, dict_len);

    CHECK_ERR(err, "deflateGetDictionary");
    printf("deflateGetDictionary(): %s\n", dict_new);

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    free(dict_new);
    free(dict_len);
    return 0;
}
