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

/* ========================================================================== */
/* zng_inflateSync                                                            */
/* ========================================================================== */

/* Compress kMessage as a raw (unwrapped) deflate stream; returns the number
 * of compressed bytes written, or 0 if compression fails. */
static size_t compress_raw(uint8_t *buffer, size_t buffer_size) {
    PREFIX3(stream) strm = zeroed_stream();
    if (PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
                             MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
        return 0;
    strm.next_in = (z_const unsigned char *)kMessage;
    strm.avail_in = (unsigned int)strlen(kMessage);
    strm.next_out = buffer;
    strm.avail_out = (unsigned int)buffer_size;
    int32_t ret = PREFIX(deflate)(&strm, Z_FINISH);
    PREFIX(deflateEnd)(&strm);
    return (ret == Z_STREAM_END) ? (size_t)strm.total_out : 0;
}

/* inflateSync() with a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, sync_null_stream) {
    EXPECT_EQ(PREFIX(inflateSync)(NULL), Z_STREAM_ERROR);
}

/* inflateSync() on a fresh stream with no input and an empty bit buffer:
 * - there is nothing to search, so Z_BUF_ERROR is returned. */
TEST(inflate_api_errors, sync_no_input) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);

    uint8_t output[16];
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflateSync)(&strm), Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflateSync() on input that does not contain the 00 00 ff ff marker:
 * - Z_DATA_ERROR is returned and the searched input is consumed. */
TEST(inflate_api_errors, sync_no_marker) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);

    uint8_t junk[] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t output[16];
    strm.next_in = junk;
    strm.avail_in = (unsigned int)sizeof(junk);
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflateSync)(&strm), Z_DATA_ERROR);
    EXPECT_EQ(strm.total_in, sizeof(junk));
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflateSync() on garbage followed by the sync marker and a valid raw
 * deflate stream:
 * - the marker is found and Z_OK is returned;
 * - inflate() then resumes at the block after the marker and decodes the
 *   original message with Z_STREAM_END. */
TEST(inflate_api_errors, sync_resync_after_garbage) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);

    uint8_t raw[128];
    size_t raw_len = compress_raw(raw, sizeof(raw));
    ASSERT_GT(raw_len, 0u);

    uint8_t feed[256];
    size_t n = 0;
    const uint8_t garbage[] = {0x5a, 0xa5, 0x13};
    memcpy(feed + n, garbage, sizeof(garbage));
    n += sizeof(garbage);
    const uint8_t marker[] = {0x00, 0x00, 0xff, 0xff};
    memcpy(feed + n, marker, sizeof(marker));
    n += sizeof(marker);
    memcpy(feed + n, raw, raw_len);
    n += raw_len;

    uint8_t output[64];
    strm.next_in = feed;
    strm.avail_in = (unsigned int)n;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    ASSERT_EQ(PREFIX(inflateSync)(&strm), Z_OK);

    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_STREAM_END);
    EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
    EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_inflateSyncPoint                                                       */
/* ========================================================================== */

/* inflateSyncPoint() with a NULL stream and on a fresh stream:
 * - the NULL call is rejected with Z_STREAM_ERROR;
 * - a fresh stream is not at a stored-block boundary, so 0 is returned. */
