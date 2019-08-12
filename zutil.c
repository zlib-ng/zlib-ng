/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2017 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "zbuild.h"
#include "zutil.h"
#ifdef WITH_GZFILEOP
#  include "gzguts.h"
#endif
#ifndef UNALIGNED_OK
#  include "malloc.h"
#endif

const char * const zng_errmsg[10] = {
    (const char *)"need dictionary",     /* Z_NEED_DICT       2  */
    (const char *)"stream end",          /* Z_STREAM_END      1  */
    (const char *)"",                    /* Z_OK              0  */
    (const char *)"file error",          /* Z_ERRNO         (-1) */
    (const char *)"stream error",        /* Z_STREAM_ERROR  (-2) */
    (const char *)"data error",          /* Z_DATA_ERROR    (-3) */
    (const char *)"insufficient memory", /* Z_MEM_ERROR     (-4) */
    (const char *)"buffer error",        /* Z_BUF_ERROR     (-5) */
    (const char *)"incompatible version",/* Z_VERSION_ERROR (-6) */
    (const char *)""
};

const char zlibng_string[] =
   " zlib-ng 1.9.9 forked from zlib 1.2.11 ";

#ifdef ZLIB_COMPAT
const char * ZEXPORT zlibVersion(void)
{
    return ZLIB_VERSION;
}
#endif

const char * ZEXPORT zlibng_version(void)
{
    return ZLIBNG_VERSION;
}

unsigned long ZEXPORT PREFIX(zlibCompileFlags)(void)
{
    unsigned long flags;

    flags = 0;
    switch ((int)(sizeof(unsigned int))) {
    case 2:     break;
    case 4:     flags += 1;     break;
    case 8:     flags += 2;     break;
    default:    flags += 3;
    }
    switch ((int)(sizeof(unsigned long))) {
    case 2:     break;
    case 4:     flags += 1 << 2;        break;
    case 8:     flags += 2 << 2;        break;
    default:    flags += 3 << 2;
    }
    switch ((int)(sizeof(void *))) {
    case 2:     break;
    case 4:     flags += 1 << 4;        break;
    case 8:     flags += 2 << 4;        break;
    default:    flags += 3 << 4;
    }
    switch ((int)(sizeof(z_off_t))) {
    case 2:     break;
    case 4:     flags += 1 << 6;        break;
    case 8:     flags += 2 << 6;        break;
    default:    flags += 3 << 6;
    }
#ifdef ZLIB_DEBUG
    flags += 1 << 8;
#endif
#ifdef ZLIB_WINAPI
    flags += 1 << 10;
#endif
#ifdef DYNAMIC_CRC_TABLE
    flags += 1 << 13;
#endif
#ifdef NO_GZCOMPRESS
    flags += 1L << 16;
#endif
#ifdef NO_GZIP
    flags += 1L << 17;
#endif
#ifdef PKZIP_BUG_WORKAROUND
    flags += 1L << 20;
#endif
    return flags;
}

#ifdef ZLIB_DEBUG
#include <stdlib.h>
#  ifndef verbose
#    define verbose 0
#  endif
int ZLIB_INTERNAL z_verbose = verbose;

void ZLIB_INTERNAL z_error (m)
    char *m;
{
    fprintf(stderr, "%s\n", m);
    exit(1);
}
#endif

/* exported to allow conversion of error code to string for compress() and
 * uncompress()
 */
const char * ZEXPORT PREFIX(zError)(int err)
{
    return ERR_MSG(err);
}

#ifndef MY_ZCALLOC /* Any system without a special alloc function */

void ZLIB_INTERNAL *zng_calloc (void *opaque, unsigned items, unsigned size)
{
    (void)opaque;
#ifndef UNALIGNED_OK
    return memalign(16, items * size);
#else
    return sizeof(unsigned int) > 2 ? (void *)malloc(items * size) :
                              (void *)calloc(items, size);
#endif
}

void ZLIB_INTERNAL zng_cfree (void *opaque, void *ptr)
{
    (void)opaque;
    free(ptr);
}

#endif /* MY_ZCALLOC */
