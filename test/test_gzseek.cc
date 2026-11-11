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
