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
    Z_REGISTER uint32_t c;
    Z_REGISTER uint16_t buf2;
    Z_REGISTER uint32_t buf4;
    Z_REGISTER uint64_t buf8;

    c = ~crc;

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
            buf2 = *((uint16_t*)buf);
            c = __crc32h(c, buf2);
            buf += sizeof(uint16_t);
            len -= sizeof(uint16_t);
        }

        if ((len >= sizeof(uint32_t)) && ((ptrdiff_t)buf & (sizeof(uint64_t) - 1))) {
            buf4 = *((uint32_t*)buf);
            c = __crc32w(c, buf4);
            len -= sizeof(uint32_t);
            buf += sizeof(uint32_t);
        }

    }

    while (len >= sizeof(uint64_t)) {
        buf8 = *((uint64_t*)buf);
        c = __crc32d(c, buf8);
        len -= sizeof(uint64_t);
        buf += sizeof(uint64_t);
    }

    if (len & sizeof(uint32_t)) {
        buf4 = *((uint32_t*)buf);
        c = __crc32w(c, buf4);
        buf += sizeof(uint32_t);
    }

    if (len & sizeof(uint16_t)) {
        buf2 = *((uint16_t*)buf);
        c = __crc32h(c, buf2);
        buf += sizeof(uint16_t);
    }

    if (len & sizeof(uint8_t)) {
        c = __crc32b(c, *buf);
    }

    c = ~c;
    return c;
}

/* Note: Based on generic crc32_fold_* implementation with functable call replaced by direct call. */
Z_INTERNAL Z_TARGET_CRC void crc32_fold_copy_armv8(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_armv8(crc->value, src, len);
    memcpy(dst, src, len);
}

Z_INTERNAL Z_TARGET_CRC void crc32_fold_armv8(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc) {
    Z_UNUSED(init_crc);
    crc->value = crc32_armv8(crc->value, src, len);
}

#endif
