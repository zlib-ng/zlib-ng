#if defined(__linux__)
# include <sys/auxv.h>
# include <asm/hwcap.h>
#endif

int arm_has_crc32() {
#if defined(__linux__) && defined(HWCAP_CRC32)
  return (getauxval(AT_HWCAP) & HWCAP_CRC32) != 0 ? 1 : 0;
#elif defined(ARM_NOCHECK_ACLE)
  return 1;
#else
  return 0;
#endif
}

int arm_has_neon()
{
  return 1; /* always available */
}
