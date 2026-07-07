/* benchmark_data_types.cc -- runtime registration of data-type benchmark variants
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test/test_data_p.h"
#include "benchmark_data_types.h"

#define MAX_HOOKS 8

static void (*hooks[MAX_HOOKS])(uint32_t);
static int num_hooks = 0;

int benchmark_data_types_hook(void (*fn)(uint32_t mask)) {
    if (num_hooks < MAX_HOOKS)
        hooks[num_hooks++] = fn;
    return num_hooks;
}

uint32_t benchmark_data_types_parse(const char *list) {
    if (list == NULL || *list == 0)
        return 1u << TEST_DATA_TEXT;
    if (strcmp(list, "all") == 0)
        return (1u << TEST_DATA_COUNT) - 1;

    uint32_t mask = 0;
    const char *p = list;
    while (*p != 0) {
        const char *end = strchr(p, ',');
        size_t len = end != NULL ? (size_t)(end - p) : strlen(p);
        int found = -1;
        for (int t = 0; t < TEST_DATA_COUNT; t++) {
            const char *name = test_data_type_name(t);
            if (strlen(name) == len && memcmp(name, p, len) == 0) {
                found = t;
                break;
            }
        }
        if (found < 0) {
            fprintf(stderr, "unknown data type '%.*s', available:", (int)len, p);
            for (int t = 0; t < TEST_DATA_COUNT; t++)
                fprintf(stderr, " %s", test_data_type_name(t));
            fprintf(stderr, ", all\n");
            return 0;
        }
        mask |= 1u << found;
        p = end != NULL ? end + 1 : p + len;
    }
    return mask;
}

void benchmark_data_types_register(uint32_t mask) {
    for (int i = 0; i < num_hooks; i++)
        hooks[i](mask);
}
