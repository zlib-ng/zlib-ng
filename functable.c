/* functable.c -- Choose relevant optimized functions at runtime
 * Copyright (C) 2017 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "functable.h"
#include "deflate.h"
#include "deflate_p.h"

#if defined(X86_CPUID)
# include "arch/x86/x86.h"
#endif

#ifdef X86_SSE4_2_CRC_HASH
extern Pos insert_string_sse(deflate_state *const s, const Pos str, unsigned int count);
#elif defined(ARM_ACLE_CRC_HASH)
extern Pos insert_string_acle(deflate_state *const s, const Pos str, unsigned int count);
#endif

#ifdef X86_SSE2_FILL_WINDOW
extern void fill_window_sse(deflate_state *s);
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM)
extern void fill_window_arm(deflate_state *s);
#endif

/* =========================================================================
 * Initialize functable
 */
ZLIB_INTERNAL void functableInit() {
    // Initialize defaults
    functable.insert_string=&insert_string_c;
    functable.fill_window=&fill_window_c;

    // insert_string
    #ifdef X86_SSE4_2_CRC_HASH
    if (x86_cpu_has_sse42)
        functable.insert_string=&insert_string_sse;
    #elif defined(ARM_ACLE_CRC_HASH)
        functable.insert_string=&insert_string_acle;
    #endif

    // fill_window
    #ifdef X86_SSE2_FILL_WINDOW
    # ifndef X86_NOCHECK_SSE2
    if (x86_cpu_has_sse2)
    # endif
        functable.fill_window=&fill_window_sse;
    #elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM)
        functable.fill_window=&fill_window_arm;
    #endif
}
