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

/* crc32_combine_gen() returns a zero operator for a negative length. crc32_combine_op() must return
 * for it instead of looping in multmodp(). */
TEST(crc32, combine_op_zero) {
    uint32_t op = (uint32_t)PREFIX(crc32_combine_gen)(-1);
    EXPECT_EQ(op, 0u);
    EXPECT_EQ(PREFIX(crc32_combine_op)(0x12345678u, 0x9abcdef0u, op), 0u);
}

/* crc32_combine() equals crc32_combine_op() with the operator from crc32_combine_gen(). */
TEST(crc32, combine_gen_op) {
    const z_off_t lens[] = {0, 1, 2, 7, 128, 4096, 65536, 0x7fffffff};
    for (size_t i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        uint32_t combined = PREFIX(crc32_combine)(0x12345678u, 0x9abcdef0u, lens[i]);
        uint32_t op = PREFIX(crc32_combine_gen)(lens[i]);
        EXPECT_EQ(PREFIX(crc32_combine_op)(0x12345678u, 0x9abcdef0u, op), combined);
    }
}
