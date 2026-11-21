/* test_inflate_copy.cc - Test copying inflate stream */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

#include "test_data_p.h"
#include "test_shared.h"

TEST(inflate, copy_back_and_forth) {
    PREFIX3(stream) d1_stream, d2_stream;
    int err;

    memset(&d1_stream, 0, sizeof(d1_stream));
    err = PREFIX(inflateInit2)(&d1_stream, MAX_WBITS + 14);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateCopy)(&d2_stream, &d1_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateEnd)(&d1_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateCopy)(&d1_stream, &d2_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateEnd)(&d1_stream);
    ASSERT_EQ(err, Z_OK);
    err = PREFIX(inflateEnd)(&d2_stream);
    ASSERT_EQ(err, Z_OK);
}

/* Copy an inflate stream halfway through a block, for both fixed (static
 * table) and dynamic (state->codes) blocks, and check that both the original
 * and the copy finish with the right output. */
static void inflate_copy_midstream(int strategy, int expected_btype) {
    PREFIX3(stream) cstrm, d1, d2;
    uint8_t in[4096], comp[8192], out1[4096], out2[4096];
    uint32_t comp_len, produced;

    uint8_t *gen = gen_text_data(sizeof(in));
    ASSERT_NE(gen, (uint8_t *)NULL);
    memcpy(in, gen, sizeof(in));
    free(gen);

    memset(&cstrm, 0, sizeof(cstrm));
    ASSERT_EQ(PREFIX(deflateInit2)(&cstrm, Z_BEST_COMPRESSION, Z_DEFLATED,
              MAX_WBITS, MAX_MEM_LEVEL, strategy), Z_OK);
    cstrm.next_in = in;
    cstrm.avail_in = sizeof(in);
    cstrm.next_out = comp;
    cstrm.avail_out = sizeof(comp);
    ASSERT_EQ(PREFIX(deflate)(&cstrm, Z_FINISH), Z_STREAM_END);
    comp_len = (uint32_t)cstrm.total_out;
    ASSERT_EQ(PREFIX(deflateEnd)(&cstrm), Z_OK);

    /* Verify first block type: 1 = fixed Huffman (static table),
     * 2 = dynamic Huffman (state->codes). */
    ASSERT_GE(comp_len, 3U);
    ASSERT_EQ((comp[2] >> 1) & 3, expected_btype);

    memset(&d1, 0, sizeof(d1));
    ASSERT_EQ(PREFIX(inflateInit2)(&d1, MAX_WBITS), Z_OK);
    d1.next_in = comp;
    d1.avail_in = comp_len;
    d1.next_out = out1;
    d1.avail_out = sizeof(out1) / 2;
    ASSERT_EQ(PREFIX(inflate)(&d1, Z_NO_FLUSH), Z_OK);
    ASSERT_GT(d1.total_out, 0U);
    ASSERT_LT(d1.total_out, sizeof(in));

    ASSERT_EQ(PREFIX(inflateCopy)(&d2, &d1), Z_OK);
    produced = (uint32_t)d1.total_out;
    memcpy(out2, out1, produced);

    d1.avail_out = sizeof(out1) - produced;
    ASSERT_EQ(PREFIX(inflate)(&d1, Z_FINISH), Z_STREAM_END);
    ASSERT_EQ(d1.total_out, sizeof(in));
    ASSERT_EQ(PREFIX(inflateEnd)(&d1), Z_OK);

    /* The copy must not depend on the original anymore. */
    d2.next_out = out2 + produced;
    d2.avail_out = sizeof(out2) - produced;
    ASSERT_EQ(PREFIX(inflate)(&d2, Z_FINISH), Z_STREAM_END);
    ASSERT_EQ(d2.total_out, sizeof(in));
    ASSERT_EQ(PREFIX(inflateEnd)(&d2), Z_OK);

    EXPECT_EQ(memcmp(in, out1, sizeof(in)), 0);
    EXPECT_EQ(memcmp(in, out2, sizeof(in)), 0);
}

TEST(inflate, copy_midstream_fixed) {
    inflate_copy_midstream(Z_FIXED, 1);
}

TEST(inflate, copy_midstream_dynamic) {
    inflate_copy_midstream(Z_DEFAULT_STRATEGY, 2);
}
