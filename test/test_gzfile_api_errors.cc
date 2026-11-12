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

/* Write raw bytes to a file, bypassing the library under test.
 * The file is created with owner-only permissions. */
static bool write_raw_file(const char *path, const uint8_t *data, size_t len) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return false;
    FILE *fp = fdopen(fd, "w");
    if (fp == NULL) {
        close(fd);
        return false;
    }
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

/* ========================================================================== */
/* gztell / gzoffset / gzeof / gzerror / gzclearerr                           */
/* ========================================================================== */

/* gztell() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, tell_null) {
    EXPECT_EQ(PREFIX(gztell)(NULL), -1);
}

/* gzoffset() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, offset_null) {
    EXPECT_EQ(PREFIX(gzoffset)(NULL), -1);
}

/* gzeof() with a NULL descriptor:
 * - the call is rejected by returning 0 (no end-of-file state). */
TEST(gzfile_api_errors, eof_null) {
    EXPECT_EQ(PREFIX(gzeof)(NULL), 0);
}

/* gzerror() with a NULL descriptor:
 * - the call returns NULL and leaves the caller's error code untouched. */
TEST(gzfile_api_errors, error_null_file) {
    int errnum = Z_OK;
    EXPECT_EQ(PREFIX(gzerror)(NULL, &errnum), (const char *)NULL);
    EXPECT_EQ(errnum, Z_OK);
}

/* gzerror() on a healthy descriptor with a NULL errnum:
 * - the call returns the (empty) message and does not write through NULL. */
TEST(gzfile_api_errors, error_null_errnum) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_error.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_error.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_STREQ(PREFIX(gzerror)(file, NULL), "");
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_error.gz");
}

/* gzclearerr() with a NULL descriptor:
 * - the call is a no-op and does not crash. */
TEST(gzfile_api_errors, clearerr_null) {
    EXPECT_NO_THROW(PREFIX(gzclearerr)(NULL));
}

/* ========================================================================== */
/* gzrewind / gzseek                                                          */
/* ========================================================================== */

/* gzrewind() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, rewind_null) {
    EXPECT_EQ(PREFIX(gzrewind)(NULL), -1);
}

/* gzrewind() on a write-mode descriptor:
 * - rewinding only makes sense while reading, so the call is rejected. */
TEST(gzfile_api_errors, rewind_write_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_rewind.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_rewind.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzrewind)(file), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_rewind.gz");
}

/* gzseek() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, seek_null) {
    EXPECT_EQ(PREFIX(gzseek)(NULL, 0, SEEK_SET), -1);
}

/* gzseek() with an unsupported whence:
 * - SEEK_END is not supported by the gzip format and is rejected. */
TEST(gzfile_api_errors, seek_invalid_whence) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_seek.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_seek.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzseek)(file, 0, SEEK_END), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_seek.gz");
}

/* gzseek() backwards on a write-mode descriptor:
 * - gzip output cannot be rewritten in place, so back seeks are rejected. */
TEST(gzfile_api_errors, seek_write_backward) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_seek.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_seek.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzseek)(file, -1, SEEK_CUR), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_seek.gz");
}

/* gzseek() to a position before the start of a read stream:
 * - after consuming all 5 bytes, seeking to -6 relative lands before byte 0
 *   and is rejected. */
TEST(gzfile_api_errors, seek_read_before_start) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_seek.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_seek.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    ASSERT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 5);
    EXPECT_EQ(PREFIX(gzseek)(file, -6, SEEK_CUR), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_seek.gz");
}

/* ========================================================================== */
/* gzbuffer                                                                   */
/* ========================================================================== */

/* gzbuffer() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, buffer_null) {
    EXPECT_EQ(PREFIX(gzbuffer)(NULL, 8192), -1);
}

/* gzbuffer() with a size below the minimum:
 * - sizes under 8 are clamped up to 8 and the call succeeds. */
TEST(gzfile_api_errors, buffer_zero_size) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_buffer.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_buffer.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzbuffer)(file, 0), 0);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 5);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_buffer.gz");
}

