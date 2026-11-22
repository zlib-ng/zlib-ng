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
 * - magnitudes 1..MIN_WBITS-1 are rejected with Z_STREAM_ERROR, except where
 *   inflateReset2()'s `windowBits &= MAX_WBITS` fold changes the value (see
 *   comment in the loop);
 * - a raw window smaller than MIN_WBITS (windowBits < -MAX_WBITS) is rejected
 *   with Z_STREAM_ERROR. */
TEST(inflate_api_errors, init2_window_bits_rejected) {
    for (int32_t window_bits = 1; window_bits < MIN_WBITS; window_bits++) {
        /* Non-negative windowBits are folded through `&= MAX_WBITS` before the
         * range check. When MAX_WBITS is not 2^n-1 (the headers allow e.g. 12)
         * that fold can map a too-small value onto 0, which inflateReset2()
         * accepts as its no-window mode. Only assert rejection where the mask
         * leaves the value untouched; with MAX_WBITS == 15 every iteration is
         * checked. */
        if ((window_bits & MAX_WBITS) != window_bits)
            continue;
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
 * - raw windows (-MAX_WBITS..-MIN_WBITS), zlib windows (MIN_WBITS..MAX_WBITS)
 *   and gzip windows (MAX_WBITS + 16) initialize successfully;
 * - MAX_WBITS + 32 selects the combined zlib+gzip wrapper and is the largest
 *   accepted value; inflateReset2() keeps only its low bits, so any larger
 *   offset would mask down below MIN_WBITS in reduced-MAX_WBITS builds;
 * - 0 is accepted as the no-window, no-checks mode;
 * - each successful init can be released with inflateEnd() == Z_OK. */
TEST(inflate_api_errors, init2_window_bits_accepted) {
    int32_t window_bits[] = {-MAX_WBITS, -MIN_WBITS, 0, MIN_WBITS, MAX_WBITS,
                             MAX_WBITS + 16, MAX_WBITS + 32};
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
    /* Force CINFO to 7, the largest window the header can declare, so the
     * stream always advertises more history than the 9 bits requested above.
     * deflateInit() already emits CINFO 7 when MAX_WBITS is 15, but reduced
     * MAX_WBITS builds start lower, so patch it unconditionally. CM occupies
     * the low nibble of CMF and must be left untouched. */
    compressed[0] = (uint8_t)((compressed[0] & 0x0f) | (7 << 4));
    /* Changing CMF invalidates the mod-31 header check, which would make
     * inflate() report "incorrect header check" before it ever looks at the
     * window size. Repair FLG's check bits with the smallest delta in [0, 30]
     * that satisfies the check; subtracting 31 instead of overflowing the byte
     * keeps the check satisfied as well. */
    uint32_t header = (uint32_t)(compressed[0] << 8) | compressed[1];
    uint32_t flg_fix = (31 - (header % 31)) % 31;
    if ((uint32_t)compressed[1] + flg_fix > 255u)
        flg_fix -= 31;
    compressed[1] = (uint8_t)(compressed[1] + flg_fix);

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

/* inflateSetDictionary() with a NULL dictionary:
 * - rejected with Z_STREAM_ERROR for any length instead of reading from the
 *   NULL buffer (symmetric with deflateSetDictionary());
 * - the stream remains usable afterwards.
 *
 * The raw window bits are used so the call reaches the dictionary handling
 * instead of being rejected by the state check. */
TEST(inflate_api_errors, set_dictionary_null_buffer) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);

    EXPECT_EQ(PREFIX(inflateSetDictionary)(&strm, NULL, 5), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateSetDictionary)(&strm, NULL, 0), Z_STREAM_ERROR);
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

/* uncompress() with an output buffer whose capacity is exactly the length of
 * the decompressed message (no slack byte):
 * - Z_OK is returned;
 * - destLen reports the full decompressed length and the data matches. */
TEST(inflate_api_errors, uncompress_exact_output) {
    uint8_t compressed[128];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 0u);

    uint8_t output[64];
    /* Offer exactly strlen(kMessage) bytes: a capacity of sizeof(output) would
     * not exercise the exact-capacity boundary this test is named for. */
    z_uintmax_t output_len = (z_uintmax_t)strlen(kMessage);
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

/* inflateBack() with a NULL input or output callback:
 * - rejected with Z_STREAM_ERROR instead of calling through the NULL
 *   function pointer once input runs out or the window fills;
 * - the stream remains usable afterwards. */
TEST(inflate_api_errors, back_null_callbacks) {
    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << 9];
    ASSERT_EQ(PREFIX(inflateBackInit)(&strm, 9, window), Z_OK);

    struct back_input in = {};
    struct back_output out = {};
    EXPECT_EQ(PREFIX(inflateBack)(&strm, NULL, &in, back_out, &out), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateBack)(&strm, back_in, &in, NULL, &out), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateBackEnd)(&strm), Z_OK);
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

