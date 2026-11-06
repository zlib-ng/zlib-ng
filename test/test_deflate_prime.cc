/* test_deflate_prime.cc - Test deflatePrime() wrapping gzip around deflate stream */

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

#include "test_shared_ng.h"

#include <gtest/gtest.h>

TEST(deflate, prime) {
    PREFIX3(stream) c_stream, d_stream;
    uint8_t compr[128], uncompr[128];
    z_size_t compr_len = sizeof(compr), uncompr_len = sizeof(uncompr);
    uint32_t crc = 0;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&d_stream, 0, sizeof(d_stream));

    /* Raw deflate windowBits is -15 */
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    EXPECT_EQ(err, Z_OK);

    /* Gzip magic number */
    err = PREFIX(deflatePrime)(&c_stream, 16, 0x8b1f);
    EXPECT_EQ(err, Z_OK);
    /* Gzip compression method (deflate) */
    err = PREFIX(deflatePrime)(&c_stream, 8, 0x08);
    EXPECT_EQ(err, Z_OK);
    /* Gzip flags (one byte, using two odd bit calls) */
    err = PREFIX(deflatePrime)(&c_stream, 3, 0x0);
    EXPECT_EQ(err, Z_OK);
    err = PREFIX(deflatePrime)(&c_stream, 5, 0x0);
    EXPECT_EQ(err, Z_OK);
    /* Gzip modified time */
    err = deflate_prime_32(&c_stream, 0x0);
    EXPECT_EQ(err, Z_OK);
    /* Gzip extra flags */
    err = PREFIX(deflatePrime)(&c_stream, 8, 0x0);
    EXPECT_EQ(err, Z_OK);
    /* Gzip operating system */
    err = PREFIX(deflatePrime)(&c_stream, 8, 255);
    EXPECT_EQ(err, Z_OK);

    c_stream.next_in = (z_const unsigned char *)hello;
    c_stream.avail_in = (uint32_t)hello_len;
    c_stream.next_out = compr;
    c_stream.avail_out = (uint32_t)compr_len;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);

    /* Gzip uncompressed data crc32 */
    crc = PREFIX(crc32_z)(0, (const uint8_t *)hello, (uint32_t)hello_len);
    err = deflate_prime_32(&c_stream, crc);
    EXPECT_EQ(err, Z_OK);
    /* Gzip uncompressed data length */
    err = deflate_prime_32(&c_stream, (uint32_t)hello_len);
    EXPECT_EQ(err, Z_OK);

    err = PREFIX(deflateEnd)(&c_stream);
    EXPECT_EQ(err, Z_OK);

    d_stream.next_in  = compr;
    d_stream.avail_in = (uint32_t)c_stream.total_out;
    d_stream.next_out = uncompr;
    d_stream.avail_out = (uint32_t)uncompr_len;
    d_stream.total_in = 0;
    d_stream.total_out = 0;

    /* Inflate with gzip header */
    err = PREFIX(inflateInit2)(&d_stream, MAX_WBITS + 32);
    EXPECT_EQ(err, Z_OK);

    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    EXPECT_EQ(err, Z_BUF_ERROR);

    err = PREFIX(inflateEnd)(&d_stream);
    EXPECT_EQ(err, Z_OK);

    EXPECT_STREQ((char *)uncompr, hello);
}

/* Finish a raw deflate stream of hello after the primed bits. The primed bytes must
   come out first, followed by an intact deflate stream. */
static void deflate_after_prime(PREFIX3(stream) *c_stream, const uint8_t *primed, uint32_t primed_len) {
    PREFIX3(stream) d_stream;
    uint8_t compr[2048], uncompr[128];
    uint32_t i;
    int err;

    c_stream->next_in = (z_const unsigned char *)hello;
    c_stream->avail_in = (uint32_t)hello_len;
    c_stream->next_out = compr;
    c_stream->avail_out = (uint32_t)sizeof(compr);
    err = PREFIX(deflate)(c_stream, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);
    err = PREFIX(deflateEnd)(c_stream);
    EXPECT_EQ(err, Z_OK);

    ASSERT_GT(c_stream->total_out, primed_len);
    for (i = 0; i < primed_len && compr[i] == primed[i]; i++)
        ;
    EXPECT_EQ(i, primed_len) << "primed byte differs";

    memset(&d_stream, 0, sizeof(d_stream));
    err = PREFIX(inflateInit2)(&d_stream, -MAX_WBITS);
    ASSERT_EQ(err, Z_OK);

    d_stream.next_in = compr + primed_len;
    d_stream.avail_in = (uint32_t)c_stream->total_out - primed_len;
    d_stream.next_out = uncompr;
    d_stream.avail_out = (uint32_t)sizeof(uncompr);
    err = PREFIX(inflate)(&d_stream, Z_FINISH);
    EXPECT_EQ(err, Z_STREAM_END);

    err = PREFIX(inflateEnd)(&d_stream);
    EXPECT_EQ(err, Z_OK);

    EXPECT_STREQ((char *)uncompr, hello);
}

