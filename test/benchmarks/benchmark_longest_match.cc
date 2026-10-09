/* benchmark_longest_match.cc -- microbenchmark for longest_match variants
 * Copyright (C) 2026
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Isolates the hash-chain match search from the rest of deflate:
 * the window is filled with test data, every position is inserted into the
 * hash table (recording the chain head each position would have seen), and
 * the timed phase replays longest_match() over that recorded history.
 *
 * Every compiled-in implementation of the match search is benchmarked in its
 * own family (gated at runtime on CPU support) so that any variant can be
 * exercised on any CPU, not just the fastest one - e.g. SSE2 code on AVX-512
 * hardware:
 *
 *   longest_match_<impl>/<data>/2|4|6        non-SLOW search
 *   longest_match_slow_knuth_<impl>/<data>/8 SLOW Knuth variant
 *   longest_match_slow_roll_<impl>/<data>/9  SLOW rolling-hash variant
 *
 * The parameter sets mirror entries of configuration_table in deflate.c
 * instead of reading the live level: levels 2, 4 and 6 drive the non-SLOW
 * search, level 8 drives the SLOW Knuth variant and level 9 the SLOW
 * rolling-hash variant (MIN_ROLL_LEVEL). The mimicked level is stored in the
 * deflate state so that level-dependent template behavior (e.g. early exit
 * below EARLY_EXIT_TRIGGER_LEVEL) matches the real strategy of that level.
 *
 * Note: inserting every position yields longer chains than the sparse
 * insertion policy of the real strategies, so absolute times overstate real
 * cost; the harness is consistent across code variants, which is what matters
 * for A/B testing.
 */

#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "deflate.h"
#  include "arch_functions.h"
#  include "../test_cpu_features.h"
#  include "insert_string_p.h"
#  include "test/test_data_p.h"
}

#include "benchmark_data_types.h"

#define LM_WSIZE (32768u)
#define LM_WINDOW_SIZE (LM_WSIZE * 2)
/* Positions must leave STD_MAX_MATCH+ bytes of valid lookahead. */
#define LM_FILL (LM_WINDOW_SIZE - MIN_LOOKAHEAD)
/* Start the timed sweep with a full window of history behind it. */
#define LM_SWEEP_START (LM_WSIZE)

/* Match search parameter sets mirroring configuration_table in deflate.c:
 * {level, good_match, nice_match, max_chain_length}. */
typedef struct {
    int level;
    uint32_t good_match;
    uint32_t nice_match;
    uint32_t max_chain_length;
} lm_params;

static const lm_params LM_PARAMS_LEVEL_2 = { 2,   4,    8,     4 }; /* deflate_fast */
static const lm_params LM_PARAMS_LEVEL_4 = { 4,   4,   32,    24 }; /* deflate_medium */
static const lm_params LM_PARAMS_LEVEL_6 = { 6,   8,  128,   128 }; /* deflate_medium_fizzle */
static const lm_params LM_PARAMS_LEVEL_8 = { 8,  32,  258,  1024 }; /* deflate_slow, Knuth hash */
static const lm_params LM_PARAMS_LEVEL_9 = { 9,  32,  258,  4096 }; /* deflate_slow, rolling hash */

/* Which match search function to replay. */
enum lm_variant {
    LM_NON_SLOW,   /* levels 2, 4 and 6 */
    LM_SLOW_KNUTH, /* level 8 */
    LM_SLOW_ROLL   /* level 9 */
};

static const lm_params *lm_params_for_level(int level) {
    switch (level) {
    case 2: return &LM_PARAMS_LEVEL_2;
    case 4: return &LM_PARAMS_LEVEL_4;
    case 6: return &LM_PARAMS_LEVEL_6;
    case 8: return &LM_PARAMS_LEVEL_8;
    case 9: return &LM_PARAMS_LEVEL_9;
    default: return NULL;
    }
}

class longest_match_bench: public benchmark::Fixture {
private:
    deflate_state *s = nullptr;
    Pos *heads = nullptr;   /* chain head visible at each position during setup */
    uint8_t *data = nullptr;

public:
    void SetUp(::benchmark::State&) {}

