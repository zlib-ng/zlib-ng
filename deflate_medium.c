/* deflate_medium.c -- The deflate_medium deflate strategy
 * Copyright (C) Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef NO_MEDIUM_STRATEGY
#include "zbuild.h"
#include "deflate.h"
#include "deflate_p.h"
#include "functable.h"
#include "insert_string_p.h"

struct match {
    uint32_t match_start;
    uint32_t match_length;
    uint32_t strstart;
    uint32_t orgstart;
};

// Make sure USE_FIZZLE is not defined from outside
#ifdef USE_FIZZLE
#  undef USE_FIZZLE
#endif

// deflate_medium without fizzle, for levels 3-4
#define SUFFIX(name) name
#include "deflate_medium_tpl.h"

#undef SUFFIX

// deflate_medium with fizzle, for levels 5-6
#define SUFFIX(name) name ## _fizzle
#define USE_FIZZLE
#include "deflate_medium_tpl.h"

#endif
