/* crc32_armv8.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * Copyright (C) 2016 Yang Zhang
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef ARM_CRC32

#include "zbuild.h"
#include "acle_intrins.h"
#include "crc32_armv8_p.h"

#include "arch/shared/crc32_hw_copy_impl_tpl.h"


Z_INTERNAL Z_TARGET_CRC uint32_t crc32_armv8(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_hw_copy_impl(crc, NULL, buf, len, 0);
}

Z_INTERNAL Z_TARGET_CRC uint32_t crc32_copy_armv8(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
#if OPTIMAL_CMP >= 32
    return crc32_hw_copy_impl(crc, dst, src, len, 1);
#else
    /* Without unaligned access, interleaved stores get decomposed into byte ops */
    crc = crc32_armv8(crc, src, len);
    memcpy(dst, src, len);
    return crc;
#endif
}
#endif
