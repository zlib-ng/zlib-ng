#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

static uint8_t cbuf[256 * 1024] = {0};
static uint8_t ubuf[256 * 1024] = {0};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  size_t cbuf_len = sizeof(cbuf);
  size_t ubuf_len = sizeof(ubuf);
  PREFIX(compress)(cbuf, &cbuf_len, data, size);
  PREFIX(uncompress)(ubuf, &ubuf_len, cbuf, cbuf_len);
  if (ubuf_len != size)
    return 1;

  return memcmp(data, ubuf, ubuf_len);
}