/* gzbuffer() with a size above GZBUFSIZE_MAX:
 * - in native mode the request is rejected with -1 and Z_MEM_ERROR;
 * - in compat mode the size is silently clamped to GZBUFSIZE_MAX
 *   (1 GiB, per gzguts.h), so the call succeeds. */
TEST(gzfile_api_errors, buffer_too_large) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_buffer.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_buffer.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
#ifdef ZLIB_COMPAT
    EXPECT_EQ(PREFIX(gzbuffer)(file, (1u << 30) + 1u), 0);
#else
    EXPECT_EQ(PREFIX(gzbuffer)(file, (1u << 30) + 1u), -1);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_MEM_ERROR);
#endif
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_buffer.gz");
}

/* gzbuffer() after the buffers have already been allocated:
 * - the size is fixed once any I/O has happened, so the call is rejected. */
TEST(gzfile_api_errors, buffer_after_io) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_buffer.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_buffer.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    ASSERT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 5);
    EXPECT_EQ(PREFIX(gzbuffer)(file, 8192), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_buffer.gz");
}

/* ========================================================================== */
/* gzread / gzfread                                                           */
/* ========================================================================== */

/* A minimal gzip member for "hello" whose CRC32 trailer is deliberately
   wrong (0xDEADBEEF), so the checksum verification in inflate fails. */
static bool write_corrupt_crc_file(const char *path) {
    const uint8_t bytes[] = {
        31, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, /* gzip header */
        0x01, 0x05, 0x00, 0xfa, 0xff,                              /* final stored block, len=5 */
        'h', 'e', 'l', 'l', 'o',                                   /* payload */
        0xde, 0xad, 0xbe, 0xef,                                    /* wrong CRC32 */
        0x05, 0x00, 0x00, 0x00                                     /* ISIZE = 5 */
    };
    return write_raw_file(path, bytes, sizeof(bytes));
}

/* A gzip member whose stored block claims 100 payload bytes but the file
   ends after 3 of them, so inflate runs out of input mid-block. */
static bool write_truncated_file(const char *path) {
    const uint8_t bytes[] = {
        31, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, /* gzip header */
        0x00,                                                     /* stored block, not final */
        0x64, 0x00,                                               /* LEN = 100 */
        0x9b, 0xff,                                               /* NLEN = ~100 */
        'a', 'b', 'c'                                             /* only 3 of 100 bytes */
    };
    return write_raw_file(path, bytes, sizeof(bytes));
}

/* gzread() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, read_null) {
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(NULL, buf, (unsigned)sizeof(buf)), -1);
}

/* gzread() on a write-mode descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, read_write_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_read.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_read.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_read.gz");
}

/* gzread() with a length that does not fit in an int:
 * - the call is rejected with -1 and Z_STREAM_ERROR. */
TEST(gzfile_api_errors, read_len_overflow) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_read.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_read.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(file, buf, 0xFFFFFFFFu), -1);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_read.gz");
}

/* gzread() with a NULL buffer and nonzero length:
 * - the call is rejected with -1 instead of writing through the NULL pointer;
 * - the descriptor remains usable for subsequent reads. */
TEST(gzfile_api_errors, read_null_buf) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_read.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_read.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzread)(file, NULL, 10), -1);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 5);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_read.gz");
}

/* gzfread() with a zero item size:
 * - the call returns 0 without touching the descriptor. */
TEST(gzfile_api_errors, fread_zero_size) {
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzfread)(buf, 0, 1, NULL), 0u);
}

/* gzfread() with a NULL descriptor:
 * - the call returns 0. */
TEST(gzfile_api_errors, fread_null_file) {
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzfread)(buf, 1, 16, NULL), 0u);
}

/* gzfread() with size * nitems overflowing size_t:
 * - the call is rejected with 0 and Z_STREAM_ERROR. */
TEST(gzfile_api_errors, fread_overflow) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_read.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_read.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzfread)(buf, SIZE_MAX / 2 + 1, 3, file), 0u);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_read.gz");
}

/* gzread() on a file with a corrupted CRC32 trailer:
 * - the read fails with -1;
 * - the error code is Z_DATA_ERROR;
 * - the error message names the file and reports the checksum mismatch,
 *   proving that validation reached the CRC trailer (a wrong NLEN would
 *   fail earlier with "invalid stored block lengths"). */
