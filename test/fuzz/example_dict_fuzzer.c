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
static size_t dictionaryLen = 0;
static unsigned long dictId; /* Adler32 value of the dictionary */

/* ===========================================================================
 * Test deflate() with preset dictionary
 */
void test_dict_deflate(unsigned char **compr, size_t *comprLen)
{
    PREFIX3(stream) c_stream; /* compression stream */
    int err;

    c_stream.zalloc = zalloc;
    c_stream.zfree = zfree;
    c_stream.opaque = (void *)0;

    err = PREFIX(deflateInit)(&c_stream, Z_BEST_COMPRESSION);
    CHECK_ERR(err, "deflateInit");

    err = PREFIX(deflateSetDictionary)(
        &c_stream, (const unsigned char *)data, dictionaryLen);
    CHECK_ERR(err, "deflateSetDictionary");

    *comprLen = PREFIX(deflateBound)(&c_stream, dataLen);
    *compr = (uint8_t *)calloc(1, *comprLen);

    dictId = c_stream.adler;
    c_stream.next_out = *compr;
    c_stream.avail_out = (unsigned int)(*comprLen);

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
void test_dict_inflate(unsigned char *compr, size_t comprLen) {
  int err;
  PREFIX3(stream) d_stream; /* decompression stream */
  unsigned char *uncompr;

  d_stream.zalloc = zalloc;
  d_stream.zfree = zfree;
  d_stream.opaque = (void *)0;

  d_stream.next_in = compr;
  d_stream.avail_in = (unsigned int)comprLen;

  err = PREFIX(inflateInit)(&d_stream);
  CHECK_ERR(err, "inflateInit");

  uncompr = (uint8_t *)calloc(1, dataLen);
  d_stream.next_out = uncompr;
  d_stream.avail_out = (unsigned int)dataLen;

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
          &d_stream, (const unsigned char *)data, dictionaryLen);
    }
    CHECK_ERR(err, "inflate with dict");
  }

  err = PREFIX(inflateEnd)(&d_stream);
  CHECK_ERR(err, "inflateEnd");

  if (memcmp(uncompr, data, dataLen)) {
    fprintf(stderr, "bad inflate with dict\n");
    exit(1);
  }

  free(uncompr);
}

int LLVMFuzzerTestOneInput(const uint8_t *d, size_t size) {
  size_t comprLen = 0;
  uint8_t *compr;

  /* Discard inputs larger than 100Kb. */
  static size_t kMaxSize = 100 * 1024;

  if (size < 1 || size > kMaxSize)
    return 0;

  data = d;
  dataLen = size;

  /* Set up the contents of the dictionary.  The size of the dictionary is
     intentionally selected to be of unusual size.  To help cover more corner
     cases, the size of the dictionary is read from the input data.  */
  dictionaryLen = data[0];
  if (dictionaryLen > dataLen)
    dictionaryLen = dataLen;

  test_dict_deflate(&compr, &comprLen);
  test_dict_inflate(compr, comprLen);

  free(compr);

  /* This function must return 0. */
  return 0;
}
