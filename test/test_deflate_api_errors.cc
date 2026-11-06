/* test_deflate_api_errors.cc - Verify that the deflate-family API functions reject invalid parameters */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>

/* A zeroed stream stands in for an uninitialized (or already ended) stream. */
static PREFIX3(stream) zeroed_stream(void) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    return strm;
}

/* Initialize a zlib-wrapped stream with the given window size. The stream is zeroed first, as callers must. */
static int32_t init_deflate(PREFIX3(stream) *strm, int32_t window_bits) {
    memset(strm, 0, sizeof(*strm));
    return PREFIX(deflateInit2)(strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, window_bits, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
}

/* Initialize a gzip-wrapped stream (windowBits > MAX_WBITS selects the gzip wrapper). */
static int32_t init_gzip(PREFIX3(stream) *strm) {
    memset(strm, 0, sizeof(*strm));
    return PREFIX(deflateInit2)(strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
}

/* Feed a little input so the stream leaves INIT_STATE and becomes BUSY. */
static int32_t start_compression(PREFIX3(stream) *strm) {
    uint8_t output[256];
    strm->next_in = (z_const unsigned char *)"hello, world";
    strm->avail_in = 12;
    strm->next_out = output;
    strm->avail_out = (unsigned int)sizeof(output);
    return PREFIX(deflate)(strm, Z_NO_FLUSH);
}

/* ========================================================================== */
/* zng_compress                                                               */
/* ========================================================================== */

/* compress() with a NULL destLen pointer:
 * - in native mode the call is rejected with Z_STREAM_ERROR;
 * - in compat mode a NULL destLen is treated as a zero-byte buffer, so the
 *   call runs out of output space and returns Z_BUF_ERROR;
 * - the caller's output length is left untouched either way. */
TEST(deflate_api_errors, compress_null_dest_len) {
    uint8_t output[64];
    z_uintmax_t output_length = sizeof(output);
#ifdef ZLIB_COMPAT
    /* The compat wrapper treats a NULL destLen as a zero-byte buffer. */
    EXPECT_EQ(PREFIX(compress)(output, NULL, (const uint8_t *)"hello", 5), Z_BUF_ERROR);
#else
    EXPECT_EQ(PREFIX(compress)(output, NULL, (const uint8_t *)"hello", 5), Z_STREAM_ERROR);
#endif
    EXPECT_EQ(output_length, sizeof(output));
}

/* zng_compress with a NULL source and a non-zero sourceLen:
 * - rejected with Z_STREAM_ERROR;
 * - the error path does not touch destLen. */
TEST(deflate_api_errors, compress_null_source_with_length) {
    uint8_t output[64];
    z_uintmax_t output_length = sizeof(output);
    EXPECT_EQ(PREFIX(compress)(output, &output_length, NULL, 5), Z_STREAM_ERROR);
    EXPECT_EQ(output_length, sizeof(output));
}

/* zng_compress with a NULL output buffer and a non-zero destLen:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, compress_null_output_with_length) {
    z_uintmax_t output_length = 64;
    EXPECT_EQ(PREFIX(compress)(NULL, &output_length, (const uint8_t *)"hello", 5), Z_STREAM_ERROR);
}

/* zng_compress with a zero-length source:
 * - a NULL source is legal when sourceLen is 0, the one case the
 *   validation allows;
 * - the empty stream compresses successfully (Z_OK). */
TEST(deflate_api_errors, compress_empty_source) {
    uint8_t output[64];
    z_uintmax_t output_length = sizeof(output);
    EXPECT_EQ(PREFIX(compress)(output, &output_length, NULL, 0), Z_OK);
}

/* ========================================================================== */
/* zng_compress2                                                              */
/* ========================================================================== */

/* compress2() with a NULL destLen pointer:
 * - in native mode the call is rejected with Z_STREAM_ERROR;
 * - in compat mode a NULL destLen is treated as a zero-byte buffer, so the
 *   call runs out of output space and returns Z_BUF_ERROR. */
TEST(deflate_api_errors, compress2_null_dest_len) {
    uint8_t output[64];
#ifdef ZLIB_COMPAT
    /* The compat wrapper treats a NULL destLen as a zero-byte buffer. */
    EXPECT_EQ(PREFIX(compress2)(output, NULL, (const uint8_t *)"hello", 5, Z_DEFAULT_COMPRESSION), Z_BUF_ERROR);
#else
    EXPECT_EQ(PREFIX(compress2)(output, NULL, (const uint8_t *)"hello", 5, Z_DEFAULT_COMPRESSION), Z_STREAM_ERROR);
#endif
}

/* zng_compress2 with a NULL source and a non-zero sourceLen:
 * - rejected with Z_STREAM_ERROR;
 * - the error path does not touch destLen. */
TEST(deflate_api_errors, compress2_null_source_with_length) {
    uint8_t output[64];
    z_uintmax_t output_length = sizeof(output);
    EXPECT_EQ(PREFIX(compress2)(output, &output_length, NULL, 5, Z_DEFAULT_COMPRESSION), Z_STREAM_ERROR);
    EXPECT_EQ(output_length, sizeof(output));
}

/* zng_compress2 with a NULL output buffer and a non-zero destLen:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, compress2_null_output_with_length) {
    z_uintmax_t output_length = 64;
    EXPECT_EQ(PREFIX(compress2)(NULL, &output_length, (const uint8_t *)"hello", 5, Z_DEFAULT_COMPRESSION), Z_STREAM_ERROR);
}

/* zng_compress2 with out-of-range compression levels:
 * - level -2, below the valid range, is rejected with Z_STREAM_ERROR;
 * - level 10, above the valid range, is rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, compress2_invalid_level) {
    uint8_t output[64];
    z_uintmax_t output_length = sizeof(output);
    EXPECT_EQ(PREFIX(compress2)(output, &output_length, (const uint8_t *)"hello", 5, -2), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(compress2)(output, &output_length, (const uint8_t *)"hello", 5, 10), Z_STREAM_ERROR);
}

/* zng_compress2 whose output buffer cannot hold the compressed data:
 * - 64 bytes of input do not fit in a 4-byte buffer -> Z_BUF_ERROR. */
TEST(deflate_api_errors, compress2_small_output) {
    uint8_t input[64];
    uint8_t output[4];
    z_uintmax_t output_length = sizeof(output);
    memset(input, 'a', sizeof(input));
    EXPECT_EQ(PREFIX(compress2)(output, &output_length, input, sizeof(input), Z_DEFAULT_COMPRESSION), Z_BUF_ERROR);
}

/* ========================================================================== */
/* zng_compressBound                                                          */
/* ========================================================================== */

/* zng_compressBound sanity check across source sizes (0 to 65536):
 * - the bound is always at least as large as the source. */
TEST(deflate_api_errors, compress_bound_sanity) {
    z_uintmax_t source_sizes[] = { 0, 1, 8, 64, 1024, 65536 };
    for (size_t index = 0; index < sizeof(source_sizes) / sizeof(source_sizes[0]); index++) {
        EXPECT_GE(PREFIX(compressBound)(source_sizes[index]), source_sizes[index]);
    }
}

/* ========================================================================== */
/* zng_deflate                                                                */
/* ========================================================================== */

/* zng_deflate on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_null_stream) {
    EXPECT_EQ(PREFIX(deflate)(NULL, Z_NO_FLUSH), Z_STREAM_ERROR);
}

/* zng_deflate on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_EQ(PREFIX(deflate)(&strm, Z_NO_FLUSH), Z_STREAM_ERROR);
}

/* zng_deflate with out-of-range flush values on an initialized stream:
 * - flush -1, below Z_NO_FLUSH, is rejected with Z_STREAM_ERROR;
 * - flush Z_BLOCK + 1, above the maximum, is rejected with Z_STREAM_ERROR;
 * - a writable output buffer with space is provided, so the rejection can
 *   only come from the flush-range validation itself (a NULL next_out would
 *   be rejected independently);
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_flush_out_of_range) {
    PREFIX3(stream) strm;
    uint8_t output[64];
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    strm.next_out = output;
    strm.avail_out = sizeof(output);
    EXPECT_EQ(PREFIX(deflate)(&strm, -1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflate)(&strm, Z_BLOCK + 1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflate with space but no output buffer (next_out NULL, avail_out > 0):
 * - rejected with Z_STREAM_ERROR;
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_missing_output_buffer) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    strm.next_out = NULL;
    strm.avail_out = 64;
    EXPECT_EQ(PREFIX(deflate)(&strm, Z_NO_FLUSH), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflate with pending input but no input buffer (next_in NULL, avail_in > 0):
 * - rejected with Z_STREAM_ERROR;
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_input_without_buffer) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    strm.next_in = NULL;
    strm.avail_in = 12;
    strm.next_out = (uint8_t *)"";
    strm.avail_out = 64;
    EXPECT_EQ(PREFIX(deflate)(&strm, Z_NO_FLUSH), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflate with zero output space on an initialized stream:
 * - reported as Z_BUF_ERROR (a buffer problem, not a state error);
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_zero_output_space) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    strm.next_out = (uint8_t *)"";
    strm.avail_out = 0;
    EXPECT_EQ(PREFIX(deflate)(&strm, Z_NO_FLUSH), Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_deflateBound                                                           */
/* ========================================================================== */

/* zng_deflateBound on broken streams must fall back to the conservative
 * bound instead of crashing:
 * - a NULL stream yields a bound of at least sourceLen;
 * - a zeroed (uninitialized) stream does the same. */
TEST(deflate_api_errors, deflate_bound_null_stream) {
    EXPECT_GE(PREFIX(deflateBound)(NULL, 1024), 1024UL);
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_GE(PREFIX(deflateBound)(&strm, 1024), 1024UL);
}

/* ========================================================================== */
/* zng_deflateCopy                                                            */
/* ========================================================================== */

/* zng_deflateCopy with a NULL destination:
 * - rejected with Z_STREAM_ERROR;
 * - the source stream is unaffected and ends cleanly. */
TEST(deflate_api_errors, deflate_copy_null_dest) {
    PREFIX3(stream) source;
    ASSERT_EQ(init_deflate(&source, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(deflateCopy)(NULL, &source), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&source), Z_OK);
}

/* zng_deflateCopy with a NULL source:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_copy_null_source) {
    PREFIX3(stream) dest;
    memset(&dest, 0, sizeof(dest));
    EXPECT_EQ(PREFIX(deflateCopy)(&dest, NULL), Z_STREAM_ERROR);
}

/* zng_deflateCopy from a zeroed (uninitialized) source:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_copy_uninitialized_source) {
    PREFIX3(stream) dest;
    PREFIX3(stream) source = zeroed_stream();
    memset(&dest, 0, sizeof(dest));
    EXPECT_EQ(PREFIX(deflateCopy)(&dest, &source), Z_STREAM_ERROR);
}

/* ========================================================================== */
/* zng_deflateEnd                                                             */
/* ========================================================================== */

/* zng_deflateEnd on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_end_null_stream) {
    EXPECT_EQ(PREFIX(deflateEnd)(NULL), Z_STREAM_ERROR);
}

/* zng_deflateEnd on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_end_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_STREAM_ERROR);
}

/* zng_deflateEnd called twice on the same stream:
 * - the first end succeeds;
 * - the second fails with Z_STREAM_ERROR, as the state is already gone. */
TEST(deflate_api_errors, deflate_end_twice) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_STREAM_ERROR);
}

