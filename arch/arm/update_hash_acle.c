/* update_hash_acle.c -- update_hash variant using ACLE's CRC instructions
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#if defined(__ARM_FEATURE_CRC32) && defined(ARM_ACLE_CRC_HASH)
#include <arm_acle.h>
#include "../../zbuild.h"
#include "../../deflate.h"

ZLIB_INTERNAL uint32_t update_hash_acle(deflate_state *const s, uint32_t hash, uint32_t val) {
    hash = __crc32w(hash, val);
    return hash;
}
#endif
