## Project Basics

- Prefer building with the CMake build system.
- Always build with `-D WITH_MAINTAINER_WARNINGS=ON` to ensure we catch all warnings before publishing.

## Key Directories

- `arch/` - Architecture specific optimizations
- `test/` - Unit tests written using Google Test Framework (gtest_zlib project)
- `test/benchmarks` - Performance benchmark testing using Google Benchmark Framework (benchmark_zlib project)

## Before Starting Work

- Look for similar issues in GitHub
- Search git commit history for related work and code changes

## Testing

- To enable gtest_zlib configure CMake using `-D BUILD_TESTING=ON -D WITH_GTEST=ON`.
- `gtest_zlib` can be found in the root of the build directory.
- Visual Studio generators place binaries in a configuration subdirectory such as `Release`.

## Benchmarking

- Verify no other `benchmark_zlib` processes are running to avoid contamination.
- To enable benchmark_zlib use `-D BUILD_TESTING=ON -D WITH_BENCHMARKS=ON`.
- Always configure CMake with `-D BUILD_SHARED_LIBS=OFF` to avoid linking time.
- Isolate benchmarks runs by configuring and compiling them to separate build directories.
- `benchmark_zlib` can be found in the build directory's `test/benchmarks` directory.
- Run `benchmark_zlib --benchmark_list_tests=true` to list all benchmarks.
- When running `benchmark_zlib` with `--benchmark_repetitions`, also use `--benchmark_display_aggregates_only=true` and `--benchmark_cooldown=5`.
- Always build all benchmarks variants before running any benchmarks.
- Run benchmark processes sequentially, otherwise it could cause contention and unreliable results.
- Combine multiple `benchmark_zlib` calls and any `compare.py` calls into a single command.
- Don't run benchmarks in the background.
- Look for other benchmarks processes running on the machine to avoid contamination and wait until they are done.
- Analyze benchmarks per data-type using `--benchmark_data_types=all`.
- Analyze benchmarks against both `silesia.tar` and `silesia-small.tar`, with `deflatebench.py` using `zlib-ng/deflatebench`.
- To check for code-layout/alignment artifacts, rebuild with `-D CMAKE_C_FLAGS="-falign-functions=64 -falign-loops=32"`.
- Analyze deflate streams using https://github.com/zlib-ng/zanalyze

### libpng Benchmarks

- To enable `benchmark_zlib_apps` use `-D BUILD_TESTING=ON -D WITH_BENCHMARKS=ON -D WITH_BENCHMARK_APPS=ON`.
- Build with `-D ZLIB_COMPAT=ON -D ZLIB_ROOT=<build_dir> -D ZLIB_INCLUDE_DIR=<build_dir>` so libpng
  links against zlib-ng instead of the system libz.
- Verify the linking with `otool -L` on macOS or `ldd` on Linux, no system libz should appear.
- `png_decode_realistic` reads PNGs from a `test_pngs/` directory, see `test/benchmarks/README.md`.

### Comparing Branches

- Use `git worktree` to check out the contender branch, then configure and build each to separate
  directories.
- Run benchmarks with `--benchmark_out=<file>.json --benchmark_out_format=json` to produce input
  files for comparison.

### Presenting Results

- Always show performance changes as percentage (e.g. -18.4%), not as speedup ratios (e.g. 1.23x).
- Always include the machine specs in the summary

### Thermal Throttling

Benchmark results are vulnerable to thermal throttling. The CPU heats up during sustained workloads
and downclocks to stay within its thermal envelope, causing later benchmarks to run slower than
earlier ones.

Signs of thermal contamination:

- Later benchmarks in a run are slower than earlier ones, even though the code under test is unchanged.
- Regressions appear in code paths that were not modified.
- The same benchmark gives wildly different results across runs.
- CV exceeds 3% on benchmarks that normally have less than 1%.

Mitigation:

- Add `sleep 10` or longer between separate benchmark processes (e.g. between base run and PR run).
- Run benchmark groups separately rather than all in one long chain to limit cumulative heat buildup.
- Verify with a quick A/B sanity check (3 reps of a stable benchmark) before committing to a full run.

## Coding Standards

- Use fixed-integer types from `stdint.h` when possible.

## Commit Messages

- When a change adopts a technique or pattern that previously existed in another project
  (e.g. libdeflate, isa-l, zstd), credit the source project and thank its authors in the
  commit message body.
- Reference any external commits using `<org>/<proj>@<commit>` or with Upstream header with the url

## Creating PRs

- Cherry-pick any upstream commits from madler/zlib rather than recreating the commits
- Always benchmark any changes that touch the deflate and inflate hot paths
- Examine the assembly to ensure there are no regressions
- For build changes, ensure they are reflected in both CMake and the configure script
- Prefer unit tests in Google Test instead of standalone
- Keep linear history and squash changes into previous commits

## Replying on GitHub

- Always reply to code review comments inline in the thread