/* zng_deflateEnd on a stream that is still mid-compression:
 * - rejected with Z_DATA_ERROR, as in upstream zlib. */
TEST(deflate_api_errors, deflate_end_mid_stream) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    ASSERT_EQ(start_compression(&strm), Z_OK);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_DATA_ERROR);
}

/* ========================================================================== */
/* zng_deflateGetDictionary                                                   */
/* ========================================================================== */

/* zng_deflateGetDictionary on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_get_dictionary_null_stream) {
    uint32_t dict_length = 0;
    EXPECT_EQ(PREFIX(deflateGetDictionary)(NULL, NULL, &dict_length), Z_STREAM_ERROR);
}

/* zng_deflateGetDictionary on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_get_dictionary_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    uint32_t dict_length = 0;
    EXPECT_EQ(PREFIX(deflateGetDictionary)(&strm, NULL, &dict_length), Z_STREAM_ERROR);
}

/* zng_deflateGetDictionary with NULL out-params on an initialized stream:
 * - both out-params NULL is legal -> Z_OK;
 * - a NULL dictionary with a live length is legal -> Z_OK, reporting the
 *   (empty) dictionary as 0 bytes. */
TEST(deflate_api_errors, deflate_get_dictionary_null_out_params) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(deflateGetDictionary)(&strm, NULL, NULL), Z_OK);
    uint32_t dict_length = 123;
    EXPECT_EQ(PREFIX(deflateGetDictionary)(&strm, NULL, &dict_length), Z_OK);
    EXPECT_EQ(dict_length, 0U);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_deflateGetParams (native API only)                                     */
