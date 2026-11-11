/* test_deflate_gh_536.cc - Test deflate() after switching levels between sync-flushed chunks
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

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

#include <vector>

#include <gtest/gtest.h>

#include "test_data_p.h"
#include "test_shared.h"

struct level_chunk {
    int32_t level;
    uint32_t size;
};

/* Compresses consecutive chunks of lcet10.txt into one gzip stream, switching the level before each
 * chunk like pigz, and checks that the stream inflates back to the same prefix. */
static void deflate_level_chunks(const level_chunk *chunks, size_t count) {
    PREFIX3(stream) strm;
    std::vector<uint8_t> compr;
    size_t plain_len = 0;
    size_t offset = 0;
    int err;

    for (size_t i = 0; i < count; i++)
        plain_len += chunks[i].size;

    uint8_t *data = get_data_file("lcet10.txt", plain_len);
    ASSERT_TRUE(data != NULL);
    std::vector<uint8_t> plain(data, data + plain_len);
    std::vector<uint8_t> uncompr(plain_len);
    free(data);

    memset(&strm, 0, sizeof(strm));

    err = PREFIX(deflateInit2)(&strm, chunks[0].level, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);

    for (size_t i = 0; i < count; i++) {
        bool last = (i == count - 1);
        unsigned long bound = PREFIX(deflateBound)(&strm, chunks[i].size);
        std::vector<uint8_t> out(bound);

        /* Only provide the output buffer to deflateParams(), it should not compress anything new */
        strm.next_in = NULL;
        strm.avail_in = 0;
        strm.next_out = out.data();
        strm.avail_out = (uint32_t)bound;
        err = PREFIX(deflateParams)(&strm, chunks[i].level, Z_DEFAULT_STRATEGY);
        EXPECT_EQ(err, Z_OK);

        /* The chunk shares the deflateBound() sized buffer with any leftovers flushed above */
        strm.next_in = plain.data() + offset;
        strm.avail_in = chunks[i].size;
        err = PREFIX(deflate)(&strm, last ? Z_FINISH : Z_SYNC_FLUSH);
        EXPECT_EQ(err, last ? Z_STREAM_END : Z_OK);
        EXPECT_EQ(strm.avail_in, 0u);

        compr.insert(compr.end(), out.data(), out.data() + (bound - strm.avail_out));
        offset += chunks[i].size;
    }

    err = PREFIX(deflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);

    memset(&strm, 0, sizeof(strm));

    err = PREFIX(inflateInit2)(&strm, MAX_WBITS + 16);
    ASSERT_EQ(err, Z_OK);

    strm.next_in = compr.data();
    strm.avail_in = (uint32_t)compr.size();
    strm.next_out = uncompr.data();
    strm.avail_out = (uint32_t)uncompr.size();

    err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    EXPECT_EQ(err, Z_STREAM_END);
    EXPECT_EQ(strm.total_out, plain_len);

    err = PREFIX(inflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);

    EXPECT_EQ(memcmp(uncompr.data(), plain.data(), plain_len), 0);
}

TEST(deflate, gh_536_segfault) {
    const level_chunk chunks[] = {{6, 9744}, {1, 91207}};
    deflate_level_chunks(chunks, sizeof(chunks) / sizeof(chunks[0]));
}

TEST(deflate, gh_536_incomplete_read) {
    const level_chunk chunks[] = {{6, 88933}, {1, 195840}, {2, 45761}};
    deflate_level_chunks(chunks, sizeof(chunks) / sizeof(chunks[0]));
}

TEST(deflate, gh_536_zero_stored_block) {
    const level_chunk chunks[] = {{6, 15248}, {1, 1050}, {2, 25217}};
    deflate_level_chunks(chunks, sizeof(chunks) / sizeof(chunks[0]));
}
