/* insert_string.c -- make insert_string functions from static inlined functions
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include "zbuild.h"
#include "deflate.h"
#include "insert_string_p.h"

Z_INTERNAL void insert_knuth_batch(deflate_state *const s, unsigned char *window, uint32_t str, uint32_t count) {
    insert_knuth_batch_static(s, window, str, count);
}

Z_INTERNAL void insert_roll_batch(deflate_state *const s, unsigned char *window, uint32_t str, uint32_t count) {
    insert_roll_batch_static(s, window, str, count);
}

Z_INTERNAL void insert_knuth_batch_head(deflate_state *const s, unsigned char *window, uint32_t str, uint32_t count) {
    insert_knuth_batch_head_static(s, window, str, count);
}
