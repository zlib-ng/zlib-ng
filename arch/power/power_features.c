/* power_features.c - POWER feature check
 * Copyright (C) 2020 Matheus Castanho <msc@linux.ibm.com>, IBM
 * Copyright (C) 2021-2022 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef HAVE_SYS_AUXV_H
#  include <sys/auxv.h>
#endif
#include "../../zbuild.h"
#include "power_features.h"

Z_INTERNAL int power_cpu_has_altivec = 0;
Z_INTERNAL int power_cpu_has_arch_2_07 = 0;
Z_INTERNAL int power_cpu_has_arch_3_00 = 0;

void Z_INTERNAL power_check_features(void) {
#ifdef PPC_FEATURES
    unsigned long hwcap;
    hwcap = getauxval(AT_HWCAP);

    if (hwcap & PPC_FEATURE_HAS_ALTIVEC)
        power_cpu_has_altivec = 1;
#endif

#ifdef POWER_FEATURES
    unsigned long hwcap2;
    hwcap2 = getauxval(AT_HWCAP2);

    if (hwcap2 & PPC_FEATURE2_ARCH_2_07)
        power_cpu_has_arch_2_07 = 1;
    if (hwcap2 & PPC_FEATURE2_ARCH_3_00)
        power_cpu_has_arch_3_00 = 1;
#endif
}
