/* test_inflate_back_window_oob.cc - Regression tests for issue #2316:
 *   inflateBack() out-of-bounds READ past the end of the caller-supplied,
 *   exact-size (2**windowBits, no padding) window.
 *
 * These tests are written purely against the behavioural contract:
 *
 *   - inflateBackInit() takes a window of EXACTLY (1 << windowBits) bytes and
 *     uses it verbatim (no internal over-allocation / padding).  This is the
 *     condition under which the SIMD chunked window-source copy could over-read
 *     past the window's end.
 *   - inflateBack() must decode valid raw-deflate streams correctly, and must
 *     never read past window+ (1<<windowBits), regardless of how back-references
 *     straddle the window boundary, what the match distance is (including 1 and
 *     small powers of two), or how the output/MATCH bailout path is exercised.
 *
 * The oracle for the "no over-read" property is ASan/UBSan: the window is sized
 * EXACTLY to the contract so any over-read lands in a red zone.  The oracle for
 * "still correct" is byte-for-byte comparison against the original input.
 *
 * NOTE: the window is deliberately malloc()'d at exactly (1 << windowBits).  Do
 * not "round up" or share a max-size buffer -- that exact sizing is the whole
 * point; a padded/oversized window would hide the very bug under test.
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

/* ------------------------------------------------------------------ */
/* inflateBack() in()/out() callback plumbing                          */
/* ------------------------------------------------------------------ */

struct in_ctx {
    const uint8_t *data;
    size_t len;
    size_t pos;
    size_t chunk;       /* 0 => hand everything over in one shot */
};

/* Mirrors the fuzz harness's in(): a 0-length return at exhaustion is the
 * normal end-of-input / truncation signal. */
static z_uint32_t in_cb(void *desc, z_const unsigned char **buf) {
    in_ctx *st = (in_ctx *)desc;
    if (st->pos >= st->len) {
        *buf = (z_const unsigned char *)(st->data + st->pos);
        return 0;
    }
    size_t remaining = st->len - st->pos;
    size_t n = st->chunk ? (remaining < st->chunk ? remaining : st->chunk)
                         : remaining;
    *buf = (z_const unsigned char *)(st->data + st->pos);
    st->pos += n;
    return (z_uint32_t)n;
}

struct out_ctx {
    uint8_t *buf;       /* capture target, or nullptr to discard */
    size_t cap;
    size_t produced;
    int abort_after;    /* if >0, return non-zero once produced >= abort_after */
    int overflowed;     /* set if produced would exceed cap */
};

static z_int32_t out_cb(void *desc, unsigned char *buf, z_uint32_t len) {
    out_ctx *st = (out_ctx *)desc;
    if (st->buf) {
        if (st->produced + len > st->cap) {
            st->overflowed = 1;
            return 1;
        }
        memcpy(st->buf + st->produced, buf, len);
    }
    st->produced += len;
    if (st->abort_after && st->produced >= (size_t)st->abort_after)
        return 1;       /* force the out() abort / MATCH-bailout path */
    return 0;
}

/* Decode `comp` with inflateBack() over an EXACT-size window.  Captures output
 * into `out` (may be nullptr) and returns the inflateBack() status. */
static int run_inflate_back(const uint8_t *comp, size_t comp_len, int wbits,
                            size_t in_chunk, int abort_after,
                            uint8_t *out, size_t out_cap, size_t *produced) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));

    /* EXACT-size, caller-supplied, unpadded window -- the bug's oracle. */
    size_t wsize = (size_t)1 << wbits;
    uint8_t *window = (uint8_t *)malloc(wsize);
    if (window == nullptr)
        return Z_MEM_ERROR;

    int err = PREFIX(inflateBackInit)(&strm, wbits, window);
    if (err != Z_OK) {
        free(window);
        return err;
    }

    in_ctx ist = { comp, comp_len, 0, in_chunk };
    out_ctx ost = { out, out_cap, 0, abort_after, 0 };

    strm.next_in = nullptr;
    strm.avail_in = 0;

    err = PREFIX(inflateBack)(&strm, in_cb, &ist, out_cb, &ost);

    PREFIX(inflateBackEnd)(&strm);
    free(window);

    if (produced)
        *produced = ost.produced;
    return err;
}

