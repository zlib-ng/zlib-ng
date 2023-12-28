#include <stdint.h>

#include "adler32_fold.h"

/* adler32 */
typedef uint32_t (*adler32_func)(uint32_t adler, const uint8_t *buf, size_t len);

extern uint32_t adler32_c(uint32_t adler, const uint8_t *buf, size_t len);
#ifdef ARM_NEON
extern uint32_t adler32_neon(uint32_t adler, const uint8_t *buf, size_t len);
#endif
#ifdef PPC_VMX
extern uint32_t adler32_vmx(uint32_t adler, const uint8_t *buf, size_t len);
#endif
#ifdef RISCV_RVV
extern uint32_t adler32_rvv(uint32_t adler, const uint8_t *buf, size_t len);
#endif
#ifdef X86_SSSE3
extern uint32_t adler32_ssse3(uint32_t adler, const uint8_t *buf, size_t len);
#endif
#ifdef X86_AVX2
extern uint32_t adler32_avx2(uint32_t adler, const uint8_t *buf, size_t len);
#endif
#ifdef X86_AVX512
extern uint32_t adler32_avx512(uint32_t adler, const uint8_t *buf, size_t len);
#endif
#ifdef X86_AVX512VNNI
extern uint32_t adler32_avx512_vnni(uint32_t adler, const uint8_t *buf, size_t len);
#endif
#ifdef POWER8_VSX
extern uint32_t adler32_power8(uint32_t adler, const uint8_t *buf, size_t len);
#endif

/* adler32 folding */
#ifdef RISCV_RVV
extern uint32_t adler32_fold_copy_rvv(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef X86_SSE42
extern uint32_t adler32_fold_copy_sse42(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef X86_AVX2
extern uint32_t adler32_fold_copy_avx2(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef X86_AVX512
extern uint32_t adler32_fold_copy_avx512(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif
#ifdef X86_AVX512VNNI
extern uint32_t adler32_fold_copy_avx512_vnni(uint32_t adler, uint8_t *dst, const uint8_t *src, size_t len);
#endif
