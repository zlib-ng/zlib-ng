/* arch_functions.h -- Arch-specific function prototypes.
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef CPU_FUNCTIONS_H_
#define CPU_FUNCTIONS_H_

#include "zbuild.h"
#include "zutil.h"
#include "deflate.h"
#include "fallback_builtins.h"

#if defined(X86_FEATURES)
#  include "arch/x86/x86_functions.h"
#elif defined(ARM_FEATURES)
#  include "arch/arm/arm_functions.h"
#elif defined(PPC_FEATURES) || defined(POWER_FEATURES)
#  include "arch/power/power_functions.h"
#elif defined(S390_FEATURES)
#  include "arch/s390/s390_functions.h"
#elif defined(RISCV_FEATURES)
#  include "arch/riscv/riscv_functions.h"
#elif defined(LOONGARCH_FEATURES)
#  include "arch/loongarch/loongarch_functions.h"
#else
/* No architecture detected - all fallbacks needed */
#  ifndef WITH_ALL_FALLBACKS
#    define WITH_ALL_FALLBACKS
#  endif
#endif

#ifdef WITH_ALL_FALLBACKS
#  ifndef ADLER32_FALLBACK
#    define ADLER32_FALLBACK
#  endif
#  ifndef CHUNKSET_FALLBACK
#    define CHUNKSET_FALLBACK
#  endif
#  ifndef COMPARE256_FALLBACK
#    define COMPARE256_FALLBACK
#  endif
#  ifndef CRC32_BRAID_FALLBACK
#    define CRC32_BRAID_FALLBACK
#  endif
#  if !defined(CRC32_CHORBA_FALLBACK) && !defined(WITHOUT_CHORBA)
#    define CRC32_CHORBA_FALLBACK
#  endif
#  ifndef SLIDE_HASH_FALLBACK
#    define SLIDE_HASH_FALLBACK
#  endif
#endif

#include "arch/generic/generic_functions.h"

#endif
