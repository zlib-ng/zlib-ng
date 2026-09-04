/* benchmark_main.cc -- benchmark suite main entry point
 * Copyright (C) 2026 Nathan Moinvaziri
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <vector>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#ifdef __APPLE__
#  include <spawn.h>
#  include <mach-o/dyld.h>
extern char **environ;
#  ifndef _POSIX_SPAWN_DISABLE_ASLR
#    define _POSIX_SPAWN_DISABLE_ASLR 0x0100
#  endif
#endif

#include <benchmark/benchmark.h>

#include "benchmark_data_types.h"

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

#ifdef __APPLE__
/* Re-execs the process image in place (same pid, execve semantics) with ASLR disabled so code
   and stack get identical addresses every launch. A zero main-image slide means ASLR is already
   off, which also terminates the re-exec after one round. Returns false when ASLR could not be
   disabled. */
static bool reenter_without_aslr(char **argv) {
    /* A zero main-image slide means ASLR is already off, nothing to do. */
    if (_dyld_get_image_vmaddr_slide(0) == 0)
        return true;

    /* A nonzero slide after the re-exec means the kernel ignored the spawn flag. */
    if (getenv("BENCHMARK_REENTERED_NO_ASLR") != nullptr)
        return false;
    setenv("BENCHMARK_REENTERED_NO_ASLR", "1", 1);

    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> exe_path(size);
    if (_NSGetExecutablePath(exe_path.data(), &size) != 0)
        return false;

    posix_spawnattr_t attr;
    if (posix_spawnattr_init(&attr) != 0)
        return false;
    if (posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETEXEC | _POSIX_SPAWN_DISABLE_ASLR) == 0)
        posix_spawn(nullptr, exe_path.data(), nullptr, &attr, argv, environ);

    /* Only reached when the exec failed */
    posix_spawnattr_destroy(&attr);
    return false;
}
#endif

#ifdef _WIN32
/* Parses a taskset-style CPU list ("3", "0,2,4", "0-3") into an affinity mask. Returns false on
   malformed input. CPUs beyond the mask width are ignored since a mask covers one processor group. */
static bool parse_cpu_mask(const char *list, DWORD_PTR *mask_out) {
    DWORD_PTR mask = 0;
    while (*list) {
        char *end;
        long lo = strtol(list, &end, 10);
        if (end == list)
            return false;
        long hi = lo;
        if (*end == '-') {
            char *range_end;
            hi = strtol(end + 1, &range_end, 10);
            if (range_end == end + 1 || hi < lo)
                return false;
            end = range_end;
        }
        if (*end != '\0' && *end != ',')
            return false;
        for (long cpu = lo; cpu <= hi && cpu < (long)(sizeof(DWORD_PTR) * CHAR_BIT); cpu++) {
            if (cpu >= 0)
                mask |= (DWORD_PTR)1 << cpu;
        }
        while (*end == ',')
            end++;
        list = end;
    }
    *mask_out = mask;
    return true;
}
#endif

int main(int argc, char** argv) {
    uint32_t cooldown_secs = 0;
#ifdef _WIN32
    const char *cpu_affinity = nullptr;
    const char *priority = nullptr;
    bool no_power_throttling = false;
#endif

#ifndef BUILD_ALT
#  ifndef DISABLE_RUNTIME_CPU_DETECTION
    cpu_check_features(&test_cpu_features);
#  endif
#endif

    const char *data_types = nullptr;
    bool no_aslr = false;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--benchmark_cooldown=", 21) == 0) {
            cooldown_secs = strtoul(argv[i] + 21, nullptr, 10);
        } else if (strncmp(argv[i], "--benchmark_data_types=", 23) == 0) {
            data_types = argv[i] + 23;
        } else if (strcmp(argv[i], "--benchmark_no_aslr") == 0) {
            no_aslr = true;
        }
