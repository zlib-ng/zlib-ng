/* test_inflate_api_errors.cc - Verify that the inflate-family API functions reject invalid parameters */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

/* A zeroed stream stands in for an uninitialized (or already ended) stream. */
static PREFIX3(stream) zeroed_stream(void) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    return strm;
}

/* ========================================================================== */
/* Induced allocation failures (after test/infcover.c)                        */
/* ========================================================================== */

/* A memory zone that counts allocations and can be told to fail after a
 * number of successful ones. Allocations are filled with 0xa5 so the code is
 * not allowed to rely on zeroed memory, as in infcover. */
struct mem_zone {
    int allocs;      /* successful allocations so far */
    int frees;       /* frees so far */
    int fail_after;  /* fail once this many allocations have succeeded */
};

static void *zone_alloc(void *opaque, unsigned int count, unsigned int size) {
    mem_zone *zone = (mem_zone *)opaque;
    if (zone == NULL || zone->allocs >= zone->fail_after)
        return NULL;
    void *ptr = malloc((size_t)count * size);
    if (ptr == NULL)
        return NULL;
    memset(ptr, 0xa5, (size_t)count * size);
    zone->allocs++;
    return ptr;
}

static void zone_free(void *opaque, void *ptr) {
    mem_zone *zone = (mem_zone *)opaque;
    if (zone == NULL)
        return;
    /* Count real frees only; free(NULL) is a no-op. */
    zone->frees += (ptr != NULL);
    free(ptr);
}

/* Point the stream's allocator at the zone. */
static void install_zone(PREFIX3(stream) *strm, mem_zone *zone) {
    strm->zalloc = zone_alloc;
    strm->zfree = zone_free;
    strm->opaque = zone;
}

/* ========================================================================== */
/* zng_inflateInit / zng_inflateInit2                                         */
/* ========================================================================== */

/* inflateInit() and inflateInit2() with a NULL stream:
 * - both are rejected with Z_STREAM_ERROR without touching anything. */
TEST(inflate_api_errors, init_null_stream) {
    EXPECT_EQ(PREFIX(inflateInit)(NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateInit2)(NULL, MAX_WBITS), Z_STREAM_ERROR);
}

/* inflateInit2() with windowBits outside the documented ranges:
 * - magnitudes 1..MIN_WBITS-1 are rejected with Z_STREAM_ERROR;
 * - a raw window smaller than MIN_WBITS (windowBits < -MAX_WBITS) is rejected
 *   with Z_STREAM_ERROR. */
TEST(inflate_api_errors, init2_window_bits_rejected) {
    for (int32_t window_bits = 1; window_bits < MIN_WBITS; window_bits++) {
        PREFIX3(stream) strm = zeroed_stream();
        EXPECT_EQ(PREFIX(inflateInit2)(&strm, window_bits), Z_STREAM_ERROR) << "windowBits=" << window_bits;
    }
    {
        PREFIX3(stream) strm = zeroed_stream();
        EXPECT_EQ(PREFIX(inflateInit2)(&strm, -(MAX_WBITS + 1)), Z_STREAM_ERROR);
    }
}

/* inflateInit2() with the boundary values of every documented windowBits
 * range:
 * - raw windows (-15..-8), zlib windows (8..15) and gzip windows (16..31)
 *   initialize successfully;
 * - 0 is accepted as the no-window, no-checks mode;
 * - each successful init can be released with inflateEnd() == Z_OK. */
TEST(inflate_api_errors, init2_window_bits_accepted) {
    int32_t window_bits[] = {-MAX_WBITS, -MIN_WBITS, 0, MIN_WBITS, MAX_WBITS,
                             MAX_WBITS + 16, MAX_WBITS + 16 + (MAX_WBITS - 1)};
    for (size_t i = 0; i < sizeof(window_bits) / sizeof(window_bits[0]); i++) {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, window_bits[i]), Z_OK) << "windowBits=" << window_bits[i];
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK) << "windowBits=" << window_bits[i];
    }
}

/* inflateInit() when the first allocation fails:
 * - Z_MEM_ERROR is returned;
 * - nothing was allocated, so nothing may be leaked and no state exists. */
TEST(inflate_api_errors, init_alloc_failure) {
    PREFIX3(stream) strm = zeroed_stream();
    mem_zone zone;
    memset(&zone, 0, sizeof(zone));
    zone.fail_after = 0; /* fail the very first allocation */
    install_zone(&strm, &zone);

    EXPECT_EQ(PREFIX(inflateInit)(&strm), Z_MEM_ERROR);
    /* Nothing was allocated, so nothing may be leaked, and no state exists. */
    EXPECT_EQ(zone.allocs, 0);
    EXPECT_EQ(zone.frees, 0);
    EXPECT_TRUE(strm.state == nullptr);
}

/* inflateInit() after a failed initialization:
 * - the first init fails with Z_MEM_ERROR;
 * - the same stream initializes fine once allocation succeeds, using exactly
 *   one allocation (inflate uses a single combined buffer);
 * - inflateEnd releases that single allocation. */
TEST(inflate_api_errors, init_recover_after_alloc_failure) {
    PREFIX3(stream) strm = zeroed_stream();
    mem_zone zone;
    memset(&zone, 0, sizeof(zone));
    zone.fail_after = 0;
    install_zone(&strm, &zone);

    ASSERT_EQ(PREFIX(inflateInit)(&strm), Z_MEM_ERROR);

    /* The same stream works once the allocation succeeds. */
    zone.fail_after = 1;
    EXPECT_EQ(PREFIX(inflateInit)(&strm), Z_OK);
    EXPECT_EQ(zone.allocs, 1);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    EXPECT_EQ(zone.frees, 1);
}
