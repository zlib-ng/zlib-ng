#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

static void check_compress_level(const uint8_t *data, size_t size,
                                 uint8_t *cbuf, size_t cbuf_len,
                                 uint8_t *ubuf, size_t ubuf_len,
                                 int level) {
  PREFIX(compress2)(cbuf, &cbuf_len, data, size, level);
  PREFIX(uncompress)(ubuf, &ubuf_len, cbuf, cbuf_len);

  /* Make sure compress + uncompress gives back the input data. */
  assert(size == ubuf_len);
  assert(0 == memcmp(data, ubuf, size));
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
static void check_decompress_no_dict(uint8_t *cbuf, size_t cbuf_len,
                                     uint8_t *data, size_t data_len) {
  write_zlib_header(data, 0);
  PREFIX(uncompress)(cbuf, &cbuf_len, data, data_len);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  size_t cbuf_len = 100 + 2 * size, ubuf_len = size;
  uint8_t *cbuf = (uint8_t *)malloc(cbuf_len);
  uint8_t *ubuf = (uint8_t *)malloc(ubuf_len);

  check_compress_level(data, size, cbuf, cbuf_len, ubuf, ubuf_len, 1);
  check_compress_level(data, size, cbuf, cbuf_len, ubuf, ubuf_len, 3);
  check_compress_level(data, size, cbuf, cbuf_len, ubuf, ubuf_len, 6);
  check_compress_level(data, size, cbuf, cbuf_len, ubuf, ubuf_len, 7);

  /* check_decompress will write two bytes for the zlib header: check that the
     buffer is long enough and cast const away from data. */
  if (size > 2)
    check_decompress_no_dict(cbuf, cbuf_len, (uint8_t *)data, size);

  free(cbuf);
  free(ubuf);

  /* This function must return 0. */
  return 0;
}
