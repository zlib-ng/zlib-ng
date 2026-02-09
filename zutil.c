/* zutil.c -- target dependent utility functions for the compression library
 * Copyright (C) 1995-2017 Jean-loup Gailly
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(HAVE_PTHREAD_H) && defined(WITH_NUMA)
#  define _GNU_SOURCE
#endif

#include "zbuild.h"
#include "zutil_p.h"
#include "zutil.h"

z_const char * const PREFIX(z_errmsg)[10] = {
    (z_const char *)"need dictionary",     /* Z_NEED_DICT       2  */
    (z_const char *)"stream end",          /* Z_STREAM_END      1  */
    (z_const char *)"",                    /* Z_OK              0  */
    (z_const char *)"file error",          /* Z_ERRNO         (-1) */
    (z_const char *)"stream error",        /* Z_STREAM_ERROR  (-2) */
    (z_const char *)"data error",          /* Z_DATA_ERROR    (-3) */
    (z_const char *)"insufficient memory", /* Z_MEM_ERROR     (-4) */
    (z_const char *)"buffer error",        /* Z_BUF_ERROR     (-5) */
    (z_const char *)"incompatible version",/* Z_VERSION_ERROR (-6) */
    (z_const char *)""
};

const char PREFIX3(vstring)[] =
    " zlib-ng 2.3.1";

#ifdef ZLIB_COMPAT
const char * Z_EXPORT zlibVersion(void) {
    return ZLIB_VERSION;
}
#else
const char * Z_EXPORT zlibng_version(void) {
    return ZLIBNG_VERSION;
}
#endif

unsigned long Z_EXPORT PREFIX(zlibCompileFlags)(void) {
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
    /* Bit 13 reserved for DYNAMIC_CRC_TABLE */
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
#  include <stdlib.h>
#  ifndef verbose
#    define verbose 0
#  endif
int Z_INTERNAL z_verbose = verbose;

void Z_INTERNAL z_error(const char *m) {
    fprintf(stderr, "%s\n", m);
    exit(1);
}
#endif

/* exported to allow conversion of error code to string for compress() and
 * uncompress()
 */
const char * Z_EXPORT PREFIX(zError)(z_int32_t err) {
    return ERR_MSG(err);
}

// Zlib-ng's default alloc/free implementation, used unless
// application supplies its own alloc/free functions.
void Z_INTERNAL *PREFIX(zcalloc)(void *opaque, unsigned items, unsigned size) {
    Z_UNUSED(opaque);
#ifdef WITH_NUMA
    return zng_numa_alloc((size_t)items * (size_t)size);
#else
    return zng_alloc((size_t)items * (size_t)size);
#endif
}

void Z_INTERNAL PREFIX(zcfree)(void *opaque, void *ptr) {
    Z_UNUSED(opaque);
#ifdef WITH_NUMA
    zng_numa_free(ptr, 0);
#else
    zng_free(ptr);
#endif
}

/* Provide aligned allocations, only used by gz* code */
void Z_INTERNAL *zng_alloc_aligned(unsigned size, unsigned align) {
    uintptr_t return_ptr, original_ptr, align_diff;
    uint32_t alloc_size;
    void *ptr;

    /* Allocate enough memory for proper alignment and to store the original memory pointer */
    alloc_size = sizeof(void *) + size + align;
#ifdef WITH_NUMA
    ptr = zng_numa_alloc(alloc_size);
#else
    ptr = zng_alloc(alloc_size);
#endif
    if (!ptr)
        return NULL;

    /* Calculate return pointer address with space enough to store original pointer */
    align_diff = ALIGN_DIFF(ptr, align);
    return_ptr = (uintptr_t)ptr + align_diff;
    if (align_diff < sizeof(void *))
        return_ptr += align;

    /* Store the original pointer for free() */
    original_ptr = return_ptr - sizeof(void *);
    memcpy((void *)original_ptr, &ptr, sizeof(void *));

    /* Return properly aligned pointer in allocation */
    return (void *)return_ptr;
}

void Z_INTERNAL zng_free_aligned(void *ptr) {
    if (!ptr)
        return;

    /* Calculate offset to original memory allocation pointer */
    void *original_ptr = (void *)((uintptr_t)ptr - sizeof(void *));
    void *free_ptr = *(void **)original_ptr;

    /* Free original memory allocation */
#ifdef WITH_NUMA
    zng_numa_free(free_ptr, 0);
#else
    zng_free(free_ptr);
#endif
}

#ifdef WITH_NUMA
void Z_INTERNAL *zng_numa_alloc(size_t size) {
    return numa_alloc_local(size);
}

void Z_INTERNAL zng_numa_free(void *ptr, size_t size) {
    numa_free(ptr, size);
}
#endif

#if defined(HAVE_PTHREAD_H)
#ifdef WITH_NUMA
#include <sched.h>
static void *zng_thread_launcher(void *arg) {
    zng_thread *thread = (zng_thread *)arg;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread->node, &cpuset);
    pthread_setaffinity_np(thread->handle, sizeof(cpu_set_t), &cpuset);
    return thread->start_routine(thread->arg);
}
#endif

int Z_INTERNAL zng_thread_create(zng_thread *thread, void *(*start_routine)(void *), void *arg, int node) {
    thread->start_routine = start_routine;
    thread->arg = arg;
    thread->node = node;
#ifdef WITH_NUMA
    return pthread_create(&thread->handle, NULL, zng_thread_launcher, thread);
#else
    return pthread_create(&thread->handle, NULL, start_routine, arg);
#endif
}

void Z_INTERNAL zng_thread_join(zng_thread *thread) {
    pthread_join(thread->handle, NULL);
}
#endif