/* ========================================================================== */
/* zng_inflateReset / zng_inflateReset2                                       */
/* ========================================================================== */

/* inflateReset() and inflateReset2() with a NULL stream:
 * - both are rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, reset_null_stream) {
    EXPECT_EQ(PREFIX(inflateReset)(NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateReset2)(NULL, MAX_WBITS), Z_STREAM_ERROR);
}

/* inflateReset2() with windowBits outside the documented ranges:
 * - magnitudes 1..MIN_WBITS-1 are rejected with Z_STREAM_ERROR, except where
 *   the `windowBits &= MAX_WBITS` fold changes the value (see comment);
 * - raw windows below MIN_WBITS (windowBits < -MAX_WBITS) are rejected;
 * - values above the gzip range (>= 48) are rejected;
 * - each rejection leaves the stream usable. */
TEST(inflate_api_errors, reset2_window_bits_rejected) {
    for (int32_t wb = 1; wb < MIN_WBITS; wb++) {
        /* Same fold as in init2_window_bits_rejected: skip values that
         * `&= MAX_WBITS` would rewrite instead of range-checking them. */
        if ((wb & MAX_WBITS) != wb)
            continue;
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
        EXPECT_EQ(PREFIX(inflateReset2)(&strm, wb), Z_STREAM_ERROR);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }

    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(inflateReset2)(&strm, -(MAX_WBITS + 1)), Z_STREAM_ERROR);
    for (int32_t wb = 48; wb <= 63; wb++)
        EXPECT_EQ(PREFIX(inflateReset2)(&strm, wb), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_inflateResetKeep / zng_inflateValidate / zng_inflateUndermine          */
/* ========================================================================== */

/* A preset dictionary and a message that compresses to almost nothing when the
 * dictionary is available as context. Inflating the result therefore requires
 * the dictionary to still be present in the window, which makes the difference
 * between inflateReset() and inflateResetKeep() observable through the public
 * API: the former drops the window, the latter keeps it. */
static const char kPresetDictionary[] =
    "the quick brown fox jumps over the lazy dog. pack my box with five dozen "
    "liquor jugs. sphinx of black quartz judge my vow. how vexingly quick daft "
    "zebras jump!";
static const char kMatchedText[] = "zebras jump! five dozen liquor jugs.";

/* Defined below with the corrupted-data tests; reused here for the gzip case. */
static size_t compress_gzip(uint8_t *buffer, size_t buffer_size);

/* Deflate kMatchedText as a raw stream with kPresetDictionary as context. */
static size_t compress_matched_with_preset(uint8_t *buffer, size_t buffer_size) {
    PREFIX3(stream) strm = zeroed_stream();
    if (PREFIX(deflateInit2)(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
                             MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
        return 0;
    if (PREFIX(deflateSetDictionary)(&strm, (const uint8_t *)kPresetDictionary,
                                     (uint32_t)strlen(kPresetDictionary)) != Z_OK) {
        PREFIX(deflateEnd)(&strm);
        return 0;
    }
    strm.next_in = (z_const unsigned char *)kMatchedText;
    strm.avail_in = (unsigned int)strlen(kMatchedText);
    strm.next_out = buffer;
    strm.avail_out = (unsigned int)buffer_size;
    int32_t ret = PREFIX(deflate)(&strm, Z_FINISH);
    size_t produced = (size_t)strm.total_out;
    PREFIX(deflateEnd)(&strm);
    return (ret == Z_STREAM_END) ? produced : 0;
}

/* inflateResetKeep() parameter and effect checks:
 * - a NULL stream is rejected with Z_STREAM_ERROR;
 * - on a live stream it returns Z_OK and clears msg, data_type and the byte
 *   counters, leaving the stream ready to decode a fresh stream. */
TEST(inflate_api_errors, reset_keep_null_and_basic) {
    EXPECT_EQ(PREFIX(inflateResetKeep)(NULL), Z_STREAM_ERROR);

    uint8_t raw[64];
    size_t raw_len = compress_raw(raw, sizeof(raw));
    ASSERT_GT(raw_len, 0u);

    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);

    uint8_t output[64];
    strm.next_in = raw;
    strm.avail_in = (unsigned int)raw_len;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    ASSERT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_STREAM_END);
    ASSERT_GT(strm.total_out, 0u);

    EXPECT_EQ(PREFIX(inflateResetKeep)(&strm), Z_OK);
    EXPECT_EQ(strm.msg, (char *)NULL);
    EXPECT_EQ(strm.data_type, 0);
    EXPECT_EQ(strm.total_in, 0u);
    EXPECT_EQ(strm.total_out, 0u);

    /* Reusable for a new stream after the reset. */
    strm.next_in = raw;
    strm.avail_in = (unsigned int)raw_len;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_STREAM_END);
    EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
    EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* The distinguishing behaviour between inflateReset() and inflateResetKeep():
 * - a raw stream compressed against a preset dictionary only decodes while that
 *   dictionary is still in the window;
 * - as control, without any dictionary the back-reference has nothing to point
 *   at and structural validation rejects the stream with Z_DATA_ERROR and
 *   "invalid distance too far back" before producing any output;
 * - inflateSetDictionary() loads the preset, reported by inflateGetDictionary();
 * - after inflateResetKeep() the window is retained, so the stream decodes;
 * - after inflateReset() the window is dropped (reported length 0) and the same
 *   stream fails like the control. */
TEST(inflate_api_errors, reset_keep_preserves_window_reset_discards_it) {
    uint8_t raw[64];
    size_t raw_len = compress_matched_with_preset(raw, sizeof(raw));
    ASSERT_GT(raw_len, 0u);
    /* Only possible if the compressor referenced the preset dictionary. */
    ASSERT_LT(raw_len, strlen(kMatchedText));

    /* Control: no dictionary loaded. */
    {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);
        uint8_t output[64];
        strm.next_in = raw;
        strm.avail_in = (unsigned int)raw_len;
        strm.next_out = output;
        strm.avail_out = (unsigned int)sizeof(output);
        EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_DATA_ERROR);
        EXPECT_NE(strstr(strm.msg ? strm.msg : "", "invalid distance too far back"), (char *)NULL);
        EXPECT_EQ(strm.total_out, 0u);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }

    /* inflateResetKeep() keeps the loaded dictionary. */
    {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);
        ASSERT_EQ(PREFIX(inflateSetDictionary)(&strm, (const uint8_t *)kPresetDictionary,
                                                (uint32_t)strlen(kPresetDictionary)), Z_OK);
        uint32_t loaded = 0;
        ASSERT_EQ(PREFIX(inflateGetDictionary)(&strm, NULL, &loaded), Z_OK);
        ASSERT_GT(loaded, 0u);

        EXPECT_EQ(PREFIX(inflateResetKeep)(&strm), Z_OK);
