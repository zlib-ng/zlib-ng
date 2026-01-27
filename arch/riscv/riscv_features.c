#ifdef RISCV_FEATURES

#include "zbuild.h"
#include "riscv_features.h"

#include <sys/utsname.h>

#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#  include <sys/auxv.h>
#endif

#define ISA_V_HWCAP (1 << ('v' - 'a'))
#define ISA_ZBC_HWCAP (1 << 29)

static int riscv_check_features_runtime_hwcap(struct riscv_cpu_features *features) {
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
    unsigned long hw_cap = getauxval(AT_HWCAP);

    features->has_rvv = hw_cap & ISA_V_HWCAP;
    features->has_zbc = hw_cap & ISA_ZBC_HWCAP;

    return 1;
#else
    return 0;
#endif
}

static void riscv_check_features_runtime(struct riscv_cpu_features *features) {
    riscv_check_features_runtime_hwcap(features);
}

void Z_INTERNAL riscv_check_features(struct riscv_cpu_features *features) {
    riscv_check_features_runtime(features);
#ifdef RISCV_RVV
    if (features->has_rvv) {
        size_t e8m1_vec_len;
        intptr_t vtype_reg_val;
        // Check that a vuint8m1_t vector is at least 16 bytes and that tail
        // agnostic and mask agnostic mode are supported
        //
        __asm__ volatile(
                "vsetvli %0, zero, e8, m1, ta, ma\n\t"
                "csrr %1, vtype"
                : "=r"(e8m1_vec_len), "=r"(vtype_reg_val));

        // The RVV target is supported if the VILL bit of VTYPE (the MSB bit of
        // VTYPE) is not set and the length of a vuint8m1_t vector is at least 16
        // bytes
        features->has_rvv = (vtype_reg_val >= 0 && e8m1_vec_len >= 16);
    }
#endif
}

#endif
