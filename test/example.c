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

#include "deflate.h"

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        return err; \
    } \
}

/* default memLevel */
#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

static const char hello[] = "hello, hello!";
/* "hello world" would be more standard, but the repeated "hello"
 * stresses the compression code better, sorry...
 */

static const char dictionary[] = "hello";
static unsigned long dictId = 0; /* Adler32 value of the dictionary */


int test_write_buffer(char *path, char *buf, size_t bufLen);
int test_deflate_params(unsigned char *buf, size_t bufLen, unsigned char *compr, size_t comprLen,
    int level, int method, int windowBits, int memLevel, int strategy);
int test_deflate_small_params(unsigned char *buf, size_t bufLen, unsigned char *compr, size_t comprLen,
    int level, int method, int windowBits, int memLevel, int strategy);
int test_inflate_params(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen,
    int windowBits);
int test_inflate_small_params(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen,
    int windowBits);
int test_deflate_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
int test_deflate_bound(void);
int test_deflate_copy(unsigned char *compr, size_t comprLen);
int test_deflate_get_dict(unsigned char *compr, size_t comprLen);
int test_deflate_pending(unsigned char *compr, size_t comprLen);
int test_deflate_prime(unsigned char *compr, size_t comprLen);
int test_deflate_set_header(unsigned char *compr, size_t comprLen);
int test_deflate_tune(unsigned char *compr, size_t comprLen);
int test_compress(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
int test_gzio(const char *fname, unsigned char *uncompr, size_t uncomprLen);
int test_large_deflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen, 
    int zng_params);
int test_large_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
int test_flush(unsigned char *compr, size_t *comprLen);
int test_sync(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
int test_dict_deflate(unsigned char *compr, size_t comprLen);
int test_dict_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen);
int test_file_deflate(char *path, int level, int memLevel, int strategy);
int test_file_deflate_inflate(char *path, int level, int memLevel, int strategy);
int test_file_inflate(char *path);
int main(int argc, char *argv[]);

static alloc_func zalloc = NULL;
static free_func zfree = NULL;

/* ===========================================================================
 * Write buffer to disk for testing
 */
int test_write_buffer(char *path, char *buf, size_t bufLen)
{
    FILE *f = fopen(path, "wb");
    if (f == NULL)
        return Z_STREAM_ERROR;
    fwrite(buf, 1, bufLen, f);
    fclose(f);
    return Z_OK;
}

/* ===========================================================================
 * Test compress() and uncompress()
 */
int test_compress(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
    int err;
    size_t len = strlen(hello)+1;

    printf("compress(): ");
    err = PREFIX(compress)(compr, &comprLen, (const unsigned char*)hello, (z_size_t)len);
    CHECK_ERR(err, "compress");
    printf("OK\n");

    strcpy((char*)uncompr, "garbage");

    printf("uncompress(): ");
    err = PREFIX(uncompress)(uncompr, &uncomprLen, compr, comprLen);
    CHECK_ERR(err, "uncompress");

    if (len != uncomprLen) {
        fprintf(stderr, "invalid uncompress size\n");
        return Z_DATA_ERROR;
    }
    if (memcmp((char*)uncompr, uncompr, len) != 0) {
        fprintf(stderr, "buffer mismatch\n");
        return Z_DATA_ERROR;
    }

    printf("OK\n");
    return Z_OK;
}

/* ===========================================================================
 * Test read/write of .gz files
 */