#ifdef S390_DFLTCC_INFLATE
        /* DFLTCC keeps its window in the hardware parameter block, and
         * inflateResetKeep() re-initializes that block, so the preset history is
         * dropped even by ResetKeep. Window retention under DFLTCC is a larger
         * question than this test should settle, and upstream already treats
         * DFLTCC dictionary reporting as special (see test/example.c and
         * test/test_dict.cc), so only the software semantics are asserted here.
         * The error-message half of this test does run on DFLTCC. */
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
#else
        uint32_t kept = 0;
        EXPECT_EQ(PREFIX(inflateGetDictionary)(&strm, NULL, &kept), Z_OK);
        EXPECT_EQ(kept, loaded);

        uint8_t output[64];
        strm.next_in = raw;
        strm.avail_in = (unsigned int)raw_len;
        strm.next_out = output;
        strm.avail_out = (unsigned int)sizeof(output);
        EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_STREAM_END);
        EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMatchedText));
        EXPECT_EQ(memcmp(output, kMatchedText, strlen(kMatchedText)), 0);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
#endif
    }

    /* inflateReset() discards it. */
    {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);
        ASSERT_EQ(PREFIX(inflateSetDictionary)(&strm, (const uint8_t *)kPresetDictionary,
                                                (uint32_t)strlen(kPresetDictionary)), Z_OK);
        uint32_t loaded = 0;
        ASSERT_EQ(PREFIX(inflateGetDictionary)(&strm, NULL, &loaded), Z_OK);
        ASSERT_GT(loaded, 0u);

        EXPECT_EQ(PREFIX(inflateReset)(&strm), Z_OK);
        uint32_t dropped = 1;
        EXPECT_EQ(PREFIX(inflateGetDictionary)(&strm, NULL, &dropped), Z_OK);
        EXPECT_EQ(dropped, 0u);

        uint8_t output[64];
        strm.next_in = raw;
        strm.avail_in = (unsigned int)raw_len;
        strm.next_out = output;
        strm.avail_out = (unsigned int)sizeof(output);
        EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_DATA_ERROR);
        EXPECT_NE(strstr(strm.msg ? strm.msg : "", "invalid distance too far back"), (char *)NULL);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }
}