/* Raw-deflate compress `src` (no zlib/gzip wrapper) at `wbits`.
 *
 * deflate() does not support a raw 8-bit window (it requires >= 9), so the
 * compressor window is clamped up to 9.  Callers that inflateBack at
 * windowBits==8 must therefore use data whose back-references are all at
 * distance < 256 (e.g. constant RLE runs, which only ever match at distance 1)
 * so the produced stream is still decodable in a 256-byte window.  For
 * windowBits >= 9 the compressor and inflate windows match, so deflate can
 * never emit a distance exceeding the inflateBack window. */
static std::vector<uint8_t> compress_raw(const uint8_t *src, size_t n, int wbits,
                                         int level = Z_BEST_COMPRESSION) {
    int cwbits = wbits < 9 ? 9 : wbits;
    PREFIX3(stream) c;
    memset(&c, 0, sizeof(c));
    int err = PREFIX(deflateInit2)(&c, level, Z_DEFLATED, -cwbits,
                                   MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    EXPECT_EQ(err, Z_OK);

    z_uintmax_t bound = PREFIX(deflateBound)(&c, (z_uintmax_t)n);
    std::vector<uint8_t> out(bound ? bound : 1);

    c.next_in = (z_const unsigned char *)src;
    c.avail_in = (uint32_t)n;
    c.next_out = out.data();
    c.avail_out = (uint32_t)out.size();

    err = PREFIX(deflate)(&c, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);
    out.resize(c.total_out);
    PREFIX(deflateEnd)(&c);
    return out;
}

/* ================================================================== */
/* A. Happy-path round trips across the full windowBits range          */
/* ================================================================== */

/* Build data that forces many back-references whose source straddles the
 * inflate window boundary.  The single-byte "rotation" emitted each cycle sweeps
 * the alignment of the window-sized copy relative to inflateBack's periodic
 * window flush, so over a long stream the window portion `op` of a match takes
 * every small value (1..CHUNKSIZE-1) -- exactly the over-read trigger.  Small
 * fixed-distance runs additionally exercise the dist==1 / 2 / 4 / 8 / 16
 * chunkmemset "magazine" copy paths. */
static std::vector<uint8_t> gen_boundary_data(int wbits, int cycles) {
    uint32_t wsize = 1u << wbits;
    std::vector<uint8_t> b;
    b.reserve((size_t)wsize * (cycles + 2));

    uint32_t lcg = 0x9e3779b9u ^ (uint32_t)wbits;
    auto rnd = [&]() -> uint8_t {
        lcg = lcg * 1103515245u + 12345u;
        return (uint8_t)(lcg >> 16);
    };

    /* Seed block: a full window of varied bytes (mostly literals). */
    for (uint32_t i = 0; i < wsize; i++)
        b.push_back(rnd());

    static const uint32_t small_d[5] = { 1, 2, 4, 8, 16 };

    for (int c = 0; c < cycles; c++) {
        /* Rotate alignment by a few fresh literals each cycle. */
        uint32_t shift = (uint32_t)c % 37u;
        for (uint32_t k = 0; k < shift; k++)
            b.push_back(rnd());

        /* Long copy at distance ~wsize: its source repeatedly crosses the
         * window boundary as inflateBack flushes, producing small `op`. */
        size_t base = b.size() - wsize;
        for (uint32_t k = 0; k < wsize; k++)
            b.push_back(b[base + k]);

        /* Small-distance run -> RLE / magazine copy path. */
        uint32_t d = small_d[c % 5];
        size_t from = b.size() - d;
        for (uint32_t k = 0; k < 300; k++)
            b.push_back(b[from + (k % d)]);
    }
    return b;
}

class inflate_back_oob : public ::testing::TestWithParam<int> {};

TEST_P(inflate_back_oob, boundary_roundtrip) {
    int wbits = GetParam();

    std::vector<uint8_t> orig = gen_boundary_data(wbits, 48);
    std::vector<uint8_t> comp = compress_raw(orig.data(), orig.size(), wbits);

    std::vector<uint8_t> got(orig.size());
    size_t produced = 0;
    int err = run_inflate_back(comp.data(), comp.size(), wbits,
                               /*in_chunk*/ 0, /*abort_after*/ 0,
                               got.data(), got.size(), &produced);

    EXPECT_EQ(err, Z_STREAM_END);
    ASSERT_EQ(produced, orig.size());
    EXPECT_EQ(memcmp(got.data(), orig.data(), orig.size()), 0);
}

/* Same data, but hand input to inflateBack() in tiny adversarial chunks so the
 * decoder is repeatedly re-entered mid-stream (independent of the window OOB,
 * a useful robustness axis). */
TEST_P(inflate_back_oob, boundary_roundtrip_tiny_input_chunks) {
    int wbits = GetParam();

    std::vector<uint8_t> orig = gen_boundary_data(wbits, 24);
    std::vector<uint8_t> comp = compress_raw(orig.data(), orig.size(), wbits);

    std::vector<uint8_t> got(orig.size());
    size_t produced = 0;
    int err = run_inflate_back(comp.data(), comp.size(), wbits,
                               /*in_chunk*/ 3, /*abort_after*/ 0,
                               got.data(), got.size(), &produced);

    EXPECT_EQ(err, Z_STREAM_END);
    ASSERT_EQ(produced, orig.size());
    EXPECT_EQ(memcmp(got.data(), orig.data(), orig.size()), 0);
}

/* Cover the full documented windowBits domain 8..15, with the two boundary
 * values from the plan (8 -> 256B window, 15 -> 32KiB).  For windowBits 8 the
 * compressor clamps to a 9-bit window whose MAX_DIST (512-262=250) still fits a
 * 256-byte inflateBack window, so the stream remains decodable there. */
INSTANTIATE_TEST_SUITE_P(all_window_bits, inflate_back_oob,
                         ::testing::Values(8, 9, 10, 11, 12, 13, 14, 15));

/* ================================================================== */
/* B. Targeted distance / match-shape edge cases                       */
/* ================================================================== */

/* Decode `orig` via raw deflate + inflateBack at `wbits`, asserting an exact
 * round trip.  Shared by the small focused cases below. */
static void expect_roundtrip(const std::vector<uint8_t> &orig, int wbits,
                             size_t in_chunk = 0) {
    std::vector<uint8_t> comp = compress_raw(orig.data(), orig.size(), wbits);
    std::vector<uint8_t> got(orig.empty() ? 1 : orig.size());
    size_t produced = 0;
    int err = run_inflate_back(comp.data(), comp.size(), wbits, in_chunk, 0,
                               got.data(), got.size(), &produced);
    EXPECT_EQ(err, Z_STREAM_END);
    ASSERT_EQ(produced, orig.size());
    if (!orig.empty()) {
        EXPECT_EQ(memcmp(got.data(), orig.data(), orig.size()), 0);
    }
}

/* dist == 1: long RLE run (memset magazine path). */
TEST(inflate_back_oob_shapes, dist1_rle_run) {
    for (int wbits = 8; wbits <= 15; wbits++) {
        std::vector<uint8_t> orig((size_t)1 << wbits, 0xA5);  /* one window of 0xA5 */
        orig.insert(orig.end(), 1000, 0x5A);                   /* + a second run */
        expect_roundtrip(orig, wbits);
    }
}

/* Small power-of-two distances 2/4/8/16 -> GET_*_MAG magazine builders. */
TEST(inflate_back_oob_shapes, small_pow2_distances) {
    for (uint32_t d : { 2u, 4u, 8u, 16u }) {
        int wbits = 12;
        std::vector<uint8_t> orig;
        for (uint32_t i = 0; i < d; i++)
            orig.push_back((uint8_t)(0x11 * (i + 1)));
        size_t target = ((size_t)1 << wbits) * 3;
        while (orig.size() < target)
            orig.push_back(orig[orig.size() - d]);
        expect_roundtrip(orig, wbits);
    }
}

/* A match at (near) the maximum legal distance for the window, emitted right
 * after the window has filled, so the back-reference source sits at the very
 * tail of the window -- the classic small-`op` over-read shape. */
TEST(inflate_back_oob_shapes, near_max_distance_after_window_fill) {
    /* Needs a window large enough for the (wsize - 270) distances below, so
     * start at 9 (windowBits 8 is covered by dist1_rle_run / the corpus). */
    for (int wbits = 9; wbits <= 15; wbits++) {
        uint32_t wsize = 1u << wbits;
        uint32_t lcg = 0x1234567u ^ (uint32_t)wbits;
        std::vector<uint8_t> orig;
        /* Fill a full window with literals. */
        for (uint32_t i = 0; i < wsize; i++) {
            lcg = lcg * 1103515245u + 12345u;
            orig.push_back((uint8_t)(lcg >> 16));
        }
        /* Then copy long runs from the largest distances deflate can encode,
         * sweeping the source across the last bytes of the window. */
        for (uint32_t off = 0; off < 64; off++) {
            size_t from = orig.size() - (wsize - 270 - off);
            for (uint32_t k = 0; k < 258; k++)
                orig.push_back(orig[from + k]);
        }
        expect_roundtrip(orig, wbits);
    }
}

/* Highly compressible long-match data forces the inflateBack MATCH bailout
 * (a match whose length exceeds the remaining window space).  Verifying a clean
 * round trip here exercises the non-fast MATCH copy path (infback.c). */
TEST(inflate_back_oob_shapes, long_matches_match_bailout_path) {
    for (int wbits = 8; wbits <= 15; wbits++) {
        size_t n = ((size_t)1 << wbits) * 40;
        std::vector<uint8_t> orig(n);
        for (size_t i = 0; i < n; i++)
            orig[i] = (uint8_t)(i % 5);          /* len=258 dist=5 tokens */
        expect_roundtrip(orig, wbits, /*in_chunk*/ 7);
    }
}

/* ================================================================== */
/* C. Error / abort paths must stay clean (no crash, sane status)      */
/* ================================================================== */

/* out() aborting mid-stream must terminate inflateBack() without over-reading
 * the window and without returning Z_STREAM_END. */
TEST(inflate_back_oob_errors, out_callback_abort) {
    int wbits = 12;
    std::vector<uint8_t> orig = gen_boundary_data(wbits, 8);
    std::vector<uint8_t> comp = compress_raw(orig.data(), orig.size(), wbits);

    size_t produced = 0;
    int err = run_inflate_back(comp.data(), comp.size(), wbits, 0,
                               /*abort_after*/ 16, nullptr, 0, &produced);
    EXPECT_NE(err, Z_STREAM_END);   /* aborted before completion */
}

/* Truncated input: in() reports end-of-data early.  Must fail cleanly, not
 * crash or over-read. */
TEST(inflate_back_oob_errors, truncated_input) {
    int wbits = 12;
    std::vector<uint8_t> orig = gen_boundary_data(wbits, 8);
    std::vector<uint8_t> comp = compress_raw(orig.data(), orig.size(), wbits);

    /* Drop the tail so the stream cannot complete. */
    size_t truncated = comp.size() / 2;
    std::vector<uint8_t> got(orig.size());
    size_t produced = 0;
    int err = run_inflate_back(comp.data(), truncated, wbits, 0, 0,
                               got.data(), got.size(), &produced);
    EXPECT_NE(err, Z_STREAM_END);
}

/* Garbage input (invalid deflate) must be rejected without touching memory it
 * shouldn't.  A distance-too-far block in particular must be caught before any
 * window-source read. */
TEST(inflate_back_oob_errors, invalid_stream_rejected) {
    int wbits = 10;
    /* A stored-block header claiming a length but no payload, plus noise. */
    const uint8_t junk[] = {
        0x01, 0xff, 0xff, 0x00, 0x00,   /* final stored block, len=0xffff */
        0xde, 0xad, 0xbe, 0xef, 0x55, 0xaa, 0x12, 0x34
    };
    std::vector<uint8_t> got(1 << wbits);
    size_t produced = 0;
    int err = run_inflate_back(junk, sizeof(junk), wbits, 0, 0,
                               got.data(), got.size(), &produced);
    EXPECT_NE(err, Z_STREAM_END);
}

/* ================================================================== */
/* D. Direct replay of committed crash reproducers (#2316 corpus)      */
/* ================================================================== */

/* The fuzz harness's input protocol:
 *   byte[0]   -> windowBits selector: wbits = 8 + (byte[0] % 8)   (8..15)
 *   byte[1]   -> in() chunk size: chunk = byte[1] + 1             (1..256)
 *   byte[2]   -> if odd, out() aborts after the first call
 *   byte[3..] -> the raw-deflate stream
 * with the window sized EXACTLY (1 << windowBits).  Replaying a crashing input
 * through the public API under ASan is the precise regression oracle: before the
 * fix these over-read the window; after it they must return cleanly. */
static void replay_fuzz_input(const uint8_t *data, size_t size) {
    if (size < 4)
        return;
    int wbits = 8 + (data[0] % 8);
    size_t chunk = (size_t)data[1] + 1;
    int abort_out = data[2] & 1;
    /* Just decode -- the assertion is "no ASan report / clean return". */
    int err = run_inflate_back(data + 3, size - 3, wbits, chunk,
                               abort_out ? 1 : 0, nullptr, 0, nullptr);
    (void)err;  /* any status is acceptable; not crashing is the point. */
}

/* crash-a7bacb6ace55f9708fc9c22057b348f485eb650a (43 bytes) */
static const uint8_t crash_a7bacb[] = {
    0x00, 0x93, 0x93, 0x93, 0x93, 0x7e, 0x01, 0x03, 0x06, 0x0c, 0x18, 0x30,
    0x60, 0xc0, 0x80, 0x01, 0x03, 0x06, 0x0c, 0x18, 0xc0, 0x60, 0x80, 0x30,
    0x01, 0x03, 0x06, 0x0c, 0x08, 0x93, 0x93, 0x5b, 0x5b, 0x5d, 0x12, 0x00,
    0x00, 0x2b, 0x00, 0x00, 0x02, 0x00, 0x07
};

/* crash-80b88703152efccdbfdb871736cac2900ccdc0ca (83 bytes) */
static const uint8_t crash_80b887[] = {
    0x00, 0x3b, 0x5b, 0x5b, 0x5b, 0x5d, 0xff, 0xff, 0xff, 0xff, 0x7e, 0xff,
    0xff, 0xfd, 0x2d, 0xb6, 0x01, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47,
    0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47,
    0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47,
    0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47,
    0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47,
    0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x47, 0x3a, 0xb7, 0x00, 0x7e
};

/* crash-325a8dc9ac3abb335a2ad61c6a1e8bd8d30c184b (89 bytes) */
static const uint8_t crash_325a8d[] = {
    0x00, 0x5b, 0x5b, 0x5b, 0x5d, 0x00, 0x01, 0x01, 0xf6, 0xf6, 0x23, 0xf6,
    0xf6, 0xf6, 0xf6, 0xf6, 0xf6, 0xf8, 0xf6, 0xf6, 0xff, 0xff, 0x01, 0x03,
    0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x01, 0x03, 0x06, 0x0c, 0x18,
    0x30, 0xf3, 0xe7, 0xcf, 0x9f, 0x3f, 0x7e, 0x01, 0x03, 0x06, 0x0c, 0x18,
    0x30, 0x60, 0xc0, 0x80, 0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
    0x80, 0x01, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf6,
    0xf6, 0x74, 0x24, 0x00, 0x7e
};

TEST(inflate_back_oob_corpus, crash_a7bacb) {
    replay_fuzz_input(crash_a7bacb, sizeof(crash_a7bacb));
}
TEST(inflate_back_oob_corpus, crash_80b887) {
    replay_fuzz_input(crash_80b887, sizeof(crash_80b887));
}
TEST(inflate_back_oob_corpus, crash_325a8d) {
    replay_fuzz_input(crash_325a8d, sizeof(crash_325a8d));
}
