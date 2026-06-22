/* mips_natives.h -- MIPS compile-time feature detection macros.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef MIPS_NATIVES_H_
#define MIPS_NATIVES_H_

#if defined(__mips_msa)
#  ifdef MIPS_MSA
#    define MIPS_MSA_NATIVE
#  endif
#endif

#endif /* MIPS_NATIVES_H_ */
