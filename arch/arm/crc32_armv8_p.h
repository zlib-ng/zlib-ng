/* crc32_armv8_p.h -- Private shared inline ARMv8 CRC32 functions
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef CRC32_ARMV8_P_H
#define CRC32_ARMV8_P_H

#include "zbuild.h"
#include "acle_intrins.h"

Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_armv8_align(uint32_t crc, const uint8_t **buf,
                                                             size_t *len, uintptr_t align_diff) {
    if (*len && (align_diff & 1)) {
        uint8_t val = **buf;
        crc = __crc32b(crc, val);
        *buf += 1;
        *len -= 1;
    }

    if (*len >= 2 && (align_diff & 2)) {
        uint16_t val = *((uint16_t*)*buf);
        crc = __crc32h(crc, val);
        *buf += 2;
        *len -= 2;
    }

    if (*len >= 4 && (align_diff & 4)) {
        uint32_t val = *((uint32_t*)*buf);
        crc = __crc32w(crc, val);
        *buf += 4;
        *len -= 4;
    }

    if (*len >= 8 && (align_diff & 8)) {
        uint64_t val = *((uint64_t*)*buf);
        crc = __crc32d(crc, val);
        *buf += 8;
        *len -= 8;
    }

    return crc;
}

Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_armv8_tail(uint32_t crc, const uint8_t *buf, size_t len) {
    while (len >= 8) {
        uint64_t val = *((uint64_t*)buf);
        crc = __crc32d(crc, val);
        buf += 8;
        len -= 8;
    }

    if (len & 4) {
        uint32_t val = *((uint32_t*)buf);
        crc = __crc32w(crc, val);
        buf += 4;
    }

    if (len & 2) {
        uint16_t val = *((uint16_t*)buf);
        crc = __crc32h(crc, val);
        buf += 2;
    }

    if (len & 1) {
        uint8_t val = *buf;
        crc = __crc32b(crc, val);
    }

    return ~crc;
}

#endif /* CRC32_ARMV8_P_H */