/* inflateValidate() parameter and effect checks:
 * - a NULL stream is rejected with Z_STREAM_ERROR for either argument;
 * - by default a broken Adler-32 trailer in a zlib stream is rejected with
 *   Z_DATA_ERROR and "incorrect data check";
 * - inflateValidate(strm, 0) clears the validation bit, so the very same bytes
 *   decode to Z_STREAM_END; this covers the zlib (Adler-32) path;
 * - re-enabling with inflateValidate(strm, 1) makes it fail again;
 * - the same holds for the gzip CRC-32 path, which uses a different comparison
 *   in the CHECK state;
 * - on a raw stream there is no checksum to validate, yet both calls are
 *   accepted with Z_OK. */
TEST(inflate_api_errors, validate_null_and_disables_checksum_enforcement) {
    EXPECT_EQ(PREFIX(inflateValidate)(NULL, 0), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateValidate)(NULL, 1), Z_STREAM_ERROR);

    uint8_t compressed[64];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 4u);
    compressed[compressed_len - 1] ^= 0xff; /* break the Adler-32 trailer */

    /* Default: enforced. */
    {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
        uint8_t output[64];
        strm.next_in = compressed;
        strm.avail_in = (unsigned int)compressed_len;
        strm.next_out = output;
        strm.avail_out = (unsigned int)sizeof(output);
        EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_DATA_ERROR);
        EXPECT_NE(strstr(strm.msg ? strm.msg : "", "incorrect data check"), (char *)NULL);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }

    /* Validation off: the corrupt trailer is ignored and the payload decodes. */
    {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
        EXPECT_EQ(PREFIX(inflateValidate)(&strm, 0), Z_OK);
        uint8_t output[64];
        strm.next_in = compressed;
        strm.avail_in = (unsigned int)compressed_len;
        strm.next_out = output;
        strm.avail_out = (unsigned int)sizeof(output);
        EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_STREAM_END);
        EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
        EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }

    /* Validation turned back on. */
    {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);
        EXPECT_EQ(PREFIX(inflateValidate)(&strm, 0), Z_OK);
        EXPECT_EQ(PREFIX(inflateValidate)(&strm, 1), Z_OK);
        uint8_t output[64];
        strm.next_in = compressed;
        strm.avail_in = (unsigned int)compressed_len;
        strm.next_out = output;
        strm.avail_out = (unsigned int)sizeof(output);
        EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_DATA_ERROR);
        EXPECT_NE(strstr(strm.msg ? strm.msg : "", "incorrect data check"), (char *)NULL);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }

    /* Gzip CRC-32 path, validation off. */
    {
        uint8_t gzip_data[128];
        size_t gzip_len = compress_gzip(gzip_data, sizeof(gzip_data));
        ASSERT_GT(gzip_len, 8u);
        gzip_data[gzip_len - 8] ^= 0xff; /* break the CRC-32 trailer */

        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS + 16), Z_OK);
        EXPECT_EQ(PREFIX(inflateValidate)(&strm, 0), Z_OK);
        uint8_t output[64];
        strm.next_in = gzip_data;
        strm.avail_in = (unsigned int)gzip_len;
        strm.next_out = output;
        strm.avail_out = (unsigned int)sizeof(output);
        EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_STREAM_END);
        EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
        EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }

    /* Raw streams have no checksum; the calls are still accepted. */
    {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);
        EXPECT_EQ(PREFIX(inflateValidate)(&strm, 1), Z_OK);
        EXPECT_EQ(PREFIX(inflateValidate)(&strm, 0), Z_OK);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }
}