TEST(gzfile_api_errors, read_corrupt_crc) {
    ASSERT_TRUE(write_corrupt_crc_file("gzapi_err_crc.gz"));

    gzFile file = PREFIX(gzopen)("gzapi_err_crc.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), -1);
    int errnum = Z_OK;
    const char *msg = PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_DATA_ERROR);
    ASSERT_NE(msg, (const char *)NULL);
    EXPECT_NE(strstr(msg, "gzapi_err_crc.gz"), (char *)NULL);
    EXPECT_NE(strstr(msg, "incorrect data check"), (char *)NULL);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_crc.gz");
}

/* gzread() on a stream truncated mid-block:
 * - the first read returns the 3 payload bytes that precede the truncation;
 * - the error code is Z_BUF_ERROR with an "unexpected end of file" message;
 * - further reads return 0 (end-of-file) while the error is retained;
 * - gzclearerr() resets the state to Z_OK and an empty message. */
TEST(gzfile_api_errors, read_truncated_stream) {
    ASSERT_TRUE(write_truncated_file("gzapi_err_trunc.gz"));

    gzFile file = PREFIX(gzopen)("gzapi_err_trunc.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 3);
    EXPECT_EQ(buf[0], 'a');
    EXPECT_EQ(buf[1], 'b');
    EXPECT_EQ(buf[2], 'c');
    int errnum = Z_OK;
    const char *msg = PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_BUF_ERROR);
    ASSERT_NE(msg, (const char *)NULL);
    EXPECT_NE(strstr(msg, "unexpected end of file"), (char *)NULL);
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 0);

    PREFIX(gzclearerr)(file);
    msg = PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_OK);
    EXPECT_STREQ(msg, "");
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_trunc.gz");
}

/* gzread() after a fatal error has been set:
 * - further reads are rejected with -1 without consuming input. */
TEST(gzfile_api_errors, read_after_fatal_error) {
    ASSERT_TRUE(write_corrupt_crc_file("gzapi_err_crc.gz"));

    gzFile file = PREFIX(gzopen)("gzapi_err_crc.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), -1);
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), -1);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_DATA_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_crc.gz");
}

/* gzread() on an empty file:
 * - the read returns 0 with no error;
 * - gzeof() reports end-of-file. */
TEST(gzfile_api_errors, read_empty_file) {
    uint8_t none = 0;
    ASSERT_TRUE(write_raw_file("gzapi_err_empty.gz", &none, 0));

    gzFile file = PREFIX(gzopen)("gzapi_err_empty.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    EXPECT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 0);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_OK);
    EXPECT_EQ(PREFIX(gzeof)(file), 1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_empty.gz");
}

/* ========================================================================== */
/* gzgetc / gzungetc                                                          */
/* ========================================================================== */

#ifdef ZLIB_COMPAT
/* gzgetc_() (the compat-mode entry point to the checked gzgetc()
 * implementation) with a NULL descriptor:
 * - the call is rejected with -1.
 * The plain gzgetc() entry point is a speed macro that does not check for a
 * NULL descriptor (by design, matching upstream zlib), so the NULL case can
 * only be exercised through gzgetc_(). */
TEST(gzfile_api_errors, getc_null) {
    EXPECT_EQ(PREFIX(gzgetc_)(NULL), -1);
}
#endif /* ZLIB_COMPAT */

/* gzgetc() on a write-mode descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, getc_write_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_getc.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_getc.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzgetc)(file), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_getc.gz");
}

/* gzgetc() at end-of-file:
 * - the call returns -1;
 * - no error is recorded and gzeof() reports end-of-file. */
TEST(gzfile_api_errors, getc_eof) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_getc.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_getc.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    ASSERT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 5);
    EXPECT_EQ(PREFIX(gzgetc)(file), -1);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_OK);
    EXPECT_EQ(PREFIX(gzeof)(file), 1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_getc.gz");
}

/* gzungetc() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, ungetc_null_file) {
    EXPECT_EQ(PREFIX(gzungetc)(0, NULL), -1);
}

/* gzungetc() with a negative character:
 * - EOF cannot be pushed back and the call is rejected with -1. */
