/* test_inflate_cve.cc - Test inflate() rejects the malformed gzip streams from past CVEs */

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

/* CVE-2002-0059 - inflateEnd to release memory more than once */
static z_const uint8_t cve_2002_0059_head[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xed, 0xd0, 0x21, 0x01, 0x00, 0x00,
    0x00, 0x10, 0xa0, 0xee, 0x1b, 0xff, 0x9f, 0x73, 0x84, 0x15, 0x26, 0xd0, 0x62
};
/* The body of the stream is the same 9 bytes repeated */
static z_const uint8_t cve_2002_0059_body[] = {
    0xc0, 0x80, 0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60
};
#define CVE_2002_0059_BODY_COUNT 507
static z_const uint8_t cve_2002_0059_tail[] = {
    0xc0, 0x80, 0x01, 0x03, 0x06, 0x0c, 0x18, 0xf8, 0x06, 0x32, 0xfb, 0xd0, 0xb3, 0x1b, 0x01, 0x00,
    0x10, 0x00
};

/* CVE-2004-0797 - Error handling in inflate and inflateBack causes crash */
static z_const uint8_t cve_2004_0797[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xed, 0xd0, 0x21, 0x01, 0x00, 0x06,
    0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x01, 0x03, 0x08, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0xbb, 0xbb,
    0xbb, 0xbb, 0x43, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
    0xc0, 0x80, 0x01, 0x03
};

/* CVE-2005-1849 - inftrees.h bug causes crash */
static z_const uint8_t cve_2005_1849[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xed, 0xd4, 0x21, 0x01, 0x00, 0x06,
    0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x01, 0x03, 0x08, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x01,
    0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
    0xc0, 0x80, 0x01, 0x03
};

/* CVE-2005-2096 - Buffer overflow when incomplete code description */
static z_const uint8_t cve_2005_2096[] = {
    0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xed, 0xd0, 0x21, 0x01, 0xc0, 0x80,
    0x01, 0x03, 0x06, 0x73, 0xde, 0xb9, 0xc0, 0x80, 0x01, 0x03, 0x80, 0x0c, 0x18, 0x30, 0x60, 0xc0,
    0x80, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
    0x80, 0x01, 0x03, 0x06
};

/* Inflate a malformed gzip stream and expect a data error instead of a crash */
static void inflate_malformed(z_const uint8_t *compr, uint32_t compr_len) {
    PREFIX3(stream) strm;
    uint8_t uncompr[1024];
    int err;

    memset(&strm, 0, sizeof(strm));

    err = PREFIX(inflateInit2)(&strm, MAX_WBITS + 16);
    ASSERT_EQ(err, Z_OK);

    strm.next_in = compr;
    strm.avail_in = compr_len;

    do {
        strm.next_out = uncompr;
        strm.avail_out = (uint32_t)sizeof(uncompr);
        err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    } while (err == Z_OK);
    EXPECT_EQ(err, Z_DATA_ERROR);

    err = PREFIX(inflateEnd)(&strm);
    EXPECT_EQ(err, Z_OK);
}

TEST(inflate, cve_2002_0059) {
    uint8_t compr[sizeof(cve_2002_0059_head) + sizeof(cve_2002_0059_body) * CVE_2002_0059_BODY_COUNT +
        sizeof(cve_2002_0059_tail)];
    uint8_t *next = compr;

    memcpy(next, cve_2002_0059_head, sizeof(cve_2002_0059_head));
    next += sizeof(cve_2002_0059_head);
    for (int i = 0; i < CVE_2002_0059_BODY_COUNT; i++) {
        memcpy(next, cve_2002_0059_body, sizeof(cve_2002_0059_body));
        next += sizeof(cve_2002_0059_body);
    }
    memcpy(next, cve_2002_0059_tail, sizeof(cve_2002_0059_tail));

    inflate_malformed(compr, (uint32_t)sizeof(compr));
}

TEST(inflate, cve_2004_0797) {
    inflate_malformed(cve_2004_0797, (uint32_t)sizeof(cve_2004_0797));
}

TEST(inflate, cve_2005_1849) {
    inflate_malformed(cve_2005_1849, (uint32_t)sizeof(cve_2005_1849));
}

TEST(inflate, cve_2005_2096) {
    inflate_malformed(cve_2005_2096, (uint32_t)sizeof(cve_2005_2096));
}

/* CINFO 8..15 used to be stored in wbits before it was rejected. inflateReset()
 * keeps wbits, and the next stream then copied past the 32K window. */