/* inflateUndermine() returns an error unless the build allows invalid distances:
 * - with INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR defined it accepts a live
 *   stream with Z_OK for both directions of the flag, and rejects a NULL stream
 *   with Z_STREAM_ERROR;
 * - without that macro the function is a stub that ignores both arguments, so
 *   it reports Z_DATA_ERROR even for a NULL stream; asserting Z_STREAM_ERROR
 *   there would be wrong. */
TEST(inflate_api_errors, undermine_returns_error_when_disabled) {
#ifdef INFLATE_ALLOW_INVALID_DISTANCE_TOOFAR_ARRR
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(inflateUndermine)(NULL, 1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateUndermine)(&strm, 1), Z_OK);
    EXPECT_EQ(PREFIX(inflateUndermine)(&strm, 0), Z_OK);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
#else
    EXPECT_EQ(PREFIX(inflateUndermine)(NULL, 1), Z_DATA_ERROR);

    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(inflateUndermine)(&strm, 1), Z_DATA_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
#endif
}

/* ========================================================================== */
/* zng_inflateCopy                                                            */
/* ========================================================================== */

/* inflateCopy() with NULL arguments:
 * - a NULL destination, a NULL source, and both are all rejected with
 *   Z_STREAM_ERROR. */
TEST(inflate_api_errors, copy_null_arguments) {
    PREFIX3(stream) dest = zeroed_stream();
    EXPECT_EQ(PREFIX(inflateCopy)(NULL, &dest), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateCopy)(&dest, NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateCopy)(NULL, NULL), Z_STREAM_ERROR);
}

/* inflateCopy() when the copy's allocation fails:
 * - Z_MEM_ERROR is returned with no allocation performed;
 * - the source stream remains fully usable afterwards.
 *
 * Note that inflateCopy() memcpys the whole source stream (allocators
 * included) before allocating, so the failing zone must be installed on the
 * source to take effect for the copy. */
