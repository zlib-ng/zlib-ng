/* test_inflate_mark.cc - Test inflateMark() when a match does not fit in the output buffer */

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

/* Raw deflate stream with one fixed block, which holds the literals a to z, a match of
   234 bytes at distance 26, the literals 0 to 9 and A to J, and the end of block code */
static z_const uint8_t match_stream[] = {
    0x4b, 0x4c, 0x4a, 0x4e, 0x49, 0x4d, 0x4b, 0xcf, 0xc8, 0xcc, 0xca, 0xce, 0xc9, 0xcd, 0xcb, 0x2f,
    0x28, 0x2c, 0x2a, 0x2e, 0x29, 0x2d, 0x2b, 0xaf, 0xa8, 0xac, 0x1a, 0x39, 0x32, 0x06, 0x86, 0x46,
    0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e,
    0x5e, 0x00
};
#define LITERALS_LEN 26
#define MATCH_LEN 234

static const char match_text[] =
    "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
    "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
    "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
    "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJ";
#define MATCH_TEXT_LEN (sizeof(match_text) - 1)

/* Fill the state with a pattern, so that a field inflate() does not set cannot pass as zero */
static void *alloc_fill(void *opaque, unsigned int items, unsigned int size) {
    size_t len = (size_t)items * size;
    void *ptr = malloc(len);
    Z_UNUSED(opaque);
    if (ptr != NULL)
        memset(ptr, 0xa5, len);
    return ptr;
}

static void free_fill(void *opaque, void *ptr) {
    Z_UNUSED(opaque);
    free(ptr);
}

/* End the output buffer inside the match and check how much of it inflateMark() reports */
static void mark_inside_match(uint32_t out_len) {
    PREFIX3(stream) strm;
    uint8_t uncompr[MATCH_TEXT_LEN];
    int err;

    memset(&strm, 0, sizeof(strm));
    strm.zalloc = alloc_fill;
    strm.zfree = free_fill;

    err = PREFIX(inflateInit2)(&strm, -MAX_WBITS);
    ASSERT_EQ(err, Z_OK);

    strm.next_in = match_stream;
    strm.avail_in = (uint32_t)sizeof(match_stream);
    strm.next_out = uncompr;
    strm.avail_out = out_len;

    err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    EXPECT_EQ(err, Z_OK);
    EXPECT_EQ(strm.avail_out, 0u);
    /* The lower 16 bits are the number of bytes already written for the match */
    EXPECT_EQ(PREFIX(inflateMark)(&strm) & 0xffff, (long)(out_len - LITERALS_LEN));

    /* The count goes on from there with more output */
    strm.avail_out = 1;
    err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    EXPECT_EQ(err, Z_OK);
    EXPECT_EQ(PREFIX(inflateMark)(&strm) & 0xffff, (long)(out_len - LITERALS_LEN + 1));

    strm.avail_out = (uint32_t)MATCH_TEXT_LEN - out_len - 1;
    err = PREFIX(inflate)(&strm, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);

    ASSERT_EQ(strm.total_out, MATCH_TEXT_LEN);
    EXPECT_EQ(memcmp(uncompr, match_text, MATCH_TEXT_LEN), 0);

    err = PREFIX(inflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);
}

TEST(inflate, mark_inside_match) {
#ifdef S390_DFLTCC_INFLATE
    GTEST_SKIP() << "inflateMark is not supported with S390_DFLTCC_INFLATE";
#endif

    /* Every output size that ends at least two bytes before the end of the match */
    for (uint32_t out_len = LITERALS_LEN + 1; out_len < LITERALS_LEN + MATCH_LEN - 1; out_len++) {
        SCOPED_TRACE(testing::Message() << "out_len " << out_len);
        mark_inside_match(out_len);
    }
}
