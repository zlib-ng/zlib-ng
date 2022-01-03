/* example.c -- usage example of the zlib compression library
 * Copyright (C) 1995-2006, 2011, 2016 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif
#include "deflate.h"

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <inttypes.h>

#define TESTFILE "foo.gz"

static const char hello[] = "hello, hello!";
/* "hello world" would be more standard, but the repeated "hello"
 * stresses the compression code better, sorry...
 */

void test_gzio          (const char *fname, unsigned char *uncompr, z_size_t uncomprLen);
void test_deflate       (unsigned char *compr, size_t comprLen);
void test_inflate       (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
void test_large_deflate (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen, int zng_params);
void test_large_inflate (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
int  main               (int argc, char *argv[]);


static alloc_func zalloc = NULL;
static free_func zfree = NULL;

/* ===========================================================================
 * Display error message and exit
 */
void error(const char *format, ...) {
    va_list va;

    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);

    exit(1);
}

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) \
        error("%s error: %d\n", msg, err); \
}

/* ===========================================================================
 * Test read/write of .gz files
 */
void test_gzio(const char *fname, unsigned char *uncompr, z_size_t uncomprLen) {
#ifdef NO_GZCOMPRESS
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
#else
    int err;
    size_t read;
    size_t len = strlen(hello)+1;
    gzFile file;
    z_off64_t pos;
    z_off64_t comprLen;

    /* Write gz file with test data */
    file = PREFIX(gzopen)(fname, "wb");
    if (file == NULL)
        error("gzopen error\n");
    /* Write hello, hello! using gzputs and gzprintf */
    PREFIX(gzputc)(file, 'h');
    if (PREFIX(gzputs)(file, "ello") != 4)
        error("gzputs err: %s\n", PREFIX(gzerror)(file, &err));
    if (PREFIX(gzprintf)(file, ", %s!", "hello") != 8)
        error("gzprintf err: %s\n", PREFIX(gzerror)(file, &err));
    /* Write string null-teriminator using gzseek */
    if (PREFIX(gzseek)(file, 1L, SEEK_CUR) < 0)
        error("gzseek error, gztell=%ld\n", (long)PREFIX(gztell)(file));
    /* Write hello, hello! using gzfwrite using best compression level */
    if (PREFIX(gzsetparams)(file, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY) != Z_OK)
        error("gzsetparams err: %s\n", PREFIX(gzerror)(file, &err));
    if (PREFIX(gzfwrite)(hello, len, 1, file) == 0)
        error("gzfwrite err: %s\n", PREFIX(gzerror)(file, &err));
    /* Flush compressed bytes to file */
    if (PREFIX(gzflush)(file, Z_SYNC_FLUSH) != Z_OK)
        error("gzflush err: %s\n", PREFIX(gzerror)(file, &err));
    comprLen = PREFIX(gzoffset)(file);
    if (comprLen <= 0)
        error("gzoffset err: %s\n", PREFIX(gzerror)(file, &err));
    PREFIX(gzclose)(file);

    /* Open gz file we previously wrote */
    file = PREFIX(gzopen)(fname, "rb");
    if (file == NULL)
        error("gzopen error\n");

    /* Read uncompressed data - hello, hello! string twice */
    strcpy((char*)uncompr, "garbages");
    if (PREFIX(gzread)(file, uncompr, (unsigned)uncomprLen) != (int)(len + len))
        error("gzread err: %s\n", PREFIX(gzerror)(file, &err));
    if (strcmp((char*)uncompr, hello))
        error("bad gzread: %s\n", (char*)uncompr);
    else
        printf("gzread(): %s\n", (char*)uncompr);
    /* Check position at the end of the gz file */
    if (PREFIX(gzeof)(file) != 1)
        error("gzeof err: not reporting end of stream\n");

    /* Seek backwards mid-string and check char reading with gzgetc and gzungetc */
    pos = PREFIX(gzseek)(file, -22L, SEEK_CUR);
    if (pos != 6 || PREFIX(gztell)(file) != pos)
        error("gzseek error, pos=%ld, gztell=%ld\n", (long)pos, (long)PREFIX(gztell)(file));
    if (PREFIX(gzgetc)(file) != ' ')
        error("gzgetc error\n");
    if (PREFIX(gzungetc)(' ', file) != ' ')
        error("gzungetc error\n");
    /* Read first hello, hello! string with gzgets */
    strcpy((char*)uncompr, "garbages");
    PREFIX(gzgets)(file, (char*)uncompr, (int)uncomprLen);
    if (strlen((char*)uncompr) != 7) /* " hello!" */
        error("gzgets err after gzseek: %s\n", PREFIX(gzerror)(file, &err));
    if (strcmp((char*)uncompr, hello + 6))
        error("bad gzgets after gzseek\n");
    else
        printf("gzgets() after gzseek: %s\n", (char*)uncompr);
    /* Seek to second hello, hello! string */
    pos = PREFIX(gzseek)(file, 14L, SEEK_SET);
    if (pos != 14 || PREFIX(gztell)(file) != pos)
        error("gzseek error, pos=%ld, gztell=%ld\n", (long)pos, (long)PREFIX(gztell)(file));
    /* Check position not at end of file */
    if (PREFIX(gzeof)(file) != 0)
        error("gzeof err: reporting end of stream\n");
    /* Read first hello, hello! string with gzfread */
    strcpy((char*)uncompr, "garbages");
    read = PREFIX(gzfread)(uncompr, uncomprLen, 1, file);
    if (strcmp((const char *)uncompr, hello) != 0)
        error("bad gzgets\n");
    else
        printf("gzgets(): %s\n", (char*)uncompr);
    pos = PREFIX(gzoffset)(file);
    if (pos < 0 || pos != (comprLen + 10))
        error("gzoffset err: wrong offset at end\n");
    /* Trigger an error and clear it with gzclearerr */
    PREFIX(gzfread)(uncompr, (size_t)-1, (size_t)-1, file);
    PREFIX(gzerror)(file, &err);
    if (err == 0)
        error("gzerror err: no error returned\n");
    PREFIX(gzclearerr)(file);
    PREFIX(gzerror)(file, &err);
    if (err != 0)
        error("gzclearerr err: not zero %d\n", err);

    PREFIX(gzclose)(file);

    if (PREFIX(gzclose)(NULL) != Z_STREAM_ERROR)
        error("gzclose unexpected return when handle null\n");
    Z_UNUSED(read);
#endif
}

