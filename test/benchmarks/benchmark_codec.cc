/* benchmark_codec.cc -- whole-buffer deflate benchmarks across implementations
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Compresses and decompresses corpus files through a minimal whole-buffer
 * codec interface so identical benchmark names can be produced for different
 * deflate implementations and compared with compare_runs.py.
 *
 * The backend is selected at compile time, zlib-ng by default, libdeflate
 * when BENCH_LIBDEFLATE is defined. Decompression input is always produced
 * by zlib-ng at level 9, both backends therefore inflate identical streams.
 * All benchmark output is verified against the original file contents.
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
#  if defined(ZLIB_COMPAT)
#    include "zlib.h"
#  else
#    include "zlib-ng.h"
#  endif
#  include "test/test_data_p.h"
}

#ifdef BENCH_LIBDEFLATE
#  include <libdeflate.h>
#endif

#include "benchmark_corpora.h"
#include "benchmark_data_types.h"

static std::vector<corpus_file> corpora_files;

#ifdef BENCH_LIBDEFLATE

/* libdeflate backend, reusable codec objects around the whole-buffer API */
struct codec_compressor {
    struct libdeflate_compressor *comp;

    bool init(int level) {
        comp = libdeflate_alloc_compressor(level);
        return comp != NULL;
    }

    size_t bound(size_t in_size) {
        return libdeflate_deflate_compress_bound(comp, in_size);
    }

    /* Returns compressed size, 0 on failure */
    size_t compress(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size) {
        return libdeflate_deflate_compress(comp, in, in_size, out, out_size);
    }

    void end() {
        libdeflate_free_compressor(comp);
    }
};

struct codec_decompressor {
    struct libdeflate_decompressor *decomp;

    bool init() {
        decomp = libdeflate_alloc_decompressor();
        return decomp != NULL;
    }

    /* Returns decompressed size, 0 on failure */
    size_t decompress(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size) {
        size_t actual = 0;
        if (libdeflate_deflate_decompress(decomp, in, in_size, out, out_size, &actual) != LIBDEFLATE_SUCCESS)
            return 0;
        return actual;
    }

    void end() {
        libdeflate_free_decompressor(decomp);
    }
};

static const int codec_levels[] = {1, 3, 6, 9, 12};

#else

/* zlib-ng backend, one-shot raw deflate calls on a persistent stream */
struct codec_compressor {
    PREFIX3(stream) strm;

    bool init(int level) {
        memset(&strm, 0, sizeof(strm));
        return PREFIX(deflateInit2)(&strm, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL,
                                    Z_DEFAULT_STRATEGY) == Z_OK;
    }

    size_t bound(size_t in_size) {
        return (size_t)PREFIX(deflateBound)(&strm, (z_uintmax_t)in_size);
    }

    /* Returns compressed size, 0 on failure */
    size_t compress(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size) {
        if (PREFIX(deflateReset)(&strm) != Z_OK)
            return 0;

        strm.next_in = (z_const uint8_t *)in;
        strm.avail_in = (uint32_t)in_size;
        strm.next_out = out;
        strm.avail_out = (uint32_t)out_size;

        if (PREFIX(deflate)(&strm, Z_FINISH) != Z_STREAM_END)
            return 0;
        return (size_t)strm.total_out;
    }

    void end() {
        PREFIX(deflateEnd)(&strm);
    }
};

struct codec_decompressor {
    PREFIX3(stream) strm;

    bool init() {
        memset(&strm, 0, sizeof(strm));
        return PREFIX(inflateInit2)(&strm, -MAX_WBITS) == Z_OK;
    }

    /* Returns decompressed size, 0 on failure */
    size_t decompress(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size) {
        if (PREFIX(inflateReset)(&strm) != Z_OK)
            return 0;

        strm.next_in = (z_const uint8_t *)in;
        strm.avail_in = (uint32_t)in_size;
        strm.next_out = out;
        strm.avail_out = (uint32_t)out_size;

        if (PREFIX(inflate)(&strm, Z_FINISH) != Z_STREAM_END)
            return 0;
        return (size_t)strm.total_out;
    }

    void end() {
        PREFIX(inflateEnd)(&strm);
    }
};

static const int codec_levels[] = {1, 3, 6, 9};

#endif

/* Compress a corpus file with zlib-ng raw deflate at level 9, so every codec
   backend decompresses an identical input stream. Returns a malloc'd buffer. */
