/* test_inflate_long_codes.cc - Test inflate_fast() refills with the longest codes deflate allows */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <tuple>
#include <vector>

#include "zutil.h"

#include <gtest/gtest.h>

#define LITLEN_COUNT 286
#define DIST_COUNT 30
#define CODELEN_COUNT 19
#define END_BLOCK 256

/* Literals grouped by code length, the long ones need a second level table lookup */
#define LIT_SHORT 'A'   /* 1 to MAX_LEN_ROOT_BITS - 2 bits, one literal for each length */
#define LIT_ROOT 'a'    /* MAX_LEN_ROOT_BITS bits */
#define LIT_LONG 0x80   /* MAX_BITS bits */

#define SHORT_COUNT (MAX_LEN_ROOT_BITS - 2)
#define ROOT_COUNT 3
#define LONG_COUNT ((1 << (MAX_BITS - MAX_LEN_ROOT_BITS)) - 4)

#define WINDOW_SIZE ((size_t)1 << MAX_WBITS)

/* Length or distance symbol with its base value and number of extra bits */
struct extra_sym {
    int sym;
    uint32_t base;
    int extra_bits;
};

static const extra_sym len_3 = {257, 3, 0};
static const extra_sym len_195 = {283, 195, 5};
static const extra_sym len_258 = {285, 258, 0};
static const extra_sym dist_1 = {0, 1, 0};
static const extra_sym dist_24577 = {29, 24577, MAX_DIST_EXTRA_BITS};

/* Writes a dynamic block along with the output that is expected from it */
class long_code_stream {
public:
    std::vector<uint8_t> compr;
    std::vector<uint8_t> plain;

    long_code_stream() : hold(0), bits(0) {
        memset(litlen_lens, 0, sizeof(litlen_lens));
        memset(dist_lens, 0, sizeof(dist_lens));
        memset(codelen_lens, 0, sizeof(codelen_lens));

        /* Code lengths 1 to MAX_LEN_ROOT_BITS - 2 leave four codes of MAX_LEN_ROOT_BITS */
        for (int i = 0; i < SHORT_COUNT; i++)
            litlen_lens[LIT_SHORT + i] = (uint8_t)(i + 1);
        /* Three of them are literals and the long codes share the last one as their prefix */
        for (int i = 0; i < ROOT_COUNT; i++)
            litlen_lens[LIT_ROOT + i] = MAX_LEN_ROOT_BITS;
        for (int i = 0; i < LONG_COUNT; i++)
            litlen_lens[LIT_LONG + i] = MAX_BITS;
        litlen_lens[END_BLOCK] = MAX_BITS;
        litlen_lens[len_3.sym] = MAX_BITS;
        litlen_lens[len_195.sym] = MAX_BITS;
        litlen_lens[len_258.sym] = MAX_BITS;

        /* Code lengths 1 to MAX_BITS - 1 leave two distance codes of MAX_BITS */
        for (int i = 0; i < MAX_BITS - 1; i++)
            dist_lens[i] = (uint8_t)(i + 1);
        dist_lens[dist_24577.sym - 1] = MAX_BITS;
        dist_lens[dist_24577.sym] = MAX_BITS;

        /* Code lengths 0 to 15 get a 4-bit code each, so repeat codes are not needed */
        for (int i = 0; i <= MAX_BITS; i++)
            codelen_lens[i] = 4;

        make_codes(litlen_lens, LITLEN_COUNT, litlen_codes);
        make_codes(dist_lens, DIST_COUNT, dist_codes);
        make_codes(codelen_lens, CODELEN_COUNT, codelen_codes);

        put_header();
    }

    void literal(int sym) {
        put_bits(litlen_codes[sym], litlen_lens[sym]);
        plain.push_back((uint8_t)sym);
    }

    /* Extra bits are either all set or all clear */
    void match(const extra_sym &len, const extra_sym &dist, bool extra_set) {
        uint32_t len_extra = extra_set ? (1U << len.extra_bits) - 1 : 0;
        uint32_t dist_extra = extra_set ? (1U << dist.extra_bits) - 1 : 0;

        put_bits(litlen_codes[len.sym], litlen_lens[len.sym]);
        put_bits(len_extra, len.extra_bits);
        put_bits(dist_codes[dist.sym], dist_lens[dist.sym]);
        put_bits(dist_extra, dist.extra_bits);

        size_t from = plain.size() - (dist.base + dist_extra);
        for (uint32_t i = 0; i < len.base + len_extra; i++) {
            uint8_t value = plain[from + i];
            plain.push_back(value);
        }
    }

    /* Bit offset of the next symbol within its byte */
    int phase() const {
        return bits;
    }

    void finish() {
        put_bits(litlen_codes[END_BLOCK], litlen_lens[END_BLOCK]);
        if (bits > 0)
            compr.push_back((uint8_t)hold);
    }

private:
    uint8_t litlen_lens[LITLEN_COUNT];
    uint8_t dist_lens[DIST_COUNT];
    uint8_t codelen_lens[CODELEN_COUNT];
    uint32_t litlen_codes[LITLEN_COUNT];
    uint32_t dist_codes[DIST_COUNT];
    uint32_t codelen_codes[CODELEN_COUNT];
    uint64_t hold;
    int bits;

    void put_bits(uint32_t value, int count) {
        hold |= (uint64_t)value << bits;
        bits += count;
        while (bits >= 8) {
            compr.push_back((uint8_t)hold);
            hold >>= 8;
            bits -= 8;
        }
    }