/* Repeated calls with no deflate() in between must stop with Z_BUF_ERROR once the
   pending buffer is full instead of writing past it */
TEST(deflate, prime_pending_full) {
    PREFIX3(stream) c_stream;
    uint8_t primed[1024];
    uint32_t pending = 0;
    int32_t bits = 0;
    int calls, err;

    memset(&c_stream, 0, sizeof(c_stream));
    /* memLevel 1 gives the smallest pending buffer */
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 1, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);

    for (calls = 0; calls < (int)sizeof(primed) / 2; calls++) {
        err = PREFIX(deflatePrime)(&c_stream, 16, calls);
        if (err != Z_OK)
            break;
        primed[calls * 2] = (uint8_t)calls;
        primed[calls * 2 + 1] = (uint8_t)(calls >> 8);
    }
    ASSERT_EQ(err, Z_BUF_ERROR) << "after " << calls << " calls";

    /* The call that did not fit inserted nothing */
    err = PREFIX(deflatePending)(&c_stream, &pending, &bits);
    ASSERT_EQ(err, Z_OK);
    EXPECT_EQ(pending, (uint32_t)calls * 2);
    EXPECT_EQ(bits, 0);

    deflate_after_prime(&c_stream, primed, pending);
}

/* Only the requested low bits of value are inserted, also into an empty bit buffer */
TEST(deflate, prime_value_bits) {
    static const uint8_t primed[] = { 0x05, 0xff };
    PREFIX3(stream) c_stream;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);

    /* Zero bits insert nothing */
    err = PREFIX(deflatePrime)(&c_stream, 0, 0x7d);
    EXPECT_EQ(err, Z_OK);
    /* 101 and 00000 make the byte 0x05 */
    err = PREFIX(deflatePrime)(&c_stream, 3, 0x7d);
    EXPECT_EQ(err, Z_OK);
    err = PREFIX(deflatePrime)(&c_stream, 5, 0x60);
    EXPECT_EQ(err, Z_OK);
    /* A full byte from a negative value */
    err = PREFIX(deflatePrime)(&c_stream, 8, -1);
    EXPECT_EQ(err, Z_OK);

    deflate_after_prime(&c_stream, primed, sizeof(primed));
}

#ifndef TEST_STOCK_ZLIB
/* Up to BIT_BUF_SIZE bits can be inserted in one call, with the bit buffer empty or not */
TEST(deflate, prime_64) {
    static const uint8_t primed[] = {
        /* 64 bits of 0x12345678 */
        0x78, 0x56, 0x34, 0x12, 0x00, 0x00, 0x00, 0x00,
        /* 5 bits of 0x15, 64 bits of -2, 33 bits of -1 and 2 zero bits */
        0xd5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f
    };
    PREFIX3(stream) c_stream;
    int err;

    memset(&c_stream, 0, sizeof(c_stream));
    err = PREFIX(deflateInit2)(&c_stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    ASSERT_EQ(err, Z_OK);

    err = PREFIX(deflatePrime)(&c_stream, 65, 0);
    EXPECT_EQ(err, Z_BUF_ERROR);

    err = PREFIX(deflatePrime)(&c_stream, 64, 0x12345678);
    EXPECT_EQ(err, Z_OK);
    err = PREFIX(deflatePrime)(&c_stream, 5, 0x15);
    EXPECT_EQ(err, Z_OK);
    err = PREFIX(deflatePrime)(&c_stream, 64, -2);
    EXPECT_EQ(err, Z_OK);
    err = PREFIX(deflatePrime)(&c_stream, 33, -1);
    EXPECT_EQ(err, Z_OK);
    err = PREFIX(deflatePrime)(&c_stream, 2, 0);
    EXPECT_EQ(err, Z_OK);

    deflate_after_prime(&c_stream, primed, sizeof(primed));
}
#endif
