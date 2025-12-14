/* crc32_vpclmulqdq.c -- VPCMULQDQ-based CRC32 folding implementation.
 * Copyright Wangyang Guo (wangyang.guo@intel.com)
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_VPCLMULQDQ_CRC

#define X86_VPCLMULQDQ
#include "crc32_pclmulqdq_tpl.h"

Z_INTERNAL uint32_t crc32_vpclmulqdq(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_copy_impl(crc, NULL, buf, len, 0);
}

Z_INTERNAL uint32_t crc32_copy_vpclmulqdq(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    return crc32_copy_impl(crc, dst, src, len, 1);
}
#endif
