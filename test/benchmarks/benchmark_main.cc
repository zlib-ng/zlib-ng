/* benchmark_main.cc -- benchmark suite main entry point
 * Copyright (C) 2022 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdint.h>
#include <stdio.h>
#include <stdint.h>

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil.h"
#  include "cpu_features.h"
}

int main(int argc, char** argv) {
    cpu_check_features();

    ::benchmark::Initialize(&argc, argv);
    ::benchmark::RunSpecifiedBenchmarks();

    return EXIT_SUCCESS;
}
