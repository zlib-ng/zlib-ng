/* crc32_acle.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
*/

#ifdef __ARM_FEATURE_CRC32
#include <arm_acle.h>
#ifdef ZLIB_COMPAT
#  include <zconf.h>
#else
#  include <zconf-ng.h>
#endif
#ifdef __linux__
#  include <stddef.h>
#endif

uint32_t crc32_acle(uint32_t crc, const unsigned char *buf, uint64_t len) {
    register uint32_t c;
    register const uint16_t *buf2;
    register const uint32_t *buf4;
    register const uint64_t *buf8;

    c = ~crc;
    if (len && ((ptrdiff_t)buf & 1)) {
        c = __crc32b(c, *buf++);
        len--;
    }

    if ((len > 2) && ((ptrdiff_t)buf & 2)) {
        buf2 = (const uint16_t *) buf;
        c = __crc32h(c, *buf2++);
        len -= 2;
        buf4 = (const uint32_t *) buf2;
    } else {
        buf4 = (const uint32_t *) buf;
    }

    if ((len > 4) && ((ptrdiff_t)buf & 4)) {
        c = __crc32b(c, *buf4++);
        len -= 4;
    }

    buf8 = (const uint64_t *) buf4;

#ifndef UNROLL_LESS
    while (len >= 32) {
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        c = __crc32d(c, *buf8++);
        len -= 32;
    }
#endif

    while (len >= 8) {
        c = __crc32d(c, *buf8++);
        len -= 8;
    }

    if (len >= 4) {
        buf4 = (const uint32_t *) buf8;
        c = __crc32w(c, *buf4++);
        len -= 4;
        buf2 = (const uint16_t *) buf4;
    } else {
        buf2 = (const uint16_t *) buf8;
    }

    if (len >= 2) {
        c = __crc32h(c, *buf2++);
        len -= 2;
    }

    if (len) {
        buf = (const unsigned char *) buf2;
        c = __crc32b(c, *buf);
    }

    c = ~c;
    return c;
}
#endif
