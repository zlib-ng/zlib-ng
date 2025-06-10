/* loongarch_features.c -- check for LoongArch features.
 *
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "loongarch_features.h"

#include <larchintrin.h>

/*
 * https://loongson.github.io/LoongArch-Documentation/LoongArch-Vol1-EN.html
 *
 * Word number Bit number  Annotation  Implication
 * 0x1         25          CRC         1 indicates support for CRC instruction
 * 0x1         6           LSX         1 indicates support for 128-bit vector extension
 * 0x1         7           LASX        1 indicates support for 256-bit vector expansion
 */

void Z_INTERNAL loongarch_check_features(struct loongarch_cpu_features *features) {
    unsigned int w1 = __cpucfg(0x1);
    features->has_crc = w1 & 0x2000000;
    features->has_lsx = w1 & 0x40;
    features->has_lasx = w1 & 0x80;
}
