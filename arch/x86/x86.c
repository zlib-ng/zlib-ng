/*
 * x86 feature check
 *
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Author:
 *  Jim Kukunas
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "../../zutil.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
// Newer versions of GCC and clang come with cpuid.h
#include <cpuid.h>
#endif

ZLIB_INTERNAL int x86_cpu_has_sse2;
ZLIB_INTERNAL int x86_cpu_has_sse42;
ZLIB_INTERNAL int x86_cpu_has_pclmulqdq;
ZLIB_INTERNAL int x86_cpu_has_tzcnt;

static void cpuid(int info, unsigned* eax, unsigned* ebx, unsigned* ecx, unsigned* edx) {
#ifdef _MSC_VER
	unsigned int registers[4];
	__cpuid(registers, info);

	*eax = registers[0];
	*ebx = registers[1];
	*ecx = registers[2];
	*edx = registers[3];
#else
	unsigned int _eax;
	unsigned int _ebx;
	unsigned int _ecx;
	unsigned int _edx;
	__cpuid(info, _eax, _ebx, _ecx, _edx);
	*eax = _eax;
	*ebx = _ebx;
	*ecx = _ecx;
	*edx = _edx;
#endif
}

void ZLIB_INTERNAL x86_check_features(void) {
	unsigned eax, ebx, ecx, edx;
	unsigned maxbasic;

	cpuid(0, &maxbasic, &ebx, &ecx, &edx);

	cpuid(1 /*CPU_PROCINFO_AND_FEATUREBITS*/, &eax, &ebx, &ecx, &edx);

	x86_cpu_has_sse2 = edx & 0x4000000;
	x86_cpu_has_sse42 = ecx & 0x100000;
	x86_cpu_has_pclmulqdq = ecx & 0x2;

	if (maxbasic >= 7) {
	  cpuid(7, &eax, &ebx, &ecx, &edx);

	  // check BMI1 bit
	  // Reference: https://software.intel.com/sites/default/files/article/405250/how-to-detect-new-instruction-support-in-the-4th-generation-intel-core-processor-family.pdf
	  x86_cpu_has_tzcnt = ebx & 0x8;
	} else {
	  x86_cpu_has_tzcnt = 0;
	}
}
