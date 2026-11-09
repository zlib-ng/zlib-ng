/* test_gzfile_api_errors.cc - Verify that the gzFile API functions reject invalid parameters and report errors correctly */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <fcntl.h>
#ifndef _WIN32
#  include <unistd.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#  include <io.h>
#endif

#include <gtest/gtest.h>

/* ========================================================================== */
/* Helpers                                                                    */
/* ========================================================================== */

/* Write raw bytes to a file, bypassing the library under test. */
static bool write_raw_file(const char *path, const uint8_t *data, size_t len) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return false;
    size_t written = fwrite(data, 1, len, fp);
    int closed = fclose(fp);
    return written == len && closed == 0;
}

/* Write data to a gzip file using the library itself. */
static bool write_gzip_file(const char *path, const uint8_t *data, size_t len) {
    gzFile file = PREFIX(gzopen)(path, "wb");
    if (file == NULL)
        return false;
    int written = PREFIX(gzwrite)(file, data, (z_uint32_t)len);
    int ret = PREFIX(gzclose)(file);
    if (written != (int)len || ret != Z_OK) {
        remove(path);
        return false;
    }
    return true;
}

/* ========================================================================== */
/* gzopen / gzdopen                                                           */
/* ========================================================================== */

/* gzopen() with a NULL path:
 * - the call is rejected without touching the file system. */
TEST(gzfile_api_errors, open_null_path) {
    EXPECT_EQ(PREFIX(gzopen)(NULL, "rb"), (gzFile)NULL);
}

/* gzopen() with a NULL mode string:
 * - the call is rejected before any file operation. */
TEST(gzfile_api_errors, open_null_mode) {
    EXPECT_EQ(PREFIX(gzopen)("gzapi_err_none.gz", NULL), (gzFile)NULL);
}

/* gzopen() with a mode that contains no "r", "w", or "a":
 * - "", "b" (binary only), and "f9" (strategy/level only) are all rejected. */
TEST(gzfile_api_errors, open_mode_without_direction) {
    EXPECT_EQ(PREFIX(gzopen)("gzapi_err_none.gz", ""), (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzopen)("gzapi_err_none.gz", "b"), (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzopen)("gzapi_err_none.gz", "f9"), (gzFile)NULL);
}

/* gzopen() with a mode requesting simultaneous read and write:
 * - "r+" and "w+" are rejected, as the state machine cannot do both;
 * - the file exists and is readable, so NULL can only come from the mode
 *   rejection itself rather than from open(2) failing. */
TEST(gzfile_api_errors, open_plus_mode) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_raw_file("gzapi_err_plus.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_plus.gz", "r+");
    EXPECT_EQ(file, (gzFile)NULL);
    if (file != NULL)
        PREFIX(gzclose)(file);
    file = PREFIX(gzopen)("gzapi_err_plus.gz", "w+");
    EXPECT_EQ(file, (gzFile)NULL);
    if (file != NULL)
        PREFIX(gzclose)(file);
    remove("gzapi_err_plus.gz");
}

/* gzopen() for reading a file that does not exist:
 * - open(2) fails and the call returns NULL. */
TEST(gzfile_api_errors, open_missing_file) {
    EXPECT_EQ(PREFIX(gzopen)("gzapi_err/no/such/directory/file.gz", "rb"), (gzFile)NULL);
}

/* gzopen() with a forced transparent read ("rT"):
 * - forcing direct mode on a read is rejected;
 * - the file exists and is readable, so NULL can only come from the mode
 *   rejection itself rather than from open(2) failing. */
TEST(gzfile_api_errors, open_forced_transparent_read) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_raw_file("gzapi_err_rt.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_rt.gz", "rT");
    EXPECT_EQ(file, (gzFile)NULL);
    if (file != NULL)
        PREFIX(gzclose)(file);
    remove("gzapi_err_rt.gz");
}

/* gzopen() with a forced raw write ("wG"):
 * - forcing gzip-off on a write is rejected. */
TEST(gzfile_api_errors, open_forced_direct_write) {
    EXPECT_EQ(PREFIX(gzopen)("gzapi_err_none.gz", "wG"), (gzFile)NULL);
}

/* gzdopen() with an invalid file descriptor:
 * - fd -1 is rejected without consulting the mode. */
TEST(gzfile_api_errors, dopen_negative_fd) {
    EXPECT_EQ(PREFIX(gzdopen)(-1, "rb"), (gzFile)NULL);
}

/* gzdopen() with a read-write mode on a valid descriptor:
 * - "+" is rejected even though the underlying fd is open. */
TEST(gzfile_api_errors, dopen_plus_mode) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_raw_file("gzapi_err_dopen.gz", data, sizeof(data)));

    int fd = open("gzapi_err_dopen.gz", O_RDWR);
    ASSERT_GE(fd, 0);
    gzFile gz = PREFIX(gzdopen)(fd, "r+b");
    EXPECT_EQ(gz, (gzFile)NULL);
    /* gzdopen() takes ownership of the fd on success; only close it via
       gzclose in that case, otherwise release it directly. */
    if (gz != NULL)
        PREFIX(gzclose)(gz);
    else
        close(fd);
    remove("gzapi_err_dopen.gz");
}

/* ========================================================================== */
/* gzclose / gzclose_r / gzclose_w                                            */
/* ========================================================================== */

/* gzclose() with a NULL descriptor:
 * - the call is rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, close_null) {
    EXPECT_EQ(PREFIX(gzclose)(NULL), Z_STREAM_ERROR);
}

/* gzclose_r() and gzclose_w() with a NULL descriptor:
 * - both are rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, close_rw_null) {
    EXPECT_EQ(PREFIX(gzclose_r)(NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose_w)(NULL), Z_STREAM_ERROR);
}

/* gzclose_r() on a write-mode descriptor:
 * - the call is rejected with Z_STREAM_ERROR;
 * - the state survives, so the regular gzclose() still finalizes and closes it. */
TEST(gzfile_api_errors, close_r_on_write_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_close_rw.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_close_rw.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzclose_r)(file), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_close_rw.gz");
}

/* gzclose_w() on a read-mode descriptor:
 * - the call is rejected with Z_STREAM_ERROR;
 * - the state survives, so the regular gzclose() still closes it. */
TEST(gzfile_api_errors, close_w_on_read_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_close_rw.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_close_rw.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzclose_w)(file), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_close_rw.gz");
}
