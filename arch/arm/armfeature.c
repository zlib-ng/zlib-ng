#if defined(__linux__)
# include <sys/auxv.h>
# include <asm/hwcap.h>
#elif defined(_WIN32)
# include <winapifamily.h>
#endif

int arm_has_crc32() {
#if defined(__linux__)
  return (getauxval(AT_HWCAP2) & HWCAP2_CRC32) != 0 ? 1 : 0;
#elif defined(ARM_NOCHECK_ACLE)
  return 1;
#else
  return 0;
#endif
}

int arm_has_neon()
{
#if defined(__linux__)
  return (getauxval(AT_HWCAP) & HWCAP_NEON) != 0 ? 1 : 0;
#elif defined(_M_ARM) && defined(WINAPI_FAMILY_PARTITION) 
# if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_PHONE_APP)
  return 1; /* Always supported */
# endif
#endif

#if defined(ARM_NOCHECK_NEON)
  return 1;
#else
  return 0;
#endif
}
