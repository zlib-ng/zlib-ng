/* loongarch_features.h -- check for LoongArch features.
 *
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef LOONGARCH_FEATURES_H_
#define LOONGARCH_FEATURES_H_

struct loongarch_cpu_features {
    int has_crc;
    int has_lsx;
    int has_lasx;
};

void Z_INTERNAL loongarch_check_features(struct loongarch_cpu_features *features);

#endif /* LOONGARCH_FEATURES_H_ */
