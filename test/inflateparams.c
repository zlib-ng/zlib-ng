#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#if defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
    } \
}

static alloc_func zalloc = NULL;
static free_func zfree = NULL;

/* ===========================================================================
 * inflate() using small buffers
 */
void inflate_params_small(unsigned char *buf, uint32_t len, int window_bits)
{
    PREFIX3(stream) d_stream; /* decompression stream */
    unsigned char uncompr[4096];
    int err;

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit2)(&d_stream, window_bits);
    CHECK_ERR(err, "inflateInit2");

    d_stream.next_in  = (const unsigned char *)buf;
    d_stream.next_out = (unsigned char *)uncompr;
    
    while (d_stream.total_in < len) {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "inflate");

        if (d_stream.next_out == uncompr + sizeof(uncompr)) {
            fwrite(uncompr, 1, (size_t)sizeof(uncompr), stdout);
            d_stream.next_out = (unsigned char *)uncompr;
        }
    }

    /* Finish the stream, still forcing small buffers: */
    d_stream.avail_in = 0;
    do {
        if (d_stream.next_out == uncompr + sizeof(uncompr)) {
            fwrite(uncompr, 1, (size_t)sizeof(uncompr), stdout);
            d_stream.next_out = (unsigned char *)uncompr;
        }

        d_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(inflate)(&d_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "inflate");
    } while (err == Z_OK);

    if (d_stream.next_out != uncompr) {
        fwrite(uncompr, 1, d_stream.next_out - uncompr, stdout);
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");
}

/* ===========================================================================
 * inflate() with large buffers
 */
void inflate_params_large(unsigned char *buf, uint32_t len, int window_bits)
{
    PREFIX3(stream) d_stream; /* decompression stream */
    unsigned char uncompr[4096];
    int err;

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit2)(&d_stream, window_bits);
    CHECK_ERR(err, "inflateInit2");
    
    d_stream.next_in  = (const unsigned char *)buf;
    d_stream.next_out = (unsigned char *)uncompr;

    while (d_stream.total_in < len) {
        d_stream.avail_in = d_stream.avail_out = sizeof(uncompr); /* Force large buffers */
        if (d_stream.avail_in > (len - d_stream.total_in))
            d_stream.avail_in = (len - d_stream.total_in);
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "inflate");
        fwrite(uncompr, 1, d_stream.next_out - uncompr, stdout);
    }

    /* Finish the stream using large buffers */
    d_stream.avail_in = 0;
    do {
        d_stream.avail_out = sizeof(uncompr);
        err = PREFIX(inflate)(&d_stream, Z_FINISH);
        fwrite(uncompr, 1, d_stream.next_out - uncompr, stdout);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "inflate");
    } while (err == Z_OK);

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");
}

/* ===========================================================================
 * inflate() with params
 */
void inflate_params(unsigned char *buf, uint32_t len, int window_bits, uint32_t uncompr_bound)
{
    PREFIX3(stream) d_stream; /* decompression stream */
    unsigned char *uncompr;
    int err;

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    uncompr = malloc(uncompr_bound);
    assert(uncompr != NULL);

    err = PREFIX(inflateInit2)(&d_stream, window_bits);
    CHECK_ERR(err, "inflateInit2");
    
    d_stream.next_in  = (const unsigned char *)buf;
    d_stream.avail_in = len;
    d_stream.next_out = uncompr;
    d_stream.avail_out = uncompr_bound;

    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    if (err != Z_STREAM_END)
        CHECK_ERR(err, "inflate");

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    fwrite(uncompr, 1, d_stream.total_out, stdout);
    free(uncompr);
}

/* ===========================================================================
 * Usage:  inflateparams [-s] [-l] [-w #] [-b #] [input file]
 *   -s : small buffer
 *   -l : large buffer
 *   -w : window bits
 *   -b : uncompress bound
 */

int main(int argc, char **argv) {
    int32_t i;
    int32_t window_bits = -MAX_WBITS;
    int32_t uncompr_bound = 0;
    uint8_t small_buf = 0;
    uint8_t large_buf = 0;
    size_t len, read;
    unsigned char *buf;
    FILE *f;


    for (i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-w") == 0) && (i + 1 < argc))
            window_bits = atoi(argv[++i]);
        else if (strcmp(argv[i], "-b") == 0)
            uncompr_bound = atoi(argv[++i]);
        else if (strcmp(argv[i], "-s") == 0)
            small_buf = 1;
        else if (strcmp(argv[i], "-l") == 0)
            large_buf = 1;
        else
            break;
    }

    printf("inflateParams: windowBits %d\n", window_bits);

    if (i == argc) {
        fprintf(stderr, "No input file specified\n");
        exit(1);
    }
    
    SET_BINARY_MODE(stdout);
    f = fopen(argv[i], "rb+");
    if (f == NULL) {
        /* Failed to open this file: it may be a directory. */
        fprintf(stderr, "Failed to open file: %s\n", argv[i]);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);

    buf = (unsigned char *)malloc(len);
    read = fread(buf, 1, len, f);
    assert(read == len);

    if (small_buf) {
        inflate_params_small(buf, len, window_bits);
    }
    else if (large_buf) {
        inflate_params_large(buf, len, window_bits);
    }
    else {
        inflate_params(buf, len, window_bits, uncompr_bound);
    }

    free(buf);
    fclose(f);

    return 0;
}