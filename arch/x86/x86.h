/* cpu.h -- check for CPU features
* Copyright (C) 2013 Intel Corporation Jim Kukunas
* For conditions of distribution and use, see copyright notice in zlib.h
*/

#ifndef CPU_H_
#define CPU_H_

/*
 A hack that helps AppleClang linker to see x86_cpu_has_* flags.
 A single call to dummy_linker_glue_y() in the compilation unit that reads
 x86_cpu_has_* flags will resolve "undefined symbol" link error.
*/
void dummy_linker_glue_y();

extern int x86_cpu_has_avx2;
extern int x86_cpu_has_sse2;
extern int x86_cpu_has_ssse3;
extern int x86_cpu_has_sse42;
extern int x86_cpu_has_pclmulqdq;
extern int x86_cpu_has_tzcnt;

#endif /* CPU_H_ */
