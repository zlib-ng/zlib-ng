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
  uint32_t crc1 = PREFIX(crc32)(0L, NULL, 0);
  uint32_t crc2 = PREFIX(crc32)(0L, NULL, 0);
  uint32_t adler1 = PREFIX(adler32)(0L, NULL, 0);
  uint32_t adler2 = PREFIX(adler32)(0L, NULL, 0);;
  /* Checksum with a buffer of size equal to the first byte in the input. */
  uint32_t buffSize = data[0];
  uint32_t offset = 0;

  /* Discard inputs larger than 1Mb. */
  static size_t kMaxSize = 1024 * 1024;
  if (dataLen < 1 || dataLen > kMaxSize)
    return 0;

  /* Make sure the buffer has at least a byte. */
  if (buffSize == 0)
    ++buffSize;

  /* CRC32 */
  for (offset = 0; offset + buffSize <= dataLen; offset += buffSize)
    crc1 = PREFIX(crc32_z)(crc1, data + offset, buffSize);
  crc1 = PREFIX(crc32_z)(crc1, data + offset, dataLen % buffSize);

  crc2 = PREFIX(crc32_z)(crc2, data, dataLen);

  assert(crc1 == crc2);
  assert(PREFIX(crc32_combine)(crc1, crc2, dataLen) ==
         PREFIX(crc32_combine)(crc1, crc1, dataLen));

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
