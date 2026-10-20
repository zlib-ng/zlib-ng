/* dfltcc_common.c - Runtime control for IBM Z DFLTCC acceleration. */

#include "zbuild.h"
#include "dfltcc_common.h"

#include <stdlib.h>
#include <string.h>

int Z_INTERNAL PREFIX(dfltcc_enabled);

void Z_INTERNAL PREFIX(dfltcc_init_env)(void) {
    const char *value;

#ifdef HAVE_SECURE_GETENV
    value = secure_getenv("DFLTCC");
#else
    value = getenv("DFLTCC");
#endif

    if (value == NULL)
        PREFIX(dfltcc_enabled) = DFLTCC_ENABLED;
    else
        PREFIX(dfltcc_enabled) = strcmp(value, "0") != 0;
}