    /* Allocate the deflate state and window, fill the window with generated
     * data, and record the chain head each position sees during insertion. */
    void DoSetUp(benchmark::State& state, enum test_data_type data_type, enum lm_variant variant) {
        s = (deflate_state *)malloc(sizeof(deflate_state));
        heads = (Pos *)malloc(LM_FILL * sizeof(Pos));
        if (s == NULL || heads == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }
        memset(s, 0, sizeof(*s));
        s->w_size = LM_WSIZE;
        s->window_size = LM_WINDOW_SIZE;
        s->window = (uint8_t *)malloc(LM_WINDOW_SIZE);
        s->head = (Pos *)malloc(HASH_SIZE * sizeof(Pos));
        s->prev = (Pos *)malloc(LM_WSIZE * sizeof(Pos));
        if (s->window == NULL || s->head == NULL || s->prev == NULL) {
            state.SkipWithError("malloc failed");
            return;
        }
        memset(s->head, 0, HASH_SIZE * sizeof(Pos));

        data = gen_test_data(data_type, LM_FILL + STD_MAX_MATCH);
        if (data == NULL) {
            state.SkipWithError("gen_test_data failed");
            return;
        }
        /* Copy the generated lookahead tail as well so that hash insertion
         * and match comparisons near LM_FILL never read uninitialized bytes. */
        memcpy(s->window, data, LM_FILL + STD_MAX_MATCH);
        memset(s->window + LM_FILL + STD_MAX_MATCH, 0,
               LM_WINDOW_SIZE - (LM_FILL + STD_MAX_MATCH));

        /* Prime the rolling hash from the first two bytes, as deflate_slow
         * does when it first sees input. */
        if (variant == LM_SLOW_ROLL)
            s->ins_h = update_hash_roll(s->window[0], s->window[1]);


        /* Fill the hash table with the batch inserters, which perform the
         * same per-position updates as a sequential pass (the SLOW
         * rolling-hash variant needs the table built with insert_roll, the
         * same function deflate_slow uses at level 9). After the full
         * insertion, prev[p & W_MASK] still holds the chain head that
         * position p saw when it was inserted: a slot is only clobbered by
         * position p + WSIZE, and every sweep position has
         * p + WSIZE > LM_FILL. */
        if (variant == LM_SLOW_ROLL)
            insert_roll_batch_static(s, s->window, 0, LM_FILL);
        else
            insert_knuth_batch_static(s, s->window, 0, LM_FILL);

        for (uint32_t p = LM_SWEEP_START; p < LM_FILL; p++)
            heads[p] = s->prev[p & W_MASK(s)];
    }

    /* Replay longest_match over the recorded chain heads and report the
     * average match length per position. */
    void Bench(benchmark::State& state, enum test_data_type data_type, enum lm_variant variant, longest_match_func longest_match) {
        int level = (int)state.range(0);
        const lm_params *params = lm_params_for_level(level);
        if (params == NULL) {
            state.SkipWithError("unknown benchmark level");
            return;
        }

        DoSetUp(state, data_type, variant);
        if (state.skipped())
            return;

        s->level = params->level;
        s->good_match = params->good_match;
        s->nice_match = params->nice_match;
        s->max_chain_length = params->max_chain_length;
        s->prev_length = 0;   /* no pending match from a previous position */

        const uint32_t sweep_end = LM_FILL;
        uint32_t positions = sweep_end - LM_SWEEP_START;
        uint64_t total_len = 0;

        for (auto _ : state) {
            total_len = 0;
            for (uint32_t p = LM_SWEEP_START; p < sweep_end; p++) {
                s->strstart = p;
                s->lookahead = sweep_end - p;
                total_len += longest_match(s, heads[p]);
            }
            benchmark::DoNotOptimize(total_len);
        }

        /* Match-search throughput. Marked as an iteration-invariant rate so
         * gbench multiplies by the iteration count and divides by duration. */
        state.counters["Items/sec"] =
            benchmark::Counter((double)positions, benchmark::Counter::kIsIterationInvariantRate);

        state.counters["avg_len"] = (double)total_len / (double)positions;
    }

