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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  size_t cbuf_len = 100 + 2 * size, ubuf_len = size;
  uint8_t *cbuf = (uint8_t *)malloc(cbuf_len);
  uint8_t *ubuf = (uint8_t *)malloc(ubuf_len);
  PREFIX(compress)(cbuf, &cbuf_len, data, size);
  PREFIX(uncompress)(ubuf, &ubuf_len, cbuf, cbuf_len);

  /* Make sure compress + uncompress gives back the input data. */
  assert(size == ubuf_len);
  assert(0 == memcmp(data, ubuf, size));

  free(cbuf);
  free(ubuf);

  /* This function must return 0. */
  return 0;
}
