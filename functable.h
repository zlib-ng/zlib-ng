/* functable.h -- Struct containing function pointers to optimized functions
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef FUNCTABLE_H_
#define FUNCTABLE_H_

#include "deflate.h"

struct functable_s {
    Pos      (* insert_string)      (deflate_state *const s, const Pos str, unsigned int count);
    Pos      (* quick_insert_string)(deflate_state *const s, const Pos str);
    uint32_t (* adler32)            (uint32_t adler, const unsigned char *buf, size_t len);
    uint32_t (* crc32)              (uint32_t crc, const unsigned char *buf, uint64_t len);
    void     (* slide_hash)         (deflate_state *s);
    int32_t  (* compare258)         (const unsigned char *src0, const unsigned char *src1);
    int32_t  (* longest_match)      (deflate_state *const s, IPos cur_match);
};

ZLIB_INTERNAL extern __thread struct functable_s functable;


#endif
