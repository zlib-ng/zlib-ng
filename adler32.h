#ifndef ADLER32_H_
#define ADLER32_H_

#include "zbuild.h"

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#endif

/* adler32 */
typedef uint32_t (*adler32_func)(uint32_t adler, const uint8_t *buf, size_t len);

extern uint32_t adler32_c(uint32_t adler, const uint8_t *buf, size_t len);
#ifdef ARM_NEON
   extern uint32_t adler32_neon(uint32_t adler, const uint8_t *buf, size_t len);
#  define native_adler32 adler32_neon
#endif
#ifdef PPC_VMX
   extern uint32_t adler32_vmx(uint32_t adler, const uint8_t *buf, size_t len);
#  define native_adler32 adler32_vmx
#endif
#ifdef POWER8_VSX
   extern uint32_t adler32_power8(uint32_t adler, const uint8_t* buf, size_t len);
#  define native_adler32 adler32_power8
#endif
#ifdef RISCV_RVV
   extern uint32_t adler32_rvv(uint32_t adler, const uint8_t *buf, size_t len);
#  define native_adler32 adler32_rvv
#endif

#ifdef X86_SSSE3
   extern uint32_t adler32_ssse3(uint32_t adler, const uint8_t *buf, size_t len);
#  define native_adler32 adler32_ssse3
#endif
#ifdef X86_AVX2
   extern uint32_t adler32_avx2(uint32_t adler, const uint8_t* buf, size_t len);
#  undef native_adler32
#  define native_adler32 adler32_avx2
#endif
#ifdef X86_AVX512
   extern uint32_t adler32_avx512(uint32_t adler, const uint8_t* buf, size_t len);
#  undef native_adler32
#  define native_adler32 adler32_avx512
#endif
#ifdef X86_AVX512VNNI
   extern uint32_t adler32_avx512_vnni(uint32_t adler, const uint8_t* buf, size_t len);
#  undef native_adler32
#  define native_adler32 adler32_avx512_vnni
#endif

/* adler32 folding */
extern uint32_t adler32_fold_copy_c(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#ifdef RISCV_RVV
   extern uint32_t adler32_fold_copy_rvv(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#  define native_adler32_fold_copy adler32_fold_copy_rvv
#endif
#ifdef X86_SSE42
   extern uint32_t adler32_fold_copy_sse42(uint32_t adler, uint8_t* dst, const uint8_t* src, size_t len);
#  define native_adler32_fold_copy adler32_fold_copy_sse42
#endif
#ifdef X86_AVX2
   extern uint32_t adler32_fold_copy_avx2(uint32_t adler, uint8_t* dst, const uint8_t* src, size_t len);
#  undef native_adler32_fold_copy
#  define native_adler32_fold_copy adler32_fold_copy_avx2
#endif
#ifdef X86_AVX512
   extern uint32_t adler32_fold_copy_avx512(uint32_t adler, uint8_t* dst, const uint8_t* src, size_t len);
#  undef native_adler32_fold_copy
#  define native_adler32_fold_copy adler32_fold_copy_avx512
#endif
#ifdef X86_AVX512VNNI
   extern uint32_t adler32_fold_copy_avx512_vnni(uint32_t adler, uint8_t* dst, const uint8_t* src, size_t len);
#  undef native_adler32_fold_copy
#  define native_adler32_fold_copy adler32_fold_copy_avx512_vnni
#endif

#ifndef native_adler32
#  define native_adler32 adler32_c
#endif
#ifndef native_adler32_fold_copy
#  define native_adler32_fold_copy adler32_fold_copy_c
#endif

#endif
