/* test_test.cc - Main entry point for test framework */

#include <stdio.h>

#include "gtest/gtest.h"

#if !defined(TEST_STOCK_ZLIB) && !defined(GTEST_SHARED_ZLIB) && \
    !defined(DISABLE_RUNTIME_CPU_DETECTION)
extern "C" {
#  include "zbuild.h"
#  include "test_cpu_features.h"
    struct cpu_features test_cpu_features;
}
#endif

GTEST_API_ int main(int argc, char **argv) {
  printf("Running main() from %s\n", __FILE__);
#if !defined(TEST_STOCK_ZLIB) && !defined(GTEST_SHARED_ZLIB) && \
    !defined(DISABLE_RUNTIME_CPU_DETECTION)
  cpu_check_features(&test_cpu_features);
#endif
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
