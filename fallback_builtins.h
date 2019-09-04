#ifndef X86_CTZL_H
#define X86_CTZL_H

#include <intrin.h>
#ifdef X86_CPUID
# include "arch/x86/x86.h"
#endif

#if defined(_MSC_VER) && !defined(__clang__)
/* This is not a general purpose replacement for __builtin_ctzl. The function expects that value is != 0
 * Because of that assumption trailing_zero is not initialized and the return value of _BitScanForward is not checked
 */
static __forceinline unsigned long __builtin_ctzl(unsigned long value)
{
#ifdef X86_CPUID
	if (x86_cpu_has_tzcnt)
		return _tzcnt_u32(value);
#endif
	unsigned long trailing_zero;
	_BitScanForward(&trailing_zero, value);
	return trailing_zero;
}
#endif

#endif
