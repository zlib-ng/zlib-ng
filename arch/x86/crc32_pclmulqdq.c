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

#include "crc32_pclmulqdq_tpl.h"

Z_INTERNAL uint32_t crc32_pclmulqdq(uint32_t crc, const uint8_t *buf, size_t len) {
    return crc32_copy_impl(crc, NULL, buf, len, 0);
}

Z_INTERNAL uint32_t crc32_copy_pclmulqdq(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    return crc32_copy_impl(crc, dst, src, len, 1);
}
#endif
