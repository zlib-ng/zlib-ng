/* detect-arch.c -- Detect compiler architecture and print to stderr
 *                  containing a simple arch identifier.
 * Copyright (C) 2019 Hans Kristian Rosbach
 * Licensed under the Zlib license, see LICENSE.md for details
 */

#include <stdio.h>

#include "../zarch.h"

int main(void) {
    fprintf(stderr, "archfound " ARCH_NAME "\n");
#ifdef ARCH_VERSION_STR
    fprintf(stderr, "archversion " ARCH_VERSION_STR "\n");
#endif
    return 0;
}
