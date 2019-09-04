#include "../../zutil.h"

#if defined(__linux__)
# include <sys/auxv.h>
# include <asm/hwcap.h>
#elif defined(_WIN32)
# include <winapifamily.h>
#endif

static int arm_has_crc32() {
#if defined(__linux__) && defined(HWCAP2_CRC32)
  return (getauxval(AT_HWCAP2) & HWCAP2_CRC32) != 0 ? 1 : 0;
#elif defined(ARM_NOCHECK_ACLE)
  return 1;
#else
  return 0;
#endif
}

/* AArch64 has neon. */
#if !defined(__aarch64__) && !defined(_M_ARM64)
static inline int arm_has_neon()
{
 #if defined(__linux__) && defined(HWCAP_NEON)
    return (getauxval(AT_HWCAP) & HWCAP_NEON) != 0 ? 1 : 0;
 #elif defined(_M_ARM) && defined(WINAPI_FAMILY_PARTITION)
  #if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
    return 1; /* Always supported */
  #endif
 #endif

 #if defined(ARM_NOCHECK_NEON)
    return 1;
 #else
    return 0;
 #endif
}
#endif

ZLIB_INTERNAL int arm_cpu_has_neon;
ZLIB_INTERNAL int arm_cpu_has_crc32;

void ZLIB_INTERNAL arm_check_features(void) {
#if defined(__aarch64__) || defined(_M_ARM64)
  arm_cpu_has_neon = 1; /* always available */
#else
  arm_cpu_has_neon = arm_has_neon();
#endif
  arm_cpu_has_crc32 = arm_has_crc32();
}
