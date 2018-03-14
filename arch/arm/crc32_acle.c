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

#ifndef UNROLL_LESS
    while (len >= 32) {
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        c = __crc32w(c, *buf4++);
        len -= 32;
    }
#endif

    while (len >= 4) {
        c = __crc32w(c, *buf4++);
        len -= 4;
    }

    if (len >= 2) {
        buf2 = (const uint16_t *) buf4;
        c = __crc32h(c, *buf2++);
        len -= 2;
        buf = (const unsigned char *) buf2;
    } else {
        buf = (const unsigned char *) buf4;
    }

    if (len) {
        c = __crc32b(c, *buf);
    }

    c = ~c;
    return c;
}
#endif
