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
#  include <intrin.h>
#else
// Newer versions of GCC and clang come with cpuid.h
#  include <cpuid.h>
#endif

#include <string.h>

Z_INTERNAL int x86_cpu_has_avx2;
Z_INTERNAL int x86_cpu_has_avx512;
Z_INTERNAL int x86_cpu_has_avx512vnni;
Z_INTERNAL int x86_cpu_has_sse2;
Z_INTERNAL int x86_cpu_has_ssse3;
Z_INTERNAL int x86_cpu_has_sse41;
Z_INTERNAL int x86_cpu_has_sse42;
Z_INTERNAL int x86_cpu_has_pclmulqdq;
Z_INTERNAL int x86_cpu_has_vpclmulqdq;
Z_INTERNAL int x86_cpu_has_tzcnt;
Z_INTERNAL int x86_cpu_well_suited_avx512;

static void cpuid(int info, unsigned* eax, unsigned* ebx, unsigned* ecx, unsigned* edx) {
#ifdef _MSC_VER
    unsigned int registers[4];
    __cpuid((int *)registers, info);

    *eax = registers[0];
    *ebx = registers[1];
    *ecx = registers[2];
    *edx = registers[3];
#else
    __cpuid(info, *eax, *ebx, *ecx, *edx);
#endif
}

static void cpuidex(int info, int subinfo, unsigned* eax, unsigned* ebx, unsigned* ecx, unsigned* edx) {
#ifdef _MSC_VER
    unsigned int registers[4];
    __cpuidex((int *)registers, info, subinfo);

    *eax = registers[0];
    *ebx = registers[1];
    *ecx = registers[2];
    *edx = registers[3];
#else
    __cpuid_count(info, subinfo, *eax, *ebx, *ecx, *edx);
#endif
}

void Z_INTERNAL x86_check_features(void) {
    unsigned eax, ebx, ecx, edx;
    unsigned maxbasic;
    unsigned family, model, extended_model;
    int intel_cpu;
    char cpu_identity[13];

    cpuid(0, &maxbasic, &ebx, &ecx, &edx);

    /* NULL terminate the string */
    memset(cpu_identity, 0, 13);
    memcpy(cpu_identity, (char*)&ebx, sizeof(int));
    memcpy(cpu_identity + 4, (char*)&edx, sizeof(int));
    memcpy(cpu_identity + 8, (char*)&ecx, sizeof(int));

    intel_cpu = strncmp(cpu_identity, "GenuineIntel", 12) == 0;

    cpuid(1 /*CPU_PROCINFO_AND_FEATUREBITS*/, &eax, &ebx, &ecx, &edx);

    x86_cpu_has_sse2 = edx & 0x4000000;
    x86_cpu_has_ssse3 = ecx & 0x200;
    x86_cpu_has_sse41 = ecx & 0x80000;
    x86_cpu_has_sse42 = ecx & 0x100000;
    x86_cpu_has_pclmulqdq = ecx & 0x2;
    x86_cpu_well_suited_avx512 = 0;

    model = (eax & 0xf0) >> 4;
    family = (eax & 0xf00) >> 8;
    extended_model = (eax & 0xf0000) >> 16;

    if (maxbasic >= 7) {
        cpuidex(7, 0, &eax, &ebx, &ecx, &edx);

        // check BMI1 bit
        // Reference: https://software.intel.com/sites/default/files/article/405250/how-to-detect-new-instruction-support-in-the-4th-generation-intel-core-processor-family.pdf
        x86_cpu_has_tzcnt = ebx & 0x8;
        // check AVX2 bit
        x86_cpu_has_avx2 = ebx & 0x20;
        x86_cpu_has_avx512 = ebx & 0x00010000;
        x86_cpu_has_avx512vnni = ecx & 0x800;
        x86_cpu_has_vpclmulqdq = ecx & 0x400;
    } else {
        x86_cpu_has_tzcnt = 0;
        x86_cpu_has_avx2 = 0;
        x86_cpu_has_vpclmulqdq = 0;
    }


    if (intel_cpu) {
        /* All of the Knights Landing and Knights Ferry _likely_ benefit
         * from the AVX512 adler checksum implementation */
        if (family == 0xb) {
            x86_cpu_well_suited_avx512 = 1;
        } else if (family == 0x6) {
            if (model == 0x5 && extended_model == 0x5) {
                /* Experimentally, on skylake-x and cascadelake-x, it has been
                 * unwaiveringly faster to use avx512 and avx512 vnni */
                x86_cpu_well_suited_avx512 = 1;
            } else if (model == 0xa && extended_model == 0x6) {
                /* Icelake server */
                x86_cpu_well_suited_avx512 = 1; 
            } else if (model == 0xf && extended_model == 0x8) {
                /* Saphire rapids */
                x86_cpu_well_suited_avx512 = 1;
            }

            /* Still need to check whether Rocket Lake and/or AlderLake
             * benefit from the AVX512VNNI accelerated adler32 implementations.
             * For now this working list is probably safe */
        }
    }

}
