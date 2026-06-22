/* mips_features.c - MIPS feature check
 * Copyright (C) 2026 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#if defined(MIPS_FEATURES)

#include "zbuild.h"
#include "mips_features.h"

#ifdef HAVE_SYS_AUXV_H
#  include <sys/auxv.h>
#endif

#include <asm/hwcap.h>

void Z_INTERNAL mips_check_features(struct mips_cpu_features *features) {
#ifdef MIPS_MSA
    unsigned long hwcap;
    hwcap = getauxval(AT_HWCAP);

    if (hwcap & HWCAP_MIPS_MSA)
        features->has_msa = 1;
#endif
}

#endif
