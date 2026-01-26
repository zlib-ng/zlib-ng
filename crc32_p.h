/* crc32_p.h -- Private inline functions and macros shared with
 *              different computation of the CRC-32 checksum
 *              of a data stream.
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef CRC32_P_H
#define CRC32_P_H

#define CRC_DO1(c, buf, i) c = crc_table[(c ^ buf[i]) & 0xff] ^ (c >> 8)
#define CRC_DO2(c, buf, i) {CRC_DO1(c, buf, i); CRC_DO1(c, buf, i+1);}
#define CRC_DO4(c, buf, i) {CRC_DO2(c, buf, i); CRC_DO2(c, buf, i+2);}
#define CRC_DO8(c, buf, i) {CRC_DO4(c, buf, i); CRC_DO4(c, buf, i+4);}

Z_FORCEINLINE static uint32_t crc32_copy_small(uint32_t crc, uint8_t *dst, const uint8_t *buf, size_t len,
                                               const int MAX_LEN, const int COPY) {
    if (MAX_LEN >= 8) {
        while (len >= 8) {
            if (COPY) {
                memcpy(dst, buf, 8);
                dst += 8;
            }
            CRC_DO8(crc, buf, 0);
            buf += 8;
            len -= 8;
        }
    }
    if (len & 4) {
        if (COPY) {
            memcpy(dst, buf, 4);
            dst += 4;
        }
        CRC_DO4(crc, buf, 0);
        buf += 4;
    }
    if (len & 2) {
        if (COPY) {
            memcpy(dst, buf, 2);
            dst += 2;
        }
        CRC_DO2(crc, buf, 0);
        buf += 2;
    }
    if (len & 1) {
        if (COPY)
            *dst = *buf;
        CRC_DO1(crc, buf, 0);
    }

    return crc;
}

#endif /* CRC32_P_H */
