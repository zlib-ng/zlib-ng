/* test_dict.c - Test deflate() and inflate() with preset dictionary */

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

/* Maximum dictionary size, according to inflateGetDictionary() description. */
#define MAX_DICTIONARY_SIZE 32768

static const char dictionary[] = "hello";

int main() {
    PREFIX3(stream) c_stream, d_stream;
    uint8_t compr[128], uncompr[128];
    z_size_t compr_len = sizeof(compr), uncompr_len = sizeof(uncompr);
    uint32_t dict_adler = 0;
    uint8_t check_dict[MAX_DICTIONARY_SIZE];
    uint32_t check_dict_len = 0;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&d_stream, 0, sizeof(d_stream));

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    err = PREFIX(deflateSetDictionary)(&c_stream,
        (const unsigned char *)dictionary, (int)sizeof(dictionary));
    CHECK_ERR(err, "deflateSetDictionary");

    dict_adler = c_stream.adler;
    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)compr_len;

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (uint32_t)hello_len;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END)
        error("deflate should report Z_STREAM_END\n");
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    strcpy((char*)uncompr, "garbage garbage garbage");

    d_stream.next_in  = compr;
    d_stream.avail_in = (unsigned int)compr_len;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    d_stream.next_out = uncompr;
    d_stream.avail_out = (unsigned int)uncompr_len;

    for (;;) {
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END)
            break;
        if (err == Z_NEED_DICT) {
            if (d_stream.adler != dict_adler)
                error("unexpected dictionary");
            err = PREFIX(inflateSetDictionary)(&d_stream, (const unsigned char*)dictionary,
                (uint32_t)sizeof(dictionary));
        }
        CHECK_ERR(err, "inflate with dict");
    }
    
    err = PREFIX(inflateGetDictionary)(&d_stream, NULL, &check_dict_len);
    CHECK_ERR(err, "inflateGetDictionary");
#ifndef S390_DFLTCC_INFLATE
    if (check_dict_len < sizeof(dictionary))
        error("bad dictionary length\n");
#endif
    
    err = PREFIX(inflateGetDictionary)(&d_stream, check_dict, &check_dict_len);
    CHECK_ERR(err, "inflateGetDictionary");
#ifndef S390_DFLTCC_INFLATE
    if (memcmp(dictionary, check_dict, sizeof(dictionary)) != 0)
        error("bad dictionary\n");
#endif

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (strncmp((char*)uncompr, hello, sizeof(hello)))
        error("bad inflate with dict\n");
    else
        printf("inflate with dictionary: %s\n", (char *)uncompr);
    return 0;
}
