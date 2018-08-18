#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#define CHECK_ERR(err, msg) { \
    if (err != Z_OK) { \
        fprintf(stderr, "%s error: %d\n", msg, err); \
        exit(1); \
    } \
}

static const uint8_t *data;
static size_t dataLen;
static alloc_func zalloc = NULL;
static free_func zfree = NULL;
static size_t dictionaryLen = 1024;
static const char *dictionary[1024];
static unsigned long dictId; /* Adler32 value of the dictionary */

static void check_compress_level(uint8_t *compr, size_t comprLen,
                                 uint8_t *uncompr, size_t uncomprLen,
                                 int level) {
  PREFIX(compress2)(compr, &comprLen, data, dataLen, level);
  PREFIX(uncompress)(uncompr, &uncomprLen, compr, comprLen);

  /* Make sure compress + uncompress gives back the input data. */
  assert(dataLen == uncomprLen);
  assert(0 == memcmp(data, uncompr, dataLen));
}

#define put_byte(s, i, c) {s[i] = (unsigned char)(c);}
#define PRESET_DICT 0x20 /* preset dictionary flag in zlib header */

static void write_zlib_header(uint8_t *s, unsigned preset_dict) {
  unsigned level_flags = 0; /* compression level (0..3) */
  unsigned w_bits = 8; /* window size log2(w_size)  (8..16) */
  unsigned int header = (Z_DEFLATED + ((w_bits-8)<<4)) << 8;
  header |= (level_flags << 6);

  if (preset_dict)
    header |= PRESET_DICT;
  header += 31 - (header % 31);

  /* s is guaranteed to be longer than 2 bytes. */
  put_byte(s, 0, (unsigned char)(header >> 8));
  put_byte(s, 1, (unsigned char)(header & 0xff));
}

/* No preset dictionary. */
static void check_decompress_no_dict(uint8_t *compr, size_t comprLen) {
  write_zlib_header((uint8_t *)data, 0);
  PREFIX(uncompress)(compr, &comprLen, data, dataLen);
}

/* ===========================================================================
 * Test deflate() with small buffers
 */
void test_deflate(unsigned char *compr, size_t comprLen) {
  PREFIX3(stream) c_stream; /* compression stream */
  int err;
  unsigned long len = dataLen;

  c_stream.zalloc = zalloc;
  c_stream.zfree = zfree;
  c_stream.opaque = (void *)0;

  err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
  CHECK_ERR(err, "deflateInit");

  c_stream.next_in = (const unsigned char *)data;
  c_stream.next_out = compr;

  while (c_stream.total_in != len && c_stream.total_out < comprLen) {
    c_stream.avail_in = c_stream.avail_out = 1; /* force small buffers */
    err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
    CHECK_ERR(err, "deflate");
  }
  /* Finish the stream, still forcing small buffers: */
  for (;;) {
    c_stream.avail_out = 1;
    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err == Z_STREAM_END)
      break;
    CHECK_ERR(err, "deflate");
  }

  err = PREFIX(deflateEnd)(&c_stream);
  CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with small buffers
 */
void test_inflate(unsigned char *compr, size_t comprLen, unsigned char *uncompr,
                  size_t uncomprLen) {
  int err;
  PREFIX3(stream) d_stream; /* decompression stream */

  strcpy((char *)uncompr, "garbage");

  d_stream.zalloc = zalloc;
  d_stream.zfree = zfree;
  d_stream.opaque = (void *)0;

  d_stream.next_in = compr;
  d_stream.avail_in = 0;
  d_stream.next_out = uncompr;

  err = PREFIX(inflateInit)(&d_stream);
  CHECK_ERR(err, "inflateInit");

  while (d_stream.total_out < uncomprLen && d_stream.total_in < comprLen) {
    d_stream.avail_in = d_stream.avail_out = 1; /* force small buffers */
    err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
    if (err == Z_STREAM_END)
      break;
    CHECK_ERR(err, "inflate");
  }

  err = PREFIX(inflateEnd)(&d_stream);
  CHECK_ERR(err, "inflateEnd");

  if (memcmp(uncompr, data, dataLen)) {
    fprintf(stderr, "bad inflate\n");
    exit(1);
  }
}

static unsigned int diff;

/* ===========================================================================
 * Test deflate() with large buffers and dynamic change of compression level
 */
