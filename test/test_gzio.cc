/* test_gzio.cc - Test read/write of .gz files */

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
#ifdef __linux__
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "test_shared.h"

#define TESTFILE "foo.gz"

TEST(gzip, readwrite) {
#ifdef NO_GZCOMPRESS
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
    GTEST_SKIP();
#else
    uint8_t uncompr[128];
    uint32_t compr_len, uncompr_len = sizeof(uncompr);
    int64_t pos;
    gzFile file;
    int err;

    /* Write gz file with test data */
    file = PREFIX(gzopen)(TESTFILE, "wb");
    ASSERT_TRUE(file != NULL);
    /* Write hello, hello! using gzputs and gzprintf */
    PREFIX(gzputc)(file, 'h');
    EXPECT_EQ(PREFIX(gzputs)(file, "ello"), 4);
    EXPECT_EQ(PREFIX(gzprintf)(file, ", %s!", "hello"), 8);
    /* Write string null-teriminator using gzseek */
    EXPECT_GE(PREFIX(gzseek)(file, 1L, SEEK_CUR), 0);
    /* Write hello, hello! using gzfwrite using best compression level */
    EXPECT_EQ(PREFIX(gzsetparams)(file, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY), Z_OK);
    EXPECT_NE(PREFIX(gzfwrite)(hello, hello_len, 1, file), 0UL);
    /* Flush compressed bytes to file */
    EXPECT_EQ(PREFIX(gzflush)(file, Z_SYNC_FLUSH), Z_OK);
    compr_len = (uint32_t)PREFIX(gzoffset)(file);
    EXPECT_GE(compr_len, 0UL);
    PREFIX(gzclose)(file);

    /* Open gz file we previously wrote */
    file = PREFIX(gzopen)(TESTFILE, "rb");
    ASSERT_TRUE(file != NULL);

    /* Read uncompressed data - hello, hello! string twice */
    strcpy((char*)uncompr, "garbages");
    EXPECT_EQ(PREFIX(gzread)(file, uncompr, (unsigned)uncompr_len), (int)(hello_len + hello_len));
    EXPECT_STREQ((char*)uncompr, hello);

    /* Check position at the end of the gz file */
    EXPECT_EQ(PREFIX(gzeof)(file), 1);

    /* Seek backwards mid-string and check char reading with gzgetc and gzungetc */
    pos = PREFIX(gzseek)(file, -22L, SEEK_CUR);
    EXPECT_EQ(pos, 6);
    EXPECT_EQ(PREFIX(gztell)(file), pos);
    EXPECT_EQ(PREFIX(gzgetc)(file), ' ');
    EXPECT_EQ(PREFIX(gzungetc)(' ', file), ' ');
    /* Read first hello, hello! string with gzgets */
    strcpy((char*)uncompr, "garbages");
    PREFIX(gzgets)(file, (char*)uncompr, (int)uncompr_len);
    EXPECT_EQ(strlen((char*)uncompr), 7UL); /* " hello!" */
    EXPECT_STREQ((char*)uncompr, hello + 6);

    /* Seek to second hello, hello! string */
    pos = PREFIX(gzseek)(file, 14L, SEEK_SET);
    EXPECT_EQ(pos, 14);
    EXPECT_EQ(PREFIX(gztell)(file), pos);

    /* Check position not at end of file */
    EXPECT_EQ(PREFIX(gzeof)(file), 0);
    /* Read first hello, hello! string with gzfread */
    strcpy((char*)uncompr, "garbages");
    PREFIX(gzfread)(uncompr, uncompr_len, 1, file);
    EXPECT_STREQ((const char *)uncompr, hello);

    pos = PREFIX(gzoffset)(file);
    EXPECT_GE(pos, 0);
    EXPECT_EQ(pos, (compr_len + 10));

    /* Trigger an error and clear it with gzclearerr */
    PREFIX(gzfread)(uncompr, (size_t)-1, (size_t)-1, file);
    PREFIX(gzerror)(file, &err);
    EXPECT_NE(err, 0);

    PREFIX(gzclearerr)(file);
    PREFIX(gzerror)(file, &err);
    EXPECT_EQ(err, 0);

    PREFIX(gzclose)(file);

    EXPECT_EQ(PREFIX(gzclose)(NULL), Z_STREAM_ERROR);
#endif
}

