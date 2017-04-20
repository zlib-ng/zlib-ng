/* Copyright (C) 1995-2011, 2016 Mark Adler
 * Copyright (C) 2017 ARM Holdings Inc.
 * Author: Adenilson Cavalcanti <adenilson.cavalcanti@arm.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not
 *  claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */
#ifndef __NEON_ADLER32__
#define __NEON_ADLER32__

#if (defined(__ARM_NEON__) || defined(__ARM_NEON))
#include <stdint.h>
uint32_t adler32_neon(uint32_t adler, const unsigned char *buf,
                      size_t len);
#endif
#endif
