/* insert_string_c -- insert_string variant for c
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#include "zbuild.h"
#include "deflate.h"

/* ===========================================================================
 * Update a hash value with the given input byte
 * IN  assertion: all calls to to UPDATE_HASH are made with consecutive
 *    input characters, so that a running hash key can be computed from the
 *    previous key instead of complete recalculation each time.
 */

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#  define UPDATE_HASH(s, h, val) \
    h = (3483  * ((val) & 0xff) +\
         23081 * (((val) >> 8) & 0xff) +\
         6954  * (((val) >> 16) & 0xff) +\
         20947 * (((val) >> 24) & 0xff));
#else
#  define UPDATE_HASH(s, h, val)\
    h = (s->ins_h = ((s->ins_h << s->hash_shift) ^ ((val) >> ((MIN_MATCH - 1) * 8))) & s->hash_mask)
#endif

#define INSERT_STRING       insert_string_c
#define QUICK_INSERT_STRING quick_insert_string_c

#include "insert_string_tpl.h"
