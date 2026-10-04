/* test_inflate_gh_1600.cc - Test inflate() with Z_FINISH and no sliding window */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <string.h>

#include <gtest/gtest.h>

/* Small git pack object followed by trailing data that is not part of the zlib stream */
static z_const uint8_t packobj[] = {
    0x78, 0x9c, 0x4b, 0xcc, 0xc9, 0xcf, 0x4b, 0x55, 0xc8, 0xcc, 0x53, 0x28, 0xc9, 0x48, 0x55, 0x48,
    0x49, 0x2c, 0xca, 0xe6, 0x02, 0x00, 0x3e, 0x03, 0x06, 0x34, 0xfa, 0xc1, 0x99, 0x06, 0xba, 0x9a,
    0x21, 0x81, 0xe9, 0xb1, 0xcd, 0xe3, 0xdc, 0xb3, 0x17, 0xf0, 0xb5, 0x2a, 0xcb, 0x31
};
#define PACKOBJ_STREAM_LEN 26

static const char packobj_text[] = "alone in the dark\n";
#define PACKOBJ_TEXT_LEN (sizeof(packobj_text) - 1)

/* Inflate should not use a sliding window when it is given Z_FINISH */
static void inflate_finish(uint32_t read_len) {
    PREFIX3(stream) strm;
    uint8_t uncompr[1024];
    int err;

    memset(&strm, 0, sizeof(strm));

    err = PREFIX(inflateInit2)(&strm, MAX_WBITS + 32);
    ASSERT_EQ(err, Z_OK);

    strm.next_in = packobj;
    strm.avail_in = read_len;
    strm.next_out = uncompr;
    strm.avail_out = (uint32_t)sizeof(uncompr);

    err = PREFIX(inflate)(&strm, Z_FINISH);
    if (read_len < PACKOBJ_STREAM_LEN) {
        /* Not enough input to finish the stream */
        EXPECT_EQ(err, Z_BUF_ERROR);
        EXPECT_EQ(strm.avail_in, 0u);

        strm.avail_in = (uint32_t)sizeof(packobj) - read_len;
        err = PREFIX(inflate)(&strm, Z_FINISH);
    }
    EXPECT_EQ(err, Z_STREAM_END);
    EXPECT_EQ(strm.avail_in, sizeof(packobj) - PACKOBJ_STREAM_LEN);

    ASSERT_EQ(strm.total_out, PACKOBJ_TEXT_LEN);
    EXPECT_EQ(memcmp(uncompr, packobj_text, PACKOBJ_TEXT_LEN), 0);

    err = PREFIX(inflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);
}

TEST(inflate, gh_1600_no_window_check) {
    inflate_finish((uint32_t)sizeof(packobj));
}

/* Limit the first read so that inflate has to bail early in CHECK mode */
TEST(inflate, gh_1600_no_window_no_check) {
    inflate_finish(PACKOBJ_STREAM_LEN - 1);
}
