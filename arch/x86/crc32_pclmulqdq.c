/*
 * Compute the CRC32 using a parallelized folding approach with the PCLMULQDQ
 * instruction.
 *
 * A white paper describing this algorithm can be found at:
 *     doc/crc-pclmulqdq.pdf
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2016 Marian Beermann (support for initial value)
 * Authors:
 *     Wajdi Feghali   <wajdi.k.feghali@intel.com>
 *     Jim Guilford    <james.guilford@intel.com>
 *     Vinodh Gopal    <vinodh.gopal@intel.com>
 *     Erdinc Ozturk   <erdinc.ozturk@intel.com>
 *     Jim Kukunas     <james.t.kukunas@linux.intel.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef X86_PCLMULQDQ_CRC

#include "crc32_fold_pclmulqdq_tpl.h"

Z_INTERNAL uint32_t crc32_pclmulqdq(uint32_t crc, const uint8_t *buf, size_t len) {
    /* For lens smaller than ~12, crc32_small method is faster.
     * But there are also minimum requirements for the pclmul functions due to alignment */
    if (len < 16)
        return crc32_small(crc, buf, len);

    crc32_fold ALIGNED_(16) crc_state;
    crc32_fold_reset(&crc_state);
    crc32_fold_copy(&crc_state, NULL, buf, len, crc, 0);
    return crc32_fold_final(&crc_state);
}

Z_INTERNAL uint32_t crc32_copy_pclmulqdq(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    /* For lens smaller than ~12, crc32_small method is faster.
     * But there are also minimum requirements for the pclmul functions due to alignment */
    if (len < 16)
        return crc32_small_copy(crc, dst, src, len);

    crc32_fold ALIGNED_(16) crc_state;
    crc32_fold_reset(&crc_state);
    crc32_fold_copy(&crc_state, dst, src, len, crc, 1);
    return crc32_fold_final(&crc_state);
}
#endif
