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
#endif
#ifdef X86_SSE2_FILL_WINDOW
extern void fill_window_sse(deflate_state *s);
#endif

/* =========================================================================
 * Initialize func_table
 */
ZLIB_INTERNAL void functableInit() {
    // Initialize defaults
    func_table.insert_string=&insert_string_c;
    func_table.fill_window=&fill_window_c;

    // insert_string
    #ifdef X86_SSE4_2_CRC_HASH
    if (x86_cpu_has_sse42)
        func_table.insert_string=&insert_string_sse;
    #endif

    // fill_window
    #ifdef X86_SSE2_FILL_WINDOW
    # ifndef X86_NOCHECK_SSE2
    if (x86_cpu_has_sse2)
    # endif
        func_table.fill_window=&fill_window_sse;
    # endif
}
