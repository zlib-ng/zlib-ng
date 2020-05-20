/* Copyright (C) 1995-2011, 2016 Mark Adler
 * Copyright (C) 2017 ARM Holdings Inc.
 * Author: Adenilson Cavalcanti <adenilson.cavalcanti@arm.com>
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */
#ifndef __ADLER32_NEON__
#define __ADLER32_NEON__

#if defined(__ARM_NEON__) || defined(__ARM_NEON)
// Depending on the compiler flavor, size_t may be defined in one or the other header. See:
// http://stackoverflow.com/questions/26410466/gcc-linaro-compiler-throws-error-unknown-type-name-size-t
#include <stdint.h>
#include <stddef.h>
uint32_t adler32_neon(uint32_t adler, const unsigned char *buf, size_t len);
#endif
#endif
