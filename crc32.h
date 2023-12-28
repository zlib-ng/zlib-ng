#include <stdint.h>

#include "zbuild.h"
#include "crc32_fold.h"

/* CRC32 */
typedef uint32_t (*crc32_func)(uint32_t crc32, const uint8_t *buf, size_t len);

extern uint32_t PREFIX(crc32_braid)(uint32_t crc, const uint8_t *buf, size_t len);
#ifdef ARM_ACLE
extern uint32_t crc32_acle(uint32_t crc, const uint8_t *buf, size_t len);
#elif defined(POWER8_VSX)
extern uint32_t crc32_power8(uint32_t crc, const uint8_t *buf, size_t len);
#elif defined(S390_CRC32_VX)
extern uint32_t crc32_s390_vx(uint32_t crc, const uint8_t *buf, size_t len);
#endif

/* CRC32 folding */
#ifdef X86_PCLMULQDQ_CRC
extern uint32_t crc32_fold_pclmulqdq_reset(crc32_fold *crc);
extern void     crc32_fold_pclmulqdq_copy(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len);
extern void     crc32_fold_pclmulqdq(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc);
extern uint32_t crc32_fold_pclmulqdq_final(crc32_fold *crc);
extern uint32_t crc32_pclmulqdq(uint32_t crc32, const uint8_t *buf, size_t len);
#endif
#if defined(X86_PCLMULQDQ_CRC) && defined(X86_VPCLMULQDQ_CRC)
extern uint32_t crc32_fold_vpclmulqdq_reset(crc32_fold *crc);
extern void     crc32_fold_vpclmulqdq_copy(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len);
extern void     crc32_fold_vpclmulqdq(crc32_fold *crc, const uint8_t *src, size_t len, uint32_t init_crc);
extern uint32_t crc32_fold_vpclmulqdq_final(crc32_fold *crc);
extern uint32_t crc32_vpclmulqdq(uint32_t crc32, const uint8_t *buf, size_t len);
#endif