static uint8_t *reference_compress(const corpus_file *cf, size_t *comp_size) {
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));

    if (PREFIX(deflateInit2)(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL,
                             Z_DEFAULT_STRATEGY) != Z_OK)
        return NULL;

    size_t bound = (size_t)PREFIX(deflateBound)(&strm, (z_uintmax_t)cf->size);
    uint8_t *buf = (uint8_t *)malloc(bound);
    if (buf == NULL) {
        PREFIX(deflateEnd)(&strm);
        return NULL;
    }

    strm.next_in = (z_const uint8_t *)cf->data;
    strm.avail_in = (uint32_t)cf->size;
    strm.next_out = buf;
    strm.avail_out = (uint32_t)bound;

    int err = PREFIX(deflate)(&strm, Z_FINISH);
    *comp_size = (size_t)strm.total_out;
    PREFIX(deflateEnd)(&strm);

    if (err != Z_STREAM_END) {
        free(buf);
        return NULL;
    }
    return buf;
}

/* Decompress with zlib-ng raw inflate and compare against the original file.
   Verifying through zlib-ng also proves cross-library interoperability. */
static bool verify_compressed(const uint8_t *comp, size_t comp_size, const corpus_file *cf) {
    uint8_t *out = (uint8_t *)malloc(cf->size);
    if (out == NULL)
        return false;

    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));

    bool ok = PREFIX(inflateInit2)(&strm, -MAX_WBITS) == Z_OK;
    if (ok) {
        strm.next_in = (z_const uint8_t *)comp;
        strm.avail_in = (uint32_t)comp_size;
        strm.next_out = out;
        strm.avail_out = (uint32_t)cf->size;

        ok = PREFIX(inflate)(&strm, Z_FINISH) == Z_STREAM_END &&
             (size_t)strm.total_out == cf->size &&
             memcmp(out, cf->data, cf->size) == 0;
        PREFIX(inflateEnd)(&strm);
    }

    free(out);
    return ok;
}

class codec_deflate : public benchmark::Fixture {
private:
    corpus_file *cf;
    int level;
    uint8_t *outbuff;
    size_t outbuff_size;
    size_t compressed_size;
    codec_compressor comp;
    bool comp_init;

public:
    codec_deflate(const std::string &name, corpus_file *cf, int level)
        : cf(cf), level(level), outbuff(NULL), outbuff_size(0), compressed_size(0),
          comp(), comp_init(false) {
        this->SetName(name);
    }

    void SetUp(const benchmark::State &) override {
        if (!load_corpus_file(cf))
            return;

        comp_init = comp.init(level);
        if (!comp_init)
            return;

        outbuff_size = comp.bound(cf->size);
        outbuff = (uint8_t *)malloc(outbuff_size);
    }

    void BenchmarkCase(benchmark::State &state) override {
        if (cf->data == NULL || !comp_init || outbuff == NULL) {
            state.SkipWithError("setup failed");
            return;
        }

        for (auto _ : state) {
            compressed_size = comp.compress(cf->data, cf->size, outbuff, outbuff_size);
            if (compressed_size == 0) {
                state.SkipWithError("compress failed");
                break;
            }
        }

        if (state.skipped())
            return;

        if (!verify_compressed(outbuff, compressed_size, cf)) {
            state.SkipWithError("roundtrip verification failed");
            return;
        }

        state.SetBytesProcessed((int64_t)state.iterations() * (int64_t)cf->size);
        state.counters["compressed"] = benchmark::Counter(double(compressed_size));
        state.counters["ratio"] = benchmark::Counter(double(cf->size) / double(compressed_size));
    }

    void TearDown(const benchmark::State &) override {
        if (comp_init) {
            comp.end();
            comp_init = false;
        }
        free(outbuff);
        outbuff = NULL;
    }
};

class codec_inflate : public benchmark::Fixture {
private:
    corpus_file *cf;
    uint8_t *compressed;
    size_t compressed_size;
    uint8_t *outbuff;
    codec_decompressor decomp;
    bool decomp_init;

public:
    codec_inflate(const std::string &name, corpus_file *cf)
        : cf(cf), compressed(NULL), compressed_size(0), outbuff(NULL),
          decomp(), decomp_init(false) {
        this->SetName(name);
    }

    void SetUp(const benchmark::State &) override {
        if (!load_corpus_file(cf))
            return;

        compressed = reference_compress(cf, &compressed_size);
        outbuff = (uint8_t *)malloc(cf->size);
        decomp_init = decomp.init();
    }

    void BenchmarkCase(benchmark::State &state) override {
        if (compressed == NULL || outbuff == NULL || !decomp_init) {
            state.SkipWithError("setup failed");
            return;
        }

        for (auto _ : state) {
            size_t out_size = decomp.decompress(compressed, compressed_size, outbuff, cf->size);
            if (out_size != cf->size) {
                state.SkipWithError("decompress failed");
                break;
            }
        }

        if (state.skipped())
            return;

        if (memcmp(outbuff, cf->data, cf->size) != 0) {
            state.SkipWithError("output does not match original");
            return;
        }

        state.SetBytesProcessed((int64_t)state.iterations() * (int64_t)cf->size);
        state.counters["compressed"] = benchmark::Counter(double(compressed_size));
        state.counters["ratio"] = benchmark::Counter(double(cf->size) / double(compressed_size));
    }