int test_gzio(const char *fname, unsigned char *uncompr, size_t uncomprLen)
{
#ifdef NO_GZCOMPRESS
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
#else
    int err;
    size_t len = strlen(hello)+1;
    gzFile file;
    z_off_t pos;

    file = PREFIX(gzopen)(fname, "wb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
        return Z_STREAM_ERROR;
    }
    PREFIX(gzputc)(file, 'h');

    if (PREFIX(gzputs)(file, "ello") != 4) {
        fprintf(stderr, "gzputs err: %s\n", PREFIX(gzerror)(file, &err));
        return Z_STREAM_ERROR;
    }
    if (PREFIX(gzprintf)(file, ", %s!", "hello") != 8) {
        fprintf(stderr, "gzprintf err: %s\n", PREFIX(gzerror)(file, &err));
        return Z_STREAM_ERROR;
    }
    PREFIX(gzseek)(file, 1L, SEEK_CUR); /* add one zero byte */
    PREFIX(gzclose)(file);

    file = PREFIX(gzopen)(fname, "rb");
    if (file == NULL) {
        fprintf(stderr, "gzopen error\n");
        return Z_STREAM_ERROR;
    }
    strcpy((char*)uncompr, "garbage");

    if (PREFIX(gzread)(file, uncompr, (unsigned)uncomprLen) != (int)len) {
        fprintf(stderr, "gzread err: %s\n", PREFIX(gzerror)(file, &err));
        return Z_STREAM_ERROR;
    }
    if (strcmp((char*)uncompr, hello)) {
        fprintf(stderr, "bad gzread: %s\n", (char*)uncompr);
        return Z_DATA_ERROR;
    } else {
        printf("gzread: OK (%s)\n", (char*)uncompr);
    }

    pos = PREFIX(gzseek)(file, -8L, SEEK_CUR);
    if (pos != 6 || PREFIX(gztell)(file) != pos) {
        fprintf(stderr, "gzseek error, pos=%ld, gztell=%ld\n",
                (long)pos, (long)PREFIX(gztell)(file));
        return Z_STREAM_ERROR;
    }

    if (PREFIX(gzgetc)(file) != ' ') {
        fprintf(stderr, "gzgetc error\n");
        return Z_STREAM_ERROR;
    }

    if (PREFIX(gzungetc)(' ', file) != ' ') {
        fprintf(stderr, "gzungetc error\n");
        return Z_STREAM_ERROR;
    }

    PREFIX(gzgets)(file, (char*)uncompr, (int)uncomprLen);
    if (strlen((char*)uncompr) != 7) { /* " hello!" */
        fprintf(stderr, "gzgets err after gzseek: %s\n", PREFIX(gzerror)(file, &err));
        return Z_DATA_ERROR;
    }
    if (strcmp((char*)uncompr, hello + 6)) {
        fprintf(stderr, "bad gzgets after gzseek\n");
        return Z_DATA_ERROR;
    } else {
        printf("gzgets after gzseek: OK (%s)\n", (char*)uncompr);
    }

    PREFIX(gzclose)(file);
#endif
    return Z_OK;
}

/* ===========================================================================
 * Test deflate()
 */
int test_deflate_params(unsigned char *buf, size_t bufLen, unsigned char *compr, size_t comprLen,
    int level, int method, int windowBits, int memLevel, int strategy)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;
    c_stream.total_in = 0;
    c_stream.total_out = 0;

    err = PREFIX(deflateInit2)(&c_stream, level, method, windowBits, memLevel, strategy);
    CHECK_ERR(err, "deflateInit2");
    
    c_stream.next_in  = (const unsigned char *)buf;
    c_stream.avail_in = (uint32_t)bufLen;
    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)comprLen;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END)
        CHECK_ERR(err, "deflate");

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    return c_stream.total_out;
}

/* ===========================================================================
 * Test deflate() using small buffers
 */
int test_deflate_small_params(unsigned char *buf, size_t bufLen, unsigned char *compr, size_t comprLen,
    int level, int method, int windowBits, int memLevel, int strategy)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;
    c_stream.total_in = 0;
    c_stream.total_out = 0;

    err = PREFIX(deflateInit2)(&c_stream, level, method, windowBits, memLevel, strategy);
    CHECK_ERR(err, "deflateInit2");
    
    c_stream.next_in  = (const unsigned char *)buf;
    c_stream.next_out = compr;

    while (c_stream.total_in < bufLen && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    c_stream.avail_in = 0;
    do {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    } while (c_stream.total_out < comprLen);

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    return (int)c_stream.total_out;
}

/* ===========================================================================
 * Test inflate()
 */
int test_inflate_params(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen,
    int windowBits)
{
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit2)(&d_stream, windowBits);
    CHECK_ERR(err, "inflateInit2");

    d_stream.next_in  = compr;
    d_stream.avail_in = (uint32_t)comprLen;
    d_stream.next_out = uncompr;
    d_stream.avail_out = (uint32_t)uncomprLen;

    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    if (err != Z_STREAM_END)
        CHECK_ERR(err, "inflate");
    
    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    return (int)d_stream.total_out;
}

