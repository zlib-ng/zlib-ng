/* crc32_vpclmulqdq.c -- VPCMULQDQ-based CRC32 folding implementation.
 * Copyright Wangyang Guo (wangyang.guo@intel.com)
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_VPCLMULQDQ_CRC

#define X86_VPCLMULQDQ
#include "crc32_fold_pclmulqdq_tpl.h"

Z_INTERNAL uint32_t crc32_vpclmulqdq(uint32_t crc, const uint8_t *buf, size_t len) {
    /* For lens smaller than ~12, crc32_small method is faster.
     * But there are also minimum requirements for the pclmul functions due to alignment */
    if (len < 16)
        return crc32_small(crc, buf, len);

    return crc32_copy_impl(crc, NULL, buf, len, 0);
}

Z_INTERNAL uint32_t crc32_copy_vpclmulqdq(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    /* For lens smaller than ~12, crc32_small method is faster.
     * But there are also minimum requirements for the pclmul functions due to alignment */
    if (len < 16)
        return crc32_small_copy(crc, dst, src, len);

    return crc32_copy_impl(crc, dst, src, len, 1);
}
#endif
