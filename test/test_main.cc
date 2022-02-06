/* test_test.cc - Main entry point for test framework */

#include <stdio.h>

#include "gtest/gtest.h"

extern "C" {
#  include "zbuild.h"
#  include "cpu_features.h"
}

GTEST_API_ int main(int argc, char **argv) {
  printf("Running main() from %s\n", __FILE__);
  cpu_check_features();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}