/* ===========================================================================
 * Test deflate() with small buffers
 */
void test_deflate(unsigned char *compr, size_t comprLen) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    size_t len = strlen(hello)+1;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;
    c_stream.total_in = 0;
    c_stream.total_out = 0;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in  = (z_const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in != len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
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
}

/* ===========================================================================
 * Test inflate() with small buffers
 */
void test_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen) {
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = 0;
    d_stream.next_out = uncompr;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
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
}

static unsigned int diff;

/* ===========================================================================
 * Test deflate() with large buffers and dynamic change of compression level
 */
void test_large_deflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen, int zng_params) {
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
#ifndef ZLIB_COMPAT
    int level = -1;
    int strategy = -1;
    zng_deflate_param_value params[2];

    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &level;
    params[0].size = sizeof(level);

    params[1].param = Z_DEFLATE_STRATEGY;
    params[1].buf = &strategy;
    params[1].size = sizeof(strategy);
#endif

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_SPEED);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_out = compr;
    c_stream.avail_out = (unsigned int)comprLen;

    /* At this point, uncompr is still mostly zeroes, so it should compress
     * very well:
     */
    c_stream.next_in = uncompr;
    c_stream.avail_in = (unsigned int)uncomprLen;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");
    if (c_stream.avail_in != 0)
        error("deflate not greedy\n");

    /* Feed in already compressed data and switch to no compression: */
    if (zng_params) {
#ifndef ZLIB_COMPAT
        zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        if (level != Z_BEST_SPEED)
            error("Expected compression level Z_BEST_SPEED, got %d\n", level);
        if (strategy != Z_DEFAULT_STRATEGY)
            error("Expected compression strategy Z_DEFAULT_STRATEGY, got %d\n", strategy);
        level = Z_NO_COMPRESSION;
        strategy = Z_DEFAULT_STRATEGY;
        zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
#else
        error("test_large_deflate() called with zng_params=1 in compat mode\n");
#endif
    } else {
        PREFIX(deflateParams)(&c_stream, Z_NO_COMPRESSION, Z_DEFAULT_STRATEGY);
    }
    c_stream.next_in = compr;
    diff = (unsigned int)(c_stream.next_out - compr);
    c_stream.avail_in = diff;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");

    /* Switch back to compressing mode: */
    if (zng_params) {
#ifndef ZLIB_COMPAT
        level = -1;
        strategy = -1;
        zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        if (level != Z_NO_COMPRESSION)
            error("Expected compression level Z_NO_COMPRESSION, got %d\n", level);
        if (strategy != Z_DEFAULT_STRATEGY)
            error("Expected compression strategy Z_DEFAULT_STRATEGY, got %d\n", strategy);
        level = Z_BEST_COMPRESSION;
        strategy = Z_FILTERED;
        zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
#else
        error("test_large_deflate() called with zng_params=1 in compat mode\n");
#endif
    } else {
        PREFIX(deflateParams)(&c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
    }
    c_stream.next_in = uncompr;
    c_stream.avail_in = (unsigned int)uncomprLen;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END)
        error("deflate should report Z_STREAM_END\n");
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with large buffers
 */
