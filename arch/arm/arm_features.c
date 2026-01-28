#ifdef ARM_FEATURES

#include "zbuild.h"
#include "arm_features.h"

#if defined(HAVE_SYS_AUXV_H)
#  include <sys/auxv.h>
#  ifdef ARM_ASM_HWCAP
#    include <asm/hwcap.h>
#  endif
#elif defined(__FreeBSD__) && defined(ARCH_64BIT)
#  include <machine/armreg.h>
#  ifndef ID_AA64ISAR0_CRC32_VAL
#    define ID_AA64ISAR0_CRC32_VAL ID_AA64ISAR0_CRC32
#  endif
#elif defined(__OpenBSD__) && defined(ARCH_64BIT)
#  include <machine/armreg.h>
#  include <machine/cpu.h>
#  include <sys/sysctl.h>
#  include <sys/types.h>
#elif defined(__APPLE__)
#  if !defined(_DARWIN_C_SOURCE)
#    define _DARWIN_C_SOURCE /* enable types aliases (eg u_int) */
#  endif
#  include <sys/sysctl.h>
#elif defined(_WIN32)
#  include <windows.h>
#endif

static int arm_has_crc32(void) {
    int has_crc32 = 0;
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_CRC32
    has_crc32 = (getauxval(AT_HWCAP) & HWCAP_CRC32) != 0;
#  elif defined(HWCAP2_CRC32)
    has_crc32 = (getauxval(AT_HWCAP2) & HWCAP2_CRC32) != 0;
#  endif
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_CRC32
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    has_crc32 = (hwcap & HWCAP_CRC32) != 0;
#  elif defined(HWCAP2_CRC32)
    unsigned long hwcap2 = 0;
    elf_aux_info(AT_HWCAP2, &hwcap2, sizeof(hwcap2));
    has_crc32 = (hwcap2 & HWCAP2_CRC32) != 0;
#  endif
#elif defined(__FreeBSD__) && defined(ARCH_64BIT)
    has_crc32 = getenv("QEMU_EMULATING") == NULL
      && ID_AA64ISAR0_CRC32_VAL(READ_SPECIALREG(id_aa64isar0_el1)) >= ID_AA64ISAR0_CRC32_BASE;
#elif defined(__OpenBSD__) && defined(ARCH_64BIT)
    int isar0_mib[] = { CTL_MACHDEP, CPU_ID_AA64ISAR0 };
    uint64_t isar0 = 0;
    size_t len = sizeof(isar0);
    if (sysctl(isar0_mib, 2, &isar0, &len, NULL, 0) != -1) {
      has_crc32 = ID_AA64ISAR0_CRC32(isar0) >= ID_AA64ISAR0_CRC32_BASE;
    }
#elif defined(__APPLE__)
    int has_feat = 0;
    size_t size = sizeof(has_feat);
    has_crc32 = sysctlbyname("hw.optional.armv8_crc32", &has_feat, &size, NULL, 0) == 0
        && has_feat == 1;
#elif defined(_WIN32)
    has_crc32 = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
#elif defined(ARM_NOCHECK_CRC32)
    has_crc32 = 1;
#endif
    return has_crc32;
}

static int arm_has_pmull(void) {
    int has_pmull = 0;
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_PMULL
    has_pmull = (getauxval(AT_HWCAP) & HWCAP_PMULL) != 0;
#  elif defined(HWCAP_AES)
    /* PMULL is part of crypto extension, check for AES as proxy */
    has_pmull = (getauxval(AT_HWCAP) & HWCAP_AES) != 0;
#  endif
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_PMULL
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    has_pmull = (hwcap & HWCAP_PMULL) != 0;
#  elif defined(HWCAP_AES)
    /* PMULL is part of crypto extension, check for AES as proxy */
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    has_pmull = (hwcap & HWCAP_AES) != 0;
#  endif
#elif defined(__FreeBSD__) && defined(ARCH_64BIT)
    /* Check for AES feature as PMULL is part of crypto extension */
    has_pmull = getenv("QEMU_EMULATING") == NULL
      && ID_AA64ISAR0_AES_VAL(READ_SPECIALREG(id_aa64isar0_el1)) >= ID_AA64ISAR0_AES_BASE;
#elif defined(__OpenBSD__) && defined(ARCH_64BIT)
    int isar0_mib[] = { CTL_MACHDEP, CPU_ID_AA64ISAR0 };
    uint64_t isar0 = 0;
    size_t len = sizeof(isar0);
    if (sysctl(isar0_mib, 2, &isar0, &len, NULL, 0) != -1) {
      has_pmull = ID_AA64ISAR0_AES(isar0) >= ID_AA64ISAR0_AES_BASE;
    }
#elif defined(__APPLE__)
    int has_feat = 0;
    size_t size = sizeof(has_feat);
    has_pmull = sysctlbyname("hw.optional.arm.FEAT_PMULL", &has_feat, &size, NULL, 0) == 0
        && has_feat == 1;
#elif defined(_WIN32)
    /* Windows checks for crypto/AES support */
#  ifdef PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE
    has_pmull = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
#  endif
#elif defined(__ARM_FEATURE_CRYPTO) || defined(__ARM_FEATURE_AES)
    /* Compile-time check */
    has_pmull = 1;
#endif
    return has_pmull;
}

