/* test_crc32_combine.cc - Verify crc32_combine() / crc32_combine_gen() / crc32_combine_op() */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stddef.h>
#include <stdint.h>

#include <gtest/gtest.h>

/* A zero operation value must return promptly instead of spinning forever in
   multmodp(), whose only loop exit is guarded by a set bit of the operation
   value. */
TEST(crc32, combine_op_zero) {
    EXPECT_EQ(PREFIX(crc32_combine_op)(0x12345678u, 0x9abcdef0u, 0), 0);
}

/* crc32_combine(crc1, crc2, len2) is crc32_combine_op() applied with the
   operation value crc32_combine_gen() produced for len2. */
TEST(crc32, combine_gen_op) {
    const z_off64_t lens[] = {0, 1, 2, 7, 128, 4096, 65536, 0x7fffffff};
    for (size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        uint32_t combined = PREFIX4(crc32_combine)(0x12345678u, 0x9abcdef0u, lens[i]);
        uint32_t op = PREFIX4(crc32_combine_gen)(lens[i]);
        EXPECT_EQ(PREFIX(crc32_combine_op)(0x12345678u, 0x9abcdef0u, op), combined);
    }
}
