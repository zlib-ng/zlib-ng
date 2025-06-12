/* lsxintrin_ext.h
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef LSXINTRIN_EXT_H
#define LSXINTRIN_EXT_H

#include <lsxintrin.h>


static inline int lsx_movemask_b(__m128i v) {
    return __lsx_vpickve2gr_w(__lsx_vmskltz_b(v), 0);
}

#endif // include guard LSXINTRIN_EXT_H
