/* benchmark_main.cc -- benchmark suite main entry point
 * Copyright (C) 2022 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdio.h>

#include <benchmark/benchmark.h>

#ifndef BUILD_ALT
extern "C" {
#  include "zbuild.h"
#  include "cpu_features.h"
}
#endif

int main(int argc, char** argv) {
#ifndef BUILD_ALT
    cpu_check_features();
#endif

    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();

    return EXIT_SUCCESS;
}