/* ===========================================================================
 * Test inflate() with small buffers
 */
int test_inflate_small_params(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen,
    int windowBits)
{
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit2)(&d_stream, windowBits);
    CHECK_ERR(err, "inflateInit2");

    d_stream.next_in  = compr;
    d_stream.next_out = uncompr;

    while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
        d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END) break;
    }

    while (d_stream.total_out < uncomprLen) {
        d_stream.avail_out = 1;
        err = PREFIX(inflate)(&d_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "inflate");
    }

    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    return (int)d_stream.total_out;
}

/* ===========================================================================
 * Test deflate() and inflate() on a string
 */
int test_deflate_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
    int err;
    size_t len = strlen(hello) + 1;

    printf("deflate() hello: ");
    err = test_deflate_params((unsigned char *)hello, len, compr, comprLen,
        Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (err > 0) {
        err = Z_OK;
    }
    printf("OK\n");
    printf("inflate() hello: ");
    if (err == Z_OK)
        err = test_inflate_params(compr, comprLen, uncompr, uncomprLen, -MAX_WBITS);
    if (err > 0) {
        err = Z_OK;
    }
    printf("OK (%s)\n", uncompr);

    printf("deflate() small hello: ");
    err = test_deflate_small_params((unsigned char *)hello, len, compr, comprLen,
        Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (err > 0) {
        err = Z_OK;
    }
    printf("OK\n");
    printf("inflate() small hello: ");
    err = test_inflate_small_params(compr, comprLen, uncompr, uncomprLen, -MAX_WBITS);
    if (err > 0) {
        err = Z_OK;
    }
    printf("OK (%s)\n", uncompr);
    return err;
}

static unsigned int diff;

/* ===========================================================================
 * Test deflate() with large buffers and dynamic change of compression level
 */
int test_large_deflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen, 
    int zng_params)
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

    printf("large_deflate() %d: ", zng_params);

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
        return Z_STREAM_ERROR;
    }

    /* Feed in already compressed data and switch to no compression: */
    if (zng_params) {
#ifndef ZLIB_COMPAT
        err = zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        CHECK_ERR(err, "deflateGetParams");
        if (level != Z_BEST_SPEED) {
            fprintf(stderr, "Expected compression level Z_BEST_SPEED, got %d\n", level);
            return Z_STREAM_ERROR;
        }
        if (strategy != Z_DEFAULT_STRATEGY) {
            fprintf(stderr, "Expected compression strategy Z_DEFAULT_STRATEGY, got %d\n", strategy);
            return Z_STREAM_ERROR;
        }
        level = Z_NO_COMPRESSION;
        strategy = Z_DEFAULT_STRATEGY;
        err = zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        CHECK_ERR(err, "deflateSetParams");
#else
        fprintf(stderr, "test_large_deflate() called with zng_params=1 in compat mode\n");
        return Z_STREAM_ERROR;
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
        err = zng_deflateGetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        CHECK_ERR(err, "deflateGetParams");
        if (level != Z_NO_COMPRESSION) {
            fprintf(stderr, "Expected compression level Z_NO_COMPRESSION, got %d\n", level);
            return Z_STREAM_ERROR;
        }
        if (strategy != Z_DEFAULT_STRATEGY) {
            fprintf(stderr, "Expected compression strategy Z_DEFAULT_STRATEGY, got %d\n", strategy);
            return Z_STREAM_ERROR;
        }
        level = Z_BEST_COMPRESSION;
        strategy = Z_FILTERED;
        err = zng_deflateSetParams(&c_stream, params, sizeof(params) / sizeof(params[0]));
        CHECK_ERR(err, "deflateSetParams");
#else
        fprintf(stderr, "test_large_deflate() called with zng_params=1 in compat mode\n");
        return Z_STREAM_ERROR;
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
        return Z_STREAM_ERROR;
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
    printf("OK\n");
    return Z_OK;
}

/* ===========================================================================
 * Test inflate() with large buffers
 */
