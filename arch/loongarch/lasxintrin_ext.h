/* lasxintrin_ext.h
 * Copyright (C) 2025 Vladislav Shchapov <vladislav@shchapov.ru>
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef LASXINTRIN_EXT_H
#define LASXINTRIN_EXT_H

#include <lasxintrin.h>


static inline int lasx_movemask_b(__m256i v) {
    v = __lasx_xvmskltz_b(v);
    return __lasx_xvpickve2gr_w(v, 0) | (__lasx_xvpickve2gr_w(v, 4) << 16);
}

#endif // include guard LASXINTRIN_EXT_H
