/* zarch.h -- Detect compiler architecture and define ARCH_* macros
 * Copyright (C) 2019 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ZARCH_H
#define ZARCH_H

#define ZARCH_STRINGIZE(X) ZARCH_DOSTRINGIZE(X)
#define ZARCH_DOSTRINGIZE(X) #X

/* x86_64 */
#if defined(__x86_64__) || defined(_M_X64)
#  define ARCH_X86
#  define ARCH_64BIT
#  define ARCH_NAME "x86_64"

/* x86 (32-bit) */
#elif defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__) || \
      defined(__i386) || defined(_M_IX86)
#  define ARCH_X86
#  define ARCH_32BIT
#  define ARCH_NAME "i686"

/* ARM 64-bit */
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
#  define ARCH_ARM
#  define ARCH_64BIT
#  define ARCH_NAME "aarch64"

/* ARM 32-bit */
#elif defined(__arm__) || defined(__arm) || defined(_M_ARM) || defined(__TARGET_ARCH_ARM)
#  define ARCH_ARM
#  define ARCH_32BIT
#  if defined(__ARM64_ARCH_8__) || defined(__ARM_ARCH_8A__) || defined(__ARMv8__) || defined(__ARMv8_A__)
#    define ARCH_NAME "armv8"
#  elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__)
#    define ARCH_NAME "armv7"
#  elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6T2__) || \
        defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6ZK__) || \
        defined(__ARM_ARCH_6M__)
#    define ARCH_NAME "armv6"
#  elif defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5TEJ__)
#    define ARCH_NAME "armv5"
#  elif defined(__ARM_ARCH_4T__)
#    define ARCH_NAME "armv4"
#  elif defined(__ARM_ARCH_3__)
#    define ARCH_NAME "armv3"
#  elif defined(__ARM_ARCH_2__)
#    define ARCH_NAME "armv2"
#  else
#    define ARCH_NAME "arm"
#  endif

/* PowerPC */
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
#  define ARCH_POWER
#  if defined(__64BIT__) || defined(__powerpc64__) || defined(__ppc64__)
#    define ARCH_64BIT
#    if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#      define ARCH_NAME "powerpc64le"
#    else
#      define ARCH_NAME "powerpc64"
#    endif
#  else
#    define ARCH_32BIT
#    define ARCH_NAME "powerpc"
#  endif

/* --------------- Less common architectures alphabetically below --------------- */

/* ALPHA */
#elif defined(__alpha__) || defined(__alpha)
#  define ARCH_ALPHA
#  define ARCH_64BIT
#  define ARCH_NAME "alpha"

/* Blackfin */
#elif defined(__BFIN__)
#  define ARCH_BLACKFIN
#  define ARCH_32BIT
#  define ARCH_NAME "blackfin"

/* Itanium */
#elif defined(__ia64) || defined(_M_IA64)
#  define ARCH_IA64
#  define ARCH_64BIT
#  define ARCH_NAME "ia64"

/* MIPS */
#elif defined(__mips__) || defined(__mips)
#  define ARCH_MIPS
#  if defined(__mips64) || defined(__mips64__)
#    define ARCH_64BIT
#    define ARCH_NAME "mips64"
#  else
#    define ARCH_32BIT
#    define ARCH_NAME "mips"
#  endif

/* Motorola 68000-series */
#elif defined(__m68k__)
#  define ARCH_M68K
#  define ARCH_32BIT
#  define ARCH_NAME "m68k"

/* SuperH */
#elif defined(__sh__)
#  define ARCH_SH
#  define ARCH_32BIT
#  define ARCH_NAME "sh"

/* SPARC */
#elif defined(__sparc__) || defined(__sparc)
#  define ARCH_SPARC
#  if defined(__sparcv9) || defined(__sparc_v9__) || defined(__sparc64__)
#    define ARCH_64BIT
#    define ARCH_NAME "sparc9"
#  elif defined(__sparcv8) || defined(__sparc_v8__)
#    define ARCH_32BIT
#    define ARCH_NAME "sparc8"
#  else
#    define ARCH_32BIT
#    define ARCH_NAME "sparc"
#  endif

/* SystemZ / S390 */
#elif defined(__s390x) || defined(__zarch__)
#  define ARCH_S390
#  define ARCH_64BIT
#  define ARCH_NAME "s390x"
#elif defined(__s390__)
#  define ARCH_S390
#  define ARCH_32BIT
#  define ARCH_NAME "s390"
#elif defined(__370__)
#  define ARCH_S390
#  define ARCH_32BIT
#  define ARCH_NAME "s370"

/* PARISC */
#elif defined(__hppa__)
#  define ARCH_PARISC
#  if defined(__hppa64__) || defined(__LP64__)
#    define ARCH_64BIT
#    define ARCH_NAME "parisc64"
#  else
#    define ARCH_32BIT
#    define ARCH_NAME "parisc"
#  endif

/* RS-6000 */
#elif defined(__THW_RS6000)
#  define ARCH_RS6000
#  define ARCH_32BIT
#  define ARCH_NAME "rs6000"

/* RISC-V */
#elif defined(__riscv)
#  define ARCH_RISCV
#  if __riscv_xlen == 64
#    define ARCH_64BIT
#    define ARCH_NAME "riscv64"
#  elif __riscv_xlen == 32
#    define ARCH_32BIT
#    define ARCH_NAME "riscv32"
#  else
#    define ARCH_NAME "riscv"
#  endif

/* LOONGARCH */
#elif defined(__loongarch_lp64)
#  define ARCH_LOONGARCH
#  define ARCH_64BIT
#  define ARCH_NAME "loongarch64"

/* Emscripten (WebAssembly) */
#elif defined(__EMSCRIPTEN__)
#  define ARCH_WASM
#  if defined(__wasm64__)
#    define ARCH_64BIT
#    define ARCH_NAME "wasm64"
#  else
#    define ARCH_32BIT
#    define ARCH_NAME "wasm32"
#  endif

/* Elbrus 2000 (aka e2k) */
#elif defined(__e2k__)
#  define ARCH_E2K
/* e2k reuse x86 optimizations */
#  define ARCH_X86
#  define ARCH_64BIT
#  define ARCH_NAME "e2k"
#  define ARCH_VERSION_STR ZARCH_STRINGIZE(__iset__)

/* Unrecognized architecture */
#else
#  if defined(__LP64__) || defined(_LP64) || defined(_WIN64)
#    define ARCH_64BIT
#  else
#    define ARCH_32BIT
#  endif
#  define ARCH_NAME "unrecognized"
#endif

#endif /* ZARCH_H */
