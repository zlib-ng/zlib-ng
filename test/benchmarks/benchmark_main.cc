/* benchmark_main.cc -- benchmark suite main entry point
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define NOMINMAX
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#include <benchmark/benchmark.h>

#ifndef BUILD_ALT
extern "C" {
#  include "zbuild.h"
#  include "../test_cpu_features.h"

#  ifndef DISABLE_RUNTIME_CPU_DETECTION
    struct cpu_features test_cpu_features;
#  endif
}
#endif

/* Inserts a cooldown sleep between different benchmark families to mitigate
   thermal throttling, but not between repetitions of the same benchmark. */
class CooldownReporter : public benchmark::BenchmarkReporter {
public:
    CooldownReporter(BenchmarkReporter *wrapped, uint32_t cooldown_secs)
        : wrapped_(wrapped), cooldown_secs_(cooldown_secs), first_report_(true) {}

    bool ReportContext(const Context &context) override {
        return wrapped_->ReportContext(context);
    }

    void ReportRunsConfig(double min_time, bool has_explicit_iters,
                          benchmark::IterationCount iters) override {
        wrapped_->ReportRunsConfig(min_time, has_explicit_iters, iters);
    }

    void ReportRuns(const std::vector<Run> &report) override {
        if (!first_report_) {
#ifdef _WIN32
            Sleep(cooldown_secs_ * 1000);
#else
            sleep(cooldown_secs_);
#endif
        }
        first_report_ = false;
        wrapped_->ReportRuns(report);
    }

    void Finalize() override {
        wrapped_->Finalize();
    }

private:
    BenchmarkReporter *wrapped_;
    uint32_t cooldown_secs_;
    bool first_report_;
};

int main(int argc, char** argv) {
    uint32_t cooldown_secs = 0;

#ifndef BUILD_ALT
#  ifndef DISABLE_RUNTIME_CPU_DETECTION
    cpu_check_features(&test_cpu_features);
#  endif
#endif

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--benchmark_cooldown=", 21) == 0) {
            cooldown_secs = strtoul(argv[i] + 21, nullptr, 10);
            break;
        }
    }

    ::benchmark::Initialize(&argc, argv);

    if (cooldown_secs > 0) {
        benchmark::BenchmarkReporter *display = benchmark::CreateDefaultDisplayReporter();
        CooldownReporter cooldown_display(display, cooldown_secs);
        ::benchmark::RunSpecifiedBenchmarks(&cooldown_display);
    } else {
        ::benchmark::RunSpecifiedBenchmarks();
    }

    return EXIT_SUCCESS;
}
