/* loongarch_features.c -- check for LoongArch features.
 *
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef LOONGARCH_FEATURES

#include "zbuild.h"
#include "loongarch_features.h"

#ifdef HAVE_SYS_AUXV_H
#  include <sys/auxv.h>
#endif

#ifndef HWCAP_LOONGARCH_LSX
#  define HWCAP_LOONGARCH_LSX   (1 << 4)
#endif
#ifndef HWCAP_LOONGARCH_LASX
#  define HWCAP_LOONGARCH_LASX  (1 << 5)
#endif
#ifndef HWCAP_LOONGARCH_CRC32
#  define HWCAP_LOONGARCH_CRC32 (1 << 6)
#endif

/*
 * Application must obtain CPU features through the getauxval system call provided by the kernel.
 *
 * https://github.com/loongson/la-softdev-convention/blob/master/la-softdev-convention.adoc#91-kernel-development
 */

void Z_INTERNAL loongarch_check_features(struct loongarch_cpu_features *features) {
    unsigned long hwcap = getauxval(AT_HWCAP);

    features->has_crc = (hwcap & HWCAP_LOONGARCH_CRC32) != 0;
    features->has_lsx = (hwcap & HWCAP_LOONGARCH_LSX) != 0;
    features->has_lasx = (hwcap & HWCAP_LOONGARCH_LASX) != 0;
}

#endif
