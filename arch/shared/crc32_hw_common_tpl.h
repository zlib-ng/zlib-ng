/* crc32_hw_common_tpl.h -- Private shared inline CRC32 functions for CPU with native crc instructions
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"


Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_hw_align(uint32_t crc, uint8_t **dst, const uint8_t **buf,
                                                          size_t *len, uintptr_t align_diff, const int COPY) {
    if (*len && (align_diff & 1)) {
        uint8_t val = **buf;
        if (COPY) {
            **dst = val;
            *dst += 1;
        }
        crc = CRC32B(crc, val);
        *buf += 1;
        *len -= 1;
    }

    if (*len >= 2 && (align_diff & 2)) {
        uint16_t val = *((uint16_t*)*buf);
        if (COPY) {
            memcpy(*dst, &val, 2);
            *dst += 2;
        }
        crc = CRC32H(crc, val);
        *buf += 2;
        *len -= 2;
    }

    if (*len >= 4 && (align_diff & 4)) {
        uint32_t val = *((uint32_t*)*buf);
        if (COPY) {
            memcpy(*dst, &val, 4);
            *dst += 4;
        }
        crc = CRC32W(crc, val);
        *buf += 4;
        *len -= 4;
    }

    if (*len >= 8 && (align_diff & 8)) {
        uint64_t val = *((uint64_t*)*buf);
        if (COPY) {
            memcpy(*dst, &val, 8);
            *dst += 8;
        }
        crc = CRC32D(crc, val);
        *buf += 8;
        *len -= 8;
    }

    return crc;
}

Z_FORCEINLINE static Z_TARGET_CRC uint32_t crc32_hw_tail(uint32_t crc, uint8_t *dst, const uint8_t *buf,
                                                         size_t len, const int COPY) {
    while (len >= 8) {
        uint64_t val = *((uint64_t*)buf);
        if (COPY) {
            memcpy(dst, &val, 8);
            dst += 8;
        }
        crc = CRC32D(crc, val);
        buf += 8;
        len -= 8;
    }

    if (len & 4) {
        uint32_t val = *((uint32_t*)buf);
        if (COPY) {
            memcpy(dst, &val, 4);
            dst += 4;
        }
        crc = CRC32W(crc, val);
        buf += 4;
    }

    if (len & 2) {
        uint16_t val = *((uint16_t*)buf);
        if (COPY) {
            memcpy(dst, &val, 2);
            dst += 2;
        }
        crc = CRC32H(crc, val);
        buf += 2;
    }

    if (len & 1) {
        uint8_t val = *buf;
        if (COPY)
            *dst = val;
        crc = CRC32B(crc, val);
    }

    return ~crc;
}