void test_large_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen) {
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = (unsigned int)comprLen;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    for (;;) {
        d_stream.next_out = uncompr;            /* discard the output */
        d_stream.avail_out = (unsigned int)uncomprLen;
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "large inflate");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (d_stream.total_out != 2*uncomprLen + diff)
        error("bad large inflate: %" PRIu64 "\n", (uint64_t)d_stream.total_out);
    else
        printf("large_inflate(): OK\n");
}

/* ===========================================================================
 * Usage:  example [output.gz  [input.gz]]
 */
int main(int argc, char *argv[]) {
    unsigned char *compr, *uncompr;
    z_size_t comprLen = 10000*sizeof(int); /* don't overflow on MSDOS */
    z_size_t uncomprLen = comprLen;
    static const char* myVersion = PREFIX2(VERSION);

    if (zVersion()[0] != myVersion[0]) {
        fprintf(stderr, "incompatible zlib version\n");
        exit(1);

    } else if (strcmp(zVersion(), PREFIX2(VERSION)) != 0) {
        fprintf(stderr, "warning: different zlib version\n");
    }

    printf("zlib-ng version %s = 0x%08lx, compile flags = 0x%lx\n",
            ZLIBNG_VERSION, ZLIBNG_VERNUM, PREFIX(zlibCompileFlags)());

    compr    = (unsigned char*)calloc((unsigned int)comprLen, 1);
    uncompr  = (unsigned char*)calloc((unsigned int)uncomprLen, 1);
    /* compr and uncompr are cleared to avoid reading uninitialized
     * data and to ensure that uncompr compresses well.
     */
    if (compr == NULL || uncompr == NULL)
        error("out of memory\n");

    test_gzio((argc > 1 ? argv[1] : TESTFILE),
              uncompr, uncomprLen);

    test_deflate(compr, comprLen);
    test_inflate(compr, comprLen, uncompr, uncomprLen);

    test_large_deflate(compr, comprLen, uncompr, uncomprLen, 0);
    test_large_inflate(compr, comprLen, uncompr, uncomprLen);

#ifndef ZLIB_COMPAT
    test_large_deflate(compr, comprLen, uncompr, uncomprLen, 1);
    test_large_inflate(compr, comprLen, uncompr, uncomprLen);
#endif

    comprLen = uncomprLen;

    free(compr);
    free(uncompr);

    return 0;
}
