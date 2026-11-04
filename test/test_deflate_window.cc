/* test_deflate_window.cc - Test that deflate match distances fill but never exceed the window */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <string.h>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>

#define DATA_SIZE (64 * 1024)

namespace {

static uint32_t next_rand(uint32_t *seed) {
    *seed = *seed * 1103515245 + 12345;
    return *seed >> 16;
}

/* Literal runs and repeats at distances up to the last byte of the window, so a distance past the
   window fails to decode */
static std::vector<uint8_t> gen_data(unsigned wsize) {
    const unsigned dists[] = {wsize - 1, wsize - 2, wsize - 130, wsize / 2, 1, 7, 33};
    std::vector<uint8_t> data;
    uint32_t seed = 1;

    while (data.size() < DATA_SIZE) {
        unsigned run = 1 + next_rand(&seed) % 40;
        for (unsigned i = 0; i < run; i++)
            data.push_back((uint8_t)next_rand(&seed));
        unsigned dist = dists[next_rand(&seed) % (sizeof(dists) / sizeof(dists[0]))];
        unsigned len = 3 + next_rand(&seed) % 120;
        if (dist <= data.size()) {
            for (unsigned i = 0; i < len; i++)
                data.push_back(data[data.size() - dist]);
        }
    }
    data.resize(DATA_SIZE);
    return data;
}

/* Compress in 1000 byte pieces so the window buffer slides */
static std::vector<uint8_t> compress(const std::vector<uint8_t> &data, int window_bits, int level, int strategy) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    std::vector<uint8_t> out(data.size() * 2 + 1024);

    EXPECT_EQ(PREFIX(deflateInit2)(&strm, level, Z_DEFLATED, window_bits, 8, strategy), Z_OK);
    strm.next_out = out.data();
    strm.avail_out = (uint32_t)out.size();
    for (size_t pos = 0; pos < data.size(); pos += 1000) {
        strm.next_in = (z_const uint8_t *)data.data() + pos;
        strm.avail_in = (uint32_t)MIN(1000, data.size() - pos);
        EXPECT_EQ(PREFIX(deflate)(&strm, Z_NO_FLUSH), Z_OK);
        EXPECT_EQ(strm.avail_in, 0u);
    }
    EXPECT_EQ(PREFIX(deflate)(&strm, Z_FINISH), Z_STREAM_END);
    out.resize((size_t)strm.total_out);
    EXPECT_EQ(PREFIX(deflateEnd)(&strm), Z_OK);
    return out;
}

/* One output byte per call, so every match copies from a window of the declared size and a distance
   past it fails with Z_DATA_ERROR */
static void inflate_strict(const std::vector<uint8_t> &compr, int window_bits, const std::vector<uint8_t> &expected) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    std::vector<uint8_t> out(expected.size() + 1);

    ASSERT_EQ(PREFIX(inflateInit2)(&strm, window_bits), Z_OK);
    strm.next_in = (z_const uint8_t *)compr.data();
    strm.avail_in = (uint32_t)compr.size();
    int err = Z_OK;
    while (err == Z_OK && strm.total_out <= expected.size()) {
        strm.next_out = out.data() + strm.total_out;
        strm.avail_out = 1;
        err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    }
    EXPECT_EQ(err, Z_STREAM_END) << (strm.msg ? strm.msg : "");
    ASSERT_EQ(strm.total_out, (z_uintmax_t)expected.size());
    EXPECT_EQ(memcmp(out.data(), expected.data(), expected.size()), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

}  // namespace

class deflate_window : public ::testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(deflate_window, strict_roundtrip) {
    int window_bits = std::get<0>(GetParam());
    int level = std::get<1>(GetParam());
    int strategy = std::get<2>(GetParam());

    std::vector<uint8_t> data = gen_data(1u << window_bits);
    std::vector<uint8_t> compr = compress(data, window_bits, level, strategy);
    inflate_strict(compr, window_bits, data);
}

INSTANTIATE_TEST_SUITE_P(deflate, deflate_window,
    ::testing::Combine(::testing::Range(9, 16), ::testing::Range(0, 10),
                       ::testing::Values(Z_DEFAULT_STRATEGY, Z_FILTERED, Z_FIXED)));

/* Random bytes repeated one byte short of the window. Window bits 9 to 11 leave room for the
   lookahead past two windows in every build, so those matches must be found. */
TEST(deflate_window, reach_last_byte) {
    for (int window_bits = 9; window_bits <= 11; window_bits++) {
        unsigned dist = (1u << window_bits) - 1;
        std::vector<uint8_t> data(dist);
        uint32_t seed = 7;
        for (unsigned i = 0; i < dist; i++)
            data[i] = (uint8_t)next_rand(&seed);
        data.insert(data.end(), data.begin(), data.begin() + dist);

        for (int level : {6, 9}) {
            std::vector<uint8_t> compr = compress(data, window_bits, level, Z_DEFAULT_STRATEGY);
            EXPECT_LT(compr.size(), dist + dist / 8) << "window bits " << window_bits << ", level " << level;
            inflate_strict(compr, window_bits, data);
        }
    }
}
