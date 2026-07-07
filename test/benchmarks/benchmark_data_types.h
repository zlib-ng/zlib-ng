/* benchmark_data_types.h -- runtime registration of data-type benchmark variants
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Hooks registered at static initialization time are invoked from main()
 * with a bitmask of the data types selected by --benchmark_data_types.
 */
#ifndef BENCHMARK_DATA_TYPES_H
#define BENCHMARK_DATA_TYPES_H

#include <stdint.h>

/* Store a registration hook, called by benchmark_data_types_register() */
int benchmark_data_types_hook(void (*fn)(uint32_t mask));

/* Parse a comma-separated list of data type names, or "all".
   NULL or empty selects the default (text). Returns 0 on unknown name. */
uint32_t benchmark_data_types_parse(const char *list);

/* Invoke all registered hooks with the given data type mask */
void benchmark_data_types_register(uint32_t mask);

#endif