/* ========================================================================== */

#ifndef ZLIB_COMPAT

/* zng_deflateGetParams on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_get_params_null_stream) {
    int32_t value = 0;
    zng_deflate_param_value params[1];
    memset(params, 0, sizeof(params));
    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &value;
    params[0].size = sizeof(value);
    EXPECT_EQ(zng_deflateGetParams(NULL, params, 1), Z_STREAM_ERROR);
}

/* zng_deflateGetParams on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_get_params_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    int32_t value = 0;
    zng_deflate_param_value params[1];
    memset(params, 0, sizeof(params));
    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &value;
    params[0].size = sizeof(value);
    EXPECT_EQ(zng_deflateGetParams(&strm, params, 1), Z_STREAM_ERROR);
}

/* zng_deflateGetParams with a value buffer too small for the parameter:
 * - the call reports Z_BUF_ERROR;
 * - the per-parameter status carries Z_BUF_ERROR as well. */
TEST(deflate_api_errors, deflate_get_params_buffer_too_small) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    int32_t value = 0;
    zng_deflate_param_value params[1];
    memset(params, 0, sizeof(params));
    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &value;
    params[0].size = 1; /* too small for an int */
    EXPECT_EQ(zng_deflateGetParams(&strm, params, 1), Z_BUF_ERROR);
    EXPECT_EQ(params[0].status, Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflateGetParams with an unknown parameter id:
 * - rejected with Z_VERSION_ERROR, the code the API uses for unsupported
 *   parameters. */
TEST(deflate_api_errors, deflate_get_params_unknown_param) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    int32_t value = 0;
    zng_deflate_param_value params[1];
    memset(params, 0, sizeof(params));
    params[0].param = (zng_deflate_param)(Z_DEFLATE_REPRODUCIBLE + 1);
    params[0].buf = &value;
    params[0].size = sizeof(value);
    EXPECT_EQ(zng_deflateGetParams(&strm, params, 1), Z_VERSION_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

#endif /* ZLIB_COMPAT */

/* ========================================================================== */
/* zng_deflateInit / zng_deflateInit_                                         */
/* ========================================================================== */

/* zng_deflateInit on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_init_null_stream) {
    EXPECT_EQ(PREFIX(deflateInit)(NULL, Z_DEFAULT_COMPRESSION), Z_STREAM_ERROR);
}

/* zng_deflateInit with out-of-range compression levels:
 * - level -2 is rejected with Z_STREAM_ERROR;
 * - level 10 is rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_init_level_out_of_range) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    EXPECT_EQ(PREFIX(deflateInit)(&strm, -2), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateInit)(&strm, 10), Z_STREAM_ERROR);
}

#ifdef ZLIB_COMPAT
/* zng_deflateInit_ (compat-only versioned entry point) with bad version
 * arguments:
 * - a NULL version string is rejected with Z_VERSION_ERROR;
 * - a wrong stream size is rejected with Z_VERSION_ERROR, before any
 *   allocation happens. */
TEST(deflate_api_errors, deflate_init_versioned_bad_args) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    /* NULL version and wrong stream size are both rejected before any allocation. */
    EXPECT_EQ(PREFIX(deflateInit_)(&strm, Z_DEFAULT_COMPRESSION, NULL, (int32_t)sizeof(strm)), Z_VERSION_ERROR);
    EXPECT_EQ(PREFIX(deflateInit_)(&strm, Z_DEFAULT_COMPRESSION, ZLIB_VERSION, 12), Z_VERSION_ERROR);
}
#endif /* ZLIB_COMPAT */

/* ========================================================================== */
/* zng_deflateInit2 / zng_deflateInit2_                                       */
/* ========================================================================== */

/* zng_deflateInit2 on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_init2_null_stream) {
    EXPECT_EQ(PREFIX(deflateInit2)(NULL, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY),
              Z_STREAM_ERROR);
}

/* zng_deflateInit2 with out-of-range compression levels:
 * - level -2 is rejected with Z_STREAM_ERROR;
 * - level 10 is rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_init2_level_out_of_range) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, -2, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, 10, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
}

/* zng_deflateInit2 with an unsupported compression method:
 * - Z_DEFLATED + 1 is rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_init2_bad_method) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED + 1, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY),
              Z_STREAM_ERROR);
}

/* zng_deflateInit2 with out-of-range windowBits:
 * - a raw window larger than MAX_WBITS (-MAX_WBITS - 1) is rejected;
 * - a zlib-wrapped window below MIN_WBITS (MIN_WBITS - 1) is rejected;
 * - an 8-bit window is only allowed with the zlib wrapper, so its raw
 *   form (-MIN_WBITS) is rejected;
 * - a gzip window below MIN_WBITS + 16 (MAX_WBITS + 1) is rejected;
 * - a gzip window above MAX_WBITS + 16 (MAX_WBITS + 16 + 1) is rejected. */
TEST(deflate_api_errors, deflate_init2_window_bits_out_of_range) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    /* Raw windows below MIN_WBITS, the 16-bit gzip window, and out-of-range gzip sizes. */
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS - 1, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY),
              Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MIN_WBITS - 1, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY),
              Z_STREAM_ERROR);
    /* windowBits == 8 is only allowed with the zlib wrapper. */
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MIN_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY),
              Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 1, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY),
              Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16 + 1, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY),
              Z_STREAM_ERROR);
}

