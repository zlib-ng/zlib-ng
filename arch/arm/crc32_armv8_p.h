/* crc32_armv8_p.h -- Private shared inline ARMv8 CRC32 functions
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef CRC32_ARMV8_P_H
#define CRC32_ARMV8_P_H

#include "zbuild.h"
#include "acle_intrins.h"

#define CRC32B(crc, val) __crc32b((crc), (val))
#define CRC32H(crc, val) __crc32h((crc), (val))
#define CRC32W(crc, val) __crc32w((crc), (val))
#define CRC32D(crc, val) __crc32d((crc), (val))

#include "arch/shared/crc32_hw_common_tpl.h"

#endif /* CRC32_ARMV8_P_H */