TEST(inflate_api_errors, sync_point) {
    EXPECT_EQ(PREFIX(inflateSyncPoint)(NULL), Z_STREAM_ERROR);

    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(inflateSyncPoint)(&strm), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_inflateSetDictionary / zng_inflateGetDictionary                        */
/* ========================================================================== */

/* A preset dictionary for the FDICT stream tests. */
static const char *const kDictionary = "hello";

/* Compress kMessage with the preset dictionary; returns the number of
 * compressed bytes written, or 0 on failure, and stores the dictionary
 * identifier (adler32 of the dictionary) in *dict_id. */
static size_t compress_with_dictionary(uint8_t *buffer, size_t buffer_size,
                                       unsigned int *dict_id) {
    PREFIX3(stream) strm = zeroed_stream();
    if (PREFIX(deflateInit)(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
        return 0;
    if (PREFIX(deflateSetDictionary)(&strm, (const uint8_t *)kDictionary,
                                     (int)strlen(kDictionary)) != Z_OK) {
        PREFIX(deflateEnd)(&strm);
        return 0;
    }
    *dict_id = (unsigned int)strm.adler;
    strm.next_in = (z_const unsigned char *)kMessage;
    strm.avail_in = (unsigned int)strlen(kMessage);
    strm.next_out = buffer;
    strm.avail_out = (unsigned int)buffer_size;
    int32_t ret = PREFIX(deflate)(&strm, Z_FINISH);
    PREFIX(deflateEnd)(&strm);
    return (ret == Z_STREAM_END) ? (size_t)strm.total_out : 0;
}

/* inflateSetDictionary() on a fresh zlib-wrapped stream that has not reached
 * the dictionary state:
 * - rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, set_dictionary_wrong_state) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    EXPECT_EQ(PREFIX(inflateSetDictionary)(&strm, (const uint8_t *)kDictionary,
                                           (uint32_t)strlen(kDictionary)), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflateSetDictionary() with a wrong dictionary in the DICT state:
 * - the adler32 of the supplied dictionary does not match the one stored in
 *   the stream header, so Z_DATA_ERROR is returned;
 * - the correct dictionary is then accepted and the stream decodes to the
 *   original message. */
TEST(inflate_api_errors, set_dictionary_adler_mismatch) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    uint8_t compressed[128];
    unsigned int dict_id = 0;
    size_t compressed_len = compress_with_dictionary(compressed, sizeof(compressed), &dict_id);
    ASSERT_GT(compressed_len, 0u);

    uint8_t output[64];
    strm.next_in = compressed;
    strm.avail_in = (unsigned int)compressed_len;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    ASSERT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_NEED_DICT);

    const uint8_t wrong[] = "world";
    EXPECT_EQ(PREFIX(inflateSetDictionary)(&strm, wrong, (uint32_t)sizeof(wrong) - 1), Z_DATA_ERROR);

    EXPECT_EQ(PREFIX(inflateSetDictionary)(&strm, (const uint8_t *)kDictionary,
                                           (uint32_t)strlen(kDictionary)), Z_OK);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_STREAM_END);
    EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
    EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflateSetDictionary() on a raw stream and inflateGetDictionary() reading
 * it back:
 * - the dictionary is accepted in any state when no wrapper is expected;
 * - getDictionary reports its length and returns the bytes as stored. */
TEST(inflate_api_errors, set_get_dictionary_raw_roundtrip) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);

    EXPECT_EQ(PREFIX(inflateSetDictionary)(&strm, (const uint8_t *)kDictionary,
                                           (uint32_t)strlen(kDictionary)), Z_OK);

    uint8_t readback[16];
    uint32_t readback_len = 0;
    ASSERT_EQ(PREFIX(inflateGetDictionary)(&strm, readback, &readback_len), Z_OK);
    EXPECT_EQ(readback_len, (uint32_t)strlen(kDictionary));
    EXPECT_EQ(memcmp(readback, kDictionary, strlen(kDictionary)), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflateGetDictionary() on a fresh stream with no dictionary yet:
 * - Z_OK is returned and the reported length is zero;
 * - a NULL dictionary buffer is accepted when nothing is copied. */
TEST(inflate_api_errors, get_dictionary_fresh) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    uint32_t dict_len = 0xdeadbeefu;
    EXPECT_EQ(PREFIX(inflateGetDictionary)(&strm, NULL, &dict_len), Z_OK);
    EXPECT_EQ(dict_len, 0u);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_inflateMark                                                            */
/* ========================================================================== */

/* inflateMark() with a NULL stream and on a fresh stream:
 * - the NULL call returns the -65536 sentinel;
 * - a fresh stream has back == -1 (before the start of the window), so the
 *   same sentinel value is returned. */
TEST(inflate_api_errors, mark_null_and_fresh) {
    EXPECT_EQ(PREFIX(inflateMark)(NULL), -65536L);

    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(inflateMark)(&strm), -65536L);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_inflatePrime                                                           */
/* ========================================================================== */

/* inflatePrime() with a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, prime_null_stream) {
    EXPECT_EQ(PREFIX(inflatePrime)(NULL, 8, 0), Z_STREAM_ERROR);
}

/* inflatePrime() bit-count rules on a fresh stream:
 * - zero bits is a no-op returning Z_OK;
 * - more than 16 bits in a single call is rejected with Z_STREAM_ERROR;
 * - filling the 32-bit accumulator (16 + 16) succeeds, and one more bit is
 *   rejected because it would exceed 32 bits. */
TEST(inflate_api_errors, prime_bit_count_rules) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    EXPECT_EQ(PREFIX(inflatePrime)(&strm, 0, 0), Z_OK);
    EXPECT_EQ(PREFIX(inflatePrime)(&strm, 17, 1), Z_STREAM_ERROR);
    ASSERT_EQ(PREFIX(inflatePrime)(&strm, 16, 0x1234), Z_OK);
    ASSERT_EQ(PREFIX(inflatePrime)(&strm, 16, 0xabcd), Z_OK);
    EXPECT_EQ(PREFIX(inflatePrime)(&strm, 1, 1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflatePrime() with a negative bit count:
 * - the bit accumulator is cleared and Z_OK is returned;
 * - subsequent primes are accepted again. */
TEST(inflate_api_errors, prime_negative_clears) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    ASSERT_EQ(PREFIX(inflatePrime)(&strm, 16, 0x1234), Z_OK);
    EXPECT_EQ(PREFIX(inflatePrime)(&strm, -1, 0), Z_OK);
    EXPECT_EQ(PREFIX(inflatePrime)(&strm, 16, 0xabcd), Z_OK);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_uncompress / zng_uncompress2                                           */
/* ========================================================================== */

/* uncompress2() with a NULL destLen pointer:
 * - in native mode the call is rejected with Z_STREAM_ERROR;
 * - in compat mode a NULL destLen is treated as a zero-byte buffer, so the
 *   stream is decoded into a one-byte probe buffer and the truncated result
 *   is reported as Z_DATA_ERROR. */
TEST(inflate_api_errors, uncompress2_null_dest_len) {
    uint8_t compressed[128];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);

    z_uintmax_t source_len = (z_uintmax_t)compressed_len;
#ifdef ZLIB_COMPAT
    EXPECT_EQ(PREFIX(uncompress2)(NULL, NULL, compressed, &source_len), Z_DATA_ERROR);
#else
    EXPECT_EQ(PREFIX(uncompress2)(NULL, NULL, compressed, &source_len), Z_STREAM_ERROR);
#endif
}

/* uncompress2() with a NULL sourceLen pointer:
 * - in native mode the call is rejected with Z_STREAM_ERROR;
 * - in compat mode a NULL sourceLen is treated as an empty input, so the
 *   call fails with Z_DATA_ERROR. */
TEST(inflate_api_errors, uncompress2_null_source_len) {
    uint8_t compressed[128];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);

    uint8_t output[64];
#ifdef ZLIB_COMPAT
    EXPECT_EQ(PREFIX(uncompress2)(output, NULL, compressed, NULL), Z_DATA_ERROR);
#else
    EXPECT_EQ(PREFIX(uncompress2)(output, NULL, compressed, NULL), Z_STREAM_ERROR);
#endif
}

/* uncompress2() with a NULL source and a non-zero source length:
 * - rejected with Z_STREAM_ERROR in both native and compat mode. */
TEST(inflate_api_errors, uncompress_null_source_with_length) {
    uint8_t output[64];
    z_uintmax_t output_len = sizeof(output);
    z_uintmax_t source_len = 5;

    EXPECT_EQ(PREFIX(uncompress2)(output, &output_len, NULL, &source_len), Z_STREAM_ERROR);
    EXPECT_EQ(output_len, sizeof(output));
}

/* uncompress2() with a NULL output buffer and a non-zero destLen:
 * - rejected with Z_STREAM_ERROR in both native and compat mode. */
TEST(inflate_api_errors, uncompress_null_dest_with_length) {
    z_uintmax_t output_len = 64;
    z_uintmax_t source_len = (z_uintmax_t)strlen(kMessage);

    EXPECT_EQ(PREFIX(uncompress2)(NULL, &output_len, (const uint8_t *)kMessage, &source_len), Z_STREAM_ERROR);
    EXPECT_EQ(output_len, 64u);
}

/* uncompress2() with an empty source:
 * - there is no stream to decode, so Z_DATA_ERROR is returned in both modes;
 * - the reported output length is zero. */
TEST(inflate_api_errors, uncompress_empty_source) {
    uint8_t output[64];
    z_uintmax_t output_len = sizeof(output);
    z_uintmax_t source_len = 0;

    EXPECT_EQ(PREFIX(uncompress2)(output, &output_len, NULL, &source_len), Z_DATA_ERROR);
    EXPECT_EQ(output_len, 0u);
}

/* uncompress2() with an output buffer smaller than the decompressed data:
 * - Z_BUF_ERROR is returned;
 * - destLen reports how much was written before the buffer filled up. */
TEST(inflate_api_errors, uncompress_undersized_output) {
    uint8_t compressed[128];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);

    uint8_t output[64];
    z_uintmax_t output_len = (z_uintmax_t)(strlen(kMessage) - 3); /* leave room for the last bytes */
    z_uintmax_t source_len = (z_uintmax_t)compressed_len;
    EXPECT_EQ(PREFIX(uncompress2)(output, &output_len, compressed, &source_len), Z_BUF_ERROR);
    EXPECT_EQ(output_len, (z_uintmax_t)(strlen(kMessage) - 3));
}

