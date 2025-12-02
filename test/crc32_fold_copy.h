#include "zbuild.h"
#include "arch_functions.h"

/* Local test implementations of crc32_fold_copy for various backends. */
#ifdef ARM_CRC32
static void crc32_fold_copy_armv8(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_armv8(crc->value, src, len);
    memcpy(dst, src, len);
}
#endif
static void crc32_fold_copy_braid(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_braid(crc->value, src, len);
    memcpy(dst, src, len);
}
#ifndef WITHOUT_CHORBA
static void crc32_fold_copy_chorba(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_chorba(crc->value, src, len);
    memcpy(dst, src, len);
}
#endif
#ifndef WITHOUT_CHORBA_SSE
#  ifdef X86_SSE2
    static void crc32_fold_copy_chorba_sse2(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
        crc->value = crc32_chorba_sse2(crc->value, src, len);
        memcpy(dst, src, len);
    }
#  endif
#  ifdef X86_SSE41
    static void crc32_fold_copy_chorba_sse41(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
        crc->value = crc32_chorba_sse41(crc->value, src, len);
        memcpy(dst, src, len);
    }
#  endif
#endif
#ifdef LOONGARCH_CRC
static void crc32_fold_copy_loongarch64(crc32_fold *crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc->value = crc32_loongarch64(crc->value, src, len);
    memcpy(dst, src, len);
}
#endif
