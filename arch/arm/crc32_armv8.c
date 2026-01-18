/* crc32_armv8.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef ARM_CRC32

#include "zbuild.h"
#include "acle_intrins.h"
#include "crc32.h"

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_armv8(uint32_t crc, const uint8_t *buf, size_t len) {
    uint32_t c = ~crc;

    if (UNLIKELY(len == 1)) {
        c = __crc32b(c, *buf);
        c = ~c;
        return c;
    }

    uintptr_t align_diff = ALIGN_DIFF(buf, 8);
    if (align_diff) {
        if (len && (align_diff & 1)) {
            c = __crc32b(c, *buf++);
            len--;
        }

        if (len >= 2 && (align_diff & 2)) {
            c = __crc32h(c, *((uint16_t*)buf));
            buf += 2;
            len -= 2;
        }

        if (len >= 4 && (align_diff & 4)) {
            c = __crc32w(c, *((uint32_t*)buf));
            len -= 4;
            buf += 4;
        }
    }

    while (len >= 8) {
        c = __crc32d(c, *((uint64_t*)buf));
        len -= 8;
        buf += 8;
    }

    if (len & 4) {
        c = __crc32w(c, *((uint32_t*)buf));
        buf += 4;
    }

    if (len & 2) {
        c = __crc32h(c, *((uint16_t*)buf));
        buf += 2;
    }

    if (len & 1) {
        c = __crc32b(c, *buf);
    }

    c = ~c;
    return c;
}

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_copy_armv8(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc = crc32_armv8(crc, src, len);
    memcpy(dst, src, len);
    return crc;
}
#endif
