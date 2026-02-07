/* power_natives.h -- POWER compile-time feature detection macros.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef POWER_NATIVES_H_
#define POWER_NATIVES_H_

#if defined(__ALTIVEC__)
#  ifdef PPC_VMX
#    define PPC_VMX_NATIVE
#  endif
#endif
#if defined(_ARCH_PWR8) && defined(__VSX__)
#  ifdef POWER8_VSX
#    define POWER8_VSX_NATIVE
#  endif
#  ifdef POWER8_VSX_CRC32
#    define POWER8_VSX_CRC32_NATIVE
#  endif
#endif
#if defined(_ARCH_PWR9)
#  ifdef POWER9
#    define POWER9_NATIVE
#  endif
#endif

#endif /* POWER_NATIVES_H_ */
