#ifndef X86_CTZL_H
#define X86_CTZL_H

#include <intrin.h>

#if defined(_MSC_VER) && !defined(__clang__)
/* __builtin_ctzl
 *  - For 0, the result is undefined
 *  - On the x86 architecture, it is typically implemented using BSF
 *  - the equivalent intrinsic on MSC is _BitScanForward
 *
 * _tzcnt_u32
 *  - For 0, the result is the size of the operand 
 *  - On processors that do not support TZCNT, the instruction byte encoding is executed as BSF. In this case the result for 0
 *    is undefined.
 *  - Performance:
 *    + AMD: The reciprocal throughput for TZCNT is 2 vs 3 for BSF
 *    + Intel: On modern Intel CPUs (Haswell), the performance of TZCNT is equivalent to BSF
 *    Reference: http://www.agner.org/optimize/instruction_tables.pdf
*/
#if defined(_M_IX86) || defined(_M_AMD64)
#define __builtin_ctzl _tzcnt_u32
#else
static __forceinline unsigned long __builtin_ctzl(unsigned long value)
{
    unsigned long trailing_zero;
    _BitScanForward(&trailing_zero, value);
    return trailing_zero;
}
#endif
#endif

#endif
