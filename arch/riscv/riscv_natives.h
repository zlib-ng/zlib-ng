/* riscv_natives.h -- RISCV compile-time feature detection macros.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef RISCV_NATIVES_H_
#define RISCV_NATIVES_H_

#if defined(__riscv_v) && defined(__linux__)
#  ifdef RISCV_RVV
#    define RISCV_RVV_NATIVE
#  endif
#endif
#if defined(__riscv_zbc)
#  ifdef RISCV_CRC32_ZBC
#    define RISCV_ZBC_NATIVE
#  endif
#endif

#endif /* RISCV_NATIVES_H_ */
