#ifndef INSERT_STRING_P_H_
#define INSERT_STRING_P_H_

/* insert_string_p.h -- insert_string function generator
 *
 * Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

// Normal insert_string, levels 1-8
#define HASH_SLIDE           16

#define HASH_CALC(h, val)    h = ((val * 2654435761U) >> HASH_SLIDE);
#define HASH_CALC_MASK       HASH_MASK
#define HASH_CALC_VAR        h
#define HASH_CALC_VAR_INIT   uint32_t h
#define HASH_CALC_OFFSET     0

#define UPDATE_HASH          update_hash
#define INSERT_STRING        insert_string_static
#define QUICK_INSERT_STRING  quick_insert_string
#define QUICK_INSERT_VALUE   quick_insert_value

#include "insert_string_tpl.h"

// Cleanup
#undef HASH_SLIDE
#undef HASH_CALC
#undef HASH_CALC_READ
#undef HASH_CALC_MASK
#undef HASH_CALC_OFFSET
#undef HASH_CALC_VAR
#undef HASH_CALC_VAR_INIT
#undef UPDATE_HASH
#undef INSERT_STRING
#undef QUICK_INSERT_STRING
#undef QUICK_INSERT_VALUE

// Rolling insert_string, level 9
#define HASH_SLIDE           5

#define HASH_CALC(h, val)    h = ((h << HASH_SLIDE) ^ ((uint8_t)val))
#define HASH_CALC_VAR        s->ins_h
#define HASH_CALC_VAR_INIT
#define HASH_CALC_READ       val = strstart[0]
#define HASH_CALC_MASK       (32768u - 1u)
#define HASH_CALC_OFFSET     (STD_MIN_MATCH-1)

#define UPDATE_HASH          update_hash_roll
#define INSERT_STRING        insert_string_roll_static
#define QUICK_INSERT_STRING  quick_insert_string_roll
#define QUICK_INSERT_VALUE   quick_insert_value_roll

#include "insert_string_tpl.h"

#endif
