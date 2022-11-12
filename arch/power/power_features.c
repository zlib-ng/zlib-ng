/* power_features.c - POWER feature check
 * Copyright (C) 2020 Matheus Castanho <msc@linux.ibm.com>, IBM
 * Copyright (C) 2021-2022 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef HAVE_SYS_AUXV_H
#  include <sys/auxv.h>
#endif
#ifdef __FreeBSD__
#  include <machine/cpu.h>
#endif
#include "../../zbuild.h"
#include "power_features.h"

Z_INTERNAL int power_cpu_has_altivec = 0;
Z_INTERNAL int power_cpu_has_arch_2_07 = 0;
Z_INTERNAL int power_cpu_has_arch_3_00 = 0;

void Z_INTERNAL power_check_features(void) {
#ifdef PPC_FEATURES
    unsigned long hwcap;
#ifdef __FreeBSD__
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
#else
    hwcap = getauxval(AT_HWCAP);
#endif

    if (hwcap & PPC_FEATURE_HAS_ALTIVEC)
        power_cpu_has_altivec = 1;
#endif

#ifdef POWER_FEATURES
    unsigned long hwcap2;
#ifdef __FreeBSD__
    elf_aux_info(AT_HWCAP2, &hwcap2, sizeof(hwcap2));
#else
    hwcap2 = getauxval(AT_HWCAP2);
#endif

    if (hwcap2 & PPC_FEATURE2_ARCH_2_07)
        power_cpu_has_arch_2_07 = 1;
    if (hwcap2 & PPC_FEATURE2_ARCH_3_00)
        power_cpu_has_arch_3_00 = 1;
#endif
}