TEST(inflate_api_errors, copy_alloc_failure) {
    PREFIX3(stream) source = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&source, MAX_WBITS), Z_OK);

    /* install a zone that fails every allocation on the source stream */
    mem_zone zone;
    memset(&zone, 0, sizeof(zone));
    zone.fail_after = 0; /* fail the very first allocation */
    install_zone(&source, &zone);

    PREFIX3(stream) dest = zeroed_stream();
    EXPECT_EQ(PREFIX(inflateCopy)(&dest, &source), Z_MEM_ERROR);
    EXPECT_EQ(zone.allocs, 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&source), Z_OK);
}

/* ========================================================================== */
/* zng_inflateGetHeader                                                       */
/* ========================================================================== */

/* inflateGetHeader() with a NULL stream and on non-gzip streams:
 * - the NULL call is rejected with Z_STREAM_ERROR;
 * - header collection is only supported for gzip-wrapped streams, so zlib-
 *   and raw-wrapped streams are rejected with Z_STREAM_ERROR. */
TEST(inflate_api_errors, get_header_wrong_stream) {
    PREFIX(gz_header) head;
    memset(&head, 0, sizeof(head));
    EXPECT_EQ(PREFIX(inflateGetHeader)(NULL, &head), Z_STREAM_ERROR);

    const int32_t non_gzip[] = {MAX_WBITS, -MAX_WBITS};
    for (size_t i = 0; i < sizeof(non_gzip) / sizeof(non_gzip[0]); i++) {
        PREFIX3(stream) strm = zeroed_stream();
        ASSERT_EQ(PREFIX(inflateInit2)(&strm, non_gzip[i]), Z_OK);
        EXPECT_EQ(PREFIX(inflateGetHeader)(&strm, &head), Z_STREAM_ERROR);
        EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    }
}

/* inflateGetHeader() with a NULL gz_header on a gzip-wrapped stream:
 * - rejected with Z_STREAM_ERROR instead of dereferencing the NULL head;
 * - the stream remains usable afterwards. */
TEST(inflate_api_errors, get_header_null_head) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS + 16), Z_OK);

    EXPECT_EQ(PREFIX(inflateGetHeader)(&strm, NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* Corrupted stream data: checksums and unwrapped streams                     */
/* ========================================================================== */

/* Compress kMessage as a gzip-wrapped stream; returns the number of compressed
 * bytes written, or 0 if compression fails. */
static size_t compress_gzip(uint8_t *buffer, size_t buffer_size) {
    PREFIX3(stream) strm = zeroed_stream();
    if (PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16,
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

/* inflate() on a zlib stream whose Adler-32 trailer does not match the data:
 * - rejected with Z_DATA_ERROR and the "incorrect data check" message;
 * - the whole message was still decoded before the trailer was examined, so
 *   total_out reports the full length (a checksum failure is not a decode
 *   failure);
 * - only the Adler-32 value is wrong, so the compressed payload itself stays
 *   valid and the stream reaches CHECK before failing. */
TEST(inflate_api_errors, inflate_adler32_mismatch) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS), Z_OK);

    uint8_t compressed[64];
    size_t compressed_len = compress_message(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 4u);
    /* Corrupt one byte of the big-endian Adler-32 trailer. */
    compressed[compressed_len - 1] ^= 0xff;

    uint8_t output[64];
    strm.next_in = compressed;
    strm.avail_in = (unsigned int)compressed_len;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_NE(strstr(strm.msg ? strm.msg : "", "incorrect data check"), (char *)NULL);
    EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a gzip stream whose CRC-32 trailer does not match the data:
 * - rejected with Z_DATA_ERROR and the same "incorrect data check" message
 *   used for Adler-32, since only the accumulator differs;
 * - the full message is decoded before the trailer check fails. */
TEST(inflate_api_errors, inflate_crc32_mismatch) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS + 16), Z_OK);

    uint8_t compressed[128];
    size_t compressed_len = compress_gzip(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 8u);
    /* The gzip trailer is CRC-32 (little-endian) followed by ISIZE; corrupt
     * the low byte of the CRC while leaving ISIZE intact. */
    compressed[compressed_len - 8] ^= 0xff;

    uint8_t output[64];
    strm.next_in = compressed;
    strm.avail_in = (unsigned int)compressed_len;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_NE(strstr(strm.msg ? strm.msg : "", "incorrect data check"), (char *)NULL);
    EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a gzip stream whose ISIZE trailer does not match the amount of
 * data produced:
 * - the CRC-32 check passes, so the failure is reported by the length check
 *   with "incorrect length check"; this is a distinct error from the checksum
 *   failure above. */
