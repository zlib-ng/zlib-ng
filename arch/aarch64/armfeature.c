#include "zutil.h"

#if defined(__linux__)
# include <sys/auxv.h>
# include <asm/hwcap.h>
#endif

static int arm_has_crc32() {
#if defined(__linux__) && defined(HWCAP_CRC32)
  return (getauxval(AT_HWCAP) & HWCAP_CRC32) != 0 ? 1 : 0;
#elif defined(ARM_NOCHECK_ACLE)
  return 1;
#else
  return 0;
#endif
}

ZLIB_INTERNAL int arm_cpu_has_neon;
ZLIB_INTERNAL int arm_cpu_has_crc32;

void ZLIB_INTERNAL arm_check_features(void) {
  arm_cpu_has_neon = 1; /* always available */
  arm_cpu_has_crc32 = arm_has_crc32();
}