#ifdef _WIN32
        else if (strncmp(argv[i], "--benchmark_cpu_affinity=", 25) == 0) {
            cpu_affinity = argv[i] + 25;
        } else if (strncmp(argv[i], "--benchmark_priority=", 21) == 0) {
            priority = argv[i] + 21;
        } else if (strcmp(argv[i], "--benchmark_no_power_throttling") == 0) {
            no_power_throttling = true;
        }
#endif
    }

    /* Disabling ASLR fixes code and stack addresses across launches, removing the
       run-to-run layout luck that shifts branch-predictor and cache behavior. It
       re-execs in place, preserving the pid so a parent waiting on the process is
       unaffected. */
    if (no_aslr) {
#if defined(__APPLE__)
        if (!reenter_without_aslr(argv))
            fprintf(stderr, "warning: --benchmark_no_aslr could not disable ASLR, continuing with it enabled\n");
#elif defined(__linux__)
        ::benchmark::MaybeReenterWithoutASLR(argc, argv);
#else
        fprintf(stderr, "warning: --benchmark_no_aslr is not supported on this platform\n");
#endif
    }

    uint32_t data_type_mask = benchmark_data_types_parse(data_types);
    if (data_type_mask == 0)
        return EXIT_FAILURE;
    benchmark_data_types_register(data_type_mask);

    ::benchmark::Initialize(&argc, argv, []() {
        ::benchmark::PrintDefaultHelp();
        printf("          [--benchmark_cooldown=<seconds>]\n");
        printf("          [--benchmark_data_types=<type,...|all>]\n");
        printf("          [--benchmark_no_aslr]\n");
#ifdef _WIN32
        printf("          [--benchmark_cpu_affinity=<cpulist>]\n");
        printf("          [--benchmark_no_power_throttling]\n");
        printf("          [--benchmark_priority=<normal|high|realtime>]\n");
#endif
    });

#ifdef _WIN32
    if (cpu_affinity != nullptr) {
        DWORD_PTR mask;
        if (!parse_cpu_mask(cpu_affinity, &mask))
            fprintf(stderr, "warning: invalid --benchmark_cpu_affinity='%s'\n", cpu_affinity);
        else if (mask == 0)
            fprintf(stderr, "warning: --benchmark_cpu_affinity='%s' parsed to an empty mask\n", cpu_affinity);
        else if (!SetProcessAffinityMask(GetCurrentProcess(), mask))
            fprintf(stderr, "warning: SetProcessAffinityMask failed (error %lu)\n", GetLastError());
    }

    if (no_power_throttling) {
#  if _WIN32_WINNT >= _WIN32_WINNT_WIN10
        /* Clear the execution-speed bit to opt out of EcoQoS so the process runs at full
           clock on performance cores. */
        PROCESS_POWER_THROTTLING_STATE state;
        memset(&state, 0, sizeof(state));
        state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        state.StateMask = 0;
        if (!SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state)))
            fprintf(stderr, "warning: failed to disable power throttling (error %lu)\n", GetLastError());
#  else
        fprintf(stderr, "warning: --benchmark_no_power_throttling requires targeting Windows 10 or later\n");
#  endif
    }

    if (priority != nullptr) {
        DWORD priority_class = NORMAL_PRIORITY_CLASS;
        if (strcmp(priority, "high") == 0)
            priority_class = HIGH_PRIORITY_CLASS;
        else if (strcmp(priority, "realtime") == 0)
            priority_class = REALTIME_PRIORITY_CLASS;
        else if (strcmp(priority, "normal") != 0)
            fprintf(stderr, "warning: unknown --benchmark_priority '%s', using normal\n", priority);
        if (!SetPriorityClass(GetCurrentProcess(), priority_class))
            fprintf(stderr, "warning: SetPriorityClass failed (error %lu)\n", GetLastError());
    }
#endif

    if (cooldown_secs > 0) {
        benchmark::BenchmarkReporter *display = benchmark::CreateDefaultDisplayReporter();
        CooldownReporter cooldown_display(display, cooldown_secs);
        ::benchmark::RunSpecifiedBenchmarks(&cooldown_display);
    } else {
        ::benchmark::RunSpecifiedBenchmarks();
    }

    return EXIT_SUCCESS;
}
