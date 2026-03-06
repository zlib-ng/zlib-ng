/* crc32_armv8.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef ARM_CRC32

#include "zbuild.h"
#include "acle_intrins.h"
#include "crc32_armv8_p.h"

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_armv8(uint32_t crc, const uint8_t *buf, size_t len) {
    uint32_t c = ~crc;

    if (UNLIKELY(len == 1)) {
        c = __crc32b(c, *buf);
        return ~c;
    }

    /* Align to 8-byte boundary for tail processing */
    uintptr_t align_diff = ALIGN_DIFF(buf, 8);
    if (align_diff)
        c = crc32_armv8_align(c, &buf, &len, align_diff);

    return crc32_armv8_tail(c, buf, len);
}

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_copy_armv8(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc = crc32_armv8(crc, src, len);
    memcpy(dst, src, len);
    return crc;
}
#endif
