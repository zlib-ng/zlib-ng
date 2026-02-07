/* arch_natives.h -- Compile-time feature detection macros for all architectures.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ARCH_NATIVES_H_
#define ARCH_NATIVES_H_

#include "zbuild.h"

#if defined(X86_FEATURES)
#  include "arch/x86/x86_natives.h"
#elif defined(ARM_FEATURES)
#  include "arch/arm/arm_natives.h"
#elif defined(PPC_FEATURES) || defined(POWER_FEATURES)
#  include "arch/power/power_natives.h"
#elif defined(S390_FEATURES)
#  include "arch/s390/s390_natives.h"
#elif defined(RISCV_FEATURES)
#  include "arch/riscv/riscv_natives.h"
#elif defined(LOONGARCH_FEATURES)
#  include "arch/loongarch/loongarch_natives.h"
#endif

#endif /* ARCH_NATIVES_H_ */
