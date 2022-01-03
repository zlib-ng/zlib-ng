/* test_compress.c - Test compress() and uncompress() using hello world string */

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
    uint8_t compr[128], uncompr[128];
    z_size_t compr_len = sizeof(compr), uncompr_len = sizeof(uncompr);
    int err;

    err = PREFIX(compress)(compr, &compr_len, (const unsigned char *)hello, hello_len);
    CHECK_ERR(err, "compress");

    strcpy((char*)uncompr, "garbage");

    err = PREFIX(uncompress)(uncompr, &uncompr_len, compr, compr_len);
    CHECK_ERR(err, "uncompress");

    if (strcmp((char*)uncompr, hello))
        error("bad uncompress\n");
    else
        printf("uncompress(): %s\n", (char *)uncompr);

    return 0;
}
