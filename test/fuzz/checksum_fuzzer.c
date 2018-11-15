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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t dataLen) {
  uint32_t crc0 = PREFIX(crc32)(0L, NULL, 0);
  uint32_t crc1 = crc0;
  uint32_t crc2 = crc0;
  uint32_t adler0 = PREFIX(adler32)(0L, NULL, 0);
  uint32_t adler1 = adler0;
  uint32_t adler2 = adler0;
  /* Checksum with a buffer of size equal to the first byte in the input. */
  uint32_t buffSize = data[0];
  uint32_t offset = 0;
  uint32_t op[32];

  /* Discard inputs larger than 1Mb. */
  static size_t kMaxSize = 1024 * 1024;
  if (dataLen < 1 || dataLen > kMaxSize)
    return 0;

  /* Make sure the buffer has at least a byte. */
  if (buffSize == 0)
    ++buffSize;

  /* CRC32 */
  PREFIX(crc32_combine_gen)(op, buffSize);
  for (offset = 0; offset + buffSize <= dataLen; offset += buffSize) {
    uint32_t crc3 = PREFIX(crc32_z)(crc0, data + offset, buffSize);
    uint32_t crc4 = PREFIX(crc32_combine_op)(crc1, crc3, op);
    crc1 = PREFIX(crc32_z)(crc1, data + offset, buffSize);
    assert(crc1 == crc4);
  }
  crc1 = PREFIX(crc32_z)(crc1, data + offset, dataLen % buffSize);

  crc2 = PREFIX(crc32_z)(crc2, data, dataLen);

  assert(crc1 == crc2);
  assert(PREFIX(crc32_combine)(crc1, crc2, dataLen) ==
         PREFIX(crc32_combine)(crc1, crc1, dataLen));

  /* Fast CRC32 combine. */
  PREFIX(crc32_combine_gen)(op, dataLen);
  assert(PREFIX(crc32_combine_op)(crc1, crc2, op) ==
         PREFIX(crc32_combine_op)(crc2, crc1, op));
  assert(PREFIX(crc32_combine)(crc1, crc2, dataLen) ==
         PREFIX(crc32_combine_op)(crc2, crc1, op));

  /* Adler32 */
  for (offset = 0; offset + buffSize <= dataLen; offset += buffSize)
    adler1 = PREFIX(adler32_z)(adler1, data + offset, buffSize);
  adler1 = PREFIX(adler32_z)(adler1, data + offset, dataLen % buffSize);

  adler2 = PREFIX(adler32_z)(adler2, data, dataLen);

  assert(adler1 == adler2);
  assert(PREFIX(adler32_combine)(adler1, adler2, dataLen) ==
         PREFIX(adler32_combine)(adler1, adler1, dataLen));

  /* This function must return 0. */
  return 0;
}
