/* crc32_vpclmulqdq.c -- VPCMULQDQ-based CRC32 folding implementation.
 * Copyright Wangyang Guo (wangyang.guo@intel.com)
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(X86_PCLMULQDQ_CRC) && defined(X86_VPCLMULQDQ_CRC)

#define X86_VPCLMULQDQ
#define CRC32_FOLD_COPY_NAME  crc32_fold_vpclmulqdq_copy
#define CRC32_FOLD_NAME       crc32_fold_vpclmulqdq
#define CRC32_FOLD_RESET_NAME crc32_fold_vpclmulqdq_reset
#define CRC32_FOLD_FINAL_NAME crc32_fold_vpclmulqdq_final
#define CRC32_NAME            crc32_vpclmulqdq

#include "crc32_pclmulqdq_tpl.h"

#endif