/* zng_deflateInit2 with out-of-range memory levels:
 * - memLevel 0 is rejected with Z_STREAM_ERROR;
 * - memLevel MAX_MEM_LEVEL + 1 is rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_init2_mem_level_out_of_range) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, 0, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL + 1, Z_DEFAULT_STRATEGY),
              Z_STREAM_ERROR);
}

/* zng_deflateInit2 with out-of-range strategies:
 * - strategy -1 is rejected with Z_STREAM_ERROR;
 * - strategy Z_FIXED + 1 is rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_init2_strategy_out_of_range) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, -1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_FIXED + 1), Z_STREAM_ERROR);
}

#ifdef ZLIB_COMPAT
/* zng_deflateInit2_ (compat-only versioned entry point) with a NULL
 * version string:
 * - rejected with Z_VERSION_ERROR before any allocation happens. */
TEST(deflate_api_errors, deflate_init2_versioned_bad_args) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    EXPECT_EQ(PREFIX(deflateInit2_)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                    NULL, (int32_t)sizeof(strm)),
              Z_VERSION_ERROR);
}
#endif /* ZLIB_COMPAT */

/* ========================================================================== */
/* zng_deflateParams                                                          */
/* ========================================================================== */

/* zng_deflateParams on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_params_null_stream) {
    EXPECT_EQ(PREFIX(deflateParams)(NULL, Z_DEFAULT_COMPRESSION, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
}

/* zng_deflateParams on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_params_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_EQ(PREFIX(deflateParams)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
}

/* zng_deflateParams with out-of-range values on an initialized stream:
 * - level -2 is rejected with Z_STREAM_ERROR;
 * - level 10 is rejected with Z_STREAM_ERROR;
 * - strategy -1 is rejected with Z_STREAM_ERROR;
 * - strategy Z_FIXED + 1 is rejected with Z_STREAM_ERROR;
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_params_out_of_range) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(deflateParams)(&strm, -2, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateParams)(&strm, 10, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateParams)(&strm, Z_DEFAULT_COMPRESSION, -1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateParams)(&strm, Z_DEFAULT_COMPRESSION, Z_FIXED + 1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_deflatePending                                                         */
/* ========================================================================== */

