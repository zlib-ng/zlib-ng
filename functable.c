/* functable.c -- Choose relevant optimized functions at runtime
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#include "functable.h"
#include "deflate.h"
#include "deflate_p.h"

#include "gzendian.h"

#if defined(X86_CPUID)
# include "arch/x86/x86.h"
#elif (defined(__arm__) || defined(__aarch64__) || defined(_M_ARM))
extern int arm_has_crc32();
extern int arm_has_neon();
#endif

/* fill_window */
#ifdef X86_SSE2_FILL_WINDOW
extern void fill_window_sse(deflate_state *s);
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM)
extern void fill_window_arm(deflate_state *s);
#endif

/* stub definitions */
ZLIB_INTERNAL void fill_window_stub(deflate_state *s);

/* functable init */
ZLIB_INTERNAL __thread struct functable_s functable = {fill_window_stub};


ZLIB_INTERNAL void fill_window_stub(deflate_state *s) {
    // Initialize default
    functable.fill_window=&fill_window_c;

    #ifdef X86_SSE2_FILL_WINDOW
    # ifndef X86_NOCHECK_SSE2
    if (x86_cpu_has_sse2)
    # endif
        functable.fill_window=&fill_window_sse;
    #elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM)
        functable.fill_window=&fill_window_arm;
    #endif

    functable.fill_window(s);
}
