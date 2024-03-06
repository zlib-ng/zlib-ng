/* benchmark_update_hash.cc -- benchmark update_hash variants
 * Copyright (C) 2024 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdio.h>
#include <assert.h>

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil_p.h"
#  include "deflate.h"
#  include "../test_cpu_features.h"
}

#define MAX_RANDOM_INTS (32 * 1024)
#define MAX_RANDOM_INTS_SIZE (MAX_RANDOM_INTS * sizeof(uint32_t))

typedef uint32_t (*update_hash_func)(uint32_t h, uint32_t val);

class update_hash_variant: public benchmark::Fixture {
private:
    uint32_t *random_ints;

public:
    void SetUp(const ::benchmark::State& state) {
        random_ints = (uint32_t *)zng_alloc(MAX_RANDOM_INTS_SIZE);
        assert(random_ints != NULL);

        for (int32_t i = 0; i < MAX_RANDOM_INTS; i++) {
            random_ints[i] = rand();
        }
    }

    void Bench(benchmark::State& state, update_hash_func update_hash) {
        const char *source_start = reinterpret_cast<const char *>(random_ints);
        const char *source_end = source_start + MAX_RANDOM_INTS_SIZE;

        for (auto _ : state) {
            uint32_t hash = 0;
            const char *source = source_start;
            while (source < source_end) {
                hash = update_hash(hash, *source++);
                hash = update_hash(hash, *source++);
                hash = update_hash(hash, *source++);
                hash = update_hash(hash, *source++);
            }

            benchmark::DoNotOptimize(hash);
        }
    }

    void TearDown(const ::benchmark::State& state) {
        zng_free(random_ints);
    }
};

#define BENCHMARK_UPDATE_HASH(name, fptr, support_flag) \
    BENCHMARK_DEFINE_F(update_hash_variant, name)(benchmark::State& state) { \
        if (!support_flag) { \
            state.SkipWithError("CPU does not support " #name); \
        } \
        Bench(state, fptr); \
    } \
    BENCHMARK_REGISTER_F(update_hash_variant, name)->Arg(MAX_RANDOM_INTS);

BENCHMARK_UPDATE_HASH(knuth, update_hash, 1);