/* zng_deflatePending on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_pending_null_stream) {
    uint32_t pending = 0;
    int32_t bits = 0;
    EXPECT_EQ(PREFIX(deflatePending)(NULL, &pending, &bits), Z_STREAM_ERROR);
}

/* zng_deflatePending on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_pending_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    uint32_t pending = 0;
    int32_t bits = 0;
    EXPECT_EQ(PREFIX(deflatePending)(&strm, &pending, &bits), Z_STREAM_ERROR);
}

/* zng_deflatePending with NULL out-params on an initialized stream:
 * - each combination of NULL pending/bits out-params is legal -> Z_OK;
 * - the stream ends cleanly. */
TEST(deflate_api_errors, deflate_pending_null_out_params) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    uint32_t pending = 0;
    int32_t bits = 0;
    EXPECT_EQ(PREFIX(deflatePending)(&strm, NULL, &bits), Z_OK);
    EXPECT_EQ(PREFIX(deflatePending)(&strm, &pending, NULL), Z_OK);
    EXPECT_EQ(PREFIX(deflatePending)(&strm, NULL, NULL), Z_OK);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_deflatePrime                                                           */
/* ========================================================================== */

/* zng_deflatePrime on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_prime_null_stream) {
    EXPECT_EQ(PREFIX(deflatePrime)(NULL, 8, 0xff), Z_STREAM_ERROR);
}

/* zng_deflatePrime on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_prime_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_EQ(PREFIX(deflatePrime)(&strm, 8, 0xff), Z_STREAM_ERROR);
}

/* zng_deflatePrime with out-of-range bit counts on an initialized stream:
 * - bits -1 is rejected with Z_BUF_ERROR;
 * - bits 65, above the bit buffer capacity, is rejected with Z_BUF_ERROR;
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_prime_bits_out_of_range) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(deflatePrime)(&strm, -1, 0xff), Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(deflatePrime)(&strm, 65, 0xff), Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_deflateReset / zng_deflateResetKeep                                    */
/* ========================================================================== */

