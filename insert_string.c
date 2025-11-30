/* insert_string.c -- make insert_string functions from static inlined functions
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include "zbuild.h"
#include "deflate.h"
#include "insert_string_p.h"

void insert_string(deflate_state *const s, uint32_t str, uint32_t count) {
    insert_string_static(s, str, count);
}

void insert_string_roll(deflate_state *const s, uint32_t str, uint32_t count) {
    insert_string_roll_static(s, str, count);
}