/* uncompress() with an exactly sized output buffer:
 * - Z_OK is returned;
 * - destLen reports the full decompressed length and the data matches. */
TEST(inflate_api_errors, uncompress_exact_output) {
    uint8_t compressed[128];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);

    uint8_t output[64];
    z_uintmax_t output_len = sizeof(output);
    EXPECT_EQ(PREFIX(uncompress)(output, &output_len, compressed, (z_uintmax_t)compressed_len), Z_OK);
    EXPECT_EQ(output_len, (z_uintmax_t)strlen(kMessage));
    EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
}

/* uncompress2() fed only the first bytes of a valid zlib stream:
 * - the incomplete stream is rejected with Z_DATA_ERROR. */
TEST(inflate_api_errors, uncompress_truncated_source) {
    uint8_t compressed[128];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);

    uint8_t output[64];
    z_uintmax_t output_len = sizeof(output);
    z_uintmax_t source_len = 3;
    EXPECT_EQ(PREFIX(uncompress2)(output, &output_len, compressed, &source_len), Z_DATA_ERROR);
}

/* uncompress2() on a stream with a corrupted payload byte:
 * - rejected with Z_DATA_ERROR. */
TEST(inflate_api_errors, uncompress_corrupt_data) {
    uint8_t compressed[128];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);
    compressed[compressed_len / 2] ^= 0xff;

    uint8_t output[64];
    z_uintmax_t output_len = sizeof(output);
    z_uintmax_t source_len = (z_uintmax_t)compressed_len;
    EXPECT_EQ(PREFIX(uncompress2)(output, &output_len, compressed, &source_len), Z_DATA_ERROR);
}

