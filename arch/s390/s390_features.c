#include "../../zbuild.h"
#include "s390_features.h"

#ifdef HAVE_SYS_AUXV_H
#  include <sys/auxv.h>
#endif

Z_INTERNAL int s390_cpu_has_vx = 0;

void Z_INTERNAL s390_check_features(void) {
#ifdef S390_FEATURES
    s390_cpu_has_vx = getauxval(AT_HWCAP) & HWCAP_S390_VX;
#endif
}