    /* Assign canonical codes, bit reversed since codes are sent starting from their top bit */
    static void make_codes(const uint8_t *lens, int count, uint32_t *codes) {
        uint32_t next_code[MAX_BITS + 1];
        uint32_t code = 0;

        for (int len = 1; len <= MAX_BITS; len++) {
            next_code[len] = code;
            for (int i = 0; i < count; i++)
                code += (lens[i] == len);
            code <<= 1;
        }
        for (int i = 0; i < count; i++) {
            codes[i] = 0;
            if (lens[i] == 0)
                continue;
            code = next_code[lens[i]]++;
            for (int bit = 0; bit < lens[i]; bit++)
                codes[i] |= ((code >> bit) & 1) << (lens[i] - 1 - bit);
        }
    }

    void put_header() {
        static const uint8_t order[CODELEN_COUNT] = {
            16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
        };

        put_bits(1, 1);     /* Last block */
        put_bits(2, 2);     /* Dynamic codes */
        put_bits(LITLEN_COUNT - 257, 5);
        put_bits(DIST_COUNT - 1, 5);
        put_bits(CODELEN_COUNT - 4, 4);
        for (int i = 0; i < CODELEN_COUNT; i++)
            put_bits(codelen_lens[order[i]], 3);
        for (int i = 0; i < LITLEN_COUNT; i++)
            put_bits(codelen_codes[litlen_lens[i]], codelen_lens[litlen_lens[i]]);
        for (int i = 0; i < DIST_COUNT; i++)
            put_bits(codelen_codes[dist_lens[i]], codelen_lens[dist_lens[i]]);
    }
};

/* Literal whose code has the given number of bits */
static int literal_sym(int bits) {
    if (bits == MAX_BITS)
        return LIT_LONG;
    if (bits == MAX_LEN_ROOT_BITS)
        return LIT_ROOT;
    return LIT_SHORT + bits - 1;
}

/* Every literal code length that the block has */
static std::vector<int> literal_bits() {
    std::vector<int> bits;

    for (int i = 1; i <= SHORT_COUNT; i++)
        bits.push_back(i);
    bits.push_back(MAX_LEN_ROOT_BITS);
    bits.push_back(MAX_BITS);
    return bits;
}

static void inflate_check(const long_code_stream &stream, size_t slack) {
    SCOPED_TRACE(testing::Message() << "slack: " << slack);
    std::vector<uint8_t> uncompr(stream.plain.size() + slack);
    PREFIX3(stream) strm;
    int err;

    memset(&strm, 0, sizeof(strm));

    err = PREFIX(inflateInit2)(&strm, -MAX_WBITS);
    ASSERT_EQ(err, Z_OK);

    strm.next_in = (z_const uint8_t *)stream.compr.data();
    strm.avail_in = (uint32_t)stream.compr.size();
    strm.next_out = uncompr.data();
    strm.avail_out = (uint32_t)uncompr.size();

    err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    EXPECT_EQ(err, Z_STREAM_END);
    EXPECT_EQ(strm.total_out, stream.plain.size());

    err = PREFIX(inflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);

    EXPECT_EQ(memcmp(uncompr.data(), stream.plain.data(), stream.plain.size()), 0);
}

/* Parameters are the code lengths of the two literals in front of the match */
class inflate_long_codes : public testing::TestWithParam<std::tuple<int, int>> {
public:
    static void SetUpTestSuite() {
        window = new long_code_stream();

        /* Fill the window with runs of different bytes, so a wrong distance copies other data */
        for (int i = 0; window->plain.size() < WINDOW_SIZE; i++) {
            window->literal(LIT_SHORT + i % SHORT_COUNT);
            window->match(len_258, dist_1, false);
        }
    }
    static void TearDownTestSuite() {
        delete window;
        window = NULL;
    }

    static long_code_stream *window;
};

long_code_stream *inflate_long_codes::window = NULL;

TEST_P(inflate_long_codes, match) {
    int first = literal_sym(std::get<0>(GetParam()));
    int second = literal_sym(std::get<1>(GetParam()));

    for (int phase = 0; phase < 8; phase++) {
        SCOPED_TRACE(testing::Message() << "phase: " << phase);
        long_code_stream stream = *window;

        /* Shift where the refills fall relative to the symbols that follow */
        while (stream.phase() != phase)
            stream.literal(LIT_SHORT);
        /* A match ends a loop iteration in inflate_fast(), so the next symbol starts one */
        stream.match(len_3, dist_1, false);

        stream.literal(first);
        stream.literal(second);
        /* Longest length and distance codes with the most extra bits, all of them set */
        stream.match(len_195, dist_24577, true);
        /* Its code starts with all ones, which a table lookup that is short on bits misreads */
        stream.literal(LIT_LONG);

        /* Leave enough input for inflate_fast() to decode the symbols above */
        for (int i = 0; i < 16; i++)
            stream.literal(LIT_ROOT + i % ROOT_COUNT);
        stream.finish();

        /* The remaining output is too small for inflate_fast(), so inflate_fast_safe() runs */
        inflate_check(stream, 0);
        inflate_check(stream, 1024);
    }
}

static std::string long_codes_name(const testing::TestParamInfo<std::tuple<int, int>> &info) {
    char name[32];
    snprintf(name, sizeof(name), "first%d_second%d",
        std::get<0>(info.param), std::get<1>(info.param));
    return name;
}

INSTANTIATE_TEST_SUITE_P(inflate, inflate_long_codes,
    testing::Combine(
        testing::ValuesIn(literal_bits()),  /* first literal */
        testing::ValuesIn(literal_bits())   /* second literal */
    ),
    long_codes_name);
