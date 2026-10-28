/* dfltcc_common.c - Runtime control for IBM Z DFLTCC acceleration.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifdef HAVE_SECURE_GETENV
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#endif

#include "zbuild.h"
#include "dfltcc_common.h"

#include <stdlib.h>
#include <string.h>

int Z_INTERNAL PREFIX(dfltcc_enabled) = DFLTCC_ENABLED;

#ifdef DFLTCC_ENV
static void __attribute__((constructor)) dfltcc_init_env(void) {
    const char *value;

#  ifdef HAVE_SECURE_GETENV
    value = secure_getenv("DFLTCC");
#  else
    value = getenv("DFLTCC");
#  endif

    if (value == NULL)
        PREFIX(dfltcc_enabled) = DFLTCC_ENABLED;
    else
        PREFIX(dfltcc_enabled) = strcmp(value, "0") != 0;
}
#endif
