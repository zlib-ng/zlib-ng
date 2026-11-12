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
/* zng_inflateInit / zng_inflateInit2                                         */
/* ========================================================================== */

/* inflateInit() and inflateInit2() with a NULL stream:
 * - both are rejected with Z_STREAM_ERROR without touching anything. */
TEST(inflate_api_errors, init_null_stream) {
    EXPECT_EQ(PREFIX(inflateInit)(NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(inflateInit2)(NULL, MAX_WBITS), Z_STREAM_ERROR);
}
