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
 */

void Z_INTERNAL loongarch_check_features(struct loongarch_cpu_features *features) {
    unsigned int w1 = __cpucfg(0x1);
    features->has_crc = w1 & 0x2000000;
}
