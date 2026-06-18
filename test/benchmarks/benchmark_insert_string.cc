/* benchmark_insert_string.cc -- benchmark insert_string variants
 * Copyright (C) 2025 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <limits.h>
#include <cstring>

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "deflate.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
#  include "insert_string_p.h"
}

#define MAX_WSIZE 32768
#define TEST_WINDOW_SIZE (MAX_WSIZE * 2)

typedef uint32_t (* insert_single_func)(deflate_state *const s, unsigned char *window, uint32_t str);

// Base class with common setup/teardown for both insert_string benchmarks
class insert_string_base: public benchmark::Fixture {
protected:
    deflate_state *s;
    unsigned char *window;

public:
    void SetUp(const ::benchmark::State&) {
        s = (deflate_state*)zng_alloc_aligned(sizeof(deflate_state), 64);
        memset(s, 0, sizeof(deflate_state));

        // Set up window parameters
        s->w_size = MAX_WSIZE;
        s->window_size = TEST_WINDOW_SIZE;

        // Allocate window
        s->window = (uint8_t*)zng_alloc_aligned(TEST_WINDOW_SIZE, 64);
        window = s->window;

        // Allocate hash tables
        s->head = (Pos*)zng_alloc_aligned(HASH_SIZE * sizeof(Pos), 64);
        s->prev = (Pos*)zng_alloc_aligned(MAX_WSIZE * sizeof(Pos), 64);

        // Initialize hash tables
        memset(s->head, 0, HASH_SIZE * sizeof(Pos));
        memset(s->prev, 0, MAX_WSIZE * sizeof(Pos));

        // Initialize rolling hash state for rolling variant
        s->ins_h = 0;

        // Fill window with deterministic data patterns
        for (size_t i = 0; i < TEST_WINDOW_SIZE; i++) {
            // Create patterns that will exercise the hash function well
            window[i] = (uint8_t)((i * 17 + (i >> 4) * 31 + (i >> 8) * 13) & 0xFF);
        }
    }

    void TearDown(const ::benchmark::State&) {
        zng_free_aligned(s->window);
        zng_free_aligned(s->head);
        zng_free_aligned(s->prev);
        zng_free_aligned(s);
    }
};

class insert_batch_bench: public insert_string_base {
public:
    void Bench(benchmark::State& state, insert_batch_func insert_batch) {
        uint32_t str_pos = (uint32_t)state.range(0);  // Starting position
        uint32_t count = (uint32_t)state.range(1);    // Number of strings to insert

        // Ensure we don't go beyond window bounds
        if (str_pos + count >= TEST_WINDOW_SIZE - 4) {
            state.SkipWithError("Parameters exceed window size");
            return;
        }

        for (auto _ : state) {
            state.PauseTiming();

            // Reset hash tables to ensure consistent starting state
            memset(s->head, 0, HASH_SIZE * sizeof(Pos));
            memset(s->prev, 0, MAX_WSIZE * sizeof(Pos));
            s->ins_h = 0;

            state.ResumeTiming();

            // Benchmark the batch insert function
            insert_batch(s, window, str_pos, count);
        }
    }
};

#define BENCHMARK_INSERT_BATCH(name, fptr, support_flag) \
    BENCHMARK_DEFINE_F(insert_batch_bench, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("Function " #name " not supported"); \
            return; \
        } \
        Bench(state, fptr); \
    } \
    BENCHMARK_REGISTER_F(insert_batch_bench, name) \
        ->Args({100, 3})        /* Most common case */ \
        ->Args({100, 4})        \
        ->Args({100, 5})        \
        ->Args({100, 7})        \
        ->Args({100, 14})       /* Mid-range cluster */ \
        ->Args({100, 32})       /* Transition point */ \
        ->Args({100, 127})      /* Large cluster around powers of 2 */ \
        ->Args({100, 255})      /* Near maximum observed values */ \
        ->Unit(benchmark::kNanosecond);

// Benchmark the Knuth multiplicative hash variant
BENCHMARK_INSERT_BATCH(knuth_hash, ::insert_knuth_batch, 1);

// Benchmark the rolling hash variant
BENCHMARK_INSERT_BATCH(roll_hash, ::insert_roll_batch, 1);

// Additional benchmark class for single insert functions
class insert_single_bench: public insert_string_base {
public:
    void Bench(benchmark::State& state, insert_single_func insert_single) {
        uint32_t start_pos = (uint32_t)state.range(0);  // Starting position
        uint32_t count = (uint32_t)state.range(1);      // Number of insertions

        if (start_pos + count >= TEST_WINDOW_SIZE - 4) {
            state.SkipWithError("Parameters exceed window size");
            return;
        }

        for (auto _ : state) {
            state.PauseTiming();

            // Reset hash tables
            memset(s->head, 0, HASH_SIZE * sizeof(Pos));
            memset(s->prev, 0, MAX_WSIZE * sizeof(Pos));
            s->ins_h = 0;

            state.ResumeTiming();

            // Benchmark single insertions
            for (uint32_t i = 0; i < count; i++) {
                uint32_t result = insert_single(s, window, start_pos + i);
                benchmark::DoNotOptimize(result);
            }
        }
    }
};

#define BENCHMARK_INSERT_SINGLE(name, fptr, support_flag) \
    BENCHMARK_DEFINE_F(insert_single_bench, name)(benchmark::State& state) { \
        if (!(support_flag)) { \
            state.SkipWithError("Function " #name " not supported"); \
            return; \
        } \
        Bench(state, fptr); \
    } \
    BENCHMARK_REGISTER_F(insert_single_bench, name) \
        ->Args({100, 1})        /* Single insertion (baseline) */ \
        ->Args({100, 100})      /* 100 insertions (measure amortized cost) */ \
        ->Args({16000, 100})    /* 100 insertions at mid window (different hash distribution) */ \
        ->Unit(benchmark::kNanosecond);

BENCHMARK_INSERT_SINGLE(knuth_hash, ::insert_knuth, 1);
BENCHMARK_INSERT_SINGLE(roll_hash, ::insert_roll, 1);