TEST(gzfile_api_errors, ungetc_negative_c) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_ungetc.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_ungetc.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzungetc)(-1, file), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_ungetc.gz");
}

/* gzungetc() on a write-mode descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, ungetc_write_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_ungetc.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_ungetc.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzungetc)(0, file), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_ungetc.gz");
}

/* gzungetc() past the push-back buffer capacity:
 * - with an 8-byte buffer (via gzbuffer) the output buffer holds 16 pushed
 *   characters, so the first 16 pushes succeed and the 17th is rejected;
 * - the rejection records Z_DATA_ERROR. */
TEST(gzfile_api_errors, ungetc_overflow) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_ungetc.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_ungetc.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzbuffer)(file, 8), 0);

    for (int i = 0; i < 16; i++)
        EXPECT_EQ(PREFIX(gzungetc)(0x41 + i, file), 0x41 + i);
    EXPECT_EQ(PREFIX(gzungetc)(0xff, file), -1);
    int errnum = Z_OK;
    const char *msg = PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_DATA_ERROR);
    ASSERT_NE(msg, (const char *)NULL);
    EXPECT_NE(strstr(msg, "out of room to push characters"), (char *)NULL);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_ungetc.gz");
}

/* ========================================================================== */
/* gzgets / gzdirect                                                          */
/* ========================================================================== */

/* gzgets() with a NULL descriptor:
 * - the call returns NULL. */
TEST(gzfile_api_errors, gets_null_file) {
    char buf[16];
    EXPECT_EQ(PREFIX(gzgets)(NULL, buf, (int)sizeof(buf)), (char *)NULL);
}

/* gzgets() with a NULL destination buffer:
 * - the call returns NULL. */
TEST(gzfile_api_errors, gets_null_buf) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_gets.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_gets.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzgets)(file, NULL, 16), (char *)NULL);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_gets.gz");
}

/* gzgets() with a length below 1:
 * - the call returns NULL. */
TEST(gzfile_api_errors, gets_zero_len) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_gets.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_gets.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    char buf[16];
    EXPECT_EQ(PREFIX(gzgets)(file, buf, 0), (char *)NULL);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_gets.gz");
}

/* gzgets() on a write-mode descriptor:
 * - the call returns NULL. */
TEST(gzfile_api_errors, gets_write_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_gets.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_gets.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    char buf[16];
    EXPECT_EQ(PREFIX(gzgets)(file, buf, (int)sizeof(buf)), (char *)NULL);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_gets.gz");
}

/* gzgets() at end-of-file with nothing buffered:
 * - the call returns NULL and records no error. */
TEST(gzfile_api_errors, gets_eof) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_gets.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_gets.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    uint8_t buf[16];
    ASSERT_EQ(PREFIX(gzread)(file, buf, (unsigned)sizeof(buf)), 5);
    char line[16];
    EXPECT_EQ(PREFIX(gzgets)(file, line, (int)sizeof(line)), (char *)NULL);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_OK);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_gets.gz");
}

/* gzdirect() with a NULL descriptor:
 * - the call returns 0. */
TEST(gzfile_api_errors, direct_null) {
    EXPECT_EQ(PREFIX(gzdirect)(NULL), 0);
}

/* gzdirect() on transparent and gzip inputs:
 * - a plain (non-gzip) file is read transparently and reports 1;
 * - a gzip file is decompressed and reports 0. */
TEST(gzfile_api_errors, direct_modes) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_raw_file("gzapi_err_direct_plain.gz", data, sizeof(data)));
    ASSERT_TRUE(write_gzip_file("gzapi_err_direct_gz.gz", data, sizeof(data)));

    gzFile plain = PREFIX(gzopen)("gzapi_err_direct_plain.gz", "rb");
    ASSERT_NE(plain, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzdirect)(plain), 1);
    EXPECT_EQ(PREFIX(gzclose)(plain), Z_OK);

    gzFile gzip = PREFIX(gzopen)("gzapi_err_direct_gz.gz", "rb");
    ASSERT_NE(gzip, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzdirect)(gzip), 0);
    EXPECT_EQ(PREFIX(gzclose)(gzip), Z_OK);
    remove("gzapi_err_direct_plain.gz");
    remove("gzapi_err_direct_gz.gz");
}

