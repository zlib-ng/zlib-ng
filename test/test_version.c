/* test_version.c - Test zVersion() and zlibCompileFlags() */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_shared.h"

int main() {
    static const char *my_version = PREFIX2(VERSION);

    if (zVersion()[0] != my_version[0]) {
        error("incompatible zlib version\n");
    } else if (strcmp(zVersion(), PREFIX2(VERSION)) != 0) {
        fprintf(stderr, "warning: different zlib version\n");
    }

    printf("zlib-ng version %s = 0x%08lx, compile flags = 0x%lx\n",
            ZLIBNG_VERSION, ZLIBNG_VERNUM, PREFIX(zlibCompileFlags)());
    return 0;
}
