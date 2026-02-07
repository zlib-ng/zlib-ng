/* loongarch_natives.h -- LoongArch compile-time feature detection macros.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef LOONGARCH_NATIVES_H_
#define LOONGARCH_NATIVES_H_

#if defined(__loongarch__)
// All known CPUs have crc instructions
#  ifdef LOONGARCH_CRC
#    define LOONGARCH_CRC_NATIVE
#  endif
#endif
#if defined(__loongarch_sx)
#  ifdef LOONGARCH_LSX
#    define LOONGARCH_LSX_NATIVE
#  endif
#endif
#if defined(__loongarch_asx)
#  ifdef LOONGARCH_LASX
#    define LOONGARCH_LASX_NATIVE
#  endif
#endif

#endif /* LOONGARCH_NATIVES_H_ */