TEST(inflate_api_errors, inflate_gzip_isize_mismatch) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS + 16), Z_OK);

    uint8_t compressed[128];
    size_t compressed_len = compress_gzip(compressed, sizeof(compressed));
    ASSERT_GT(compressed_len, 8u);
    /* Corrupt the low byte of the little-endian ISIZE field (last four bytes). */
    compressed[compressed_len - 4] ^= 0xff;

    uint8_t output[64];
    strm.next_in = compressed;
    strm.avail_in = (unsigned int)compressed_len;
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_NE(strstr(strm.msg ? strm.msg : "", "incorrect length check"), (char *)NULL);
    EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a raw (unwrapped) stream with trailing bytes appended:
 * - a valid raw stream ends with Z_STREAM_END; there is no wrapper, so no
 *   Adler-32 or CRC-32 trailer exists and none is demanded;
 * - the appended bytes are never examined: inflate() stops at the end-of-block
 *   code and reports them as still-available input rather than as corrupt
 *   data. This is what distinguishes raw mode from the wrapped cases above,
 *   where the same trailing garbage produced "incorrect data check". */
TEST(inflate_api_errors, inflate_raw_has_no_checksum_trailer) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);

    uint8_t raw[128];
    size_t raw_len = compress_raw(raw, sizeof(raw));
    ASSERT_GT(raw_len, 0u);

    uint8_t feed[160];
    memcpy(feed, raw, raw_len);
    /* Four bytes that would have corrupted an Adler-32 trailer. */
    feed[raw_len] = 0xff; feed[raw_len + 1] = 0xff; feed[raw_len + 2] = 0xff; feed[raw_len + 3] = 0xff;

    uint8_t output[64];
    strm.next_in = feed;
    strm.avail_in = (unsigned int)(raw_len + 4);
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_STREAM_END);
    EXPECT_EQ(strm.total_out, (uint32_t)strlen(kMessage));
    EXPECT_EQ(memcmp(output, kMessage, strlen(kMessage)), 0);
    /* The garbage was left unconsumed. */
    EXPECT_EQ(strm.avail_in, 4u);
    EXPECT_EQ(strm.total_in, (uint64_t)raw_len);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a raw stream whose block header names an undefined block type:
 * - without any wrapper there is no checksum to catch corruption, so the only
 *   thing that can reject the stream is structural validation;
 * - BTYPE = 11 is reserved, giving Z_DATA_ERROR with "invalid block type". */
TEST(inflate_api_errors, inflate_raw_invalid_block_type) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);

    /* BFINAL = 1, BTYPE = 11 (reserved) in the first byte. */
    uint8_t bad[] = {0x07, 0x00};
    uint8_t output[64];
    strm.next_in = bad;
    strm.avail_in = (unsigned int)sizeof(bad);
    strm.next_out = output;
    strm.avail_out = (unsigned int)sizeof(output);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_NE(strstr(strm.msg ? strm.msg : "", "invalid block type"), (char *)NULL);
    EXPECT_EQ(strm.total_out, 0u);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* Gzip wrapper header errors                                                 */
