/* benchmark_corpora.cc -- benchmark deflate/inflate with real-world data
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Benchmarks against the zlib-ng/corpora repository which contains multiple
 * test corpora (silesia, calgary, canterbury, large, snappy, etc.).
 *
 * Clone the corpora repo:
 *   git clone https://github.com/zlib-ng/corpora test/data/corpora
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>
#include <benchmark/benchmark.h>

extern "C" {
#  include "zbuild.h"
#  include "zutil_p.h"
#  if defined(ZLIB_COMPAT)
#    include "zlib.h"
#  else
#    include "zlib-ng.h"
#  endif
}

#include "benchmark_corpora.h"

static std::vector<corpus_file> corpora_files;

class deflate_corpora : public benchmark::Fixture {
private:
    corpus_file *cf;
    int level;
    uint8_t *outbuff;
    z_uintmax_t outbuff_size;
    PREFIX3(stream) strm;

public:
    deflate_corpora(const std::string &name, corpus_file *cf, int level)
        : cf(cf), level(level), outbuff(NULL), outbuff_size(0), strm{} {
        this->SetName(name);
    }

    void SetUp(const benchmark::State &) override {
        if (!load_corpus_file(cf))
            return;

        outbuff_size = PREFIX(deflateBound)(NULL, (unsigned long)cf->size);
        outbuff = (uint8_t *)malloc(outbuff_size);
        if (outbuff == NULL)
            return;

        memset(&strm, 0, sizeof(strm));

        PREFIX(deflateInit2)(&strm, level, Z_DEFLATED, MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    }

    void BenchmarkCase(benchmark::State &state) override {
        int err;

        if (cf->data == NULL || outbuff == NULL) {
            state.SkipWithError("setup failed");
            return;
        }

        for (auto _ : state) {
            err = PREFIX(deflateReset)(&strm);
            if (err != Z_OK) {
                state.SkipWithError("deflateReset did not return Z_OK");
                break;
            }

            strm.avail_in = (uint32_t)cf->size;
            strm.next_in = (z_const uint8_t *)cf->data;
            strm.next_out = outbuff;
            strm.avail_out = (uint32_t)outbuff_size;

            err = PREFIX(deflate)(&strm, Z_FINISH);
            if (err != Z_STREAM_END) {
                state.SkipWithError("deflate did not return Z_STREAM_END");
                break;
            }
        }

        state.counters["compressed"] = benchmark::Counter(double(strm.total_out));
        state.counters["ratio"] = benchmark::Counter(double(cf->size) / double(strm.total_out));
    }

    void TearDown(const benchmark::State &) override {
        if (outbuff != NULL) {
            PREFIX(deflateEnd)(&strm);
            free(outbuff);
            outbuff = NULL;
        }
    }
};

class inflate_corpora : public benchmark::Fixture {
private:
    corpus_file *cf;
    uint8_t *compressed;
    size_t compressed_size;
    uint8_t *outbuff;
    PREFIX3(stream) strm;

public:
    inflate_corpora(const std::string &name, corpus_file *cf)
        : cf(cf), compressed(NULL), compressed_size(0), outbuff(NULL), strm{} {
        this->SetName(name);
    }

    void SetUp(const benchmark::State &) override {
        int err;

        if (!load_corpus_file(cf))
            return;

        outbuff = (uint8_t *)malloc(cf->size);

        /* Pre-compress the file for inflate benchmarking */
        z_uintmax_t comp_bound = PREFIX(deflateBound)(NULL, (unsigned long)cf->size);
        compressed = (uint8_t *)malloc(comp_bound);

        if (compressed != NULL) {
            PREFIX3(stream) dstrm;
            memset(&dstrm, 0, sizeof(dstrm));

            err = PREFIX(deflateInit2)(&dstrm, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS,
                                       MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
            if (err == Z_OK) {
                dstrm.avail_in = (uint32_t)cf->size;
                dstrm.next_in = (z_const uint8_t *)cf->data;
                dstrm.next_out = compressed;
                dstrm.avail_out = (uint32_t)comp_bound;

                err = PREFIX(deflate)(&dstrm, Z_FINISH);
                if (err == Z_STREAM_END)
                    compressed_size = dstrm.total_out;
                PREFIX(deflateEnd)(&dstrm);
            }
        }

        memset(&strm, 0, sizeof(strm));

        PREFIX(inflateInit2)(&strm, -MAX_WBITS);
    }

    void BenchmarkCase(benchmark::State &state) override {
        int err;

        if (compressed == NULL || outbuff == NULL) {
            state.SkipWithError("setup failed");
            return;
        }

        for (auto _ : state) {
            err = PREFIX(inflateReset)(&strm);
            if (err != Z_OK) {
                state.SkipWithError("inflateReset did not return Z_OK");
                break;
            }

            strm.avail_in = (uint32_t)compressed_size;
            strm.next_in = compressed;
            strm.avail_out = (uint32_t)cf->size;
            strm.next_out = outbuff;

            err = PREFIX(inflate)(&strm, Z_FINISH);
            if (err != Z_STREAM_END) {
                state.SkipWithError("inflate did not return Z_STREAM_END");
                break;
            }
        }

        state.counters["compressed"] = benchmark::Counter(double(strm.total_out));
        state.counters["ratio"] = benchmark::Counter(double(cf->size) / double(strm.total_out));
    }

    void TearDown(const benchmark::State &) override {
        if (compressed != NULL) {
            PREFIX(inflateEnd)(&strm);
            free(compressed);
            compressed = NULL;
        }
        free(outbuff);
        outbuff = NULL;
    }
};

/* Dynamic benchmark registration at static init time */
static int register_corpora_benchmarks(void) {
    corpora_files = discover_corpora();
    if (corpora_files.empty())
        return 0;

    int levels[] = {1, 3, 6, 8, 9};

    size_t prefix_len = strlen(CORPORA_DIR) + 1;

    for (size_t i = 0; i < corpora_files.size(); i++) {
        corpus_file *cf = &corpora_files[i];
        std::string label = cf->path.substr(prefix_len);
        std::replace(label.begin(), label.end(), '\\', '/');

        for (size_t l = 0; l < sizeof(levels) / sizeof(levels[0]); l++) {
            int level = levels[l];
            std::string name = "deflate_corpora/" + label + "/level:" + std::to_string(level);
            benchmark::internal::RegisterBenchmarkInternal(
                ::benchmark::internal::make_unique<deflate_corpora>(name, cf, level));
        }

        std::string name = "inflate_corpora/" + label;
        benchmark::internal::RegisterBenchmarkInternal(
            ::benchmark::internal::make_unique<inflate_corpora>(name, cf));
    }

    return 0;
}

static int corpora_init = register_corpora_benchmarks();