int test_large_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    printf("large_inflate(): ");

    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    d_stream.next_in  = compr;
    d_stream.avail_in = (unsigned int)comprLen;

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
        return Z_STREAM_ERROR;
    }
    printf("OK\n");
    return Z_OK;
}

/* ===========================================================================
 * Test deflate() with full flush
 */
int test_flush(unsigned char *compr, size_t *comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    size_t len = strlen(hello)+1;

    printf("deflate() full flush: ");
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
    c_stream.avail_in = (uint32_t)(len - 3);

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        CHECK_ERR(err, "deflate");
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    *comprLen = c_stream.total_out;
    printf("OK\n");
    return Z_OK;
}

/* ===========================================================================
 * Test inflateSync()
 */
int test_sync(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    printf("inflateSync(): ");
    strcpy((char*)uncompr, "garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");
    
    d_stream.next_in  = compr;
    d_stream.avail_in = 2; /* just read the zlib header */
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
        return Z_DATA_ERROR;
    }
    err = PREFIX(inflateEnd)(&d_stream);
    CHECK_ERR(err, "inflateEnd");

    printf("OK (hel%s)\n", (char *)uncompr);
    return Z_OK;
}

/* ===========================================================================
 * Test deflate() with preset dictionary
 */
int test_dict_deflate(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;

    printf("deflate() preset dictionary: ");

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
        return err;
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
    printf("OK\n");
    return Z_OK;
}

/* ===========================================================================
 * Test inflate() with a preset dictionary
 */
int test_dict_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr, size_t uncomprLen)
{
    int err;
    PREFIX3(stream) d_stream; /* decompression stream */

    printf("inflate() preset dictionary: ");
    strcpy((char*)uncompr, "garbage garbage garbage");

    d_stream.zalloc = zalloc;
    d_stream.zfree = zfree;
    d_stream.opaque = (void *)0;
    d_stream.adler = 0;

    err = PREFIX(inflateInit)(&d_stream);
    CHECK_ERR(err, "inflateInit");

    d_stream.next_in  = compr;
    d_stream.avail_in = (unsigned int)comprLen;
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
        return Z_STREAM_ERROR;
    }
    printf("OK (%s)\n", (char *)uncompr);
    return Z_OK;
}
/* ===========================================================================
 * Test deflateBound() with small buffers
 */
int test_deflate_bound(void)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    size_t len = strlen(hello)+1;
    int estimateLen = 0;
    unsigned char *outBuf = NULL;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;
    c_stream.avail_in = (uint32_t)len;
    c_stream.next_in = (const unsigned char *)hello;

    printf("deflateBound(): "); 
    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    /* calculate actual output length and update structure */
    estimateLen = PREFIX(deflateBound)(&c_stream, (unsigned long)len);
    outBuf = malloc(estimateLen);
    if (outBuf == NULL) {
        return Z_MEM_ERROR;
    }

    c_stream.avail_out = estimateLen;
    c_stream.next_out = outBuf;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err == Z_STREAM_END)
        printf("OK\n");
    else
        CHECK_ERR(err, "deflate");

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    free(outBuf);
    return err;
}

/* ===========================================================================
 * Test deflateCopy() with small buffers
 */
int test_deflate_copy(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream, c_stream_copy; /* compression stream */
    int err;
    size_t len = strlen(hello)+1;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    printf("deflateCopy(): ");
    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in = (const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in < len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    c_stream.avail_in = 0;
    do {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    } while (c_stream.total_out < comprLen);

    err = PREFIX(deflateCopy)(&c_stream_copy, &c_stream);
    CHECK_ERR(err, "deflate_copy");
    
    if (c_stream.state->status == c_stream_copy.state->status) {
        printf("OK\n");
    }

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd original");

    err = PREFIX(deflateEnd)(&c_stream_copy);
    CHECK_ERR(err, "deflateEnd copy");;

    return Z_OK;
}

/* ===========================================================================
 * Test deflateGetDictionary() with small buffers
 */
int test_deflate_get_dict(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    unsigned char *dictNew = NULL;
    unsigned int *dictLen;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    printf("deflateGetDictionary: ");
    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)comprLen;

    c_stream.next_in = (const unsigned char*)hello;
    c_stream.avail_in = (uint32_t)strlen(hello)+1;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);

    if (err != Z_STREAM_END) {
        fprintf(stderr, "deflate should report Z_STREAM_END\n");
        exit(1);
    }

    dictNew = calloc(256, 1);
    dictLen = (unsigned int *)calloc(4, 1);

    err = PREFIX(deflateGetDictionary)(&c_stream, dictNew, dictLen);
    CHECK_ERR(err, "deflateGetDictionary");

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    if (err == Z_OK) {
        printf("OK (%s)\n", dictNew);
    }

    free(dictNew);
    free(dictLen);
    return err;
}

