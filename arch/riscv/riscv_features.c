#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#  include <sys/auxv.h>
#endif

#include "zbuild.h"
#include "riscv_features.h"

#define ISA_V_HWCAP (1 << ('v' - 'a'))
#define ISA_ZBC_HWCAP (1 << 29)

void Z_INTERNAL riscv_check_features(struct riscv_cpu_features *features) {
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
    unsigned long hw_cap = getauxval(AT_HWCAP);
#else
    unsigned long hw_cap = 0;
#endif
    features->has_rvv = hw_cap & ISA_V_HWCAP;
    features->has_zbc = hw_cap & ISA_ZBC_HWCAP;
}