static int arm_has_eor3(void) {
    int has_eor3 = 0;
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
    /* EOR3 is part of SHA3 extension, check HWCAP2_SHA3 */
#  ifdef HWCAP2_SHA3
    has_eor3 = (getauxval(AT_HWCAP2) & HWCAP2_SHA3) != 0;
#  elif defined(HWCAP_SHA3)
    has_eor3 = (getauxval(AT_HWCAP) & HWCAP_SHA3) != 0;
#  endif
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP2_SHA3
    unsigned long hwcap2 = 0;
    elf_aux_info(AT_HWCAP2, &hwcap2, sizeof(hwcap2));
    has_eor3 = (hwcap2 & HWCAP2_SHA3) != 0;
#  elif defined(HWCAP_SHA3)
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    has_eor3 = (hwcap & HWCAP_SHA3) != 0;
#  endif
#elif defined(__FreeBSD__) && defined(ARCH_64BIT)
    /* FreeBSD: check for SHA3 in id_aa64isar0_el1 */
#  ifdef ID_AA64ISAR0_SHA3_VAL
    has_eor3 = getenv("QEMU_EMULATING") == NULL
      && ID_AA64ISAR0_SHA3_VAL(READ_SPECIALREG(id_aa64isar0_el1)) >= ID_AA64ISAR0_SHA3_BASE;
#  endif
#elif defined(__OpenBSD__) && defined(ARCH_64BIT)
#  ifdef ID_AA64ISAR0_SHA3
    int isar0_mib[] = { CTL_MACHDEP, CPU_ID_AA64ISAR0 };
    uint64_t isar0 = 0;
    size_t len = sizeof(isar0);
    if (sysctl(isar0_mib, 2, &isar0, &len, NULL, 0) != -1) {
      has_eor3 = ID_AA64ISAR0_SHA3(isar0) >= ID_AA64ISAR0_SHA3_IMPL;
    }
#  endif
#elif defined(__APPLE__)
    /* All Apple Silicon (M1+) has SHA3/EOR3 support */
    int has_feat = 0;
    size_t size = sizeof(has_feat);
    has_eor3 = sysctlbyname("hw.optional.arm.FEAT_SHA3", &has_feat, &size, NULL, 0) == 0
        && has_feat == 1;
    /* Fallback to legacy name for older macOS versions */
    if (!has_eor3) {
        size = sizeof(has_feat);
        has_eor3 = sysctlbyname("hw.optional.armv8_2_sha3", &has_feat, &size, NULL, 0) == 0
            && has_feat == 1;
    }
#elif defined(_WIN32)
#  ifdef PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE
    has_eor3 = IsProcessorFeaturePresent(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE);
#  endif
#elif defined(__ARM_FEATURE_SHA3)
    /* Compile-time check */
    has_eor3 = 1;
#endif
    return has_eor3;
}

/* AArch64 has neon. */
#if defined(ARCH_ARM) && defined(ARCH_32BIT)
static inline int arm_has_neon(void) {
    int has_neon = 0;
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_ARM_NEON
    has_neon = (getauxval(AT_HWCAP) & HWCAP_ARM_NEON) != 0;
#  elif defined(HWCAP_NEON)
    has_neon = (getauxval(AT_HWCAP) & HWCAP_NEON) != 0;
#  endif
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_NEON
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    has_neon = (hwcap & HWCAP_NEON) != 0;
#  endif
#elif defined(__APPLE__)
    int has_feat = 0;
    size_t size = sizeof(has_feat);
    has_neon = sysctlbyname("hw.optional.neon", &has_feat, &size, NULL, 0) == 0
        && has_feat == 1;
#elif defined(_M_ARM) && defined(WINAPI_FAMILY_PARTITION)
#  if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
    has_neon = 1; /* Always supported */
#  endif
#endif

#ifdef ARM_NOCHECK_NEON
    has_neon = 1;
#endif
    return has_neon;
}
#endif

/* AArch64 does not have ARMv6 SIMD. */
#if defined(ARCH_ARM) && defined(ARCH_32BIT)
static inline int arm_has_simd(void) {
    int has_simd = 0;
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
    const char *platform = (const char *)getauxval(AT_PLATFORM);
    has_simd = platform
       && (strncmp(platform, "v6l", 3) == 0
        || strncmp(platform, "v7l", 3) == 0
        || strncmp(platform, "v8l", 3) == 0);
#elif defined(ARM_NOCHECK_SIMD)
    has_simd = 1;
#endif
    return has_simd;
}
#endif

