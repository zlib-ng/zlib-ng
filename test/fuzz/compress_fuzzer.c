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

static void write_zlib_header(uint8_t *s, unsigned level_flags, unsigned w_bits,
                              unsigned preset_dict) {
  unsigned int header = (Z_DEFLATED + ((w_bits-8)<<4)) << 8;
  header |= (level_flags << 6);

  if (preset_dict)
    header |= PRESET_DICT;
  header += 31 - (header % 31);

  /* s is guaranteed to be longer than 2 bytes. */
  put_byte(s, 0, (unsigned char)(header >> 8));
  put_byte(s, 1, (unsigned char)(header & 0xff));
}

static void check_decompress(uint8_t *cbuf, size_t cbuf_len, uint8_t *dbuf,
                             size_t dbuf_len) {
  /* Write a gzip header for each level and for each window size and try
     decompress the random input data. */
  unsigned level_flags = 0; /* compression level (0..3) */
  for (; level_flags < 4; ++level_flags) {
    unsigned w_bits = 8; /* window size log2(w_size)  (8..16) */
    for (; w_bits <= 16; ++w_bits) {
      unsigned preset_dict = 0; /* (0..1) */
      for (; preset_dict <= 1; ++preset_dict) {
        write_zlib_header(dbuf, level_flags, w_bits, preset_dict);
        PREFIX(uncompress)(cbuf, &cbuf_len, dbuf, dbuf_len);
      }
    }
  }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  size_t cbuf_len = 100 + 2 * size, ubuf_len = size;
  /* Need at least two bytes for the gzip header. */
  size_t dbuf_len = size + 2;
  uint8_t *cbuf = (uint8_t *)malloc(cbuf_len);
  uint8_t *ubuf = (uint8_t *)malloc(ubuf_len);
  uint8_t *dbuf = (uint8_t *)malloc(dbuf_len);

  check_compress_level(data, size, cbuf, cbuf_len, ubuf, ubuf_len, 1);
  check_compress_level(data, size, cbuf, cbuf_len, ubuf, ubuf_len, 3);
  check_compress_level(data, size, cbuf, cbuf_len, ubuf, ubuf_len, 6);
  check_compress_level(data, size, cbuf, cbuf_len, ubuf, ubuf_len, 7);

  /* Copy the input data after the first two bytes left for the gzip header. */
  memcpy(dbuf + 2, data, size);
  check_decompress(cbuf, cbuf_len, dbuf, dbuf_len);
  check_decompress(cbuf, size, dbuf, dbuf_len);
  check_decompress(cbuf, 100, dbuf, dbuf_len);
  check_decompress(cbuf, 10, dbuf, dbuf_len);

  free(cbuf);
  free(ubuf);

  /* This function must return 0. */
  return 0;
}
