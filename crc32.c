/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2022 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * This interleaved implementation of a CRC makes use of pipelined multiple
 * arithmetic-logic units, commonly found in modern CPU cores. It is due to
 * Kadatch and Jenkins (2010). See doc/crc-doc.1.0.pdf in this distribution.
 */

#include "zbuild.h"
#include "functable.h"
#include "crc32_p.h"

/* ========================================================================= */

Z_EXPORT const uint32_t * PREFIX(get_crc_table)(void) {
    return (const uint32_t *)crc_table;
}

/* crc32 function meant for short buffers like gzip headers */
Z_INTERNAL uint32_t crc32_small(uint32_t crc, const uint8_t *buf, size_t len) {
    if (UNLIKELY(len >= 32)){
        return FUNCTABLE_CALL(crc32)(crc, buf, len);
    }

    return ~crc32_copy_small(~crc, NULL, buf, len, 32, 0);
}


#ifdef ZLIB_COMPAT
Z_EXPORT unsigned long crc32_z(unsigned long crc, const unsigned char *buf, size_t len) {
    if (buf == NULL)
        return CRC32_INITIAL_VALUE;
    return (unsigned long)FUNCTABLE_CALL(crc32)((uint32_t)crc, buf, len);
}
Z_EXPORT unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned int len) {
    if (buf == NULL)
        return CRC32_INITIAL_VALUE;
    return (unsigned long)FUNCTABLE_CALL(crc32)((uint32_t)crc, buf, len);
}
#endif

#ifndef ZLIB_COMPAT
#  if defined(HAVE_SYMVER)
// Preferred function
ZSYMVER_DEF(zng_crc32_sizet, zng_crc32, "ZLIB_NG_2.4.0")
Z_EXPORT uint32_t zng_crc32_sizet(uint32_t crc, const unsigned char *buf, size_t len) {
    if (buf == NULL)
        return CRC32_INITIAL_VALUE;
    return FUNCTABLE_CALL(crc32)(crc, buf, len);
}
// Deprecated function
ZSYMVER(zng_crc32_uint32, zng_crc32, "ZLIB_NG_2.0.0")
Z_EXPORT uint32_t zng_crc32_uint32(uint32_t crc, const unsigned char *buf, uint32_t len) {
    if (buf == NULL)
        return CRC32_INITIAL_VALUE;
    return FUNCTABLE_CALL(crc32)(crc, buf, len);
}

#  else
// Fallback to preferred function
Z_EXPORT uint32_t zng_crc32(uint32_t crc, const unsigned char *buf, size_t len) {
    if (buf == NULL)
        return CRC32_INITIAL_VALUE;
    return FUNCTABLE_CALL(crc32)(crc, buf, len);
}
#  endif

#    ifdef zng_crc32_z
#      undef zng_crc32_z
#    endif
// Deprecated function
Z_EXPORT uint32_t zng_crc32_z(uint32_t crc, const unsigned char *buf, size_t len) {
    if (buf == NULL)
        return CRC32_INITIAL_VALUE;
    return FUNCTABLE_CALL(crc32)(crc, buf, len);
}
#endif