TEST(gzip, setparams_invalid_parameters) {
#ifdef NO_GZCOMPRESS
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
    GTEST_SKIP();
#else
    gzFile file = PREFIX(gzopen)(TESTFILE, "wb");
    ASSERT_TRUE(file != NULL);
    EXPECT_EQ(PREFIX(gzsetparams)(file, Z_BEST_COMPRESSION + 1, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);

    file = PREFIX(gzopen)(TESTFILE, "wb");
    ASSERT_TRUE(file != NULL);
    EXPECT_EQ(PREFIX(gzsetparams)(file, Z_DEFAULT_COMPRESSION, Z_FIXED + 1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);

    file = PREFIX(gzopen)(TESTFILE, "wb");
    ASSERT_TRUE(file != NULL);
    EXPECT_EQ(PREFIX(gzwrite)(file, "x", 1), 1);
    EXPECT_EQ(PREFIX(gzsetparams)(file, Z_BEST_COMPRESSION + 1, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzsetparams)(file, Z_DEFAULT_COMPRESSION, Z_FIXED + 1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
#endif
}

TEST(gzip, setparams_small_output_buffer) {
#ifdef NO_GZCOMPRESS
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
    GTEST_SKIP();
#else
    std::vector<uint8_t> input(100000);
    std::vector<uint8_t> output(input.size());
    uint32_t random = 1;

    for (uint8_t &byte : input) {
        random = random * 1664525U + 1013904223U;
        byte = (uint8_t)(random >> 24);
    }

    gzFile file = PREFIX(gzopen)(TESTFILE, "wb1");
    ASSERT_TRUE(file != NULL);
    ASSERT_EQ(PREFIX(gzbuffer)(file, 9), 0);
    ASSERT_EQ(PREFIX(gzwrite)(file, input.data(), (unsigned)input.size()), (int)input.size());
    EXPECT_EQ(PREFIX(gzsetparams)(file, 9, Z_DEFAULT_STRATEGY), Z_OK);
    ASSERT_EQ(PREFIX(gzclose)(file), Z_OK);

    file = PREFIX(gzopen)(TESTFILE, "rb");
    ASSERT_TRUE(file != NULL);
    EXPECT_EQ(PREFIX(gzread)(file, output.data(), (unsigned)output.size()), (int)output.size());
    EXPECT_EQ(output, input);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
#endif
}

#if defined(__linux__) && !defined(NO_GZCOMPRESS) && !defined(TEST_STOCK_ZLIB) && defined(F_SETPIPE_SZ)
/* gzprintf() rounds whose writes fail, with gzclearerr() between them: a
 * failed write leaves input in the buffer behind the place gzprintf() formats
 * at, and the gzip stream written in the end must still be valid. The
 * descriptor is a non-blocking pipe of 1 MiB whose room is set before each
 * call (by filling it with whole pages and reading them back), so that each
 * write() either completes or fails with EAGAIN without writing anything. */
struct printf_pipe {
    int rfd, wfd;
    size_t junk;                    /* fill bytes at the head of the pipe */
    std::vector<uint8_t> got;       /* the gzip stream read back */

    void drain(long limit) {        /* read up to limit bytes, all if < 0 */
        uint8_t b[4096];
        while (limit != 0) {
            size_t want = limit < 0 || limit > (long)sizeof(b) ? sizeof(b) : (size_t)limit;
            ssize_t r = read(rfd, b, want);
            if (r <= 0)
                return;
            for (ssize_t i = 0; i < r; i++) {
                if (junk > 0)
                    junk--;
                else
                    got.push_back(b[i]);
            }
            if (limit > 0)
                limit -= r;
        }
    }
    bool fill() {                   /* leave no room */
        uint8_t b[4096];
        memset(b, 'j', sizeof(b));
        for (;;) {
            ssize_t r = write(wfd, b, sizeof(b));
            if (r < 0)
                return true;
            junk += (size_t)r;
            if (r < (ssize_t)sizeof(b))
                return false;
        }
    }
};

static int printf_and_clear(gzFile file, std::vector<char> &text, size_t len) {
    memset(text.data(), 'a' + (int)(len % 26), len);
    text[len] = 0;
    int ret = PREFIX(gzprintf)(file, "%s", text.data());
    PREFIX(gzclearerr)(file);
    return ret;
}

TEST(gzip, printf_after_failed_writes) {
    printf_pipe p;
    int fds[2];
    ASSERT_EQ(pipe(fds), 0);
    p.rfd = fds[0];
    p.wfd = fds[1];
    p.junk = 0;
    if (fcntl(p.wfd, F_SETPIPE_SZ, 1 << 20) < (1 << 20)) {
        close(p.rfd);
        close(p.wfd);
        GTEST_SKIP() << "cannot set the pipe size to 1 MiB";
    }
    ASSERT_EQ(fcntl(p.wfd, F_SETFL, fcntl(p.wfd, F_GETFL) | O_NONBLOCK), 0);
    ASSERT_EQ(fcntl(p.rfd, F_SETFL, fcntl(p.rfd, F_GETFL) | O_NONBLOCK), 0);

    std::vector<char> text(262144);
    gzFile file = PREFIX(gzdopen)(p.wfd, "wb0");
    ASSERT_TRUE(file != NULL);
    ASSERT_EQ(PREFIX(gzbuffer)(file, 262144), 0);
    printf_and_clear(file, text, 75605);
    ASSERT_EQ(PREFIX(gzflush)(file, Z_SYNC_FLUSH), Z_OK);
    p.drain(-1);
    printf_and_clear(file, text, 188859);
    ASSERT_TRUE(p.fill());                  /* the next write fails */
    printf_and_clear(file, text, 102132);
    p.drain(46 * 4096);                     /* room for one write */
    printf_and_clear(file, text, 233532);
    printf_and_clear(file, text, 176283);
    printf_and_clear(file, text, 200000);
    p.drain(-1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    p.drain(-1);
    close(p.rfd);

    /* the stream inflates completely, with a valid trailer */
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    ASSERT_EQ(PREFIX(inflateInit2)(&strm, 31), Z_OK);
    std::vector<uint8_t> out(1 << 20);
    strm.next_in = p.got.data();
    strm.avail_in = (uint32_t)p.got.size();
    int ret;
    do {
        strm.next_out = out.data();
        strm.avail_out = (uint32_t)out.size();
        ret = PREFIX(inflate)(&strm, Z_NO_FLUSH);
    } while (ret == Z_OK && strm.avail_in > 0);
    EXPECT_EQ(ret, Z_STREAM_END);
    EXPECT_EQ(strm.avail_in, 0U);
    PREFIX(inflateEnd)(&strm);
}
#endif