/* ========================================================================== */
/* zng_inflateBackInit / zng_inflateBack / zng_inflateBackEnd                 */
/* ========================================================================== */

/* Input callback state for inflateBack() tests. */
struct back_input {
    uint8_t *data;
    size_t len;
    size_t pos;
    int fail; /* if set, in() reports no input */
};

/* Output callback state for inflateBack() tests. */
struct back_output {
    uint8_t *data;
    size_t size;
    size_t used;
    int fail; /* if set, out() reports a write failure */
};

/* Provide the remaining input in one piece, or nothing when fail is set.
 * The z_const-qualified buffer pointer matches both the native (const)
 * and compat (non-const) in_func signatures. */
static z_uint32_t back_in(void *descriptor, z_const unsigned char **buffer) {
    struct back_input *in = (struct back_input *)descriptor;
    if (in->fail || in->pos >= in->len)
        return 0;
    *buffer = in->data + in->pos;
    uint32_t available = (uint32_t)(in->len - in->pos);
    in->pos = in->len;
    return available;
}

/* Append the output chunk, failing when told to or when the buffer is full. */
static int32_t back_out(void *descriptor, uint8_t *buffer, uint32_t length) {
    struct back_output *out = (struct back_output *)descriptor;
    if (out->fail || out->used + length > out->size)
        return 1;
    memcpy(out->data + out->used, buffer, length);
    out->used += length;
    return 0;
}

/* inflateBackInit() with a NULL stream or a NULL window:
 * - both are rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, back_init_null_arguments) {
    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << 9];

    EXPECT_EQ(PREFIX(inflateBackInit)(NULL, 9, window), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateBackInit)(&strm, 9, NULL), Z_STREAM_ERROR);
}

/* inflateBackInit() with windowBits outside the 8..15 range:
 * - both a too-small and a too-large value are rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, back_init_window_bits_out_of_range) {
    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << 9];

    EXPECT_EQ(PREFIX(inflateBackInit)(&strm, MIN_WBITS - 1, window), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateBackInit)(&strm, MAX_WBITS + 1, window), Z_STREAM_ERROR);
}

/* inflateBackInit() when the first allocation fails:
 * - Z_MEM_ERROR is returned and no state exists. */
TEST(inflate_api_errors, back_init_alloc_failure) {
    PREFIX3(stream) strm = zeroed_stream();
    mem_zone zone;
    memset(&zone, 0, sizeof(zone));
    zone.fail_after = 0; /* fail the very first allocation */
    install_zone(&strm, &zone);
    uint8_t window[1 << 9];

    EXPECT_EQ(PREFIX(inflateBackInit)(&strm, 9, window), Z_MEM_ERROR);
    EXPECT_EQ(zone.allocs, 0);
    EXPECT_TRUE(strm.state == nullptr);
}

