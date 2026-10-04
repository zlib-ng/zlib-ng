/* test_deflate_gh_361.cc - Test deflate_medium() with overlapping matches */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>

/* Excerpt of hexdump output that triggered the match[2] assertion in longest_match() */
static const char hexdump[] =
    ".....-.u..|u....-...!..A.#?)9.._B..F..|\n"
    "00000650  fa 13 88 89 2c 1f 81 0f  e4 e9 ce 39 a0 87 2e 2e  |....,......9....|\n"
    "00000660  a5 0c 08 9c ec fc 88 6d  16 02 0a a0 3d fc 36 29  |.......m....=.6)|\n"
    "00000670  8d f5 c3 ba 1d 07 f4 78  e1 a0 41 f9 89 15 a5 69  |.......x..A....";
#define HEXDUMP_LEN (sizeof(hexdump) - 1)

TEST(deflate, gh_361) {
    PREFIX3(stream) strm;
    uint8_t compr[1024];
    uint8_t uncompr[HEXDUMP_LEN];
    uint32_t compr_len;
    int err;

    memset(&strm, 0, sizeof(strm));

    err = PREFIX(deflateInit2)(&strm, 4, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);

    strm.next_in = (z_const uint8_t *)hexdump;
    strm.avail_in = (uint32_t)HEXDUMP_LEN;
    strm.next_out = compr;
    strm.avail_out = (uint32_t)sizeof(compr);

    err = PREFIX(deflate)(&strm, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);
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
    EXPECT_EQ(strm.total_out, HEXDUMP_LEN);

    err = PREFIX(inflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);

    EXPECT_EQ(memcmp(uncompr, hexdump, HEXDUMP_LEN), 0);
}
