#include <stdio.h>
#ifdef CONSUMER_ZLIB_COMPAT
#  include "zlib.h"
#  define VERSION_FUNC zlibVersion
#else
#  include "zlib-ng.h"
#  define VERSION_FUNC zlibng_version
#endif

int main(void) {
    printf("zlib-ng: %s\n", VERSION_FUNC());
    return 0;
}
