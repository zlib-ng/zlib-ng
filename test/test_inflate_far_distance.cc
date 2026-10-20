/* test_inflate_far_distance.cc - Test inflate of matches near the window limit with small output buffers */

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

/* zlib-ng's deflate never emits a distance above 32506, so the stream here is a fixed Huffman block built
   by hand. Other encoders, libdeflate among them, emit distances up to 32768. */

#define WSIZE 32768

namespace {

class bit_writer {
public:
    std::vector<uint8_t> out;

    void bits(uint32_t val, unsigned n) {
        buf |= (uint64_t)val << cnt;
        cnt += n;
        while (cnt >= 8) {
            out.push_back((uint8_t)buf);
            buf >>= 8;
            cnt -= 8;
        }
    }

    /* Huffman codes go out most significant bit first */
    void huff(uint32_t code, unsigned len) {
        uint32_t rev = 0;
        for (unsigned i = 0; i < len; i++)
            rev |= ((code >> i) & 1) << (len - 1 - i);
        bits(rev, len);
    }

    void align(void) {
        if (cnt)
            bits(0, 8 - cnt);
    }

private:
    uint64_t buf = 0;
    unsigned cnt = 0;
};

static const uint16_t len_base[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
                                      67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
                                      5, 5, 5, 5, 0};
static const uint16_t dist_base[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
                                       513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
                                       10, 11, 11, 12, 12, 13, 13};

class far_stream {
public:
    std::vector<uint8_t> expected;
    bit_writer bw;

    uint8_t next_byte(void) {
        seed = seed * 1103515245 + 12345;
        return (uint8_t)(seed >> 16);
    }

    void literal(uint8_t c) {
        if (c < 144)
            bw.huff(0x30 + c, 8);
        else
            bw.huff(0x190 + c - 144, 9);
        expected.push_back(c);
    }

    void match(unsigned len, unsigned dist) {
        int lc = 28;
        while (len_base[lc] > len)
            lc--;
        unsigned sym = 257 + lc;
        if (sym < 280)
            bw.huff(sym - 256, 7);
        else
            bw.huff(0xc0 + sym - 280, 8);
        bw.bits(len - len_base[lc], len_extra[lc]);

        int dc = 29;
        while (dist_base[dc] > dist)
            dc--;
        bw.huff(dc, 5);
        bw.bits(dist - dist_base[dc], dist_extra[dc]);

        /* Byte at a time, the reference semantics for an overlapped copy */
        for (unsigned i = 0; i < len; i++)
            expected.push_back(expected[expected.size() - dist]);
    }

    void end_block(void) {
        bw.huff(0, 7);
        bw.align();
    }

private:
    uint32_t seed = 1;
};

/* A stored block fills the window with incompressible bytes. A fixed block then follows each short match,
   whose tail stores may run past its end, with a match that reaches back to the far edge of the window. */
static void build_stream(std::vector<uint8_t> &compr, std::vector<uint8_t> &expected) {
    static const unsigned short_len[] = {3, 9, 17, 4, 40};
    static const unsigned short_dist[] = {2, 3, 5, 20, 7};
    far_stream fs;

    fs.bw.bits(0, 1);
    fs.bw.bits(0, 2);
    fs.bw.align();
    fs.bw.bits(WSIZE & 0xffff, 16);
    fs.bw.bits(~WSIZE & 0xffff, 16);
    for (unsigned i = 0; i < WSIZE; i++) {
        uint8_t c = fs.next_byte();
        fs.bw.bits(c, 8);
        fs.expected.push_back(c);
    }

    fs.bw.bits(1, 1);
    fs.bw.bits(1, 2);
    for (unsigned dist = WSIZE - 96; dist <= WSIZE; dist++) {
        for (unsigned s = 0; s < sizeof(short_len) / sizeof(short_len[0]); s++) {
            fs.literal(fs.next_byte());
            fs.match(short_len[s], short_dist[s]);
            fs.match((dist + s) % 4 == 0 ? 258 : 3 + (dist + s) % 61, dist);
        }
    }
    fs.end_block();

    compr = fs.bw.out;
    expected = fs.expected;
}

}  // namespace

class inflate_far_distance : public ::testing::TestWithParam<unsigned> {};

TEST_P(inflate_far_distance, chunked_output) {
    std::vector<uint8_t> compr, expected;
    build_stream(compr, expected);

    const size_t chunk = GetParam() ? GetParam() : expected.size();
    std::vector<uint8_t> out(expected.size() + chunk);

    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, -MAX_WBITS), Z_OK);
    strm.next_in = compr.data();
    strm.avail_in = (uint32_t)compr.size();

    int err = Z_OK;
    size_t produced = 0;
    while (err == Z_OK) {
        strm.next_out = out.data() + produced;
        strm.avail_out = (uint32_t)chunk;
        err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
        produced = (size_t)strm.total_out;
        ASSERT_LE(produced, expected.size());
    }
    EXPECT_EQ(err, Z_STREAM_END);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    ASSERT_EQ(produced, expected.size());

    size_t first = 0;
    while (first < produced && out[first] == expected[first])
        first++;
    EXPECT_EQ(first, produced) << "first mismatch at byte " << first << " of " << produced << " with "
                               << chunk << "-byte output buffers";
}

/* 0 decodes into one buffer. Smaller buffers leave the history in the window, so far matches take
   the window copies of inflate_fast and of the slow path. */
INSTANTIATE_TEST_SUITE_P(inflate, inflate_far_distance,
    ::testing::Values(0u, 1u, 7u, 64u, 300u, 1000u, 4096u, 16384u, 32767u));
