/* functable.h -- Struct containing function pointers to optimized functions
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef FUNCTABLE_H_
#define FUNCTABLE_H_

#include "deflate.h"

typedef void     (* fill_window_cb) (deflate_state *s);
typedef uint32_t (* update_hash_cb) (deflate_state *const s, uint32_t hash, uint32_t val);
typedef uint32_t (* adler32_cb)     (uint32_t adler, const unsigned char *buf, size_t len);
typedef uint32_t (* crc32_cb)       (uint32_t crc, const unsigned char *buf, uint64_t len);
typedef void     (* slide_hash_cb)  (deflate_state *s);

struct functable_s {
    fill_window_cb  fill_window;
    update_hash_cb  update_hash;
    adler32_cb      adler32;
    crc32_cb        crc32;
    slide_hash_cb   slide_hash;
};

ZLIB_INTERNAL extern __thread struct functable_s functable;


#endif
