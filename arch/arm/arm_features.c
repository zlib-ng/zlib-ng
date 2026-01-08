#include "zbuild.h"
#include "arm_features.h"

#if defined(HAVE_SYS_AUXV_H)
#  include <sys/auxv.h>
#  ifdef ARM_ASM_HWCAP
#    include <asm/hwcap.h>
#  endif
#elif defined(__FreeBSD__) && defined(__aarch64__)
#  include <machine/armreg.h>
#  ifndef ID_AA64ISAR0_CRC32_VAL
#    define ID_AA64ISAR0_CRC32_VAL ID_AA64ISAR0_CRC32
#  endif
#elif defined(__OpenBSD__) && defined(__aarch64__)
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
#if defined(ARM_AUXV_HAS_CRC32)
#  if defined(__FreeBSD__) || defined(__OpenBSD__)
#    ifdef HWCAP_CRC32
       unsigned long hwcap = 0;
       elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
       return (hwcap & HWCAP_CRC32) != 0 ? 1 : 0;
#    else
       unsigned long hwcap2 = 0;
       elf_aux_info(AT_HWCAP2, &hwcap2, sizeof(hwcap2));
       return (hwcap2 & HWCAP2_CRC32) != 0 ? 1 : 0;
#    endif
#  else
#    ifdef HWCAP_CRC32
       return (getauxval(AT_HWCAP) & HWCAP_CRC32) != 0 ? 1 : 0;
#    else
       return (getauxval(AT_HWCAP2) & HWCAP2_CRC32) != 0 ? 1 : 0;
#    endif
#  endif
#elif defined(__FreeBSD__) && defined(__aarch64__)
    return getenv("QEMU_EMULATING") == NULL
      && ID_AA64ISAR0_CRC32_VAL(READ_SPECIALREG(id_aa64isar0_el1)) >= ID_AA64ISAR0_CRC32_BASE;
#elif defined(__OpenBSD__) && defined(__aarch64__)
    int hascrc32 = 0;
    int isar0_mib[] = { CTL_MACHDEP, CPU_ID_AA64ISAR0 };
    uint64_t isar0 = 0;
    size_t len = sizeof(isar0);
    if (sysctl(isar0_mib, 2, &isar0, &len, NULL, 0) != -1) {
      if (ID_AA64ISAR0_CRC32(isar0) >= ID_AA64ISAR0_CRC32_BASE)
          hascrc32 = 1;
    }
    return hascrc32;
#elif defined(__APPLE__)
    int hascrc32;
    size_t size = sizeof(hascrc32);
    return sysctlbyname("hw.optional.armv8_crc32", &hascrc32, &size, NULL, 0) == 0
      && hascrc32 == 1;
#elif defined(_WIN32)
    return IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE);
#elif defined(ARM_NOCHECK_CRC32)
    return 1;
#else
    return 0;
#endif
}

static int arm_has_pmull(void) {
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
#  ifdef HWCAP_PMULL
    return (getauxval(AT_HWCAP) & HWCAP_PMULL) != 0 ? 1 : 0;
#  else
    /* PMULL is part of crypto extension, check for AES as proxy */
#    ifdef HWCAP_AES
    return (getauxval(AT_HWCAP) & HWCAP_AES) != 0 ? 1 : 0;
#    else
    return 0;
#    endif
#  endif
#elif defined(__FreeBSD__) && defined(__aarch64__)
    /* Check for AES feature as PMULL is part of crypto extension */
    return getenv("QEMU_EMULATING") == NULL
      && ID_AA64ISAR0_AES_VAL(READ_SPECIALREG(id_aa64isar0_el1)) >= ID_AA64ISAR0_AES_BASE;
#elif defined(__OpenBSD__) && defined(__aarch64__)
    int haspmull = 0;
    int isar0_mib[] = { CTL_MACHDEP, CPU_ID_AA64ISAR0 };
    uint64_t isar0 = 0;
    size_t len = sizeof(isar0);
    if (sysctl(isar0_mib, 2, &isar0, &len, NULL, 0) != -1) {
      if (ID_AA64ISAR0_AES(isar0) >= ID_AA64ISAR0_AES_BASE)
          haspmull = 1;
    }
    return haspmull;
#elif defined(__APPLE__)
#  if defined(__aarch64__) || defined(_M_ARM64)
    int haspmull;
    size_t size = sizeof(haspmull);
    return sysctlbyname("hw.optional.arm.FEAT_PMULL", &haspmull, &size, NULL, 0) == 0
      && haspmull == 1;
#  else
    return 0;
#  endif
#elif defined(_WIN32)
    /* Windows checks for crypto/AES support */
#  ifdef PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE
    return IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE);
#  else
    return 0;
#  endif
#elif defined(__ARM_FEATURE_CRYPTO) || defined(__ARM_FEATURE_AES)
    /* Compile-time check */
    return 1;
#else
    return 0;
#endif
}

