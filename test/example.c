/* example.c -- usage example of the zlib compression library
 * Copyright (C) 1995-2006, 2011, 2016 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define TESTFILE "foo.gz"

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
    } \
}

static const char hello[] = "hello, hello!";
/* "hello world" would be more standard, but the repeated "hello"
 * stresses the compression code better, sorry...
 */

static const char dictionary[] = "hello";
static unsigned long dictId = 0; /* Adler32 value of the dictionary */


void test_compress      (unsigned char *compr, z_size_t comprLen,unsigned char *uncompr, z_size_t uncomprLen);
void test_gzio          (const char *fname, unsigned char *uncompr, z_size_t uncomprLen);
void test_deflate       (unsigned char *compr, size_t comprLen);
void test_inflate       (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
void test_large_deflate (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen, int zng_params);
void test_large_inflate (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
void test_flush         (unsigned char *compr, z_size_t *comprLen);
void test_sync          (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
void test_dict_deflate  (unsigned char *compr, size_t comprLen);
void test_dict_inflate  (unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
int  main               (int argc, char *argv[]);


static alloc_func zalloc = NULL;
static free_func zfree = NULL;

/* ===========================================================================
 * Test compress() and uncompress()
 */
void test_compress(unsigned char *compr, z_size_t comprLen, unsigned char *uncompr, z_size_t uncomprLen)
{
    int err;
    size_t len = strlen(hello)+1;

    err = PREFIX(compress)(compr, &comprLen, (const unsigned char*)hello, (z_size_t)len);
    CHECK_ERR(err, "compress");

    strcpy((char*)uncompr, "garbage");

    err = PREFIX(uncompress)(uncompr, &uncomprLen, compr, comprLen);
    CHECK_ERR(err, "uncompress");

    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad uncompress\n");
        exit(1);
    } else {
        printf("uncompress(): %s\n", (char *)uncompr);
    }
}

/* ===========================================================================
 * Test read/write of .gz files
 */
void test_gzio(const char *fname, unsigned char *uncompr, z_size_t uncomprLen)
{
#ifdef NO_GZCOMPRESS
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
#else
    int err;
    int len = (int)strlen(hello)+1;
    gzFile file;
    z_off_t pos;

    file = PREFIX(gzopen)(fname, "wb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
        exit(1);
    }
    PREFIX(gzputc)(file, 'h');
    if (PREFIX(gzputs)(file, "ello") != 4) {
        fprintf(stderr, "gzputs err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    if (PREFIX(gzprintf)(file, ", %s!", "hello") != 8) {
        fprintf(stderr, "gzprintf err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    PREFIX(gzseek)(file, 1L, SEEK_CUR); /* add one zero byte */
    PREFIX(gzclose)(file);

    file = PREFIX(gzopen)(fname, "rb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
        exit(1);
    }
    strcpy((char*)uncompr, "garbage");

    if (PREFIX(gzread)(file, uncompr, (unsigned)uncomprLen) != len) {
        fprintf(stderr, "gzread err: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad gzread: %s\n", (char*)uncompr);
        exit(1);
    } else {
        printf("gzread(): %s\n", (char*)uncompr);
    }

    pos = PREFIX(gzseek)(file, -8L, SEEK_CUR);
    if (pos != 6 || PREFIX(gztell)(file) != pos) {
        fprintf(stderr, "gzseek error, pos=%ld, gztell=%ld\n",
                (long)pos, (long)PREFIX(gztell)(file));
        exit(1);
    }

    if (PREFIX(gzgetc)(file) != ' ') {
        fprintf(stderr, "gzgetc error\n");
        exit(1);
    }

    if (PREFIX(gzungetc)(' ', file) != ' ') {
        fprintf(stderr, "gzungetc error\n");
        exit(1);
    }

    PREFIX(gzgets)(file, (char*)uncompr, (int)uncomprLen);
    if (strlen((char*)uncompr) != 7) { /* " hello!" */
        fprintf(stderr, "gzgets err after gzseek: %s\n", PREFIX(gzerror)(file, &err));
        exit(1);
    }
    if (strcmp((char*)uncompr, hello + 6)) {
        fprintf(stderr, "bad gzgets after gzseek\n");
        exit(1);
    } else {
        printf("gzgets() after gzseek: %s\n", (char*)uncompr);
    }

    PREFIX(gzclose)(file);
#endif
}

/* ===========================================================================
 * Test deflate() with small buffers
 */
void test_deflate(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    unsigned long len = (unsigned long)strlen(hello)+1;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;
    c_stream.total_in = 0;
    c_stream.total_out = 0;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in  = (const unsigned char *)hello;
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
void test_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
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

    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad inflate\n");
        exit(1);
    } else {
        printf("inflate(): %s\n", (char *)uncompr);
    }
}

static unsigned int diff;

/* ===========================================================================
 * Test deflate() with large buffers and dynamic change of compression level
 */
void test_large_deflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen, int zng_params)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
#ifndef ZLIB_COMPAT
    int level = -1;
    int strategy = -1;
    zng_deflate_param_value params[] = {
        { .param = Z_DEFLATE_LEVEL, .buf = &level, .size = sizeof(level) },
        { .param = Z_DEFLATE_STRATEGY, .buf = &strategy, .size = sizeof(strategy) },
    };
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
    if (c_stream.avail_in != 0) {
        fprintf(stderr, "deflate not greedy\n");
        exit(1);
    }

    /* Feed in already compressed data and switch to no compression: */
    if (zng_params) {
#ifndef ZLIB_COMPAT
        zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        if (level != Z_BEST_SPEED) {
            fprintf(stderr, "Expected compression level Z_BEST_SPEED, got %d\n", level);
            exit(1);
        }
        if (strategy != Z_DEFAULT_STRATEGY) {
            fprintf(stderr, "Expected compression strategy Z_DEFAULT_STRATEGY, got %d\n", strategy);
            exit(1);
        }
        level = Z_NO_COMPRESSION;
        strategy = Z_DEFAULT_STRATEGY;
        zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
#else
        fprintf(stderr, "test_large_deflate() called with zng_params=1 in compat mode\n");
        exit(1);
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
        if (level != Z_NO_COMPRESSION) {
            fprintf(stderr, "Expected compression level Z_NO_COMPRESSION, got %d\n", level);
            exit(1);
        }
        if (strategy != Z_DEFAULT_STRATEGY) {
            fprintf(stderr, "Expected compression strategy Z_DEFAULT_STRATEGY, got %d\n", strategy);
            exit(1);
        }
        level = Z_BEST_COMPRESSION;
        strategy = Z_FILTERED;
        zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
#else
        fprintf(stderr, "test_large_deflate() called with zng_params=1 in compat mode\n");
        exit(1);
#endif
    } else {
        PREFIX(deflateParams)(&c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
    }
    c_stream.next_in = uncompr;
    c_stream.avail_in = (unsigned int)uncomprLen;
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        fprintf(stderr, "deflate should report Z_STREAM_END\n");
        exit(1);
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with large buffers
 */
void test_large_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
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

    if (d_stream.total_out != 2*uncomprLen + diff) {
        fprintf(stderr, "bad large inflate: %zu\n", d_stream.total_out);
        exit(1);
    } else {
        printf("large_inflate(): OK\n");
    }
}

/* ===========================================================================
 * Test deflate() with full flush
 */
void test_flush(unsigned char *compr, z_size_t *comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    unsigned int len = (unsigned int)strlen(hello)+1;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in  = (const unsigned char *)hello;
    c_stream.next_out = compr;
    c_stream.avail_in = 3;
    c_stream.avail_out = (unsigned int)*comprLen;
    err = PREFIX(deflate)(&c_stream, Z_FULL_FLUSH);
    CHECK_ERR(err, "deflate");

    compr[3]++; /* force an error in first compressed block */
    c_stream.avail_in = len - 3;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        CHECK_ERR(err, "deflate");
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    *comprLen = (z_size_t)c_stream.total_out;
}

/* ===========================================================================
 * Test inflateSync()
 */
void test_sync(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;

    d_stream.next_in  = compr;
    d_stream.avail_in = 2; /* just read the zlib header */

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    d_stream.next_out = uncompr;
    d_stream.avail_out = (unsigned int)uncomprLen;

    err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "inflate");

    d_stream.avail_in = (unsigned int)comprLen-2;   /* read all compressed data */
    err = PREFIX(inflateSync)(&d_stream);           /* but skip the damaged part */
    CHECK_ERR(err, "inflateSync");

    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    if (err != Z_DATA_ERROR) {
        fprintf(stderr, "inflate should report DATA_ERROR\n");
        /* Because of incorrect adler32 */
        exit(1);
    }
    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    printf("after inflateSync(): hel%s\n", (char *)uncompr);
}

/* ===========================================================================
 * Test deflate() with preset dictionary
 */
void test_dict_deflate(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;
    c_stream.adler = 0;

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    err = PREFIX(deflateSetDictionary)(&c_stream,
                (const unsigned char*)dictionary, (int)sizeof(dictionary));
    CHECK_ERR(err, "deflateSetDictionary");

    dictId = c_stream.adler;
    c_stream.next_out = compr;
    c_stream.avail_out = (unsigned int)comprLen;

    c_stream.next_in = (const unsigned char *)hello;
    c_stream.avail_in = (unsigned int)strlen(hello)+1;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        fprintf(stderr, "deflate should report Z_STREAM_END\n");
        exit(1);
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with a preset dictionary
 */
void test_dict_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    strcpy((char*)uncompr, "garbage garbage garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.adler = 0;
    d_stream.next_in  = compr;
    d_stream.avail_in = (unsigned int)comprLen;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    d_stream.next_out = uncompr;
    d_stream.avail_out = (unsigned int)uncomprLen;

    for (;;) {
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
        if (err == Z_NEED_DICT) {
            if (d_stream.adler != dictId) {
                fprintf(stderr, "unexpected dictionary");
                exit(1);
            }
            err = PREFIX(inflateSetDictionary)(&d_stream, (const unsigned char*)dictionary,
                                       (int)sizeof(dictionary));
        }
        CHECK_ERR(err, "inflate with dict");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    if (strncmp((char*)uncompr, hello, sizeof(hello))) {
        fprintf(stderr, "bad inflate with dict\n");
        exit(1);
    } else {
        printf("inflate with dictionary: %s\n", (char *)uncompr);
    }
}

/* ===========================================================================
 * Usage:  example [output.gz  [input.gz]]
 */

int main(int argc, char *argv[])
{
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

    printf("zlib version %s = 0x%04x, compile flags = 0x%lx\n",
            PREFIX2(VERSION), PREFIX2(VERNUM), PREFIX(zlibCompileFlags)());

    compr    = (unsigned char*)calloc((unsigned int)comprLen, 1);
    uncompr  = (unsigned char*)calloc((unsigned int)uncomprLen, 1);
    /* compr and uncompr are cleared to avoid reading uninitialized
     * data and to ensure that uncompr compresses well.
     */
    if (compr == NULL || uncompr == NULL) {
        printf("out of memory\n");
        exit(1);
    }

    test_compress(compr, comprLen, uncompr, uncomprLen);

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

    test_flush(compr, &comprLen);
    test_sync(compr, comprLen, uncompr, uncomprLen);
    comprLen = uncomprLen;

    test_dict_deflate(compr, comprLen);
    test_dict_inflate(compr, comprLen, uncompr, uncomprLen);

    free(compr);
    free(uncompr);

    return 0;
}
