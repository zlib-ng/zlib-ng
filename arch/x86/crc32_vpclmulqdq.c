/* crc32_vpclmulqdq.c -- VPCMULQDQ-based CRC32 folding implementation.
 * Copyright Wangyang Guo (wangyang.guo@intel.com)
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_VPCLMULQDQ_CRC

#define X86_VPCLMULQDQ
#include "crc32_fold_pclmulqdq_tpl.h"

Z_INTERNAL uint32_t crc32_fold_vpclmulqdq_reset(crc32_fold *crc) {
    return crc32_fold_reset(crc);
}

Z_INTERNAL uint32_t crc32_fold_vpclmulqdq_final(crc32_fold *crc) {
    return crc32_fold_final(crc);
}

Z_INTERNAL void crc32_fold_vpclmulqdq(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc) {
    crc32_fold_copy(crc, NULL, src, len, init_crc, 0);
}

Z_INTERNAL void crc32_fold_vpclmulqdq_copy(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc32_fold_copy(crc, dst, src, len, 0, 1);
}

Z_INTERNAL uint32_t crc32_vpclmulqdq(uint32_t crc32, const uint8_t *buf, size_t len) {
    /* For lens smaller than ~12, crc32_small method is faster.
     * But there are also minimum requirements for the pclmul functions due to alignment */
    if (len < 16)
        return crc32_small(crc32, buf, len);

    crc32_fold ALIGNED_(16) crc_state;
    crc32_fold_reset(&crc_state);
    crc32_fold_copy(&crc_state, NULL, buf, len, crc32, 0);
    return crc32_fold_final(&crc_state);
}

#endif
