/* test_deflate_gh_364.cc - Test deflate() after switching compression levels */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>

#define LEVEL1_LEN 5
#define LEVEL9_LEN 3

TEST(deflate, gh_364) {
    PREFIX3(stream) strm;
    uint8_t plain[LEVEL1_LEN + LEVEL9_LEN];
    uint8_t compr[1024];
    uint8_t uncompr[sizeof(plain)];
    uint32_t compr_len;
    int err;

    memset(plain, 0, sizeof(plain));
    memset(&strm, 0, sizeof(strm));

    err = PREFIX(deflateInit2)(&strm, 1, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);

    strm.next_out = compr;
    strm.avail_out = (uint32_t)sizeof(compr);

    /* Only provide the output buffer to deflateParams(), it should not compress anything new */
    err = PREFIX(deflateParams)(&strm, 1, Z_DEFAULT_STRATEGY);
    EXPECT_EQ(err, Z_OK);

    strm.next_in = plain;
    strm.avail_in = LEVEL1_LEN;
    err = PREFIX(deflate)(&strm, Z_SYNC_FLUSH);
    EXPECT_EQ(err, Z_OK);
    EXPECT_EQ(strm.avail_in, 0u);

    err = PREFIX(deflateParams)(&strm, 9, Z_DEFAULT_STRATEGY);
    EXPECT_EQ(err, Z_OK);

    strm.avail_in = LEVEL9_LEN;
    err = PREFIX(deflate)(&strm, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);
    EXPECT_EQ(strm.avail_in, 0u);
    compr_len = (uint32_t)strm.total_out;

    err = PREFIX(deflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);

    memset(&strm, 0, sizeof(strm));

    err = PREFIX(inflateInit2)(&strm, MAX_WBITS + 16);
    ASSERT_EQ(err, Z_OK);

    strm.next_in = compr;
    strm.avail_in = compr_len;
    strm.next_out = uncompr;
    strm.avail_out = (uint32_t)sizeof(uncompr);

    err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    EXPECT_EQ(err, Z_STREAM_END);
    EXPECT_EQ(strm.total_out, sizeof(plain));

    err = PREFIX(inflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);

    EXPECT_EQ(memcmp(uncompr, plain, sizeof(plain)), 0);
}
