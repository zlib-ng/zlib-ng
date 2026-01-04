/* crc32_armv8.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(ARM_CRC32)
#include "acle_intrins.h"
#include "zbuild.h"
#include "crc32.h"

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_armv8(uint32_t crc, const uint8_t *buf, size_t len) {
    uint32_t c = ~crc;

    if (UNLIKELY(len == 1)) {
        c = __crc32b(c, *buf);
        c = ~c;
        return c;
    }

    if ((ptrdiff_t)buf & (sizeof(uint64_t) - 1)) {
        if (len && ((ptrdiff_t)buf & 1)) {
            c = __crc32b(c, *buf++);
            len--;
        }

        if ((len >= sizeof(uint16_t)) && ((ptrdiff_t)buf & (sizeof(uint32_t) - 1))) {
            c = __crc32h(c, *((uint16_t*)buf));
            buf += sizeof(uint16_t);
            len -= sizeof(uint16_t);
        }

        if ((len >= sizeof(uint32_t)) && ((ptrdiff_t)buf & (sizeof(uint64_t) - 1))) {
            c = __crc32w(c, *((uint32_t*)buf));
            len -= sizeof(uint32_t);
            buf += sizeof(uint32_t);
        }
    }

    while (len >= sizeof(uint64_t)) {
        c = __crc32d(c, *((uint64_t*)buf));
        len -= sizeof(uint64_t);
        buf += sizeof(uint64_t);
    }

    if (len & sizeof(uint32_t)) {
        c = __crc32w(c, *((uint32_t*)buf));
        buf += sizeof(uint32_t);
    }

    if (len & sizeof(uint16_t)) {
        c = __crc32h(c, *((uint16_t*)buf));
        buf += sizeof(uint16_t);
    }

    if (len & sizeof(uint8_t)) {
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
