/* x86_natives.h -- x86 compile-time feature detection macros.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef X86_NATIVES_H_
#define X86_NATIVES_H_

#if defined(__SSE2__) || (defined(ARCH_X86) && defined(ARCH_64BIT))
#  ifdef X86_SSE2
#    define X86_SSE2_NATIVE
#  endif
#endif
#if defined(__SSSE3__)
#  ifdef X86_SSSE3
#    define X86_SSSE3_NATIVE
#  endif
#endif
#if defined(__SSE4_1__)
#  ifdef X86_SSE41
#    define X86_SSE41_NATIVE
#  endif
#endif
#if defined(__SSE4_2__)
#  ifdef X86_SSE42
#    define X86_SSE42_NATIVE
#  endif
#endif
#if defined(__PCLMUL__)
#  ifdef X86_PCLMULQDQ_CRC
#    define X86_PCLMULQDQ_NATIVE
#  endif
#endif
#if defined(__AVX2__)
#  ifdef X86_AVX2
#    define X86_AVX2_NATIVE
#  endif
#endif
#if defined(__AVX512F__) && defined(__AVX512DQ__) && defined(__AVX512BW__) && defined(__AVX512VL__)
#  ifdef X86_AVX512
#    define X86_AVX512_NATIVE
#  endif
#endif
#if defined(__AVX512VNNI__)
#  ifdef X86_AVX512VNNI
#    define X86_AVX512VNNI_NATIVE
#  endif
#endif
#if defined(__VPCLMULQDQ__)
#  if defined(X86_VPCLMULQDQ_AVX2) && defined(X86_AVX2_NATIVE)
#    define X86_VPCLMULQDQ_AVX2_NATIVE
#  endif
#  if defined(X86_VPCLMULQDQ_AVX512) && defined(X86_AVX512_NATIVE)
#    define X86_VPCLMULQDQ_AVX512_NATIVE
#  endif
#endif

#endif /* X86_NATIVES_H_ */
