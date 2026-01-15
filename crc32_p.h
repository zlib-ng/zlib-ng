/* crc32_p.h -- Private inline functions and macros shared with
 *              different computation of the CRC-32 checksum
 *              of a data stream.
 * Copyright (C) 1995-2011, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef CRC32_P_H
#define CRC32_P_H

#define CRC_DO1 c = crc_table[(c ^ *buf++) & 0xff] ^ (c >> 8)
#define CRC_DO8 CRC_DO1; CRC_DO1; CRC_DO1; CRC_DO1; CRC_DO1; CRC_DO1; CRC_DO1; CRC_DO1

Z_FORCEINLINE static uint32_t crc32_copy_small(uint32_t crc, uint8_t *dst, const uint8_t *buf, size_t len, const int COPY) {
    uint32_t c = crc;
    if (COPY) {
        memcpy(dst, buf, len);
    }
    while (len >= 8) {
        len -= 8;
        CRC_DO8;
    }
    while (len--) {
        CRC_DO1;
    }

    return ~c;
}

#endif /* CRC32_P_H */
