/* arm_natives.h -- ARM compile-time feature detection macros.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ARM_NATIVES_H_
#define ARM_NATIVES_H_

#if defined(__ARM_FEATURE_SIMD32)
#  ifdef ARM_SIMD
#    define ARM_SIMD_NATIVE
#  endif
#endif
/* NEON is guaranteed on ARM64 (like SSE2 on x86-64) */
#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(ARCH_64BIT)
#  ifdef ARM_NEON
#    define ARM_NEON_NATIVE
#  endif
#endif
/* CRC32 is optional in ARMv8.0, mandatory in ARMv8.1+ */
#if defined(__ARM_FEATURE_CRC32) || (defined(__ARM_ARCH) && __ARM_ARCH >= 801)
#  ifdef ARM_CRC32
#    define ARM_CRC32_NATIVE
#  endif
#endif
#if defined(__ARM_FEATURE_CRC32) && defined(__ARM_FEATURE_CRYPTO) && defined(__ARM_FEATURE_SHA3)
#  ifdef ARM_PMULL_EOR3
#    define ARM_PMULL_EOR3_NATIVE
#  endif
#endif

#endif /* ARM_NATIVES_H_ */