/* ========================================================================== */

/* inflate() on a gzip stream whose compression method byte is not deflate:
 * - Z_DATA_ERROR is returned with the "unknown compression method" message. */
TEST(inflate_api_errors, gzip_unknown_method) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS + 16), Z_OK);

    uint8_t in[] = {0x1f, 0x8b, 0x00, 0x00};
    uint8_t out[16];
    strm.next_in = in;
    strm.avail_in = sizeof(in);
    strm.next_out = out;
    strm.avail_out = sizeof(out);

    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_STREQ(strm.msg, "unknown compression method");
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a gzip stream with reserved header flag bits set:
 * - Z_DATA_ERROR is returned with the "unknown header flags set" message. */
TEST(inflate_api_errors, gzip_reserved_flags) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS + 16), Z_OK);

    uint8_t in[] = {0x1f, 0x8b, 0x08, 0x80};
    uint8_t out[16];
    strm.next_in = in;
    strm.avail_in = sizeof(in);
    strm.next_out = out;
    strm.avail_out = sizeof(out);

    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_STREQ(strm.msg, "unknown header flags set");
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflate() on a gzip stream with FHCRC set and a wrong header CRC:
 * - Z_DATA_ERROR is returned with the "header crc mismatch" message. */
TEST(inflate_api_errors, gzip_header_crc_mismatch) {
    PREFIX3(stream) strm = zeroed_stream();
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, MAX_WBITS + 16), Z_OK);

    /* magic, CM=8, FLG=FHCRC, mtime=0, XFL=0, OS=255, bogus header CRC */
    uint8_t in[] = {0x1f, 0x8b, 0x08, 0x02, 0, 0, 0, 0, 0, 0xff, 0x34, 0x12};
    uint8_t out[16];
    strm.next_in = in;
    strm.avail_in = sizeof(in);
    strm.next_out = out;
    strm.avail_out = sizeof(out);

    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_STREQ(strm.msg, "header crc mismatch");
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

#ifdef ZLIB_COMPAT
/* ========================================================================== */
/* Version validation (compat mode only: the underscore init variants are     */
/* only declared by the compat header)                                        */
/* ========================================================================== */

/* inflateInit_() with a wrong version string, a NULL version, or a wrong
 * stream size:
 * - all three mismatches are rejected with Z_VERSION_ERROR;
 * - the correct pair initializes successfully. */
TEST(inflate_api_errors, init_version_validation) {
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_EQ(PREFIX(inflateInit_)(&strm, "0", (int)sizeof(strm)), Z_VERSION_ERROR);
    EXPECT_EQ(PREFIX(inflateInit_)(&strm, NULL, (int)sizeof(strm)), Z_VERSION_ERROR);
    EXPECT_EQ(PREFIX(inflateInit_)(&strm, PREFIX2(VERSION), (int)sizeof(strm) - 1), Z_VERSION_ERROR);
    ASSERT_EQ(PREFIX(inflateInit_)(&strm, PREFIX2(VERSION), (int)sizeof(strm)), Z_OK);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* inflateBackInit_() with a wrong version string:
 * - rejected with Z_VERSION_ERROR before any allocation;
 * - the correct pair initializes successfully. */
TEST(inflate_api_errors, back_init_version_validation) {
    PREFIX3(stream) strm = zeroed_stream();
    uint8_t window[1 << MAX_WBITS];
    EXPECT_EQ(PREFIX(inflateBackInit_)(&strm, MAX_WBITS, window, "0", 0), Z_VERSION_ERROR);
    ASSERT_EQ(PREFIX(inflateBackInit_)(&strm, MAX_WBITS, window, PREFIX2(VERSION),
                                      (int)sizeof(strm)), Z_OK);
    EXPECT_EQ(PREFIX(inflateBackEnd)(&strm), Z_OK);
}
#endif