/* ===========================================================================
 * Test deflatePending() with small buffers
 */
int test_deflate_pending(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    int *bits = calloc(256, 1);
    unsigned *ped = calloc(256, 1);
    size_t len = strlen(hello)+1;


    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    printf("deflatePending(): ");
    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    c_stream.next_in = (const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in < len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    err = PREFIX(deflatePending)(&c_stream, ped, bits);
    CHECK_ERR(err, "deflatePending");

    /* Finish the stream, still forcing small buffers: */
    c_stream.avail_in = 0;
    do {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    } while (c_stream.total_out < comprLen);

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
    if ((err == Z_OK) && (*bits >= 0 && *bits <=7 && *ped >= 0)) {
        printf("OK\n");
    }

    free(bits);
    free(ped);

    return err;
}

/* ===========================================================================
 * Test deflatePrime() with small buffers
 */
int test_deflate_prime(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    int bits = 0;
    int values = 0;
    size_t len = strlen(hello)+1;


    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    printf("deflatePrime(): ");
    /* windowBits can also be ?8..?15 for raw deflate,The default value is 15 */
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    CHECK_ERR(err, "deflateInit2");

    err = PREFIX(deflatePrime)(&c_stream, bits,values);
    CHECK_ERR(err, "deflatePrime");

    c_stream.next_in = (const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in < len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    c_stream.avail_in = 0;
    do {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    } while (c_stream.total_out < comprLen);

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
    if (err == Z_OK) {
        printf("OK\n");
    }


    return err;
}

/* ===========================================================================
 * Test deflateSetHeader() with small buffers
 */
int test_deflate_set_header(unsigned char *compr, size_t comprLen)
{
    PREFIX(gz_header) *head = calloc(256, 1);
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    size_t len = strlen(hello)+1;


    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    printf("deflateSetHeader(): ");
    /* gzip */
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    CHECK_ERR(err, "deflateInit2");

    head->text = 1;
    err = PREFIX(deflateSetHeader)(&c_stream, head);
    CHECK_ERR(err, "deflateSetHeader");

    c_stream.next_in  = (unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in < len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    c_stream.avail_in = 0;
    do {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    } while (c_stream.total_out < comprLen);

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
    if (err == Z_OK) {
        printf("OK\n");
    }

    free(head);
    return err;
}

/* ===========================================================================
 * Test deflateTune() with small buffers
 */
int test_deflate_tune(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;
    int good_length = 3;
    int max_lazy = 5;
    int nice_length = 18;
    int max_chain = 6;
    size_t len = strlen(hello)+1;


    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (voidpf)0;

    printf("deflateTune(): ");
    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    err = PREFIX(deflateTune)(&c_stream, good_length, max_lazy, nice_length, max_chain);
    CHECK_ERR(err, "deflateTune");

    c_stream.next_in = (const unsigned char *)hello;
    c_stream.next_out = compr;

    while (c_stream.total_in < len && c_stream.total_out < comprLen) {
        c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        CHECK_ERR(err, "deflate");
    }

    /* Finish the stream, still forcing small buffers: */
    c_stream.avail_in = 0;
    do {
        c_stream.avail_out = 1;
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END) break;
        CHECK_ERR(err, "deflate");
    } while (c_stream.total_out < comprLen);

    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");

    if (err == Z_OK) {
        printf("OK\n");
    }

    return err;
}

/* ===========================================================================
 * Test deflate() on a file
 */
int test_file_deflate(char *path, int level, int memLevel, int strategy)
{
    unsigned char *buf, *compr;
    FILE *f = NULL;
    int len = 0;
    int comprLen = 0;
    int err;
    size_t read = 0;


    printf("deflate() %s level %d mem %d: ", path, level, memLevel);

    f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "unable to open file\n");
        return Z_STREAM_ERROR;
    } 
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len == 0) {
        fprintf(stderr, "invalid file length\n");
        return Z_DATA_ERROR;
    } 

    buf = malloc(len);
    if (buf == NULL) {
        fprintf(stderr, "cannot alloc buf\n");
        fclose(f);
        return Z_MEM_ERROR;
    }
    
    read = fread(buf, 1, len, f);
    fclose(f);
    if ((int)read != len) {
        fprintf(stderr, "invalid number of bytes read\n");
        free(buf);
        return Z_BUF_ERROR;
    }

    comprLen = len << 2;
    compr = malloc(comprLen);
    if (compr == NULL) {
        fprintf(stderr, "cannot alloc compr buf\n");
        free(buf);
        return Z_MEM_ERROR;
    }

    err = test_deflate_params(buf, len, compr, comprLen, level, Z_DEFLATED, -MAX_WBITS, memLevel, strategy);
    if (err > 0) {
        printf("OK\n");
        err = Z_OK;
    }
    printf("deflate() %s small level %d mem %d: ", path, level, memLevel);
    err = test_deflate_small_params(buf, len, compr, comprLen, level, Z_DEFLATED, -MAX_WBITS, memLevel, strategy);
    if (err > 0) {
        printf("OK\n");
        err = Z_OK;
    }

    free(buf);
    free(compr);
    return err;
}