/* ========================================================================== */
/* gzwrite / gzfwrite                                                         */
/* ========================================================================== */

/* gzwrite() with a NULL descriptor:
 * - the call returns 0. */
TEST(gzfile_api_errors, write_null) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    EXPECT_EQ(PREFIX(gzwrite)(NULL, data, (unsigned)sizeof(data)), 0);
}

/* gzwrite() on a read-mode descriptor:
 * - the call returns 0. */
TEST(gzfile_api_errors, write_read_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_write.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_write.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzwrite)(file, data, (unsigned)sizeof(data)), 0);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_write.gz");
}

/* gzwrite() with a length that does not fit in an int:
 * - the call returns 0 and records Z_DATA_ERROR. */
TEST(gzfile_api_errors, write_len_overflow) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_write.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_write.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzwrite)(file, data, 0xFFFFFFFFu), 0);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_DATA_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_write.gz");
}

/* gzwrite() with a NULL buffer and nonzero length:
 * - the call is rejected with 0 instead of reading through the NULL pointer;
 * - a NULL buffer with zero length is accepted (nothing is read from it);
 * - the descriptor remains usable for subsequent writes. */
TEST(gzfile_api_errors, write_null_buf) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_write.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_write.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzwrite)(file, NULL, 10), 0);
    EXPECT_EQ(PREFIX(gzwrite)(file, NULL, 0), 0);
    EXPECT_EQ(PREFIX(gzwrite)(file, data, sizeof(data)), 5);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_write.gz");
}

/* gzfwrite() with a zero item size:
 * - the call returns 0 without touching the descriptor;
 * - on a valid write stream the guard still applies: the call returns 0
 *   and the stream position is unchanged (without the guard this would
 *   divide by zero). */
TEST(gzfile_api_errors, fwrite_zero_size) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    EXPECT_EQ(PREFIX(gzfwrite)(data, 0, 1, NULL), 0u);

    gzFile file = PREFIX(gzopen)("gzapi_err_fwrite_zero.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gztell)(file), 0);
    EXPECT_EQ(PREFIX(gzfwrite)(data, 0, 1, file), 0u);
    EXPECT_EQ(PREFIX(gztell)(file), 0);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_fwrite_zero.gz");
}

/* gzfwrite() with a NULL descriptor:
 * - the call returns 0. */
TEST(gzfile_api_errors, fwrite_null_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    EXPECT_EQ(PREFIX(gzfwrite)(data, 1, 5, NULL), 0u);
}

/* gzfwrite() with size * nitems overflowing size_t:
 * - the call is rejected with 0 and Z_STREAM_ERROR. */
TEST(gzfile_api_errors, fwrite_overflow) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_write.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_write.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzfwrite)(data, SIZE_MAX / 2 + 1, 3, file), 0u);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_write.gz");
}

/* ========================================================================== */
/* gzputc / gzputs                                                            */
/* ========================================================================== */

/* gzputc() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, putc_null) {
    EXPECT_EQ(PREFIX(gzputc)(NULL, 'a'), -1);
}

/* gzputc() on a read-mode descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, putc_read_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_putc.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_putc.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzputc)(file, 'a'), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_putc.gz");
}

/* gzputs() with a NULL descriptor:
 * - the call is rejected with -1. */
TEST(gzfile_api_errors, puts_null_file) {
    EXPECT_EQ(PREFIX(gzputs)(NULL, "hello"), -1);
}

/* gzputs() on a read-mode descriptor:
 * - the call is rejected with -1 before the string is examined. */
TEST(gzfile_api_errors, puts_read_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_puts.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_puts.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzputs)(file, "hello"), -1);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_puts.gz");
}

/* gzputs() with a NULL string:
 * - the call is rejected with -1 instead of passing NULL to strlen();
 * - the descriptor remains usable for subsequent writes. */
TEST(gzfile_api_errors, puts_null_string) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_puts.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_puts.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzputs)(file, NULL), -1);
    EXPECT_EQ(PREFIX(gzputs)(file, "hello"), 5);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_puts.gz");
}