#if defined(ARCH_64BIT) && !defined(__APPLE__)
/* MIDR_EL1 bit field definitions */
#define MIDR_IMPLEMENTOR(midr)  (((midr) & (0xffU << 24)) >> 24)
#define MIDR_PARTNUM(midr)      (((midr) & (0xfffU << 4)) >> 4)

/* ARM CPU Implementer IDs */
#define ARM_IMPLEMENTER_ARM      0x41
#define ARM_IMPLEMENTER_QUALCOMM 0x51
#define ARM_IMPLEMENTER_APPLE    0x61

/* ARM CPU Part Numbers */

/* Cortex-X series - Multiple PMULL lanes */
#define ARM_PART_CORTEX_X1   0xd44
#define ARM_PART_CORTEX_X1C  0xd4c
#define ARM_PART_CORTEX_X2   0xd48
#define ARM_PART_CORTEX_X3   0xd4e
#define ARM_PART_CORTEX_X4   0xd82
#define ARM_PART_CORTEX_X925 0xd85

/* Neoverse V/N2 series - Multiple PMULL lanes */
#define ARM_PART_NEOVERSE_N2 0xd49
#define ARM_PART_NEOVERSE_V1 0xd40
#define ARM_PART_NEOVERSE_V2 0xd4f
#define ARM_PART_NEOVERSE_V3 0xd8e

/* Snapdragon X Elite/Plus - Custom core */
#define QUALCOMM_PART_ORYON 0x001
#endif

static inline int arm_has_cpuid(void) {
    int has_cpuid = 0;
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_CPUID
    has_cpuid = (getauxval(AT_HWCAP) & HWCAP_CPUID) != 0;
#  elif defined(HWCAP2_CPUID)
    has_cpuid = (getauxval(AT_HWCAP2) & HWCAP2_CPUID) != 0;
#  endif
#elif (defined(__FreeBSD__) || defined(__OpenBSD__)) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_CPUID
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    has_cpuid = (hwcap & HWCAP_CPUID) != 0;
#  endif
#endif
    return has_cpuid;
}

/* Determine if CPU has fast PMULL (multiple execution units) */
static inline int arm_cpu_has_fast_pmull(void) {
    int has_fast_pmull = 0;
#if defined(__APPLE__)
    /* On macOS, all Apple Silicon has fast PMULL */
    has_fast_pmull = 1;
#elif defined(ARCH_64BIT) && !defined(_WIN32)
    /* We need CPUID feature to read MIDR register */
    if (!arm_has_cpuid())
        return has_fast_pmull;

    uint64_t midr;
    __asm__ ("mrs %0, midr_el1" : "=r" (midr));

    uint32_t implementer = MIDR_IMPLEMENTOR(midr);
    uint32_t part = MIDR_PARTNUM(midr);

    if (implementer == ARM_IMPLEMENTER_APPLE) {
        /* All Apple Silicon (M1+) have fast PMULL */
        has_fast_pmull = 1;
    } else if (implementer == ARM_IMPLEMENTER_ARM) {
        /* ARM Cortex-X and Neoverse V/N2 series have multi-lane PMULL */
        switch (part) {
            case ARM_PART_CORTEX_X1:
            case ARM_PART_CORTEX_X1C:
            case ARM_PART_CORTEX_X2:
            case ARM_PART_CORTEX_X3:
            case ARM_PART_CORTEX_X4:
            case ARM_PART_CORTEX_X925:
            case ARM_PART_NEOVERSE_N2:
            case ARM_PART_NEOVERSE_V1:
            case ARM_PART_NEOVERSE_V2:
            case ARM_PART_NEOVERSE_V3:
                has_fast_pmull = 1;
        }
    } else if (implementer == ARM_IMPLEMENTER_QUALCOMM) {
        /* Qualcomm Oryon (Snapdragon X Elite/Plus) has fast PMULL */
        if (part == QUALCOMM_PART_ORYON)
            has_fast_pmull = 1;
    }
#endif
    return has_fast_pmull;
}

void Z_INTERNAL arm_check_features(struct arm_cpu_features *features) {
#if defined(ARCH_ARM) && defined(ARCH_64BIT)
    features->has_simd = 0; /* never available */
    features->has_neon = 1; /* always available */
#else
    features->has_simd = arm_has_simd();
    features->has_neon = arm_has_neon();
#endif
    features->has_crc32 = arm_has_crc32();
    features->has_pmull = arm_has_pmull();
    features->has_eor3 = arm_has_eor3();
    features->has_fast_pmull = features->has_pmull && arm_cpu_has_fast_pmull();
}

#endif
