#ifdef RISCV_FEATURES

#define _DEFAULT_SOURCE 1 /* For syscall() */

#include "zbuild.h"
#include "riscv_features.h"

#include <sys/utsname.h>

#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#  include <sys/auxv.h>
#endif

#if defined(__linux__) && defined(HAVE_ASM_HWPROBE_H)
#  include <asm/hwprobe.h>
#  include <sys/syscall.h> /* For __NR_riscv_hwprobe */
#  include <unistd.h> /* For syscall() */
#endif

#define ISA_V_HWCAP (1 << ('v' - 'a'))
#define ISA_ZBC_HWCAP (1 << 29)

static int riscv_check_features_runtime_hwprobe(struct riscv_cpu_features *features) {
#if defined(__NR_riscv_hwprobe) && defined(RISCV_HWPROBE_KEY_IMA_EXT_0)
    struct riscv_hwprobe probes[] = {
        {RISCV_HWPROBE_KEY_IMA_EXT_0, 0},
    };
    int ret;
    unsigned i;

    ret = syscall(__NR_riscv_hwprobe, probes, sizeof(probes) / sizeof(probes[0]), 0, NULL, 0);

    if (ret != 0) {
        /* Kernel does not support hwprobe */
        return 0;
    }

    for (i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        switch (probes[i].key) {
        case RISCV_HWPROBE_KEY_IMA_EXT_0:
#  ifdef RISCV_HWPROBE_IMA_V
            features->has_rvv = !!(probes[i].value & RISCV_HWPROBE_IMA_V);
#  endif
#  ifdef RISCV_HWPROBE_EXT_ZBC
            features->has_zbc = !!(probes[i].value & RISCV_HWPROBE_EXT_ZBC);
#  endif
            break;
        }
    }

    return 1;
#else
    return 0;
#endif
}

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
    if (riscv_check_features_runtime_hwprobe(features))
        return;

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