static int arm_has_eor3(void) {
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
    /* EOR3 is part of SHA3 extension, check HWCAP2_SHA3 */
#  ifdef HWCAP2_SHA3
    return (getauxval(AT_HWCAP2) & HWCAP2_SHA3) != 0 ? 1 : 0;
#  elif defined(HWCAP_SHA3)
    return (getauxval(AT_HWCAP) & HWCAP_SHA3) != 0 ? 1 : 0;
#  else
    return 0;
#  endif
#elif defined(__FreeBSD__) && defined(__aarch64__)
    /* FreeBSD: check for SHA3 in id_aa64isar0_el1 */
#  ifdef ID_AA64ISAR0_SHA3_VAL
    return getenv("QEMU_EMULATING") == NULL
      && ID_AA64ISAR0_SHA3_VAL(READ_SPECIALREG(id_aa64isar0_el1)) >= ID_AA64ISAR0_SHA3_BASE;
#  else
    return 0;
#  endif
#elif defined(__OpenBSD__) && defined(__aarch64__)
#  ifdef ID_AA64ISAR0_SHA3
    int haseor3 = 0;
    int isar0_mib[] = { CTL_MACHDEP, CPU_ID_AA64ISAR0 };
    uint64_t isar0 = 0;
    size_t len = sizeof(isar0);
    if (sysctl(isar0_mib, 2, &isar0, &len, NULL, 0) != -1) {
      if (ID_AA64ISAR0_SHA3(isar0) >= ID_AA64ISAR0_SHA3_IMPL)
          haseor3 = 1;
    }
    return haseor3;
#  else
    return 0;
#  endif
#elif defined(__APPLE__)
    /* All Apple Silicon (M1+) has SHA3/EOR3 support */
#  if defined(__aarch64__) || defined(_M_ARM64)
    int hassha3;
    size_t size = sizeof(hassha3);
    if (sysctlbyname("hw.optional.arm.FEAT_SHA3", &hassha3, &size, NULL, 0) == 0 && hassha3 == 1)
        return 1;
    /* Fallback to legacy name for older macOS versions */
    size = sizeof(hassha3);
    return sysctlbyname("hw.optional.armv8_2_sha3", &hassha3, &size, NULL, 0) == 0
      && hassha3 == 1;
#  else
    return 0;
#  endif
#elif defined(_WIN32)
    /* Windows: No direct API for SHA3, return 0 for now */
    return 0;
#elif defined(__ARM_FEATURE_SHA3)
    /* Compile-time check */
    return 1;
#else
    return 0;
#endif
}

/* AArch64 has neon. */
#if defined(ARCH_ARM) && defined(ARCH_32BIT)
static inline int arm_has_neon(void) {
#if defined(ARM_AUXV_HAS_NEON)
#  if defined(__FreeBSD__) || defined(__OpenBSD__)
     unsigned long hwcap = 0;
     elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
     return (hwcap & HWCAP_NEON) != 0 ? 1 : 0;
#  else
#    ifdef HWCAP_ARM_NEON
       return (getauxval(AT_HWCAP) & HWCAP_ARM_NEON) != 0 ? 1 : 0;
#    else
       return (getauxval(AT_HWCAP) & HWCAP_NEON) != 0 ? 1 : 0;
#    endif
#  endif
#elif defined(__APPLE__)
    int hasneon;
    size_t size = sizeof(hasneon);
    return sysctlbyname("hw.optional.neon", &hasneon, &size, NULL, 0) == 0
      && hasneon == 1;
#elif defined(_M_ARM) && defined(WINAPI_FAMILY_PARTITION)
#  if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
    return 1; /* Always supported */
#  endif
#endif

#if defined(ARM_NOCHECK_NEON)
    return 1;
#else
    return 0;
#endif
}
#endif

/* AArch64 does not have ARMv6 SIMD. */
#if defined(ARCH_ARM) && defined(ARCH_32BIT)
static inline int arm_has_simd(void) {
#if defined(__linux__) && defined(HAVE_SYS_AUXV_H)
    const char *platform = (const char *)getauxval(AT_PLATFORM);
    return strncmp(platform, "v6l", 3) == 0
        || strncmp(platform, "v7l", 3) == 0
        || strncmp(platform, "v8l", 3) == 0;
#elif defined(ARM_NOCHECK_SIMD)
    return 1;
#else
    return 0;
#endif
}
#endif

#if defined(__aarch64__) && !defined(__APPLE__)
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

/* Determine if CPU has fast PMULL (multiple execution units) */
static inline int arm_cpu_has_fast_pmull(void) {
#if defined(__APPLE__)
    /* On macOS, all Apple Silicon has fast PMULL */
    return 1;
#elif defined(__aarch64__)
#  if defined(__linux__)
    /* We have to support the CPUID feature in HWCAP */
    if (!(getauxval(AT_HWCAP) & HWCAP_CPUID))
        return 0;
#  elif defined(__FreeBSD__) || defined(__OpenBSD__)
    unsigned long hwcap = 0;
    elf_aux_info(AT_HWCAP, &hwcap, sizeof(hwcap));
    if (!(hwcap & HWCAP_CPUID))
        return 0;
#  else
    return 0;
#  endif
    uint64_t midr;
    __asm__ ("mrs %0, midr_el1" : "=r" (midr));

    uint32_t implementer = MIDR_IMPLEMENTOR(midr);
    uint32_t part = MIDR_PARTNUM(midr);

    if (implementer == ARM_IMPLEMENTER_APPLE) {
        /* All Apple Silicon (M1+) have fast PMULL */
        return 1;
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
                return 1;
        }
    } else if (implementer == ARM_IMPLEMENTER_QUALCOMM) {
        /* Qualcomm Oryon (Snapdragon X Elite/Plus) has fast PMULL */
        if (part == QUALCOMM_PART_ORYON)
            return 1;
    }
#endif
    return 0;
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
