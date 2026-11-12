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

/* ========================================================================== */
/* zng_inflate / zng_inflateEnd                                               */
/* ========================================================================== */

/* A fixed message used to build zlib streams for the inflate() tests. */
static const char *const kMessage = "hello inflate world";

/* Compress kMessage into buffer with default settings; returns the number of
 * compressed bytes written, or 0 if compression fails. */
static size_t compress_message(uint8_t *buffer, size_t buffer_size) {
    PREFIX3(stream) strm = zeroed_stream();
    if (PREFIX(deflateInit)(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
        return 0;
    strm.next_in = (z_const unsigned char *)kMessage;
    strm.avail_in = (unsigned int)strlen(kMessage);
    strm.next_out = buffer;
    strm.avail_out = (unsigned int)buffer_size;
    int32_t ret = PREFIX(deflate)(&strm, Z_FINISH);
    PREFIX(deflateEnd)(&strm);
    return (ret == Z_STREAM_END) ? (size_t)strm.total_out : 0;
}

/* inflate() with a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, inflate_null_stream) {
    EXPECT_EQ(PREFIX(inflate)(NULL, Z_NO_FLUSH), Z_STREAM_ERROR);
}

/* inflate() on an initialized stream with a NULL output buffer:
 * - rejected with Z_STREAM_ERROR even though input is available. */
TEST(inflate_api_errors, inflate_null_output_buffer) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
    uint8_t compressed[64];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);

    strm.next_in = compressed;
    strm.avail_in = (unsigned int)compressed_len;
    strm.next_out = NULL;
    strm.avail_out = 64;
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on an initialized stream with a NULL input pointer but non-zero
 * avail_in:
 * - rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, inflate_null_input_with_avail) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    strm.next_in = NULL;
    strm.avail_in = 4;
    uint8_t output[64];
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a fresh stream with no input and an available output buffer:
 * - the call makes no progress and returns Z_BUF_ERROR;
 * - a NULL next_in with avail_in == 0 is legal (this is how callers wait for
 *   more input), so the error must come from the no-progress rule, not the
 *   entry checks; nothing is consumed or produced. */
TEST(inflate_api_errors, inflate_no_progress) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    uint8_t output[64];
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_BUF_ERROR);
    EXPECT_EQ(strm.total_in, 0u);
    EXPECT_EQ(strm.total_out, 0u);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a zlib stream whose header fails the mod-31 check:
 * - rejected with Z_DATA_ERROR and the "incorrect header check" message;
 * - only the two header bytes are consumed. */
TEST(inflate_api_errors, inflate_corrupt_header_check) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
    uint8_t compressed[64];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);
    /* Flip the lowest FLG bit so (CMF*256 + FLG) % 31 != 0. */
    compressed[1] ^= 0x01;

    uint8_t output[64];
    strm.next_in = compressed;
    strm.avail_in = (unsigned int)compressed_len;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_NE(strstr(strm.msg ? strm.msg : "", "incorrect header check"), (char *)NULL);
    EXPECT_EQ(strm.total_in, 2u);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a stream whose header passes the mod-31 check but names a
 * compression method other than deflate:
 * - rejected with Z_DATA_ERROR and the "unknown compression method" message.
 * CMF = 0x77 (method 7, CINFO 7) with FLG = 0x09 satisfies (0x7709) % 31 == 0. */
TEST(inflate_api_errors, inflate_unknown_compression_method) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    uint8_t header[] = {0x77, 0x09, 0x00};
    uint8_t output[64];
    strm.next_in = header;
    strm.avail_in = (unsigned int)sizeof(header);
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_NE(strstr(strm.msg ? strm.msg : "", "unknown compression method"), (char *)NULL);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a stream whose header declares a window larger than the one
 * requested at initialization:
 * - rejected with Z_DATA_ERROR and the "invalid window size" message, before
 *   the oversized window bits can be stored in the state. */
TEST(inflate_api_errors, inflate_window_larger_than_requested) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, 9), Z_OK);
    uint8_t compressed[64];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);
    /* Force CINFO to 7 (32K window) in the CMF byte. */
    compressed[0] = (uint8_t)((compressed[0] & 0x0f) | 0x70);

    uint8_t output[64];
    strm.next_in = compressed;
    strm.avail_in = (unsigned int)compressed_len;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_NE(strstr(strm.msg ? strm.msg : "", "invalid window size"), (char *)NULL);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() fed only the first bytes of a valid zlib stream:
 * - with Z_NO_FLUSH the partial input is accepted and Z_OK is returned with
 *   no output yet;
 * - feeding the rest then completes the stream with Z_STREAM_END and the
 *   original message. */
TEST(inflate_api_errors, inflate_truncated_stream_resumes) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
    uint8_t compressed[64];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);

    uint8_t output[64];
    strm.next_in = compressed;
    strm.avail_in = 3; /* header plus one bit of block data */
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    ASSERT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_OK);
    EXPECT_EQ(strm.total_out, 0u);

    /* inflate() has already advanced next_in; expose the remaining input. */
    strm.avail_in = (unsigned int)(compressed_len - strm.total_in);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_STREAM_END);
    EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
    EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflateEnd() with a NULL stream and on an already ended stream:
 * - the NULL call is rejected with Z_STREAM_ERROR;
 * - the first end of a live stream returns Z_OK;
 * - the second end is rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, inflate_end_null_and_double) {
    EXPECT_EQ(PREFIX(inflateEnd)(NULL), Z_STREAM_ERROR);

    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_STREAM_ERROR);
}
