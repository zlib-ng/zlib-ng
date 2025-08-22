/* benchmark_insert_string.cc -- benchmark insert_string variants
 * Copyright (C) 2025 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <limits.h>
#include <cstring>

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil_p.h"
#  include "deflate.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
}

#define MAX_WSIZE 32768
#define TEST_WINDOW_SIZE (MAX_WSIZE * 2)

// Base class with common setup/teardown for both insert_string benchmarks
class insert_string_base: public benchmark::Fixture {
protected:
    deflate_state *s;

public:
    void SetUp(const ::benchmark::State& state) {
        s = (deflate_state*)zng_alloc(sizeof(deflate_state));
        memset(s, 0, sizeof(deflate_state));

        // Set up window parameters
        s->w_size = MAX_WSIZE;
        s->w_bits = 15;
        s->w_mask = MAX_WSIZE - 1;
        s->window_size = TEST_WINDOW_SIZE;

        // Allocate window
        s->window = (uint8_t*)zng_alloc(TEST_WINDOW_SIZE);

        // Allocate hash tables
        s->head = (Pos*)zng_alloc(HASH_SIZE * sizeof(Pos));
        s->prev = (Pos*)zng_alloc(MAX_WSIZE * sizeof(Pos));

        // Initialize hash tables
        memset(s->head, 0, HASH_SIZE * sizeof(Pos));
        memset(s->prev, 0, MAX_WSIZE * sizeof(Pos));

        // Initialize rolling hash state for rolling variant
        s->ins_h = 0;

        // Fill window with deterministic data patterns
        for (size_t i = 0; i < TEST_WINDOW_SIZE; i++) {
            // Create patterns that will exercise the hash function well
            s->window[i] = (uint8_t)((i * 17 + (i >> 4) * 31 + (i >> 8) * 13) & 0xFF);
        }
    }

    void TearDown(const ::benchmark::State& state) {
        zng_free(s->window);
        zng_free(s->head);
        zng_free(s->prev);
        zng_free(s);
    }
};

class insert_string_bench: public insert_string_base {
public:
    void Bench(benchmark::State& state, insert_string_cb insert_func) {
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

            // Benchmark the insert_string function
            insert_func(s, str_pos, count);
        }
    }
};

#define BENCHMARK_INSERT_STRING(name, fptr, support_flag) \
    BENCHMARK_DEFINE_F(insert_string_bench, name)(benchmark::State& state) { \
        if (!support_flag) { \
            state.SkipWithError("Function " #name " not supported"); \
        } \
        Bench(state, fptr); \
    } \
    BENCHMARK_REGISTER_F(insert_string_bench, name) \
        ->Args({100, 3})        /* Most common case */ \
        ->Args({100, 4})        \
        ->Args({100, 5})        \
        ->Args({100, 7})        \
        ->Args({100, 14})       /* Mid-range cluster */ \
        ->Args({100, 32})       /* Transition point */ \
        ->Args({100, 127})      /* Large cluster around powers of 2 */ \
        ->Args({100, 255})      /* Near maximum observed values */ \
        ->Unit(benchmark::kNanosecond);

// Benchmark the standard integer hash variant
BENCHMARK_INSERT_STRING(integer_hash, ::insert_string, 1);

// Benchmark the rolling hash variant
BENCHMARK_INSERT_STRING(rolling_hash, ::insert_string_roll, 1);

// Additional benchmark class for quick_insert_string functions
class quick_insert_string_bench: public insert_string_base {
public:
    void Bench(benchmark::State& state, quick_insert_string_cb quick_insert_func) {
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

            // Benchmark quick_insert_string (single insertions)
            for (uint32_t i = 0; i < count; i++) {
                Pos result = quick_insert_func(s, start_pos + i);
                benchmark::DoNotOptimize(result);
            }
        }
    }
};

#define BENCHMARK_QUICK_INSERT_STRING(name, fptr, support_flag) \
    BENCHMARK_DEFINE_F(quick_insert_string_bench, name)(benchmark::State& state) { \
        if (!support_flag) { \
            state.SkipWithError("Function " #name " not supported"); \
        } \
        Bench(state, fptr); \
    } \
    BENCHMARK_REGISTER_F(quick_insert_string_bench, name) \
        ->Args({100, 1})        /* Single insertion (baseline) */ \
        ->Args({100, 100})      /* 100 insertions (measure amortized cost) */ \
        ->Args({16000, 100})    /* 100 insertions at mid window (different hash distribution) */ \
        ->Unit(benchmark::kNanosecond);

BENCHMARK_QUICK_INSERT_STRING(quick_integer_hash, ::quick_insert_string, 1);
BENCHMARK_QUICK_INSERT_STRING(quick_rolling_hash, ::quick_insert_string_roll, 1);