/* ===========================================================================
 * Test inflate() on a file
 */
int test_file_inflate(char *path)
{
    unsigned char *compr, *uncompr;
    FILE *f = NULL;
    int comprLen = 0;
    int uncomprLen = 0;
    int err;
    size_t read = 0;
   
    printf("inflate() %s: ", path);

    f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "unable to open file\n");
        return Z_STREAM_ERROR;
    } 
    fseek(f, 0, SEEK_END);
    comprLen = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (comprLen == 0) {
        fprintf(stderr, "invalid file length\n");
        return Z_DATA_ERROR;
    } 

    compr = malloc(comprLen);
    if (compr == NULL) {
        fprintf(stderr, "cannot alloc buf\n");
        fclose(f);
        return Z_MEM_ERROR;
    }
    
    read = fread(compr, 1, comprLen, f);
    fclose(f);
    if ((int)read != comprLen) {
        fprintf(stderr, "invalid number of bytes read\n");
        free(compr);
        return Z_BUF_ERROR;
    }

    uncomprLen = comprLen << 2;
    uncompr = malloc(uncomprLen);
    if (uncompr == NULL) {
        fprintf(stderr, "cannot alloc compr buf\n");
        free(compr);
        return Z_MEM_ERROR;
    }
    
    err = test_inflate_params(compr, comprLen, uncompr, uncomprLen, -MAX_WBITS);
    if (err > 0) {
        printf("OK\n");
        err = Z_OK;
    }
    printf("inflate() %s small: ", path);
    err = test_inflate_small_params(compr, comprLen, uncompr, uncomprLen, -MAX_WBITS);
    if (err > 0) {
        printf("OK\n");
        err = Z_OK;
    }

    free(compr);
    free(uncompr);
    return err;
}

/* ===========================================================================
 * Test deflate() and inflate() on a file
 */