/* zng_deflateReset / zng_deflateResetKeep on a NULL stream:
 * - both are rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_reset_null_stream) {
    EXPECT_EQ(PREFIX(deflateReset)(NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateResetKeep)(NULL), Z_STREAM_ERROR);
}

/* zng_deflateReset / zng_deflateResetKeep on a zeroed (uninitialized) stream:
 * - both are rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_reset_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_EQ(PREFIX(deflateReset)(&strm), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateResetKeep)(&strm), Z_STREAM_ERROR);
}

/* ========================================================================== */
/* zng_deflateSetDictionary                                                   */
/* ========================================================================== */

/* zng_deflateSetDictionary on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_set_dictionary_null_stream) {
    EXPECT_EQ(PREFIX(deflateSetDictionary)(NULL, (const uint8_t *)"abc", 3), Z_STREAM_ERROR);
}

/* zng_deflateSetDictionary on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_set_dictionary_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_EQ(PREFIX(deflateSetDictionary)(&strm, (const uint8_t *)"abc", 3), Z_STREAM_ERROR);
}

/* zng_deflateSetDictionary with a NULL dictionary on an initialized stream:
 * - rejected with Z_STREAM_ERROR even when the length is 0;
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_set_dictionary_null_dictionary) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(deflateSetDictionary)(&strm, NULL, 0), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflateSetDictionary on a gzip-wrapped stream:
 * - rejected with Z_STREAM_ERROR, as dictionaries are not supported for
 *   gzip streams;
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_set_dictionary_gzip_stream) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_gzip(&strm), Z_OK);
    EXPECT_EQ(PREFIX(deflateSetDictionary)(&strm, (const uint8_t *)"abc", 3), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflateSetDictionary after compression has started:
 * - rejected with Z_STREAM_ERROR once the stream is BUSY;
 * - the half-used stream ends with Z_DATA_ERROR, as it never finished. */
