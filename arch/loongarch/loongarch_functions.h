/* loongarch_functions.h -- LoongArch implementations for arch-specific functions.
 *
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef LOONGARCH_FUNCTIONS_H_
#define LOONGARCH_FUNCTIONS_H_

#ifdef LOONGARCH_CRC
uint32_t crc32_loongarch64(uint32_t crc, const uint8_t *buf, size_t len);
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
// LOONGARCH - CRC32 - All known CPUs has crc instructions
#  if defined(LOONGARCH_CRC)
#    undef native_crc32
#    define native_crc32 crc32_loongarch64
#  endif
#  if defined(LOONGARCH_LSX) && defined(__loongarch_sx)
#  endif
#  if defined(LOONGARCH_LASX) && defined(__loongarch_asx)
#  endif
#endif

#endif /* LOONGARCH_FUNCTIONS_H_ */