int test_file_deflate_inflate(char *path, int level, int memLevel, int strategy)
{
    unsigned char *buf, *compr, *uncompr;
    FILE *f = NULL;
    int len = 0;
    int comprLen = 0;
    int uncomprLen = 0;
    int err;
    size_t read = 0;


    printf("deflate() %s level %d mem %d: ", path, level, memLevel);

    f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "unable to open file\n");
        return Z_STREAM_ERROR;
    } 
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len == 0) {
        fprintf(stderr, "invalid file length\n");
        return Z_DATA_ERROR;
    } 

    buf = malloc(len);
    if (buf == NULL) {
        fprintf(stderr, "cannot alloc buf\n");
        fclose(f);
        return Z_MEM_ERROR;
    }
    uncomprLen = len;
    uncompr = malloc(uncomprLen);
    if (buf == NULL) {
        fprintf(stderr, "cannot alloc uncompr\n");
        free(buf);
        fclose(f);
        return Z_MEM_ERROR;
    }

    read = fread(buf, 1, len, f);
    fclose(f);
    if ((int)read != len) {
        fprintf(stderr, "invalid number of bytes read\n");
        free(uncompr);
        free(buf);
        return Z_BUF_ERROR;
    }

    comprLen = len << 4;
    compr = malloc(comprLen);
    if (compr == NULL) {
        fprintf(stderr, "cannot alloc compr buf\n");
        free(uncompr);
        free(buf);
        return Z_MEM_ERROR;
    }

    err = test_deflate_params(buf, len, compr, comprLen, level, Z_DEFLATED, -MAX_WBITS, memLevel, strategy);
    if (err > 0) {
        printf("OK\n");
        err = Z_OK;
    }
    printf("deflate() %s small level %d mem %d: ", path, level, memLevel);
    err = test_deflate_small_params(buf, len, compr, comprLen, level, Z_DEFLATED, -MAX_WBITS, memLevel, strategy);
    if (err > 0) {
        printf("OK\n");
        err = Z_OK;
    }
    printf("inflate() %s: ", path);
    err = test_inflate_params(compr, comprLen, uncompr, uncomprLen, -MAX_WBITS);
    if (err > 0) {
        printf("OK\n");
        err = Z_OK;
    }
    printf("inflate() %s small: ", path);
    err = test_inflate_small_params(compr, comprLen, uncompr, uncomprLen, -MAX_WBITS);
    if (err > 0) {
        printf("OK\n");
        err = Z_OK;
    }

    free(buf);
    free(compr);
    free(uncompr);
    return err;
}

/* ===========================================================================
 * Usage:  example [output.gz  [input.gz]]
 */

int main(int argc, char *argv[])
{
    unsigned char *compr, *uncompr;
    size_t comprLen = 10000*sizeof(int); /* don't overflow on MSDOS */
    size_t uncomprLen = comprLen;
    int err = Z_OK;
    int memLevel = 0;
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

    err |= test_compress(compr, comprLen, uncompr, uncomprLen);
    err |= test_gzio((argc > 1 ? argv[1] : "foo.gz"), uncompr, uncomprLen);

    err |= test_deflate_inflate(compr, comprLen, uncompr, uncomprLen);

    err |= test_large_deflate(compr, comprLen, uncompr, uncomprLen, 0);
    err |= test_large_inflate(compr, comprLen, uncompr, uncomprLen);

#ifndef ZLIB_COMPAT
    err |= test_large_deflate(compr, comprLen, uncompr, uncomprLen, 1);
    err |= test_large_inflate(compr, comprLen, uncompr, uncomprLen);
#endif

    err |= test_flush(compr, &comprLen);
    err |= test_sync(compr, comprLen, uncompr, uncomprLen);
    comprLen = uncomprLen;

    err |= test_dict_deflate(compr, comprLen);
    err |= test_dict_inflate(compr, comprLen, uncompr, uncomprLen);
    
    err |= test_deflate_bound();
    err |= test_deflate_copy(compr, comprLen);
    err |= test_deflate_get_dict(compr, comprLen);
    err |= test_deflate_set_header(compr, comprLen);
    err |= test_deflate_tune(compr, comprLen);
    err |= test_deflate_pending(compr, comprLen);
    err |= test_deflate_prime(compr, comprLen);
    
    err |= test_file_deflate_inflate("data/defneg3.dat", Z_BEST_SPEED, 1, Z_DEFAULT_STRATEGY);

    for (memLevel = 1; memLevel <= MAX_MEM_LEVEL; memLevel++) {
        err |= test_file_deflate_inflate("data/lcet10.txt", Z_BEST_SPEED, memLevel, Z_DEFAULT_STRATEGY);
        err |= test_file_deflate_inflate("data/lcet10.txt", Z_DEFAULT_COMPRESSION, memLevel, Z_DEFAULT_STRATEGY);
        err |= test_file_deflate_inflate("data/lcet10.txt", Z_BEST_COMPRESSION, memLevel, Z_DEFAULT_STRATEGY);
    }

    free(compr);
    free(uncompr);

    if (err != Z_OK)
        return 1;

    return 0;
}