void test_large_deflate(unsigned char *compr, size_t comprLen,
                        unsigned char *uncompr, size_t uncomprLen) {
  PREFIX3(stream) c_stream; /* compression stream */
  int err;

  c_stream.zalloc = zalloc;
  c_stream.zfree = zfree;
  c_stream.opaque = (void *)0;

  err = PREFIX(deflateInit)(&c_stream, Z_BEST_SPEED);
  CHECK_ERR(err, "deflateInit");

  c_stream.next_out = compr;
  c_stream.avail_out = (unsigned int)comprLen;

  /* At this point, uncompr is still mostly zeroes, so it should compress
   * very well:
   */
  c_stream.next_in = uncompr;
  c_stream.avail_in = (unsigned int)uncomprLen;
  err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
  CHECK_ERR(err, "deflate");
  if (c_stream.avail_in != 0) {
    fprintf(stderr, "deflate not greedy\n");
    exit(1);
  }

  /* Feed in already compressed data and switch to no compression: */
  PREFIX(deflateParams)(&c_stream, Z_NO_COMPRESSION, Z_DEFAULT_STRATEGY);
  c_stream.next_in = compr;
  diff = (unsigned int)(c_stream.next_out - compr);
  c_stream.avail_in = diff;
  err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
  CHECK_ERR(err, "deflate");

  /* Switch back to compressing mode: */
  PREFIX(deflateParams)(&c_stream, Z_BEST_COMPRESSION, Z_FILTERED);
  c_stream.next_in = uncompr;
  c_stream.avail_in = (unsigned int)uncomprLen;
  err = PREFIX(deflate)(&c_stream, Z_NO_FLUSH);
  CHECK_ERR(err, "deflate");

  err = PREFIX(deflate)(&c_stream, Z_FINISH);
  if (err != Z_STREAM_END) {
    fprintf(stderr, "deflate should report Z_STREAM_END\n");
    exit(1);
  }
  err = PREFIX(deflateEnd)(&c_stream);
  CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with large buffers
 */
void test_large_inflate(unsigned char *compr, size_t comprLen,
                        unsigned char *uncompr, size_t uncomprLen) {
  int err;
  PREFIX3(stream) d_stream; /* decompression stream */

  strcpy((char *)uncompr, "garbage");

  d_stream.zalloc = zalloc;
  d_stream.zfree = zfree;
  d_stream.opaque = (void *)0;

  d_stream.next_in = compr;
  d_stream.avail_in = (unsigned int)comprLen;

  err = PREFIX(inflateInit)(&d_stream);
  CHECK_ERR(err, "inflateInit");

  for (;;) {
    d_stream.next_out = uncompr; /* discard the output */
    d_stream.avail_out = (unsigned int)uncomprLen;
    err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
    if (err == Z_STREAM_END)
      break;
    CHECK_ERR(err, "large inflate");
  }

  err = PREFIX(inflateEnd)(&d_stream);
  CHECK_ERR(err, "inflateEnd");

  if (d_stream.total_out != 2 * uncomprLen + diff) {
    fprintf(stderr, "bad large inflate: %zu\n", d_stream.total_out);
    exit(1);
  }
}

/* ===========================================================================
 * Test deflate() with full flush
 */
void test_flush(unsigned char *compr, z_size_t *comprLen) {
  PREFIX3(stream) c_stream; /* compression stream */
  int err;
  unsigned int len = dataLen;

  c_stream.zalloc = zalloc;
  c_stream.zfree = zfree;
  c_stream.opaque = (void *)0;

  err = PREFIX(deflateInit)(&c_stream, Z_DEFAULT_COMPRESSION);
  CHECK_ERR(err, "deflateInit");

  c_stream.next_in = (const unsigned char *)data;
  c_stream.next_out = compr;
  c_stream.avail_in = 3;
  c_stream.avail_out = (unsigned int)*comprLen;
  err = PREFIX(deflate)(&c_stream, Z_FULL_FLUSH);
  CHECK_ERR(err, "deflate");

  compr[3]++; /* force an error in first compressed block */
  c_stream.avail_in = len - 3;

  err = PREFIX(deflate)(&c_stream, Z_FINISH);
  if (err != Z_STREAM_END) {
    CHECK_ERR(err, "deflate");
  }
  err = PREFIX(deflateEnd)(&c_stream);
  CHECK_ERR(err, "deflateEnd");

  *comprLen = (z_size_t)c_stream.total_out;
}

/* ===========================================================================
 * Test inflateSync()
 */
void test_sync(unsigned char *compr, size_t comprLen, unsigned char *uncompr,
               size_t uncomprLen) {
  int err;
  PREFIX3(stream) d_stream; /* decompression stream */

  strcpy((char *)uncompr, "garbage");

  d_stream.zalloc = zalloc;
  d_stream.zfree = zfree;
  d_stream.opaque = (void *)0;

  d_stream.next_in = compr;
  d_stream.avail_in = 2; /* just read the zlib header */

  err = PREFIX(inflateInit)(&d_stream);
  CHECK_ERR(err, "inflateInit");

  d_stream.next_out = uncompr;
  d_stream.avail_out = (unsigned int)uncomprLen;

  err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
  CHECK_ERR(err, "inflate");

  d_stream.avail_in = (unsigned int)comprLen - 2; /* read all compressed data */
  err = PREFIX(inflateSync)(&d_stream); /* but skip the damaged part */
  CHECK_ERR(err, "inflateSync");

  err = PREFIX(inflate)(&d_stream, Z_FINISH);
  if (err != Z_DATA_ERROR) {
    fprintf(stderr, "inflate should report DATA_ERROR\n");
    /* Because of incorrect adler32 */
    exit(1);
  }
  err = PREFIX(inflateEnd)(&d_stream);
  CHECK_ERR(err, "inflateEnd");
}

/* ===========================================================================
 * Test deflate() with preset dictionary
 */
void test_dict_deflate(unsigned char *compr, size_t comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    err = PREFIX(deflateSetDictionary)(
        &c_stream, (const unsigned char *)dictionary, dictionaryLen);
    CHECK_ERR(err, "deflateSetDictionary");

    dictId = c_stream.adler;
    c_stream.next_out = compr;
    c_stream.avail_out = (unsigned int)comprLen;

    c_stream.next_in = data;
    c_stream.avail_in = dataLen;

    err = PREFIX(deflate)(&c_stream, Z_FINISH);
    if (err != Z_STREAM_END) {
        fprintf(stderr, "deflate should report Z_STREAM_END\n");
        exit(1);
    }
    err = PREFIX(deflateEnd)(&c_stream);
    CHECK_ERR(err, "deflateEnd");
}

/* ===========================================================================
 * Test inflate() with a preset dictionary
 */
void test_dict_inflate(unsigned char *compr, size_t comprLen,
                       unsigned char *uncompr, size_t uncomprLen) {
  int err;
  PREFIX3(stream) d_stream; /* decompression stream */

  strcpy((char *)uncompr, "garbage");

  d_stream.zalloc = zalloc;
  d_stream.zfree = zfree;
  d_stream.opaque = (void *)0;

  d_stream.next_in = compr;
  d_stream.avail_in = (unsigned int)comprLen;

  err = PREFIX(inflateInit)(&d_stream);
  CHECK_ERR(err, "inflateInit");

  d_stream.next_out = uncompr;
  d_stream.avail_out = (unsigned int)uncomprLen;

  for (;;) {
    err = PREFIX(inflate)(&d_stream, Z_NO_FLUSH);
    if (err == Z_STREAM_END)
      break;
    if (err == Z_NEED_DICT) {
      if (d_stream.adler != dictId) {
        fprintf(stderr, "unexpected dictionary");
        exit(1);
      }
      err = PREFIX(inflateSetDictionary)(
          &d_stream, (const unsigned char *)dictionary, dictionaryLen);
    }
    CHECK_ERR(err, "inflate with dict");
  }

  err = PREFIX(inflateEnd)(&d_stream);
  CHECK_ERR(err, "inflateEnd");

  if (memcmp(uncompr, data, dataLen)) {
    fprintf(stderr, "bad inflate with dict\n");
    exit(1);
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t size) {
  size_t comprLen = 100 + 2 * size;
  size_t uncomprLen = comprLen;
  uint8_t *compr, *uncompr;

  if (size < 1)
    return 0;

  data = d;
  dataLen = size;
  compr = (uint8_t *)malloc(comprLen);
  uncompr = (uint8_t *)malloc(uncomprLen);

  check_compress_level(compr, comprLen, uncompr, uncomprLen, 1);
  check_compress_level(compr, comprLen, uncompr, uncomprLen, 3);
  check_compress_level(compr, comprLen, uncompr, uncomprLen, 6);
  check_compress_level(compr, comprLen, uncompr, uncomprLen, 7);

  test_deflate(compr, comprLen);
  test_inflate(compr, comprLen, uncompr, uncomprLen);

  test_large_deflate(compr, comprLen, uncompr, uncomprLen);
  test_large_inflate(compr, comprLen, uncompr, uncomprLen);

  if (dataLen > 3) {
    // This test requires at least 3 bytes of input data.
    test_flush(compr, &comprLen);
    test_sync(compr, comprLen, uncompr, uncomprLen);
    comprLen = uncomprLen;
  }

  /* Set up the contents of the dictionary. */
  if (dictionaryLen < dataLen)
    memcpy(dictionary, data, dictionaryLen);
  else {
    size_t index = 0;
    for (; index < dictionaryLen; index += dataLen)
      memcpy(dictionary + index, data, dataLen);
    memcpy(dictionary + index, data, dictionaryLen - index);
  }

  test_dict_deflate(compr, comprLen);
  test_dict_inflate(compr, comprLen, uncompr, uncomprLen);

  /* check_decompress will write two bytes for the zlib header: check that the
     buffer is long enough. */
  if (dataLen > 2)
    check_decompress_no_dict(compr, comprLen);

  free(compr);
  free(uncompr);

  /* This function must return 0. */
  return 0;
}
