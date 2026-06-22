/* mips_features.h -- check for MIPS CPU features
 * Copyright (C) 2026 Mika T. Lindqvist <postmaster@raasu.org>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef MIPS_FEATURES_H_
#define MIPS_FEATURES_H_

struct mips_cpu_features {
    int has_msa;
};

void Z_INTERNAL mips_check_features(struct mips_cpu_features *features);

#endif /* MIPS_FEATURES_H_ */