/* inflateBack() with a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, back_null_stream) {
    struct back_input in = {};
    struct back_output out = {};
    EXPECT_EQ(PREFIX(inflateBack)(NULL, back_in, &in, back_out, &out), Z_STREAM_ERROR);
}

/* inflateBack() when the input callback reports no data:
 * - Z_BUF_ERROR is returned. */
TEST(inflate_api_errors, back_input_exhausted) {
    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << 9];
    ASSERT_EQ(PREFIX(inflateBackInit)(&strm, 9, window), Z_OK);

    struct back_input in = {};
    in.fail = 1;
    struct back_output out = {};
    EXPECT_EQ(PREFIX(inflateBack)(&strm, back_in, &in, back_out, &out), Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(inflateBackEnd)(&strm), Z_OK);
}

/* inflateBack() when the output callback reports a write failure:
 * - Z_BUF_ERROR is returned. */
TEST(inflate_api_errors, back_output_failure) {
    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << 9];
    ASSERT_EQ(PREFIX(inflateBackInit)(&strm, 9, window), Z_OK);

    uint8_t raw[128];
    size_t raw_len = compress_raw(raw, sizeof(raw));
    ASSERT_GT(raw_len, 0u);

    struct back_input in = {};
    in.data = raw;
    in.len = raw_len;
    struct back_output out = {};
    out.fail = 1;
    EXPECT_EQ(PREFIX(inflateBack)(&strm, back_in, &in, back_out, &out), Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(inflateBackEnd)(&strm), Z_OK);
}

/* inflateBack() on a valid raw deflate stream:
 * - Z_STREAM_END is returned;
 * - the decompressed data matches the original message and all input was
 *   consumed. */
TEST(inflate_api_errors, back_valid_raw_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << 9];
    ASSERT_EQ(PREFIX(inflateBackInit)(&strm, 9, window), Z_OK);

    uint8_t raw[128];
    size_t raw_len = compress_raw(raw, sizeof(raw));
    ASSERT_GT(raw_len, 0u);

    struct back_input in = {};
    in.data = raw;
    in.len = raw_len;
    uint8_t output[64];
    struct back_output out = {};
    out.data = output;
    out.size = sizeof(output);

    EXPECT_EQ(PREFIX(inflateBack)(&strm, back_in, &in, back_out, &out), Z_STREAM_END);
    EXPECT_EQ(out.used, strlen(kMessage));
    EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
    EXPECT_EQ(in.pos, raw_len);
    EXPECT_EQ(PREFIX(inflateBackEnd)(&strm), Z_OK);
}

/* inflateBack() on a stored block whose LEN and NLEN fields disagree:
 * - rejected with Z_DATA_ERROR. The stream is BFINAL=1, BTYPE=00 (stored),
 *   LEN = 5, NLEN = 0. */
TEST(inflate_api_errors, back_corrupt_stored_block) {
    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << 9];
    ASSERT_EQ(PREFIX(inflateBackInit)(&strm, 9, window), Z_OK);

    uint8_t bad[] = {0x01, 0x00, 0x05, 0x00, 0x00, 0x00};
    struct back_input in = {};
    in.data = bad;
    in.len = sizeof(bad);
    uint8_t output[64];
    struct back_output out = {};
    out.data = output;
    out.size = sizeof(output);

    EXPECT_EQ(PREFIX(inflateBack)(&strm, back_in, &in, back_out, &out), Z_DATA_ERROR);
    EXPECT_EQ(PREFIX(inflateBackEnd)(&strm), Z_OK);
}

/* inflateBackEnd() with a NULL stream and on an already ended stream:
 * - the NULL call is rejected with Z_STREAM_ERROR;
 * - the first end of a live stream returns Z_OK;
 * - the second end is rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, back_end_null_and_double) {
    EXPECT_EQ(PREFIX(inflateBackEnd)(NULL), Z_STREAM_ERROR);

    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << 9];
    ASSERT_EQ(PREFIX(inflateBackInit)(&strm, 9, window), Z_OK);
    EXPECT_EQ(PREFIX(inflateBackEnd)(&strm), Z_OK);
    EXPECT_EQ(PREFIX(inflateBackEnd)(&strm), Z_STREAM_ERROR);
}
