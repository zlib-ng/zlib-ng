/* crc_pclmulqdq.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include "zbuild.h"
#include "x86.h"
#include "crc_folding.h"
#include "deflate.h"

#ifdef X86_PCLMULQDQ_CRC
ZLIB_INTERNAL void crc_reset(deflate_state *const s) {
    if (x86_cpu_has_pclmulqdq) {
        crc_fold_init(s);
        return;
    }
    s->strm->adler = PREFIX(crc32)(0L, NULL, 0);
}

ZLIB_INTERNAL void crc_finalize(deflate_state *const s) {
    if (x86_cpu_has_pclmulqdq)
        s->strm->adler = crc_fold_512to32(s);
}

ZLIB_INTERNAL void copy_with_crc(PREFIX3(stream) *strm, unsigned char *dst, unsigned long size) {
    if (x86_cpu_has_pclmulqdq) {
        crc_fold_copy(strm->state, dst, strm->next_in, size);
        return;
    }
    memcpy(dst, strm->next_in, size);
    strm->adler = PREFIX(crc32)(strm->adler, dst, size);
}
#endif
