#include <stdint.h>

#include "zbuild.h"
#include "crc32_fold.h"

/* CRC32 */
typedef uint32_t (*crc32_func)(uint32_t crc32, const uint8_t *buf, size_t len);

extern uint32_t PREFIX(crc32_braid)(uint32_t crc, const uint8_t* buf, size_t len);

#ifdef ARM_ACLE
extern uint32_t crc32_acle(uint32_t crc, const uint8_t *buf, size_t len);
#  define CRC32 crc32_acle
#elif defined(POWER8_VSX)
extern uint32_t crc32_power8(uint32_t crc, const uint8_t *buf, size_t len);
#  define CRC32 crc32_power8
#elif defined(S390_CRC32_VX)
extern uint32_t crc32_s390_vx(uint32_t crc, const uint8_t *buf, size_t len);
#  define CRC32 crc32_s390_vx
#endif

/* CRC32 folding */
extern uint32_t crc32_fold_reset_c(crc32_fold *crc);
extern void     crc32_fold_copy_c(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len);
extern void     crc32_fold_c(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc);
extern uint32_t crc32_fold_final_c(crc32_fold *crc);
#ifdef X86_PCLMULQDQ_CRC
extern uint32_t crc32_fold_pclmulqdq_reset(crc32_fold *crc);
extern void     crc32_fold_pclmulqdq_copy(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len);
extern void     crc32_fold_pclmulqdq(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc);
extern uint32_t crc32_fold_pclmulqdq_final(crc32_fold *crc);
extern uint32_t crc32_pclmulqdq(uint32_t crc32, const uint8_t *buf, size_t len);
#  define CRC32_FOLD_RESET crc32_fold_pclmulqdq_reset
#  define CRC32_FOLD_COPY crc32_fold_pclmulqdq_copy
#  define CRC32_FOLD crc32_fold_pclmulqdq
#  define CRC32_FOLD_FINAL crc32_fold_pclmulqdq_final
#  undef CRC32
#  define CRC32 crc32_pclmulqdq
#endif
#if defined(X86_PCLMULQDQ_CRC) && defined(X86_VPCLMULQDQ_CRC)
extern uint32_t crc32_fold_vpclmulqdq_reset(crc32_fold *crc);
extern void     crc32_fold_vpclmulqdq_copy(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len);
extern void     crc32_fold_vpclmulqdq(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc);
extern uint32_t crc32_fold_vpclmulqdq_final(crc32_fold *crc);
extern uint32_t crc32_vpclmulqdq(uint32_t crc32, const uint8_t *buf, size_t len);
#  undef CRC32_FOLD_RESET
#  define CRC32_FOLD_RESET crc32_fold_vpclmulqdq_reset
#  undef CRC32_FOLD_COPY
#  define CRC32_FOLD_COPY crc32_fold_vpclmulqdq_copy
#  undef CRC32_FOLD
#  define CRC32_FOLD crc32_fold_vpclmulqdq
#  undef CRC32_FOLD_FINAL
#  define CRC32_FOLD_FINAL crc32_fold_vpclmulqdq_final
#  undef CRC32
#  define CRC32 crc32_vpclmulqdq
#endif

#ifndef HAVE_NATIVE_INSTRUCTIONS
#  include "functable.h"
#  undef CRC32
#  define CRC32 functable.crc32
#  undef CRC32_FOLD_RESET
#  define CRC32_FOLD_RESET functable.crc32_fold_reset
#  undef CRC32_FOLD_COPY
#  define CRC32_FOLD_COPY functable.crc32_fold_copy
#  undef CRC32_FOLD
#  define CRC32_FOLD functable.crc32_fold
#  undef CRC32_FOLD_FINAL
#  define CRC32_FOLD_FINAL functable.crc32_fold_final
#else
#  ifndef CRC32
#    define CRC32 PREFIX(crc32_braid)
#  endif
#  ifndef CRC32_FOLD_RESET
#    define CRC32_FOLD_RESET crc32_fold_reset_c
#    define CRC32_FOLD_COPY crc32_fold_copy_c
#    define CRC32_FOLD crc32_fold_c
#    define CRC32_FOLD_FINAL crc32_fold_final_c
#  endif
#endif