    void TearDown(const benchmark::State &) override {
        if (decomp_init) {
            decomp.end();
            decomp_init = false;
        }
        free(compressed);
        compressed = NULL;
        free(outbuff);
        outbuff = NULL;
    }
};

/* Synthetic data-type inflate benchmark, isolates decoder paths by stream composition */
class codec_inflate_type : public benchmark::Fixture {
private:
    enum test_data_type type;
    corpus_file cf;
    uint8_t *compressed;
    size_t compressed_size;
    uint8_t *outbuff;
    codec_decompressor decomp;
    bool decomp_init;

public:
    codec_inflate_type(const std::string &name, enum test_data_type type)
        : type(type), cf{"", NULL, 1024 * 1024}, compressed(NULL), compressed_size(0),
          outbuff(NULL), decomp(), decomp_init(false) {
        this->SetName(name);
    }

    void SetUp(const benchmark::State &) override {
        cf.data = gen_test_data(type, cf.size);
        if (cf.data == NULL)
            return;

        compressed = reference_compress(&cf, &compressed_size);
        outbuff = (uint8_t *)malloc(cf.size);
        decomp_init = decomp.init();
    }

    void BenchmarkCase(benchmark::State &state) override {
        if (compressed == NULL || outbuff == NULL || !decomp_init) {
            state.SkipWithError("setup failed");
            return;
        }

        for (auto _ : state) {
            size_t out_size = decomp.decompress(compressed, compressed_size, outbuff, cf.size);
            if (out_size != cf.size) {
                state.SkipWithError("decompress failed");
                break;
            }
        }

        if (state.skipped())
            return;

        if (memcmp(outbuff, cf.data, cf.size) != 0) {
            state.SkipWithError("output does not match original");
            return;
        }

        state.SetBytesProcessed((int64_t)state.iterations() * (int64_t)cf.size);
        state.counters["compressed"] = benchmark::Counter(double(compressed_size));
        state.counters["ratio"] = benchmark::Counter(double(cf.size) / double(compressed_size));
    }

    void TearDown(const benchmark::State &) override {
        if (decomp_init) {
            decomp.end();
            decomp_init = false;
        }
        free(compressed);
        compressed = NULL;
        free(outbuff);
        outbuff = NULL;
        free(cf.data);
        cf.data = NULL;
    }
};

/* Registered at runtime for the data types selected by --benchmark_data_types */
static void codec_register_data_types(uint32_t mask) {
    static const struct {
        const char *name;
        enum test_data_type type;
    } types[] = {
        {"text",          TEST_DATA_TEXT},
        {"short_match",   TEST_DATA_SHORT_MATCH},
        {"dna",           TEST_DATA_DNA},
        {"random",        TEST_DATA_RANDOM},
        {"literals",      TEST_DATA_LITERALS},
        {"mixed",         TEST_DATA_MIXED},
        {"realistic_rgb", TEST_DATA_REALISTIC_RGB},
        {"striped_rgb",   TEST_DATA_STRIPED_RGB},
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (!(mask & (1u << types[i].type)))
            continue;
        std::string name = std::string("codec_inflate/data/") + types[i].name;
        benchmark::internal::RegisterBenchmarkInternal(
            ::benchmark::internal::make_unique<codec_inflate_type>(name, types[i].type));
    }
}

static int codec_data_types = benchmark_data_types_hook(codec_register_data_types);

/* Dynamic benchmark registration at static init time */
static int register_codec_benchmarks(void) {
    corpora_files = discover_corpora();
    if (corpora_files.empty())
        return 0;

    size_t prefix_len = strlen(CORPORA_DIR) + 1;

    for (size_t i = 0; i < corpora_files.size(); i++) {
        corpus_file *cf = &corpora_files[i];
        std::string label = cf->path.substr(prefix_len);
        std::replace(label.begin(), label.end(), '\\', '/');

        for (size_t l = 0; l < sizeof(codec_levels) / sizeof(codec_levels[0]); l++) {
            int level = codec_levels[l];
            std::string name = "codec_deflate/" + label + "/level:" + std::to_string(level);
            benchmark::internal::RegisterBenchmarkInternal(
                ::benchmark::internal::make_unique<codec_deflate>(name, cf, level));
        }

        std::string name = "codec_inflate/" + label;
        benchmark::internal::RegisterBenchmarkInternal(
            ::benchmark::internal::make_unique<codec_inflate>(name, cf));
    }

    return 0;
}

static int codec_init = register_codec_benchmarks();
