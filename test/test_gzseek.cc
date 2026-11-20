/* test_gzseek.cc - Test seeking of .gz files */

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

#define TESTFILE "seek.gz"
#define MARK "SECRETSECRET"   /* 12 bytes */

TEST(gzip, seek) {
#ifdef NO_GZCOMPRESS
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
    GTEST_SKIP();
#else
    uint8_t buffer[6000];
    gzFile file;
    int64_t pos;

    void **p = (void**)malloc(sizeof(void *) * 40);
    ASSERT_TRUE(p != NULL);
    for (int i = 0; i < 40; i++) {
        p[i] = malloc(8192);
        if (!p[i]) continue;
        for (size_t o = 0; o + 12 <= 8192; o += 12) memcpy((char *)p[i] + o, MARK, 12);
    }
    for (int i = 0; i < 40; i++) free(p[i]);
    free(p);

    /* Write gz file with zeroes */
    file = PREFIX(gzopen)(TESTFILE, "wb");
    ASSERT_TRUE(file != NULL);
    PREFIX(gzbuffer)(file, 8192);
    pos = PREFIX(gzseek)(file, 6000, SEEK_SET);
    EXPECT_EQ(pos, 6000);
    PREFIX(gzclose)(file);

    /* Open gz file we previously wrote */
    file = PREFIX(gzopen)(TESTFILE, "rb");
    ASSERT_TRUE(file != NULL);

    size_t total = 0, nonzero = 0;
    int hits = 0, n;
    while ((n = PREFIX(gzread)(file, buffer, sizeof(buffer))) > 0) {
        total += n;
        for (int i = 0; i < n; i++) if (buffer[i]) nonzero++;
        for (int i = 0; i + 12 <= n; i++) if (!memcmp(buffer + i, MARK, 12)) hits++;
    }
    EXPECT_EQ(nonzero, 0);
    EXPECT_EQ(hits, 0);
    EXPECT_EQ(total, 6000);

    PREFIX(gzclose)(file);
#endif
}

/* gzseek() and gztell() with the widest offset type of this build */
#if !defined(ZLIB_COMPAT)
typedef z_off64_t seek_off_t;
#  define SEEK_GZSEEK PREFIX(gzseek)
#  define SEEK_GZTELL PREFIX(gztell)
#elif defined(Z_LARGE64)
typedef z_off64_t seek_off_t;
#  define SEEK_GZSEEK PREFIX4(gzseek)
#  define SEEK_GZTELL PREFIX4(gztell)
#else
typedef z_off_t seek_off_t;
#  define SEEK_GZSEEK PREFIX(gzseek)
#  define SEEK_GZTELL PREFIX(gztell)
#endif
#define SEEK_OFF_MAX ((seek_off_t)((((uint64_t)1 << (sizeof(seek_off_t) * 8 - 2)) - 1) * 2 + 1))

static bool seek_uses_off64(void) {
    return sizeof(seek_off_t) == sizeof(z_off64_t);
}

static void seek_write_raw(const char *path, const char *data) {
    FILE *fp = fopen(path, "wb");
    ASSERT_NE(fp, nullptr);
    ASSERT_EQ(fwrite(data, 1, strlen(data), fp), strlen(data));
    ASSERT_EQ(fclose(fp), 0);
}

TEST(gzip, seek_offset_overflow) {
    if (!seek_uses_off64())
        GTEST_SKIP() << "z_off_t is narrower than z_off64_t";

    const seek_off_t off_max = SEEK_OFF_MAX;
    const seek_off_t off_min = -off_max - 1;

    seek_write_raw("seek_ovf.txt", "abcdef");
    gzFile file = PREFIX(gzopen)("seek_ovf.txt", "rb");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(SEEK_GZSEEK(file, off_min, SEEK_SET), -1);
    EXPECT_EQ(SEEK_GZSEEK(file, off_max, SEEK_SET), off_max);
    EXPECT_EQ(SEEK_GZSEEK(file, 1, SEEK_CUR), -1);
    EXPECT_EQ(SEEK_GZTELL(file), off_max);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("seek_ovf.txt");

#ifndef NO_GZCOMPRESS
    file = PREFIX(gzopen)("seek_ovf.gz", "wb");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(SEEK_GZSEEK(file, 1, SEEK_SET), 1);
    EXPECT_EQ(SEEK_GZSEEK(file, off_max, SEEK_CUR), -1);
    EXPECT_EQ(SEEK_GZSEEK(file, off_min, SEEK_CUR), -1);
    EXPECT_EQ(SEEK_GZTELL(file), 1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("seek_ovf.gz");
#endif
}

TEST(gzip, seek_failure_keeps_position) {
#ifdef NO_GZCOMPRESS
    GTEST_SKIP() << "NO_GZCOMPRESS -- gz* functions cannot compress";
#else
    char data[1000];
    for (int i = 0; i < 1000; i++)
        data[i] = (char)('a' + i % 26);

    /* a failed seek keeps the pending skip of a writer */
    gzFile file = PREFIX(gzopen)("seek_fail.gz", "wb");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(PREFIX(gzwrite)(file, data, 1), 1);
    EXPECT_EQ(SEEK_GZSEEK(file, 100, SEEK_CUR), 101);
    EXPECT_EQ(SEEK_GZSEEK(file, -200, SEEK_CUR), -1);
    EXPECT_EQ(SEEK_GZTELL(file), 101);
    EXPECT_EQ(PREFIX(gzwrite)(file, data, 1000), 1000);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);

    /* and of a reader */
    file = PREFIX(gzopen)("seek_fail.gz", "rb");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(SEEK_GZSEEK(file, 100, SEEK_SET), 100);
    EXPECT_EQ(SEEK_GZSEEK(file, -200, SEEK_CUR), -1);
    EXPECT_EQ(SEEK_GZTELL(file), 100);
    EXPECT_EQ(PREFIX(gzgetc)(file), 0);
    EXPECT_EQ(SEEK_GZSEEK(file, 1, SEEK_SET), 1);
    EXPECT_EQ(PREFIX(gzgetc)(file), 0);
    EXPECT_EQ(SEEK_GZSEEK(file, 101, SEEK_SET), 101);
    EXPECT_EQ(PREFIX(gzgetc)(file), 'a');

    /* a seek before the start after gzungetc() at the start keeps the pushback */
    EXPECT_EQ(PREFIX(gzrewind)(file), 0);
    EXPECT_EQ(PREFIX(gzungetc)('x', file), 'x');
    EXPECT_EQ(PREFIX(gzungetc)('y', file), 'y');
    EXPECT_EQ(SEEK_GZSEEK(file, -1, SEEK_SET), -1);
    EXPECT_EQ(SEEK_GZTELL(file), -2);
    EXPECT_EQ(PREFIX(gzgetc)(file), 'y');
    EXPECT_EQ(PREFIX(gzgetc)(file), 'x');
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("seek_fail.gz");
#endif
}