TEST(inflate, invalid_cinfo_does_not_stick) {
    const size_t n = 128 * 1024;
    PREFIX3(stream) strm, comp;
    uint8_t hdr[2];
    uint8_t *src, *dst, *compressed;
    uint32_t dictlen;
    unsigned long bound;
    size_t produced;
    int err;

    src = (uint8_t *)calloc(1, n);
    dst = (uint8_t *)malloc(n);
    compressed = nullptr;
    /* scan-build treats ASSERT_* as a return, so free before leaving. */
    if (src == nullptr || dst == nullptr) {
        free(src);
        free(dst);
        FAIL() << "out of memory";
    }

    memset(&comp, 0, sizeof(comp));
    err = PREFIX(deflateInit)(&comp, 1);
    if (err != Z_OK) {
        free(dst);
        free(src);
        ASSERT_EQ(err, Z_OK);
    }
    bound = PREFIX(deflateBound)(&comp, (unsigned long)n);
    compressed = (uint8_t *)malloc(bound);
    if (compressed == nullptr) {
        PREFIX(deflateEnd)(&comp);
        free(dst);
        free(src);
        FAIL() << "out of memory";
    }
    comp.next_in = src;
    comp.avail_in = (uint32_t)n;
    comp.next_out = compressed;
    comp.avail_out = (uint32_t)bound;
    err = PREFIX(deflate)(&comp, Z_FINISH);
    bound = (unsigned long)comp.total_out;
    PREFIX(deflateEnd)(&comp);
    if (err != Z_STREAM_END) {
        free(compressed);
        free(dst);
        free(src);
        ASSERT_EQ(err, Z_STREAM_END);
    }

    memset(&strm, 0, sizeof(strm));
    err = PREFIX(inflateInit2)(&strm, 0);
    if (err != Z_OK) {
        free(compressed);
        free(dst);
        free(src);
        ASSERT_EQ(err, Z_OK);
    }

    /* CMF CINFO 15, CM 8. 0xF800 is divisible by 31, so FLG is 0. */
    hdr[0] = 0xf8;
    hdr[1] = 0x00;
    strm.next_in = hdr;
    strm.avail_in = sizeof(hdr);
    strm.next_out = dst;
    strm.avail_out = 64;
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);

    err = PREFIX(inflateReset)(&strm);
    if (err != Z_OK) {
        PREFIX(inflateEnd)(&strm);
        free(compressed);
        free(dst);
        free(src);
        ASSERT_EQ(err, Z_OK);
    }
    strm.next_in = compressed;
    strm.avail_in = (uint32_t)bound;
    produced = 0;
    do {
        strm.next_out = dst + produced;
        strm.avail_out = (uint32_t)MIN(n - produced, (size_t)64 * 1024);
        err = PREFIX(inflate)(&strm, Z_NO_FLUSH);
        produced = (size_t)strm.total_out;
        if (produced > n) {
            PREFIX(inflateEnd)(&strm);
            free(compressed);
            free(dst);
            free(src);
            FAIL() << "inflate wrote " << produced << " bytes into " << n;
        }
    } while (err == Z_OK);

    EXPECT_EQ(err, Z_STREAM_END);
    EXPECT_EQ(produced, n);
    EXPECT_EQ(memcmp(src, dst, n), 0);
    dictlen = 0;
    /* NULL dictionary: only the length is written, and it stays within the 32K window. */
    EXPECT_EQ(PREFIX(inflateGetDictionary)(&strm, NULL, &dictlen), Z_OK);
    EXPECT_LE(dictlen, 32768u);

    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
    free(compressed);
    free(dst);
    free(src);
}

/* windowBits 0 still takes a valid size from the zlib header. */
TEST(inflate, window_size_from_header) {
    PREFIX3(stream) strm, comp;
    uint8_t src[64], dst[64], compressed[128];
    unsigned long bound;
    int i;

    for (i = 0; i < (int)sizeof(src); i++)
        src[i] = (uint8_t)(i * 3 + 1);

    memset(&comp, 0, sizeof(comp));
    ASSERT_EQ(PREFIX(deflateInit)(&comp, Z_DEFAULT_COMPRESSION), Z_OK);
    comp.next_in = src;
    comp.avail_in = sizeof(src);
    comp.next_out = compressed;
    comp.avail_out = sizeof(compressed);
    ASSERT_EQ(PREFIX(deflate)(&comp, Z_FINISH), Z_STREAM_END);
    bound = (unsigned long)comp.total_out;
    PREFIX(deflateEnd)(&comp);

    memset(&strm, 0, sizeof(strm));
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, 0), Z_OK);
    strm.next_in = compressed;
    strm.avail_in = (uint32_t)bound;
    strm.next_out = dst;
    strm.avail_out = sizeof(dst);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_FINISH), Z_STREAM_END);
    EXPECT_EQ(memcmp(src, dst, sizeof(src)), 0);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}

/* A header window larger than the one the caller asked for stays an error. */
TEST(inflate, header_window_exceeds_request) {
    PREFIX3(stream) strm, comp;
    uint8_t src[32], dst[32], compressed[64];

    memset(src, 0x11, sizeof(src));
    memset(&comp, 0, sizeof(comp));
    ASSERT_EQ(PREFIX(deflateInit)(&comp, Z_DEFAULT_COMPRESSION), Z_OK);
    comp.next_in = src;
    comp.avail_in = sizeof(src);
    comp.next_out = compressed;
    comp.avail_out = sizeof(compressed);
    ASSERT_EQ(PREFIX(deflate)(&comp, Z_FINISH), Z_STREAM_END);
    unsigned long bound = (unsigned long)comp.total_out;
    PREFIX(deflateEnd)(&comp);

    memset(&strm, 0, sizeof(strm));
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, 10), Z_OK);
    strm.next_in = compressed;
    strm.avail_in = (uint32_t)bound;
    strm.next_out = dst;
    strm.avail_out = sizeof(dst);
    EXPECT_EQ(PREFIX(inflate)(&strm, Z_NO_FLUSH), Z_DATA_ERROR);
    EXPECT_EQ(PREFIX(inflateEnd)(&strm), Z_OK);
}