TEST(deflate_api_errors, deflate_set_dictionary_after_compression) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    ASSERT_EQ(start_compression(&strm), Z_OK);
    EXPECT_EQ(PREFIX(deflateSetDictionary)(&strm, (const uint8_t *)"abc", 3), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_DATA_ERROR);
}

/* ========================================================================== */
/* zng_deflateSetHeader                                                       */
/* ========================================================================== */

/* zng_deflateSetHeader on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_set_header_null_stream) {
    PREFIX(gz_header) header;
    memset(&header, 0, sizeof(header));
    EXPECT_EQ(PREFIX(deflateSetHeader)(NULL, &header), Z_STREAM_ERROR);
}

/* zng_deflateSetHeader on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_set_header_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    PREFIX(gz_header) header;
    memset(&header, 0, sizeof(header));
    EXPECT_EQ(PREFIX(deflateSetHeader)(&strm, &header), Z_STREAM_ERROR);
}

/* zng_deflateSetHeader on a zlib-wrapped (non-gzip) stream:
 * - rejected with Z_STREAM_ERROR, even when the header is NULL;
 * - the stream remains usable and ends cleanly. */
TEST(deflate_api_errors, deflate_set_header_non_gzip_stream) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(deflateSetHeader)(&strm, NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflateSetHeader on a gzip-wrapped stream:
 * - a NULL header is legal (it means "no extra header") -> Z_OK;
 * - a zeroed gz_header is accepted -> Z_OK;
 * - the stream ends cleanly. */
TEST(deflate_api_errors, deflate_set_header_gzip_stream) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_gzip(&strm), Z_OK);
    EXPECT_EQ(PREFIX(deflateSetHeader)(&strm, NULL), Z_OK);
    PREFIX(gz_header) header;
    memset(&header, 0, sizeof(header));
    EXPECT_EQ(PREFIX(deflateSetHeader)(&strm, &header), Z_OK);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_deflateSetParams (native API only)                                     */
/* ========================================================================== */

#ifndef ZLIB_COMPAT

/* zng_deflateSetParams on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_set_params_null_stream) {
    int32_t value = 1;
    zng_deflate_param_value params[1];
    memset(params, 0, sizeof(params));
    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &value;
    params[0].size = sizeof(value);
    EXPECT_EQ(zng_deflateSetParams(NULL, params, 1), Z_STREAM_ERROR);
}

/* zng_deflateSetParams on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_set_params_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    int32_t value = 1;
    zng_deflate_param_value params[1];
    memset(params, 0, sizeof(params));
    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &value;
    params[0].size = sizeof(value);
    EXPECT_EQ(zng_deflateSetParams(&strm, params, 1), Z_STREAM_ERROR);
}

/* zng_deflateSetParams with a value buffer too small for the parameter:
 * - the call reports Z_BUF_ERROR;
 * - the per-parameter status carries Z_BUF_ERROR as well. */