    void TearDown(const ::benchmark::State&) {
        if (s != NULL) {
            free(s->window);
            free(s->head);
            free(s->prev);
            free(s);
        }
        free(heads);
        free(data);
        s = nullptr;
        heads = nullptr;
        data = nullptr;
    }
};

/* One benchmark class per (search variant, implementation, data type). The
 * support flag gates the benchmark at runtime on CPU features. */
#define LM_DEFINE_BENCH(family, impl, data, dt, variant, func, support) \
    BENCHMARK_DEFINE_F(longest_match_bench, family##_##impl##_##data)(benchmark::State& state) { \
        if (!(support)) { \
            state.SkipWithError("CPU does not support " #impl); \
            return; \
        } \
        Bench(state, dt, variant, func); \
    }

#define LM_DEFINE_DATA(family, impl, variant, func, support) \
    LM_DEFINE_BENCH(family, impl, text,          TEST_DATA_TEXT,          variant, func, support); \
    LM_DEFINE_BENCH(family, impl, short_match,   TEST_DATA_SHORT_MATCH,   variant, func, support); \
    LM_DEFINE_BENCH(family, impl, dna,           TEST_DATA_DNA,           variant, func, support); \
    LM_DEFINE_BENCH(family, impl, random,        TEST_DATA_RANDOM,        variant, func, support); \
    LM_DEFINE_BENCH(family, impl, literals,      TEST_DATA_LITERALS,      variant, func, support); \
    LM_DEFINE_BENCH(family, impl, mixed,         TEST_DATA_MIXED,         variant, func, support); \
    LM_DEFINE_BENCH(family, impl, realistic_rgb, TEST_DATA_REALISTIC_RGB, variant, func, support); \
    LM_DEFINE_BENCH(family, impl, striped_rgb,   TEST_DATA_STRIPED_RGB,   variant, func, support); \
    LM_DEFINE_BENCH(family, impl, logfile,       TEST_DATA_LOGFILE,       variant, func, support)

#define LM_DEFINE_VARIANT(family, impl, variant, func, support) \
    LM_DEFINE_DATA(family, impl, variant, func, support)

/* Define all three search-variant families for one implementation. */
#define LM_DEFINE_ALL(impl, func_non_slow, func_knuth, func_roll, support) \
    LM_DEFINE_VARIANT(non_slow,   impl, LM_NON_SLOW,   func_non_slow, support); \
    LM_DEFINE_VARIANT(slow_knuth, impl, LM_SLOW_KNUTH, func_knuth,    support); \
    LM_DEFINE_VARIANT(slow_roll,  impl, LM_SLOW_ROLL,  func_roll,     support)

/* Registration mirrors the #ifdef guards above; keep the two in sync. The
 * benchmark name prefix differs per search variant (the non-SLOW family is
 * plain "longest_match_<impl>"). */
#define LM_REGISTER_BENCH(name_prefix, cls_family, impl, data, dt, args) \
    if (mask & (1u << (dt))) \
        ::benchmark::internal::RegisterBenchmarkInternal( \
            ::benchmark::internal::make_unique<longest_match_bench_##cls_family##_##impl##_##data##_Benchmark>()) \
            ->Name(name_prefix "/" #data) args

#define LM_REGISTER_DATA(name_prefix, cls_family, impl, args) \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, text,          TEST_DATA_TEXT,          args); \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, short_match,   TEST_DATA_SHORT_MATCH,   args); \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, dna,           TEST_DATA_DNA,           args); \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, random,        TEST_DATA_RANDOM,        args); \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, literals,      TEST_DATA_LITERALS,      args); \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, mixed,         TEST_DATA_MIXED,         args); \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, realistic_rgb, TEST_DATA_REALISTIC_RGB, args); \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, striped_rgb,   TEST_DATA_STRIPED_RGB,   args); \
    LM_REGISTER_BENCH(name_prefix, cls_family, impl, logfile,       TEST_DATA_LOGFILE,       args)

#define LM_REGISTER_ALL(impl) \
    LM_REGISTER_DATA("longest_match_" #impl,                non_slow,   impl, ->Args({2})->Args({4})->Args({6})); \
    LM_REGISTER_DATA("longest_match_slow_knuth_" #impl,     slow_knuth, impl, ->Args({8})); \
    LM_REGISTER_DATA("longest_match_slow_roll_" #impl,      slow_roll,  impl, ->Args({9}))

#ifdef COMPARE256_FALLBACK
LM_DEFINE_ALL(c, longest_match_c, longest_match_slow_knuth_c, longest_match_slow_roll_c, 1)
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
/* Native build: one family for the compile-time selected implementation. */
LM_DEFINE_ALL(native, native_longest_match, native_longest_match_slow_knuth, native_longest_match_slow_roll, 1)
#else

#ifdef ARM_NEON
LM_DEFINE_ALL(neon, longest_match_neon, longest_match_slow_knuth_neon, longest_match_slow_roll_neon, test_cpu_features.arm.has_neon)
#endif

#ifdef LOONGARCH_LSX
LM_DEFINE_ALL(lsx, longest_match_lsx, longest_match_slow_knuth_lsx, longest_match_slow_roll_lsx, test_cpu_features.loongarch.has_lsx)
#endif
#ifdef LOONGARCH_LASX
LM_DEFINE_ALL(lasx, longest_match_lasx, longest_match_slow_knuth_lasx, longest_match_slow_roll_lasx, test_cpu_features.loongarch.has_lasx)
#endif

#ifdef X86_SSE2
LM_DEFINE_ALL(sse2, longest_match_sse2, longest_match_slow_knuth_sse2, longest_match_slow_roll_sse2, test_cpu_features.x86.has_sse2)
#endif
#ifdef X86_AVX2
LM_DEFINE_ALL(avx2, longest_match_avx2, longest_match_slow_knuth_avx2, longest_match_slow_roll_avx2, test_cpu_features.x86.has_avx2)
#endif
#ifdef X86_AVX512
LM_DEFINE_ALL(avx512, longest_match_avx512, longest_match_slow_knuth_avx512, longest_match_slow_roll_avx512, test_cpu_features.x86.has_avx512_common)
#endif

#ifdef POWER9
LM_DEFINE_ALL(power9, longest_match_power9, longest_match_slow_knuth_power9, longest_match_slow_roll_power9, test_cpu_features.power.has_arch_3_00)
#endif

#ifdef RISCV_RVV
LM_DEFINE_ALL(rvv, longest_match_rvv, longest_match_slow_knuth_rvv, longest_match_slow_roll_rvv, test_cpu_features.riscv.has_rvv)
#endif

#endif

static void longest_match_register_data_types(uint32_t mask) {
    /* Configurations without any compiled family (e.g. an arch whose SIMD
     * variants are all gated off) still need the parameter marked used. */
    Z_UNUSED(mask);

#ifdef COMPARE256_FALLBACK
    LM_REGISTER_ALL(c);
#endif

#ifdef DISABLE_RUNTIME_CPU_DETECTION
    LM_REGISTER_ALL(native);
#else

#ifdef ARM_NEON
    LM_REGISTER_ALL(neon);
#endif

#ifdef LOONGARCH_LSX
    LM_REGISTER_ALL(lsx);
#endif
#ifdef LOONGARCH_LASX
    LM_REGISTER_ALL(lasx);
#endif

#ifdef X86_SSE2
    LM_REGISTER_ALL(sse2);
#endif
#ifdef X86_AVX2
    LM_REGISTER_ALL(avx2);
#endif
#ifdef X86_AVX512
    LM_REGISTER_ALL(avx512);
#endif

#ifdef POWER9
    LM_REGISTER_ALL(power9);
#endif

#ifdef RISCV_RVV
    LM_REGISTER_ALL(rvv);
#endif

#endif
}

static int longest_match_data_types = benchmark_data_types_hook(longest_match_register_data_types);
