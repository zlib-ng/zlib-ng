/* test_deflate_deterministic.cc - Test deterministic output after deflateReset */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <string>
#include <gtest/gtest.h>

#include "deflate.h"
#include "test_shared.h"



/* Issue: https://github.com/zlib-ng/zlib-ng/issues/2100 */

/* len(data_b) must be greater len(data_a) */
static const uint8_t data_a[] = {  0 , 'A', 'A', 'A', 'A',  0 , 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A' };
static const uint8_t data_b[] = { 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B' };

static std::string compress_data(const uint8_t *src, size_t len, PREFIX3(stream)* z_stream) {
    const uint32_t buffer_size = 1024;
    uint8_t buffer[buffer_size];
    int err;

    z_stream->next_in = (z_const uint8_t *)src;
    z_stream->avail_in = (uint32_t)len;
    z_stream->next_out = buffer;
    z_stream->avail_out = buffer_size;
    err = PREFIX(deflate)(z_stream, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);
    return std::string((const char *)buffer, (size_t)(z_stream->next_out - buffer));
}

TEST(deflate, deterministic) {
    const int compression_level = 6;
    const int window_bits = 15;
    int err;

    PREFIX3(stream) a_stream;
    memset(&a_stream, 0, sizeof(a_stream));

    /* Compress a with newly created z_stream. */
    err = PREFIX(deflateInit2)(&a_stream, compression_level, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY);
    EXPECT_EQ(err, Z_OK);

    const std::string a_compressed = compress_data(data_a, sizeof(data_a), &a_stream);
    err = PREFIX(deflateEnd)(&a_stream);
    EXPECT_EQ(err, Z_OK);

    /* Compress b with newly created z_stream. */
    PREFIX3(stream) b_stream;
    memset(&b_stream, 0, sizeof(b_stream));

    err = PREFIX(deflateInit2)(&b_stream, compression_level, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY);
    EXPECT_EQ(err, Z_OK);
    const std::string b_compressed = compress_data(data_b, sizeof(data_b), &b_stream);

    /* Reset the stream. */
    err = PREFIX(deflateReset)(&b_stream);
    EXPECT_EQ(err, Z_OK);

    const std::string a_compressed2 = compress_data(data_a, sizeof(data_a), &b_stream);
    err = PREFIX(deflateEnd)(&b_stream);
    EXPECT_EQ(err, Z_OK);

    EXPECT_EQ(a_compressed, a_compressed2);
}