TEST(deflate_api_errors, deflate_set_params_buffer_too_small) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    int32_t value = 1;
    zng_deflate_param_value params[1];
    memset(params, 0, sizeof(params));
    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &value;
    params[0].size = 1; /* too small for an int */
    EXPECT_EQ(zng_deflateSetParams(&strm, params, 1), Z_BUF_ERROR);
    EXPECT_EQ(params[0].status, Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflateSetParams with the same parameter listed twice:
 * - rejected with Z_BUF_ERROR. */
TEST(deflate_api_errors, deflate_set_params_duplicate_param) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    int32_t first = 1;
    int32_t second = 2;
    zng_deflate_param_value params[2];
    memset(params, 0, sizeof(params));
    params[0].param = Z_DEFLATE_LEVEL;
    params[0].buf = &first;
    params[0].size = sizeof(first);
    params[1].param = Z_DEFLATE_LEVEL;
    params[1].buf = &second;
    params[1].size = sizeof(second);
    EXPECT_EQ(zng_deflateSetParams(&strm, params, 2), Z_BUF_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* zng_deflateSetParams with an unknown parameter id:
 * - the call reports Z_VERSION_ERROR, the code the API uses for unsupported
 *   parameters;
 * - the per-parameter status carries Z_VERSION_ERROR as well. */
TEST(deflate_api_errors, deflate_set_params_unknown_param) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    int32_t value = 1;
    zng_deflate_param_value params[1];
    memset(params, 0, sizeof(params));
    params[0].param = (zng_deflate_param)(Z_DEFLATE_REPRODUCIBLE + 1);
    params[0].buf = &value;
    params[0].size = sizeof(value);
    EXPECT_EQ(zng_deflateSetParams(&strm, params, 1), Z_VERSION_ERROR);
    EXPECT_EQ(params[0].status, Z_VERSION_ERROR);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

#endif /* ZLIB_COMPAT */

/* ========================================================================== */
/* zng_deflateTune                                                            */
/* ========================================================================== */

/* zng_deflateTune on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_tune_null_stream) {
    EXPECT_EQ(PREFIX(deflateTune)(NULL, 4, 4, 8, 4), Z_STREAM_ERROR);
}

/* zng_deflateTune on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_tune_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    EXPECT_EQ(PREFIX(deflateTune)(&strm, 4, 4, 8, 4), Z_STREAM_ERROR);
}

/* zng_deflateTune on an initialized stream:
 * - accepted with Z_OK; the tuning values themselves are not range-checked,
 *   as in upstream zlib;
 * - the stream ends cleanly. */
TEST(deflate_api_errors, deflate_tune_initialized_stream) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    EXPECT_EQ(PREFIX(deflateTune)(&strm, 4, 4, 8, 4), Z_OK);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}

/* ========================================================================== */
/* zng_deflateUsed                                                            */
/* ========================================================================== */

/* zng_deflateUsed on a NULL stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_used_null_stream) {
    int32_t bits = 0;
    EXPECT_EQ(PREFIX(deflateUsed)(NULL, &bits), Z_STREAM_ERROR);
}

/* zng_deflateUsed on a zeroed (uninitialized) stream:
 * - rejected with Z_STREAM_ERROR. */
TEST(deflate_api_errors, deflate_used_uninitialized_stream) {
    PREFIX3(stream) strm = zeroed_stream();
    int32_t bits = 0;
    EXPECT_EQ(PREFIX(deflateUsed)(&strm, &bits), Z_STREAM_ERROR);
}

/* zng_deflateUsed with a NULL out-param on an initialized stream:
 * - the NULL variant is legal and returns Z_OK;
 * - the live-pointer variant also returns Z_OK;
 * - the stream ends cleanly. */
TEST(deflate_api_errors, deflate_used_null_out_param) {
    PREFIX3(stream) strm;
    ASSERT_EQ(init_deflate(&strm, MAX_WBITS), Z_OK);
    int32_t bits = 0;
    EXPECT_EQ(PREFIX(deflateUsed)(&strm, NULL), Z_OK);
    EXPECT_EQ(PREFIX(deflateUsed)(&strm, &bits), Z_OK);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
}
