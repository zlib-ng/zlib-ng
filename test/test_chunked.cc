/* test_chunked.cc - Test deflate() and inflate() with various buffer sizes */

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

#include "compressible_data_p.h"

struct chunked_params {
    size_t compr_size;
    size_t uncompr_size;
    size_t avail_out;
    const char *name;
};

class chunked : public ::testing::TestWithParam<chunked_params> {
protected:
    uint8_t *compr = nullptr;
    uint8_t *uncompr = nullptr;
    uint8_t *decompressed = nullptr;

    void SetUp() override {
        const auto& p = GetParam();

        compr = (uint8_t *)calloc(1, p.compr_size);
        ASSERT_NE(compr, nullptr);
        decompressed = (uint8_t *)calloc(1, p.uncompr_size);
        ASSERT_NE(decompressed, nullptr);

        uncompr = gen_compressible_data(p.uncompr_size);
        ASSERT_NE(uncompr, nullptr);
    }

    void TearDown() override {
        free(compr);
        free(uncompr);
        free(decompressed);
    }
};

TEST_P(chunked, roundtrip) {
    const auto& p = GetParam();
    PREFIX3(stream) c_stream, d_stream;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&d_stream, 0, sizeof(d_stream));

    err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
    ASSERT_EQ(err, Z_OK);

    c_stream.next_in = uncompr;
    c_stream.avail_in = (uint32_t)p.uncompr_size;
    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)p.avail_out;

    /* Consume all input with Z_NO_FLUSH, refilling avail_out as it drains. */
    while (c_stream.avail_in > 0) {
        err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
        ASSERT_EQ(err, Z_OK);
        if (c_stream.avail_out == 0) {
            size_t remaining = p.compr_size - c_stream.total_out;
            ASSERT_NE(remaining, 0u) << "compr buffer too small";
            c_stream.avail_out = (uint32_t)MIN(p.avail_out, remaining);
        }
    }

    /* Finish the stream, still chunking the output. */
    for (;;) {
        err = PREFIX(deflate)(&c_stream, Z_FINISH);
        if (err == Z_STREAM_END)
            break;
        ASSERT_EQ(err, Z_OK);
        ASSERT_EQ(c_stream.avail_out, 0u) << "Z_FINISH returned Z_OK with space remaining";
        size_t remaining = p.compr_size - c_stream.total_out;
        ASSERT_NE(remaining, 0u) << "compr buffer too small";
        c_stream.avail_out = (uint32_t)MIN(p.avail_out, remaining);
    }

    err = PREFIX(deflateEnd)(&c_stream);
    ASSERT_EQ(err, Z_OK);

    err = PREFIX(inflateInit)(&d_stream);
    ASSERT_EQ(err, Z_OK);

    d_stream.next_in = compr;
    d_stream.avail_in = (uint32_t)c_stream.total_out;
    d_stream.next_out = decompressed;
    d_stream.avail_out = (uint32_t)p.avail_out;

    for (;;) {
        err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
        if (err == Z_STREAM_END)
            break;
        ASSERT_EQ(err, Z_OK);
        if (d_stream.avail_out == 0) {
            size_t remaining = p.uncompr_size - d_stream.total_out;
            ASSERT_NE(remaining, 0u) << "decompressed buffer full before inflate done";
            d_stream.avail_out = (uint32_t)MIN(p.avail_out, remaining);
        }
    }

    err = PREFIX(inflateEnd)(&d_stream);
    ASSERT_EQ(err, Z_OK);

    EXPECT_EQ(d_stream.total_out, p.uncompr_size);
    EXPECT_EQ(memcmp(decompressed, uncompr, p.uncompr_size), 0);
}

/* chunked_params fields:
 *   compr_size   - size of the compressed buffer allocation
 *   uncompr_size - size of the uncompressed input and the decompressed output buffers
 *   avail_out    - per-chunk output size used for both deflate and inflate; the output is
 *                  drained and refilled in chunks of this size
 *   name         - test case name suffix
 */
INSTANTIATE_TEST_SUITE_P(
    chunked, chunked,
    ::testing::Values(
        chunked_params{128, 128, 1, "small_buffers"},
        chunked_params{48 * 1024, 32 * 1024, 32 * 1024, "large_buffers"},
        /* test inflate_fast() safe mode MATCH state bailout at various avail_out */
        chunked_params{48 * 1024, 32 * 1024, 3, "avail_out_3"},
        chunked_params{48 * 1024, 32 * 1024, 64, "avail_out_64"},
        chunked_params{48 * 1024, 32 * 1024, 128, "avail_out_128"},
        chunked_params{48 * 1024, 32 * 1024, 256, "avail_out_256"},
        chunked_params{48 * 1024, 32 * 1024, 259, "avail_out_259"}
    ),
    [](const ::testing::TestParamInfo<chunked_params>& info) {
        return std::string(info.param.name);
    });
