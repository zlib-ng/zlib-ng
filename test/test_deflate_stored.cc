/* test_deflate_stored.cc - Test deflate_stored() with input lengths near the 32-bit limit */

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

/* With bytes still in the window, an avail_in close to 4 GiB made left + avail_in wrap around to a
 * length that fit in avail_out. That block was then taken to be all of the input, so Z_FINISH wrote
 * it as the last block and returned Z_STREAM_END with nearly all of the input never consumed. */
TEST(deflate, stored_avail_in_wrap) {
    const size_t big_len = UINT32_MAX;
    const uint32_t buffered = 100;
    PREFIX3(stream) strm;
    uint8_t out[256];
    uint8_t *big;
    int err;

    if (sizeof(size_t) < 8)
        GTEST_SKIP() << "needs a 4 GiB input buffer";
    big = (uint8_t *)malloc(big_len);
    if (big == NULL)
        GTEST_SKIP() << "could not allocate a 4 GiB input buffer";
    /* Only the start of the input is read before deflate() runs out of output space. */
    memset(big, 'a', 1024 * 1024);

    memset(&strm, 0, sizeof(strm));
    ASSERT_EQ(PREFIX(deflateInit2)(&strm, Z_NO_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY), Z_OK);

    /* Leave a short run of input in the window. */
    strm.next_in = big;
    strm.avail_in = buffered;
    strm.next_out = out;
    strm.avail_out = sizeof(out);
    EXPECT_EQ(PREFIX(deflate)(&strm, Z_NO_FLUSH), Z_OK);
    EXPECT_EQ(strm.avail_in, 0u);
    EXPECT_EQ(strm.total_out, 0u);

    /* left + avail_in wraps to buffered - 1, which is exactly what fits after the 5 header bytes. */
    strm.next_in = big + buffered;
    strm.avail_in = (uint32_t)big_len;
    strm.next_out = out;
    strm.avail_out = 5 + buffered - 1;
    err = PREFIX(deflate)(&strm, Z_FINISH);
    EXPECT_EQ(err, Z_OK);
    /* The block written is not the last one. */
    EXPECT_EQ(out[0] & 1, 0);
    EXPECT_LT(strm.avail_in, (uint32_t)big_len);

    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_DATA_ERROR);
    free(big);
}
