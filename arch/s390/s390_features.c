#ifdef S390_FEATURES

#include "zbuild.h"
#include "cpu_features.h"

#ifdef HAVE_SYS_AUXV_H
#  include <sys/auxv.h>
#endif

#ifndef HWCAP_S390_VXRS
#define HWCAP_S390_VXRS (1 << 11)
#endif

void Z_INTERNAL s390_check_features(struct s390_cpu_features *features) {
#ifdef HAVE_ZNG_GETAUXVAL
    features->has_vx = zng_getauxval(AT_HWCAP) & HWCAP_S390_VXRS;
#endif
}

#endif
