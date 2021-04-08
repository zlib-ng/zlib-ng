/* arm.h -- check for ARM features.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ARM_H_
#define ARM_H_

/*
 A hack that helps AppleClang linker to see arm_cpu_has_* flags.
 A single call to dummy_linker_glue() in the compilation unit that reads
 arm_cpu_has_* flags will resolve "undefined symbol" link error.
*/
void dummy_linker_glue();

extern int arm_cpu_has_neon;
extern int arm_cpu_has_crc32;

void Z_INTERNAL arm_check_features(void);

#endif /* ARM_H_ */
