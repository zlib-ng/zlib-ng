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
#define DICTIONARY_LEN 1024
static size_t dictionaryLen = DICTIONARY_LEN;
static const char *dictionary[DICTIONARY_LEN];
static unsigned long dictId; /* Adler32 value of the dictionary */

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
        fprintf(stderr, "deflate dict should report Z_STREAM_END\n");
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

  /* Discard inputs larger than 100Kb. */
  static size_t kMaxSize = 100 * 1024;

  if (size < 1 || size > kMaxSize)
    return 0;

  data = d;
  dataLen = size;
  compr = (uint8_t *)calloc(1, comprLen);
  uncompr = (uint8_t *)calloc(1, uncomprLen);

  /* Set up the contents of the dictionary. */
  dictionaryLen = DICTIONARY_LEN;
  if (dictionaryLen > dataLen)
    dictionaryLen = dataLen;

  memcpy(dictionary, data, dictionaryLen);

  test_dict_deflate(compr, comprLen);
  test_dict_inflate(compr, comprLen, uncompr, uncomprLen);

  free(compr);
  free(uncompr);

  /* This function must return 0. */
  return 0;
}
