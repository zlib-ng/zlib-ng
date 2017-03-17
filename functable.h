/* functable.h -- Struct containing function pointers to optimized functions
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef FUNCTABLE_H_
#define FUNCTABLE_H_

#include "deflate.h"

void functableInit();

struct func_table_s {
    void     (* fill_window)    (deflate_state *s);
    Pos      (* insert_string)  (deflate_state *const s, const Pos str, unsigned int count);
} func_table;

#endif
