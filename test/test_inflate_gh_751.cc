/* test_inflate_gh_751.cc - Test inflate() with long distance 3 matches
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <string.h>

#include <vector>

#include <gtest/gtest.h>

/* "abc" repeated, then a newline, decodes as long distance 3 matches that corrupted the heap on
 * aarch64 */
#define ABC_REPEAT 60000
#define PLAIN_LEN (ABC_REPEAT * 3 + 1)

TEST(inflate, gh_751) {
    const int32_t levels[] = {3, Z_DEFAULT_COMPRESSION};
    std::vector<uint8_t> plain(PLAIN_LEN);
    PREFIX3(stream) strm;
    int err;

    for (size_t i = 0; i < PLAIN_LEN - 1; i++)
        plain[i] = (uint8_t)("abc"[i % 3]);
    plain[PLAIN_LEN - 1] = '\n';

    for (int32_t level : levels) {
        SCOPED_TRACE(level);

        memset(&strm, 0, sizeof(strm));

        err = PREFIX(deflateInit2)(&strm, level, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
        ASSERT_EQ(err, Z_OK);

        std::vector<uint8_t> compr(PREFIX(deflateBound)(&strm, PLAIN_LEN));
        strm.next_in = plain.data();
        strm.avail_in = PLAIN_LEN;
        strm.next_out = compr.data();
        strm.avail_out = (uint32_t)compr.size();

        err = PREFIX(deflate)(&strm, Z_FINISH);
        EXPECT_EQ(err, Z_STREAM_END);
        compr.resize(strm.total_out);

        err = PREFIX(deflateEnd)(&strm);
        EXPECT_EQ(err, Z_OK);

        /* Inflate into an exactly sized heap buffer so sanitizers catch any overrun */
        std::vector<uint8_t> uncompr(PLAIN_LEN);

        memset(&strm, 0, sizeof(strm));

        err = PREFIX(inflateInit2)(&strm, MAX_WBITS + 16);
        ASSERT_EQ(err, Z_OK);

        strm.next_in = compr.data();
        strm.avail_in = (uint32_t)compr.size();
        strm.next_out = uncompr.data();
        strm.avail_out = (uint32_t)uncompr.size();

        err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
        EXPECT_EQ(err, Z_STREAM_END);
        EXPECT_EQ(strm.total_out, (unsigned long)PLAIN_LEN);

        err = PREFIX(inflateEnd)(&strm);
        EXPECT_EQ(err, Z_OK);

        EXPECT_EQ(memcmp(uncompr.data(), plain.data(), PLAIN_LEN), 0);
    }
}
