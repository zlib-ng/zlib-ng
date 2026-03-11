/* cpu_features.c -- CPU architecture feature check
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "cpu_features.h"
#include <string.h>

Z_INTERNAL void cpu_check_features(struct cpu_features *features) {
    memset(features, 0, sizeof(struct cpu_features));
#if defined(X86_FEATURES)
    x86_check_features(&features->x86);
#elif defined(ARM_FEATURES)
    arm_check_features(&features->arm);
#elif defined(PPC_FEATURES) || defined(POWER_FEATURES)
    power_check_features(&features->power);
#elif defined(S390_FEATURES)
    s390_check_features(&features->s390);
#elif defined(RISCV_FEATURES)
    riscv_check_features(&features->riscv);
#elif defined(LOONGARCH_FEATURES)
    loongarch_check_features(&features->loongarch);
#endif
}

#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#include <sys/auxv.h>

Z_INTERNAL unsigned long zng_getauxval(unsigned long type) {
    return getauxval(type);
}
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(HAVE_SYS_AUXV_H)
#include <sys/auxv.h>

Z_INTERNAL unsigned long zng_getauxval(unsigned long type) {
    unsigned long val = 0;
    elf_aux_info(type, &val, sizeof(val));
    return val;
}
#endif