/* ========================================================================== */
/* gzprintf                                                                   */
/* ========================================================================== */

/* gzprintf() with a NULL descriptor:
 * - the call is rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, printf_null_file) {
    EXPECT_EQ(PREFIX(gzprintf)(NULL, "hello %d", 42), Z_STREAM_ERROR);
}

/* gzprintf() on a read-mode descriptor:
 * - the call is rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, printf_read_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_printf.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_printf.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzprintf)(file, "hello %d", 42), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_printf.gz");
}

/* gzprintf() with a NULL format string:
 * - the call is rejected with Z_STREAM_ERROR instead of passing the NULL
 *   format to vsnprintf();
 * - the descriptor remains usable for subsequent writes. */
TEST(gzfile_api_errors, printf_null_format) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_printf.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_printf.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzprintf)(file, NULL), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzputs)(file, "hello"), 5);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_printf.gz");
}

/* gzprintf() with output that does not fit in the buffer:
 * - with an 8-byte buffer (via gzbuffer) a 20-character result is refused;
 * - the call returns 0 and records no error, leaving the stream usable. */
TEST(gzfile_api_errors, printf_output_too_large) {
    ASSERT_TRUE(write_gzip_file("gzapi_err_printf.gz", (const uint8_t *)"x", 1));

    gzFile file = PREFIX(gzopen)("gzapi_err_printf.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzbuffer)(file, 8), 0);
    EXPECT_EQ(PREFIX(gzprintf)(file, "%.*s", 20, "aaaaaaaaaaaaaaaaaaaa"), 0);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_OK);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_printf.gz");
}

/* ========================================================================== */
/* gzflush / gzsetparams                                                      */
/* ========================================================================== */

/* gzflush() with a NULL descriptor:
 * - the call is rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, flush_null) {
    EXPECT_EQ(PREFIX(gzflush)(NULL, Z_FINISH), Z_STREAM_ERROR);
}

/* gzflush() on a read-mode descriptor:
 * - the call is rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, flush_read_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_flush.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_flush.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzflush)(file, Z_FINISH), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_flush.gz");
}

/* gzflush() with an out-of-range flush value:
 * - values below 0 and above Z_FINISH are rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, flush_invalid_value) {
    ASSERT_TRUE(write_gzip_file("gzapi_err_flush.gz", (const uint8_t *)"x", 1));

    gzFile file = PREFIX(gzopen)("gzapi_err_flush.gz", "wb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzflush)(file, -1), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzflush)(file, Z_FINISH + 1), Z_STREAM_ERROR);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_OK);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_flush.gz");
}

/* gzsetparams() with a NULL descriptor:
 * - the call is rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, setparams_null) {
    EXPECT_EQ(PREFIX(gzsetparams)(NULL, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
}

/* gzsetparams() on a read-mode descriptor:
 * - the call is rejected with Z_STREAM_ERROR. */
TEST(gzfile_api_errors, setparams_read_file) {
    const uint8_t data[5] = {'h', 'e', 'l', 'l', 'o'};
    ASSERT_TRUE(write_gzip_file("gzapi_err_setparams.gz", data, sizeof(data)));

    gzFile file = PREFIX(gzopen)("gzapi_err_setparams.gz", "rb");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzsetparams)(file, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_setparams.gz");
}

/* gzsetparams() on a transparent ("wT") write descriptor:
 * - there is no deflate state to retune, so the call is rejected. */
TEST(gzfile_api_errors, setparams_direct_write) {
    ASSERT_TRUE(write_gzip_file("gzapi_err_setparams.gz", (const uint8_t *)"x", 1));

    gzFile file = PREFIX(gzopen)("gzapi_err_setparams.gz", "wT");
    ASSERT_NE(file, (gzFile)NULL);
    EXPECT_EQ(PREFIX(gzsetparams)(file, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY), Z_STREAM_ERROR);
    int errnum = Z_OK;
    PREFIX(gzerror)(file, &errnum);
    EXPECT_EQ(errnum, Z_OK);
    EXPECT_EQ(PREFIX(gzclose)(file), Z_OK);
    remove("gzapi_err_setparams.gz");
}
