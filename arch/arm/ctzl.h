#ifndef ARM_CTZL_H
#define ARM_CTZL_H

#include <armintr.h>

#if defined(_MSC_VER) && !defined(__clang__)
static __forceinline int __builtin_ctzl(unsigned long value) {
    unsigned long trailing_zero;
    _BitScanForward(&trailing_zero, value);
    return (int)trailing_zero;
}
#endif

#endif
