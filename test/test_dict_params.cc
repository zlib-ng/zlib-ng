/* test_dict_params.cc - Test deflateParams() after a preset dictionary */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_data_p.h"

#include <gtest/gtest.h>

#define DICT_SIZE       (16u * 1024)
#define DICT_INPUT_SIZE 4096u
#define DICT_COMPR_SIZE (2 * DICT_INPUT_SIZE)

struct dictionary_params {
    int level;
    int strategy;
    int to_level;
    int to_strategy;
    const char *name;
};

/* Verifies that deflateParams() can switch from a configuration that never reads the hash table
 * to one that does, and still compress input that matches the dictionary. */
class dictionary_rebuild : public ::testing::TestWithParam<dictionary_params> {
protected:
    uint8_t *dict = nullptr;
    uint8_t *compr = nullptr;
    uint8_t *uncompr = nullptr;

    void SetUp() override {
        dict = gen_random_data(DICT_SIZE);
        ASSERT_NE(dict, nullptr);
        compr = (uint8_t *)malloc(DICT_COMPR_SIZE);
        ASSERT_NE(compr, nullptr);
        uncompr = (uint8_t *)malloc(DICT_INPUT_SIZE);
        ASSERT_NE(uncompr, nullptr);
    }

    void TearDown() override {
        free(dict);
        free(compr);
        free(uncompr);
    }
};

TEST_P(dictionary_rebuild, round_trip) {
    const auto& p = GetParam();
    /* The input is the end of the dictionary, so it compresses only if the dictionary is hashed. */
    const uint8_t *input = dict + DICT_SIZE - DICT_INPUT_SIZE;
    PREFIX3(stream) c_stream, d_stream;

    memset(&c_stream, 0, sizeof(c_stream));
    memset(&d_stream, 0, sizeof(d_stream));

    ASSERT_EQ(PREFIX(deflateInit2)(&c_stream, p.level, Z_DEFLATED, 15, 8, p.strategy), Z_OK);
    /* A configuration that never reads the hash table loads the dictionary without hashing it. */
    EXPECT_EQ(PREFIX(deflateSetDictionary)(&c_stream, dict, DICT_SIZE), Z_OK);
    /* Switching to a configuration that reads the hash table must rebuild it from the window. */
    EXPECT_EQ(PREFIX(deflateParams)(&c_stream, p.to_level, p.to_strategy), Z_OK);

    c_stream.next_in = (z_const unsigned char *)input;
    c_stream.avail_in = DICT_INPUT_SIZE;
    c_stream.next_out = compr;
    c_stream.avail_out = DICT_COMPR_SIZE;
    EXPECT_EQ(PREFIX(deflate)(&c_stream, Z_FINISH), Z_STREAM_END);
    uint32_t compr_len = (uint32_t)c_stream.total_out;
    EXPECT_EQ(PREFIX(deflateEnd)(&c_stream), Z_OK);

    ASSERT_EQ(PREFIX(inflateInit)(&d_stream), Z_OK);
    d_stream.next_in = compr;
    d_stream.avail_in = compr_len;
    d_stream.next_out = uncompr;
    d_stream.avail_out = DICT_INPUT_SIZE;
    EXPECT_EQ(PREFIX(inflate)(&d_stream, Z_NO_FLUSH), Z_NEED_DICT);
    EXPECT_EQ(PREFIX(inflateSetDictionary)(&d_stream, dict, DICT_SIZE), Z_OK);
    EXPECT_EQ(PREFIX(inflate)(&d_stream, Z_NO_FLUSH), Z_STREAM_END);
    EXPECT_EQ(PREFIX(inflateEnd)(&d_stream), Z_OK);

    EXPECT_EQ(d_stream.total_out, DICT_INPUT_SIZE);
    EXPECT_EQ(memcmp(uncompr, input, DICT_INPUT_SIZE), 0);
    /* Matches are found through the hash table, so if it was not rebuilt from the window the
     * input is emitted as literals. */
    EXPECT_LT(compr_len, DICT_INPUT_SIZE / 2);
}

INSTANTIATE_TEST_SUITE_P(
    dictionary, dictionary_rebuild,
    ::testing::Values(
        dictionary_params{0, Z_DEFAULT_STRATEGY, 6, Z_DEFAULT_STRATEGY, "stored_to_level6"},
        dictionary_params{6, Z_HUFFMAN_ONLY, 6, Z_DEFAULT_STRATEGY, "huffman_to_default"},
        dictionary_params{6, Z_RLE, 6, Z_DEFAULT_STRATEGY, "rle_to_default"}
    ),
    [](const ::testing::TestParamInfo<dictionary_params>& info) {
        return std::string(info.param.name);
    });
