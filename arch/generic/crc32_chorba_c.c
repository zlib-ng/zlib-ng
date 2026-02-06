#include "zbuild.h"
#if defined(__EMSCRIPTEN__)
#  include "zutil_p.h"
#endif
#include "zmemory.h"
#include "crc32_braid_p.h"
#include "crc32_braid_tbl.h"
#include "generic_functions.h"

/* Implement Chorba algorithm from https://arxiv.org/abs/2412.16398 */
#define bitbuffer_size_bytes (16 * 1024 * sizeof(z_word_t))
#define bitbuffer_size_zwords (bitbuffer_size_bytes / sizeof(z_word_t))
#define bitbuffer_size_qwords (bitbuffer_size_bytes / sizeof(uint64_t))

#if defined(HAVE_MAY_ALIAS) && BRAID_W != 8
    typedef uint64_t __attribute__ ((__may_alias__)) uint64a_t;
#else
    typedef uint64_t uint64a_t;
#endif

/**
 * Implements the Chorba algorithm for CRC32 computation (https://arxiv.org/abs/2412.16398).
 *
 * This implementation processes data in three phases:
 * 1. Initial pass: Zeros out bitbuffer
 * 2. Intermediate pass: Processes half the values
 * 3. Main pass: Processes remaining data
 *
 * @param crc Initial CRC value
 * @param input Input data buffer
 * @param len Length of input data
 * @return Computed CRC32 value
 *
 * @note Requires minimum input size of 118960 + 512 bytes
 * @note Uses 128KB temporary buffer
 */
Z_INTERNAL uint32_t crc32_chorba_118960_nondestructive(uint32_t crc, const z_word_t *input, size_t len) {
#if defined(__EMSCRIPTEN__)
    z_word_t *bitbuffer = (z_word_t*)zng_alloc(bitbuffer_size_bytes);
#else
    ALIGNED_(16) z_word_t bitbuffer[bitbuffer_size_zwords];
#endif
    const uint8_t *bitbuffer_bytes = (const uint8_t*)bitbuffer;
    uint64a_t *bitbuffer_qwords = (uint64a_t*)bitbuffer;
    const uint64a_t *input_qwords = (const uint64a_t*)input;

    size_t i = 0;

    z_word_t next1 = Z_WORD_FROM_LE(~crc);

    z_word_t next2 = 0;
    z_word_t next3 = 0;
    z_word_t next4 = 0;
    z_word_t next5 = 0;
    z_word_t next6 = 0;
    z_word_t next7 = 0;
    z_word_t next8 = 0;
    z_word_t next9 = 0;
    z_word_t next10 = 0;
    z_word_t next11 = 0;
    z_word_t next12 = 0;
    z_word_t next13 = 0;
    z_word_t next14 = 0;
    z_word_t next15 = 0;
    z_word_t next16 = 0;
    z_word_t next17 = 0;
    z_word_t next18 = 0;
    z_word_t next19 = 0;
    z_word_t next20 = 0;
    z_word_t next21 = 0;
    z_word_t next22 = 0;
    crc = 0;

    // do a first pass to zero out bitbuffer
    for (; i < (14848 * sizeof(z_word_t)); i += (32 * sizeof(z_word_t))) {
        z_word_t in1, in2, in3, in4, in5, in6, in7, in8;
        z_word_t in9, in10, in11, in12, in13, in14, in15, in16;
        z_word_t in17, in18, in19, in20, in21, in22, in23, in24;
        z_word_t in25, in26, in27, in28, in29, in30, in31, in32;
        int out_offset1 = ((i / sizeof(z_word_t)) + 14848) % bitbuffer_size_zwords;
        int out_offset2 = ((i / sizeof(z_word_t)) + 14880) % bitbuffer_size_zwords;

        in1 = input[i / sizeof(z_word_t) + 0] ^ next1;
        in2 = input[i / sizeof(z_word_t) + 1] ^ next2;
        in3 = input[i / sizeof(z_word_t) + 2] ^ next3;
        in4 = input[i / sizeof(z_word_t) + 3] ^ next4;
        in5 = input[i / sizeof(z_word_t) + 4] ^ next5;
        in6 = input[i / sizeof(z_word_t) + 5] ^ next6;
        in7 = input[i / sizeof(z_word_t) + 6] ^ next7;
        in8 = input[i / sizeof(z_word_t) + 7] ^ next8 ^ in1;
        in9 = input[i / sizeof(z_word_t) + 8] ^ next9 ^ in2;
        in10 = input[i / sizeof(z_word_t) + 9] ^ next10 ^ in3;
        in11 = input[i / sizeof(z_word_t) + 10] ^ next11 ^ in4;
        in12 = input[i / sizeof(z_word_t) + 11] ^ next12 ^ in1 ^ in5;
        in13 = input[i / sizeof(z_word_t) + 12] ^ next13 ^ in2 ^ in6;
        in14 = input[i / sizeof(z_word_t) + 13] ^ next14 ^ in3 ^ in7;
        in15 = input[i / sizeof(z_word_t) + 14] ^ next15 ^ in4 ^ in8;
        in16 = input[i / sizeof(z_word_t) + 15] ^ next16 ^ in5 ^ in9;
        in17 = input[i / sizeof(z_word_t) + 16] ^ next17 ^ in6 ^ in10;
        in18 = input[i / sizeof(z_word_t) + 17] ^ next18 ^ in7 ^ in11;
        in19 = input[i / sizeof(z_word_t) + 18] ^ next19 ^ in8 ^ in12;
        in20 = input[i / sizeof(z_word_t) + 19] ^ next20 ^ in9 ^ in13;
        in21 = input[i / sizeof(z_word_t) + 20] ^ next21 ^ in10 ^ in14;
        in22 = input[i / sizeof(z_word_t) + 21] ^ next22 ^ in11 ^ in15;
        in23 = input[i / sizeof(z_word_t) + 22] ^ in1 ^ in12 ^ in16;
        in24 = input[i / sizeof(z_word_t) + 23] ^ in2 ^ in13 ^ in17;
        in25 = input[i / sizeof(z_word_t) + 24] ^ in3 ^ in14 ^ in18;
        in26 = input[i / sizeof(z_word_t) + 25] ^ in4 ^ in15 ^ in19;
        in27 = input[i / sizeof(z_word_t) + 26] ^ in5 ^ in16 ^ in20;
        in28 = input[i / sizeof(z_word_t) + 27] ^ in6 ^ in17 ^ in21;
        in29 = input[i / sizeof(z_word_t) + 28] ^ in7 ^ in18 ^ in22;
        in30 = input[i / sizeof(z_word_t) + 29] ^ in8 ^ in19 ^ in23;
        in31 = input[i / sizeof(z_word_t) + 30] ^ in9 ^ in20 ^ in24;
        in32 = input[i / sizeof(z_word_t) + 31] ^ in10 ^ in21 ^ in25;

        next1 = in11 ^ in22 ^ in26;
        next2 = in12 ^ in23 ^ in27;
        next3 = in13 ^ in24 ^ in28;
        next4 = in14 ^ in25 ^ in29;
        next5 = in15 ^ in26 ^ in30;
        next6 = in16 ^ in27 ^ in31;
        next7 = in17 ^ in28 ^ in32;
        next8 = in18 ^ in29;
        next9 = in19 ^ in30;
        next10 = in20 ^ in31;
        next11 = in21 ^ in32;
        next12 = in22;
        next13 = in23;
        next14 = in24;
        next15 = in25;
        next16 = in26;
        next17 = in27;
        next18 = in28;
        next19 = in29;
        next20 = in30;
        next21 = in31;
        next22 = in32;

        bitbuffer[out_offset1 + 22] = in1;
        bitbuffer[out_offset1 + 23] = in2;
        bitbuffer[out_offset1 + 24] = in3;
        bitbuffer[out_offset1 + 25] = in4;
        bitbuffer[out_offset1 + 26] = in5;
        bitbuffer[out_offset1 + 27] = in6;
        bitbuffer[out_offset1 + 28] = in7;
        bitbuffer[out_offset1 + 29] = in8;
        bitbuffer[out_offset1 + 30] = in9;
        bitbuffer[out_offset1 + 31] = in10;
        bitbuffer[out_offset2 + 0] = in11;
        bitbuffer[out_offset2 + 1] = in12;
        bitbuffer[out_offset2 + 2] = in13;
        bitbuffer[out_offset2 + 3] = in14;
        bitbuffer[out_offset2 + 4] = in15;
        bitbuffer[out_offset2 + 5] = in16;
        bitbuffer[out_offset2 + 6] = in17;
        bitbuffer[out_offset2 + 7] = in18;
        bitbuffer[out_offset2 + 8] = in19;
        bitbuffer[out_offset2 + 9] = in20;
        bitbuffer[out_offset2 + 10] = in21;
        bitbuffer[out_offset2 + 11] = in22;
        bitbuffer[out_offset2 + 12] = in23;
        bitbuffer[out_offset2 + 13] = in24;
        bitbuffer[out_offset2 + 14] = in25;
        bitbuffer[out_offset2 + 15] = in26;
        bitbuffer[out_offset2 + 16] = in27;
        bitbuffer[out_offset2 + 17] = in28;
        bitbuffer[out_offset2 + 18] = in29;
        bitbuffer[out_offset2 + 19] = in30;
        bitbuffer[out_offset2 + 20] = in31;
        bitbuffer[out_offset2 + 21] = in32;
    }

    // one intermediate pass where we pull half the values
    for (; i < (14880 * sizeof(z_word_t)); i += (32 * sizeof(z_word_t))) {
        z_word_t in1, in2, in3, in4, in5, in6, in7, in8;
        z_word_t in9, in10, in11, in12, in13, in14, in15, in16;
        z_word_t in17, in18, in19, in20, in21, in22, in23, in24;
        z_word_t in25, in26, in27, in28, in29, in30, in31, in32;
        int in_offset = (i / sizeof(z_word_t)) % bitbuffer_size_zwords;
        int out_offset1 = ((i / sizeof(z_word_t)) + 14848) % bitbuffer_size_zwords;
        int out_offset2 = ((i / sizeof(z_word_t)) + 14880) % bitbuffer_size_zwords;

        in1 = input[i / sizeof(z_word_t) + 0] ^ next1;
        in2 = input[i / sizeof(z_word_t) + 1] ^ next2;
        in3 = input[i / sizeof(z_word_t) + 2] ^ next3;
        in4 = input[i / sizeof(z_word_t) + 3] ^ next4;
        in5 = input[i / sizeof(z_word_t) + 4] ^ next5;
        in6 = input[i / sizeof(z_word_t) + 5] ^ next6;
        in7 = input[i / sizeof(z_word_t) + 6] ^ next7;
        in8 = input[i / sizeof(z_word_t) + 7] ^ next8 ^ in1;
        in9 = input[i / sizeof(z_word_t) + 8] ^ next9 ^ in2;
        in10 = input[i / sizeof(z_word_t) + 9] ^ next10 ^ in3;
        in11 = input[i / sizeof(z_word_t) + 10] ^ next11 ^ in4;
        in12 = input[i / sizeof(z_word_t) + 11] ^ next12 ^ in1 ^ in5;
        in13 = input[i / sizeof(z_word_t) + 12] ^ next13 ^ in2 ^ in6;
        in14 = input[i / sizeof(z_word_t) + 13] ^ next14 ^ in3 ^ in7;
        in15 = input[i / sizeof(z_word_t) + 14] ^ next15 ^ in4 ^ in8;
        in16 = input[i / sizeof(z_word_t) + 15] ^ next16 ^ in5 ^ in9;
        in17 = input[i / sizeof(z_word_t) + 16] ^ next17 ^ in6 ^ in10;
        in18 = input[i / sizeof(z_word_t) + 17] ^ next18 ^ in7 ^ in11;
        in19 = input[i / sizeof(z_word_t) + 18] ^ next19 ^ in8 ^ in12;
        in20 = input[i / sizeof(z_word_t) + 19] ^ next20 ^ in9 ^ in13;
        in21 = input[i / sizeof(z_word_t) + 20] ^ next21 ^ in10 ^ in14;
        in22 = input[i / sizeof(z_word_t) + 21] ^ next22 ^ in11 ^ in15;
        in23 = input[i / sizeof(z_word_t) + 22] ^ in1 ^ in12 ^ in16 ^ bitbuffer[in_offset + 22];
        in24 = input[i / sizeof(z_word_t) + 23] ^ in2 ^ in13 ^ in17 ^ bitbuffer[in_offset + 23];
        in25 = input[i / sizeof(z_word_t) + 24] ^ in3 ^ in14 ^ in18 ^ bitbuffer[in_offset + 24];
        in26 = input[i / sizeof(z_word_t) + 25] ^ in4 ^ in15 ^ in19 ^ bitbuffer[in_offset + 25];
        in27 = input[i / sizeof(z_word_t) + 26] ^ in5 ^ in16 ^ in20 ^ bitbuffer[in_offset + 26];
        in28 = input[i / sizeof(z_word_t) + 27] ^ in6 ^ in17 ^ in21 ^ bitbuffer[in_offset + 27];
        in29 = input[i / sizeof(z_word_t) + 28] ^ in7 ^ in18 ^ in22 ^ bitbuffer[in_offset + 28];
        in30 = input[i / sizeof(z_word_t) + 29] ^ in8 ^ in19 ^ in23 ^ bitbuffer[in_offset + 29];
        in31 = input[i / sizeof(z_word_t) + 30] ^ in9 ^ in20 ^ in24 ^ bitbuffer[in_offset + 30];
        in32 = input[i / sizeof(z_word_t) + 31] ^ in10 ^ in21 ^ in25 ^ bitbuffer[in_offset + 31];

        next1 = in11 ^ in22 ^ in26;
        next2 = in12 ^ in23 ^ in27;
        next3 = in13 ^ in24 ^ in28;
        next4 = in14 ^ in25 ^ in29;
        next5 = in15 ^ in26 ^ in30;
        next6 = in16 ^ in27 ^ in31;
        next7 = in17 ^ in28 ^ in32;
        next8 = in18 ^ in29;
        next9 = in19 ^ in30;
        next10 = in20 ^ in31;
        next11 = in21 ^ in32;
        next12 = in22;
        next13 = in23;
        next14 = in24;
        next15 = in25;
        next16 = in26;
        next17 = in27;
        next18 = in28;
        next19 = in29;
        next20 = in30;
        next21 = in31;
        next22 = in32;

        bitbuffer[out_offset1 + 22] = in1;
        bitbuffer[out_offset1 + 23] = in2;
        bitbuffer[out_offset1 + 24] = in3;
        bitbuffer[out_offset1 + 25] = in4;
        bitbuffer[out_offset1 + 26] = in5;
        bitbuffer[out_offset1 + 27] = in6;
        bitbuffer[out_offset1 + 28] = in7;
        bitbuffer[out_offset1 + 29] = in8;
        bitbuffer[out_offset1 + 30] = in9;
        bitbuffer[out_offset1 + 31] = in10;
        bitbuffer[out_offset2 + 0] = in11;
        bitbuffer[out_offset2 + 1] = in12;
        bitbuffer[out_offset2 + 2] = in13;
        bitbuffer[out_offset2 + 3] = in14;
        bitbuffer[out_offset2 + 4] = in15;
        bitbuffer[out_offset2 + 5] = in16;
        bitbuffer[out_offset2 + 6] = in17;
        bitbuffer[out_offset2 + 7] = in18;
        bitbuffer[out_offset2 + 8] = in19;
        bitbuffer[out_offset2 + 9] = in20;
        bitbuffer[out_offset2 + 10] = in21;
        bitbuffer[out_offset2 + 11] = in22;
        bitbuffer[out_offset2 + 12] = in23;
        bitbuffer[out_offset2 + 13] = in24;
        bitbuffer[out_offset2 + 14] = in25;
        bitbuffer[out_offset2 + 15] = in26;
        bitbuffer[out_offset2 + 16] = in27;
        bitbuffer[out_offset2 + 17] = in28;
        bitbuffer[out_offset2 + 18] = in29;
        bitbuffer[out_offset2 + 19] = in30;
        bitbuffer[out_offset2 + 20] = in31;
        bitbuffer[out_offset2 + 21] = in32;
    }

    for (; (i + (14870 + 64) * sizeof(z_word_t)) < len; i += (32 * sizeof(z_word_t))) {
        z_word_t in1, in2, in3, in4, in5, in6, in7, in8;
        z_word_t in9, in10, in11, in12, in13, in14, in15, in16;
        z_word_t in17, in18, in19, in20, in21, in22, in23, in24;
        z_word_t in25, in26, in27, in28, in29, in30, in31, in32;
        int in_offset = (i / sizeof(z_word_t)) % bitbuffer_size_zwords;
        int out_offset1 = ((i / sizeof(z_word_t)) + 14848) % bitbuffer_size_zwords;
        int out_offset2 = ((i / sizeof(z_word_t)) + 14880) % bitbuffer_size_zwords;

        in1 = input[i / sizeof(z_word_t) + 0] ^ next1 ^ bitbuffer[in_offset + 0];
        in2 = input[i / sizeof(z_word_t) + 1] ^ next2 ^ bitbuffer[in_offset + 1];
        in3 = input[i / sizeof(z_word_t) + 2] ^ next3 ^ bitbuffer[in_offset + 2];
        in4 = input[i / sizeof(z_word_t) + 3] ^ next4 ^ bitbuffer[in_offset + 3];
        in5 = input[i / sizeof(z_word_t) + 4] ^ next5 ^ bitbuffer[in_offset + 4];
        in6 = input[i / sizeof(z_word_t) + 5] ^ next6 ^ bitbuffer[in_offset + 5];
        in7 = input[i / sizeof(z_word_t) + 6] ^ next7 ^ bitbuffer[in_offset + 6];
        in8 = input[i / sizeof(z_word_t) + 7] ^ next8 ^ in1 ^ bitbuffer[in_offset + 7];
        in9 = input[i / sizeof(z_word_t) + 8] ^ next9 ^ in2 ^ bitbuffer[in_offset + 8];
        in10 = input[i / sizeof(z_word_t) + 9] ^ next10 ^ in3 ^ bitbuffer[in_offset + 9];
        in11 = input[i / sizeof(z_word_t) + 10] ^ next11 ^ in4 ^ bitbuffer[in_offset + 10];
        in12 = input[i / sizeof(z_word_t) + 11] ^ next12 ^ in1 ^ in5 ^ bitbuffer[in_offset + 11];
        in13 = input[i / sizeof(z_word_t) + 12] ^ next13 ^ in2 ^ in6 ^ bitbuffer[in_offset + 12];
        in14 = input[i / sizeof(z_word_t) + 13] ^ next14 ^ in3 ^ in7 ^ bitbuffer[in_offset + 13];
        in15 = input[i / sizeof(z_word_t) + 14] ^ next15 ^ in4 ^ in8 ^ bitbuffer[in_offset + 14];
        in16 = input[i / sizeof(z_word_t) + 15] ^ next16 ^ in5 ^ in9 ^ bitbuffer[in_offset + 15];
        in17 = input[i / sizeof(z_word_t) + 16] ^ next17 ^ in6 ^ in10 ^ bitbuffer[in_offset + 16];
        in18 = input[i / sizeof(z_word_t) + 17] ^ next18 ^ in7 ^ in11 ^ bitbuffer[in_offset + 17];
        in19 = input[i / sizeof(z_word_t) + 18] ^ next19 ^ in8 ^ in12 ^ bitbuffer[in_offset + 18];
        in20 = input[i / sizeof(z_word_t) + 19] ^ next20 ^ in9 ^ in13 ^ bitbuffer[in_offset + 19];
        in21 = input[i / sizeof(z_word_t) + 20] ^ next21 ^ in10 ^ in14 ^ bitbuffer[in_offset + 20];
        in22 = input[i / sizeof(z_word_t) + 21] ^ next22 ^ in11 ^ in15 ^ bitbuffer[in_offset + 21];
        in23 = input[i / sizeof(z_word_t) + 22] ^ in1 ^ in12 ^ in16 ^ bitbuffer[in_offset + 22];
        in24 = input[i / sizeof(z_word_t) + 23] ^ in2 ^ in13 ^ in17 ^ bitbuffer[in_offset + 23];
        in25 = input[i / sizeof(z_word_t) + 24] ^ in3 ^ in14 ^ in18 ^ bitbuffer[in_offset + 24];
        in26 = input[i / sizeof(z_word_t) + 25] ^ in4 ^ in15 ^ in19 ^ bitbuffer[in_offset + 25];
        in27 = input[i / sizeof(z_word_t) + 26] ^ in5 ^ in16 ^ in20 ^ bitbuffer[in_offset + 26];
        in28 = input[i / sizeof(z_word_t) + 27] ^ in6 ^ in17 ^ in21 ^ bitbuffer[in_offset + 27];
        in29 = input[i / sizeof(z_word_t) + 28] ^ in7 ^ in18 ^ in22 ^ bitbuffer[in_offset + 28];
        in30 = input[i / sizeof(z_word_t) + 29] ^ in8 ^ in19 ^ in23 ^ bitbuffer[in_offset + 29];
        in31 = input[i / sizeof(z_word_t) + 30] ^ in9 ^ in20 ^ in24 ^ bitbuffer[in_offset + 30];
        in32 = input[i / sizeof(z_word_t) + 31] ^ in10 ^ in21 ^ in25 ^ bitbuffer[in_offset + 31];

        next1 = in11 ^ in22 ^ in26;
        next2 = in12 ^ in23 ^ in27;
        next3 = in13 ^ in24 ^ in28;
        next4 = in14 ^ in25 ^ in29;
        next5 = in15 ^ in26 ^ in30;
        next6 = in16 ^ in27 ^ in31;
        next7 = in17 ^ in28 ^ in32;
        next8 = in18 ^ in29;
        next9 = in19 ^ in30;
        next10 = in20 ^ in31;
        next11 = in21 ^ in32;
        next12 = in22;
        next13 = in23;
        next14 = in24;
        next15 = in25;
        next16 = in26;
        next17 = in27;
        next18 = in28;
        next19 = in29;
        next20 = in30;
        next21 = in31;
        next22 = in32;

        bitbuffer[out_offset1 + 22] = in1;
        bitbuffer[out_offset1 + 23] = in2;
        bitbuffer[out_offset1 + 24] = in3;
        bitbuffer[out_offset1 + 25] = in4;
        bitbuffer[out_offset1 + 26] = in5;
        bitbuffer[out_offset1 + 27] = in6;
        bitbuffer[out_offset1 + 28] = in7;
        bitbuffer[out_offset1 + 29] = in8;
        bitbuffer[out_offset1 + 30] = in9;
        bitbuffer[out_offset1 + 31] = in10;
        bitbuffer[out_offset2 + 0] = in11;
        bitbuffer[out_offset2 + 1] = in12;
        bitbuffer[out_offset2 + 2] = in13;
        bitbuffer[out_offset2 + 3] = in14;
        bitbuffer[out_offset2 + 4] = in15;
        bitbuffer[out_offset2 + 5] = in16;
        bitbuffer[out_offset2 + 6] = in17;
        bitbuffer[out_offset2 + 7] = in18;
        bitbuffer[out_offset2 + 8] = in19;
        bitbuffer[out_offset2 + 9] = in20;
        bitbuffer[out_offset2 + 10] = in21;
        bitbuffer[out_offset2 + 11] = in22;
        bitbuffer[out_offset2 + 12] = in23;
        bitbuffer[out_offset2 + 13] = in24;
        bitbuffer[out_offset2 + 14] = in25;
        bitbuffer[out_offset2 + 15] = in26;
        bitbuffer[out_offset2 + 16] = in27;
        bitbuffer[out_offset2 + 17] = in28;
        bitbuffer[out_offset2 + 18] = in29;
        bitbuffer[out_offset2 + 19] = in30;
        bitbuffer[out_offset2 + 20] = in31;
        bitbuffer[out_offset2 + 21] = in32;
    }

    bitbuffer[(i / sizeof(z_word_t) + 0) % bitbuffer_size_zwords] ^= next1;
    bitbuffer[(i / sizeof(z_word_t) + 1) % bitbuffer_size_zwords] ^= next2;
    bitbuffer[(i / sizeof(z_word_t) + 2) % bitbuffer_size_zwords] ^= next3;
    bitbuffer[(i / sizeof(z_word_t) + 3) % bitbuffer_size_zwords] ^= next4;
    bitbuffer[(i / sizeof(z_word_t) + 4) % bitbuffer_size_zwords] ^= next5;
    bitbuffer[(i / sizeof(z_word_t) + 5) % bitbuffer_size_zwords] ^= next6;
    bitbuffer[(i / sizeof(z_word_t) + 6) % bitbuffer_size_zwords] ^= next7;
    bitbuffer[(i / sizeof(z_word_t) + 7) % bitbuffer_size_zwords] ^= next8;
    bitbuffer[(i / sizeof(z_word_t) + 8) % bitbuffer_size_zwords] ^= next9;
    bitbuffer[(i / sizeof(z_word_t) + 9) % bitbuffer_size_zwords] ^= next10;
    bitbuffer[(i / sizeof(z_word_t) + 10) % bitbuffer_size_zwords] ^= next11;
    bitbuffer[(i / sizeof(z_word_t) + 11) % bitbuffer_size_zwords] ^= next12;
    bitbuffer[(i / sizeof(z_word_t) + 12) % bitbuffer_size_zwords] ^= next13;
    bitbuffer[(i / sizeof(z_word_t) + 13) % bitbuffer_size_zwords] ^= next14;
    bitbuffer[(i / sizeof(z_word_t) + 14) % bitbuffer_size_zwords] ^= next15;
    bitbuffer[(i / sizeof(z_word_t) + 15) % bitbuffer_size_zwords] ^= next16;
    bitbuffer[(i / sizeof(z_word_t) + 16) % bitbuffer_size_zwords] ^= next17;
    bitbuffer[(i / sizeof(z_word_t) + 17) % bitbuffer_size_zwords] ^= next18;
    bitbuffer[(i / sizeof(z_word_t) + 18) % bitbuffer_size_zwords] ^= next19;
    bitbuffer[(i / sizeof(z_word_t) + 19) % bitbuffer_size_zwords] ^= next20;
    bitbuffer[(i / sizeof(z_word_t) + 20) % bitbuffer_size_zwords] ^= next21;
    bitbuffer[(i / sizeof(z_word_t) + 21) % bitbuffer_size_zwords] ^= next22;

    for (int j = 14870; j < 14870 + 64; j++) {
        bitbuffer[(j + (i / sizeof(z_word_t))) % bitbuffer_size_zwords] = 0;
    }

    uint64_t next1_64 = 0;
    uint64_t next2_64 = 0;
    uint64_t next3_64 = 0;
    uint64_t next4_64 = 0;
    uint64_t next5_64 = 0;
    uint64_t final[9] = {0};

    for (; (i + 72 < len); i += 32) {
        uint64_t in1;
        uint64_t in2;
        uint64_t in3;
        uint64_t in4;
        uint64_t a1, a2, a3, a4;
        uint64_t b1, b2, b3, b4;
        uint64_t c1, c2, c3, c4;
        uint64_t d1, d2, d3, d4;

        uint64_t out1;
        uint64_t out2;
        uint64_t out3;
        uint64_t out4;
        uint64_t out5;

        in1 = input_qwords[i / sizeof(uint64_t)] ^ bitbuffer_qwords[(i / sizeof(uint64_t)) % bitbuffer_size_qwords];
        in2 = input_qwords[i / sizeof(uint64_t) + 1] ^ bitbuffer_qwords[(i / sizeof(uint64_t) + 1) % bitbuffer_size_qwords];
        in1 = Z_U64_FROM_LE(in1) ^ next1_64;
        in2 = Z_U64_FROM_LE(in2) ^ next2_64;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = input_qwords[i / sizeof(uint64_t) + 2] ^ bitbuffer_qwords[(i / sizeof(uint64_t) + 2) % bitbuffer_size_qwords];
        in4 = input_qwords[i / sizeof(uint64_t) + 3] ^ bitbuffer_qwords[(i / sizeof(uint64_t) + 3) % bitbuffer_size_qwords];
        in3 = Z_U64_FROM_LE(in3) ^ next3_64 ^ a1;
        in4 = Z_U64_FROM_LE(in4) ^ next4_64 ^ a2 ^ b1;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1_64 = next5_64 ^ out1;
        next2_64 = out2;
        next3_64 = out3;
        next4_64 = out4;
        next5_64 = out5;

    }

    memcpy(final, input_qwords + (i / sizeof(uint64_t)), len-i);
    final[0] ^= Z_U64_TO_LE(next1_64);
    final[1] ^= Z_U64_TO_LE(next2_64);
    final[2] ^= Z_U64_TO_LE(next3_64);
    final[3] ^= Z_U64_TO_LE(next4_64);
    final[4] ^= Z_U64_TO_LE(next5_64);

    uint8_t *final_bytes = (uint8_t*)final;

    for (size_t j = 0; j < (len-i); j++) {
        crc = crc_table[(crc ^ final_bytes[j] ^ bitbuffer_bytes[(j+i) % bitbuffer_size_bytes]) & 0xff] ^ (crc >> 8);
    }

#if defined(__EMSCRIPTEN__)
    zng_free(bitbuffer);
#endif
    return ~crc;
}

#  if OPTIMAL_CMP == 64
/* Implement Chorba algorithm from https://arxiv.org/abs/2412.16398 */
Z_INTERNAL uint32_t crc32_chorba_32768_nondestructive(uint32_t crc, const uint64_t* input, size_t len) {
    uint64_t bitbuffer[32768 / sizeof(uint64_t)];
    const uint8_t *bitbuffer_bytes = (const uint8_t*)bitbuffer;
    memset(bitbuffer, 0, 32768);
    bitbuffer[0] = Z_U64_TO_LE(~crc);

    crc = 0;

    size_t i = 0;

    for(; i + 300*8+64 < len; i += 64) {
        uint64_t in1, in2, in3, in4;
        uint64_t in5, in6, in7, in8;
        size_t in_offset = (i/8);

        in1 = input[i / sizeof(uint64_t) + 0] ^ bitbuffer[in_offset + 0];
        in2 = input[i / sizeof(uint64_t) + 1] ^ bitbuffer[in_offset + 1];
        in3 = input[i / sizeof(uint64_t) + 2] ^ bitbuffer[in_offset + 2];
        in4 = input[i / sizeof(uint64_t) + 3] ^ bitbuffer[in_offset + 3];
        in5 = input[i / sizeof(uint64_t) + 4] ^ bitbuffer[in_offset + 4];
        in6 = input[i / sizeof(uint64_t) + 5] ^ bitbuffer[in_offset + 5];
        in7 = input[i / sizeof(uint64_t) + 6] ^ bitbuffer[in_offset + 6];
        in8 = input[i / sizeof(uint64_t) + 7] ^ bitbuffer[in_offset + 7];

        // [0, 145, 183, 211]

        bitbuffer[(i/8 + 0 + 145)] ^= in1;
        bitbuffer[(i/8 + 1 + 145)] ^= in2;
        bitbuffer[(i/8 + 2 + 145)] ^= in3;
        bitbuffer[(i/8 + 3 + 145)] ^= in4;
        bitbuffer[(i/8 + 4 + 145)] ^= in5;
        bitbuffer[(i/8 + 5 + 145)] ^= in6;
        bitbuffer[(i/8 + 6 + 145)] ^= in7;
        bitbuffer[(i/8 + 7 + 145)] ^= in8;

        bitbuffer[(i/8 + 0 + 183)] ^= in1;
        bitbuffer[(i/8 + 1 + 183)] ^= in2;
        bitbuffer[(i/8 + 2 + 183)] ^= in3;
        bitbuffer[(i/8 + 3 + 183)] ^= in4;
        bitbuffer[(i/8 + 4 + 183)] ^= in5;
        bitbuffer[(i/8 + 5 + 183)] ^= in6;
        bitbuffer[(i/8 + 6 + 183)] ^= in7;
        bitbuffer[(i/8 + 7 + 183)] ^= in8;

        bitbuffer[(i/8 + 0 + 211)] ^= in1;
        bitbuffer[(i/8 + 1 + 211)] ^= in2;
        bitbuffer[(i/8 + 2 + 211)] ^= in3;
        bitbuffer[(i/8 + 3 + 211)] ^= in4;
        bitbuffer[(i/8 + 4 + 211)] ^= in5;
        bitbuffer[(i/8 + 5 + 211)] ^= in6;
        bitbuffer[(i/8 + 6 + 211)] ^= in7;
        bitbuffer[(i/8 + 7 + 211)] ^= in8;

        bitbuffer[(i/8 + 0 + 300)] = in1;
        bitbuffer[(i/8 + 1 + 300)] = in2;
        bitbuffer[(i/8 + 2 + 300)] = in3;
        bitbuffer[(i/8 + 3 + 300)] = in4;
        bitbuffer[(i/8 + 4 + 300)] = in5;
        bitbuffer[(i/8 + 5 + 300)] = in6;
        bitbuffer[(i/8 + 6 + 300)] = in7;
        bitbuffer[(i/8 + 7 + 300)] = in8;
    }

    uint64_t next1_64 = 0;
    uint64_t next2_64 = 0;
    uint64_t next3_64 = 0;
    uint64_t next4_64 = 0;
    uint64_t next5_64 = 0;
    uint64_t final[9] = {0};

    for (; (i + 72 < len); i += 32) {
        uint64_t in1;
        uint64_t in2;
        uint64_t in3;
        uint64_t in4;
        uint64_t a1, a2, a3, a4;
        uint64_t b1, b2, b3, b4;
        uint64_t c1, c2, c3, c4;
        uint64_t d1, d2, d3, d4;

        uint64_t out1;
        uint64_t out2;
        uint64_t out3;
        uint64_t out4;
        uint64_t out5;

        in1 = input[i / sizeof(z_word_t)] ^ bitbuffer[(i / sizeof(uint64_t))];
        in2 = input[(i + 8) / sizeof(z_word_t)] ^ bitbuffer[(i / sizeof(uint64_t) + 1)];
        in1 = Z_U64_FROM_LE(in1) ^ next1_64;
        in2 = Z_U64_FROM_LE(in2) ^ next2_64;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = input[(i + 16) / sizeof(z_word_t)] ^ bitbuffer[(i / sizeof(uint64_t) + 2)];
        in4 = input[(i + 24) / sizeof(z_word_t)] ^ bitbuffer[(i / sizeof(uint64_t) + 3)];
        in3 = Z_U64_FROM_LE(in3) ^ next3_64 ^ a1;
        in4 = Z_U64_FROM_LE(in4) ^ next4_64 ^ a2 ^ b1;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1_64 = next5_64 ^ out1;
        next2_64 = out2;
        next3_64 = out3;
        next4_64 = out4;
        next5_64 = out5;

    }

    memcpy(final, input+(i / sizeof(uint64_t)), len-i);
    final[0] ^= Z_U64_TO_LE(next1_64);
    final[1] ^= Z_U64_TO_LE(next2_64);
    final[2] ^= Z_U64_TO_LE(next3_64);
    final[3] ^= Z_U64_TO_LE(next4_64);
    final[4] ^= Z_U64_TO_LE(next5_64);

    uint8_t *final_bytes = (uint8_t*)final;

    for (size_t j = 0; j < (len-i); j++) {
        crc = crc_table[(crc ^ final_bytes[j] ^ bitbuffer_bytes[(j+i)]) & 0xff] ^ (crc >> 8);
    }

    return ~crc;
}

/* Implement Chorba algorithm from https://arxiv.org/abs/2412.16398 */
Z_INTERNAL uint32_t crc32_chorba_small_nondestructive(uint32_t crc, const uint64_t *input, size_t len) {
    uint64_t final[9] = {0};
    uint64_t next1 = ~crc;
    crc = 0;
    uint64_t next2 = 0;
    uint64_t next3 = 0;
    uint64_t next4 = 0;
    uint64_t next5 = 0;

    size_t i = 0;

    /* This is weird, doing for vs while drops 10% off the exec time */
    for (; (i + 256 + 40 + 32 + 32) < len; i += 32) {
        uint64_t in1;
        uint64_t in2;
        uint64_t in3;
        uint64_t in4;
        uint64_t a1, a2, a3, a4;
        uint64_t b1, b2, b3, b4;
        uint64_t c1, c2, c3, c4;
        uint64_t d1, d2, d3, d4;

        uint64_t out1;
        uint64_t out2;
        uint64_t out3;
        uint64_t out4;
        uint64_t out5;

        uint64_t chorba1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1;
        uint64_t chorba2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2;
        uint64_t chorba3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3;
        uint64_t chorba4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4;
        uint64_t chorba5 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 4]) ^ next5;
        uint64_t chorba6 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 5]);
        uint64_t chorba7 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 6]) ^ chorba1;
        uint64_t chorba8 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 7]) ^ chorba2;

        i += 8 * 8;

        /* 0-3 */
        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ chorba3;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ chorba4 ^ chorba1;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ a1 ^ chorba5 ^ chorba2 ^ chorba1;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ a2 ^ b1 ^ chorba6 ^ chorba3 ^ chorba2;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;

        i += 32;

        /* 4-7 */
        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1 ^ chorba7 ^ chorba4 ^ chorba3;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2 ^ chorba8 ^ chorba5 ^ chorba4;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3 ^ a1 ^ chorba6 ^ chorba5;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4 ^ a2 ^ b1 ^ chorba7 ^ chorba6;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = next5 ^ out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;

        i += 32;

        /* 8-11 */
        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1 ^ chorba8 ^ chorba7 ^ chorba1;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2 ^ chorba8 ^ chorba2;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3 ^ a1 ^ chorba3;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4 ^ a2 ^ b1 ^ chorba4;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = next5 ^ out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;

        i += 32;

        /* 12-15 */
        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1 ^ chorba5 ^ chorba1;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2 ^ chorba6 ^ chorba2 ^ chorba1;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3 ^ a1 ^ chorba7 ^ chorba3 ^ chorba2 ^ chorba1;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4 ^ a2 ^ b1 ^ chorba8 ^ chorba4 ^ chorba3 ^ chorba2;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = next5 ^ out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;

        i += 32;

        /* 16-19 */
        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1 ^ chorba5 ^ chorba4 ^ chorba3 ^ chorba1;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2 ^ chorba6 ^ chorba5 ^ chorba4 ^ chorba1 ^ chorba2;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3 ^ a1 ^ chorba7 ^ chorba6 ^ chorba5 ^ chorba2 ^ chorba3;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4 ^ a2 ^ b1 ^ chorba8 ^ chorba7 ^ chorba6 ^ chorba3 ^ chorba4 ^ chorba1;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = next5 ^ out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;

        i += 32;

        /* 20-23 */
        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1 ^ chorba8 ^ chorba7 ^ chorba4 ^ chorba5 ^ chorba2 ^ chorba1;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2 ^ chorba8 ^ chorba5 ^ chorba6 ^ chorba3 ^ chorba2;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3 ^ a1 ^ chorba7 ^ chorba6 ^ chorba4 ^ chorba3 ^ chorba1;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4 ^ a2 ^ b1 ^ chorba8 ^ chorba7 ^ chorba5 ^ chorba4 ^ chorba2 ^ chorba1;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = next5 ^ out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;

        i += 32;

        /* 24-27 */
        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1 ^ chorba8 ^ chorba6 ^ chorba5 ^ chorba3 ^ chorba2 ^ chorba1;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2 ^ chorba7 ^ chorba6 ^ chorba4 ^ chorba3 ^ chorba2;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3 ^ a1 ^ chorba8 ^ chorba7 ^ chorba5 ^ chorba4 ^ chorba3;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4 ^ a2 ^ b1 ^ chorba8 ^ chorba6 ^ chorba5 ^ chorba4;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = next5 ^ out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;

        i += 32;

        /* 28-31 */
        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1 ^ chorba7 ^ chorba6 ^ chorba5;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2 ^ chorba8 ^ chorba7 ^ chorba6;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3 ^ a1 ^ chorba8 ^ chorba7;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4 ^ a2 ^ b1 ^ chorba8;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = next5 ^ out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;
    }

    for (; (i + 40 + 32) < len; i += 32) {
        uint64_t in1;
        uint64_t in2;
        uint64_t in3;
        uint64_t in4;
        uint64_t a1, a2, a3, a4;
        uint64_t b1, b2, b3, b4;
        uint64_t c1, c2, c3, c4;
        uint64_t d1, d2, d3, d4;

        uint64_t out1;
        uint64_t out2;
        uint64_t out3;
        uint64_t out4;
        uint64_t out5;

        in1 = Z_U64_FROM_LE(input[i / sizeof(uint64_t)]) ^ next1;
        in2 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 1]) ^ next2;

        a1 = (in1 << 17) ^ (in1 << 55);
        a2 = (in1 >> 47) ^ (in1 >> 9) ^ (in1 << 19);
        a3 = (in1 >> 45) ^ (in1 << 44);
        a4 = (in1 >> 20);

        b1 = (in2 << 17) ^ (in2 << 55);
        b2 = (in2 >> 47) ^ (in2 >> 9) ^ (in2 << 19);
        b3 = (in2 >> 45) ^ (in2 << 44);
        b4 = (in2 >> 20);

        in3 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 2]) ^ next3 ^ a1;
        in4 = Z_U64_FROM_LE(input[i / sizeof(uint64_t) + 3]) ^ next4 ^ a2 ^ b1;

        c1 = (in3 << 17) ^ (in3 << 55);
        c2 = (in3 >> 47) ^ (in3 >> 9) ^ (in3 << 19);
        c3 = (in3 >> 45) ^ (in3 << 44);
        c4 = (in3 >> 20);

        d1 = (in4 << 17) ^ (in4 << 55);
        d2 = (in4 >> 47) ^ (in4 >> 9) ^ (in4 << 19);
        d3 = (in4 >> 45) ^ (in4 << 44);
        d4 = (in4 >> 20);

        out1 = a3 ^ b2 ^ c1;
        out2 = a4 ^ b3 ^ c2 ^ d1;
        out3 = b4 ^ c3 ^ d2;
        out4 = c4 ^ d3;
        out5 = d4;

        next1 = next5 ^ out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;
    }

    memcpy(final, input+(i / sizeof(uint64_t)), len-i);
    final[0] ^= Z_U64_TO_LE(next1);
    final[1] ^= Z_U64_TO_LE(next2);
    final[2] ^= Z_U64_TO_LE(next3);
    final[3] ^= Z_U64_TO_LE(next4);
    final[4] ^= Z_U64_TO_LE(next5);

    return crc32_braid(~crc, (uint8_t*)final, len-i);
}

#else // OPTIMAL_CMP == 64

Z_INTERNAL uint32_t crc32_chorba_small_nondestructive_32bit(uint32_t crc, const uint32_t *input, size_t len) {
    uint32_t final[20] = {0};

    uint32_t next1 = ~crc;
    crc = 0;
    uint32_t next2 = 0;
    uint32_t next3 = 0;
    uint32_t next4 = 0;
    uint32_t next5 = 0;
    uint32_t next6 = 0;
    uint32_t next7 = 0;
    uint32_t next8 = 0;
    uint32_t next9 = 0;
    uint32_t next10 = 0;

    size_t i = 0;
    for (; i + 80 < len; i += 40) {
        uint32_t in1;
        uint32_t in2;
        uint32_t in3;
        uint32_t in4;
        uint32_t in5;
        uint32_t in6;
        uint32_t in7;
        uint32_t in8;
        uint32_t in9;
        uint32_t in10;

        uint32_t a1, a2, a3, a4, a6, a7;
        uint32_t b1, b2, b3, b4, b6, b7;
        uint32_t c1, c2, c3, c4, c6, c7;
        uint32_t d1, d2, d3, d4, d6, d7;
        uint32_t e1, e2, e3, e4, e6, e7;
        uint32_t f1, f2, f3, f4, f6, f7;
        uint32_t g1, g2, g3, g4, g6, g7;
        uint32_t h1, h2, h3, h4, h6, h7;
        uint32_t i1, i2, i3, i4, i6, i7;
        uint32_t j1, j2, j3, j4, j6, j7;

        uint32_t out1;
        uint32_t out2;
        uint32_t out3;
        uint32_t out4;
        uint32_t out5;
        uint32_t out6;
        uint32_t out7;
        uint32_t out8;
        uint32_t out9;
        uint32_t out10;

        in1 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 0]) ^ next1;
        in2 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 1]) ^ next2;
        in3 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 2]) ^ next3;
        in4 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 3]) ^ next4;

        a1 = (in1 << 17);
        a2 = (in1 >> 15) ^ (in1 << 23);
        a3 = (in1 >> 9) ^ (in1 << 19);
        a4 = (in1 >> 13);
        a6 = (in1 << 12);
        a7 = (in1 >> 20);

        b1 = (in2 << 17);
        b2 = (in2 >> 15) ^ (in2 << 23);
        b3 = (in2 >> 9) ^ (in2 << 19);
        b4 = (in2 >> 13);
        b6 = (in2 << 12);
        b7 = (in2 >> 20);

        c1 = (in3 << 17);
        c2 = (in3 >> 15) ^ (in3 << 23);
        c3 = (in3 >> 9) ^ (in3 << 19);
        c4 = (in3 >> 13);
        c6 = (in3 << 12);
        c7 = (in3 >> 20);

        d1 = (in4 << 17);
        d2 = (in4 >> 15) ^ (in4 << 23);
        d3 = (in4 >> 9) ^ (in4 << 19);
        d4 = (in4 >> 13);
        d6 = (in4 << 12);
        d7 = (in4 >> 20);

        in5 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 4]) ^ next5 ^ a1;
        in6 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 5]) ^ next6 ^ a2 ^ b1;
        in7 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 6]) ^ next7 ^ a3 ^ b2 ^ c1;
        in8 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 7]) ^ next8 ^ a4 ^ b3 ^ c2 ^ d1;

        e1 = (in5 << 17);
        e2 = (in5 >> 15) ^ (in5 << 23);
        e3 = (in5 >> 9) ^ (in5 << 19);
        e4 = (in5 >> 13);
        e6 = (in5 << 12);
        e7 = (in5 >> 20);

        f1 = (in6 << 17);
        f2 = (in6 >> 15) ^ (in6 << 23);
        f3 = (in6 >> 9) ^ (in6 << 19);
        f4 = (in6 >> 13);
        f6 = (in6 << 12);
        f7 = (in6 >> 20);

        g1 = (in7 << 17);
        g2 = (in7 >> 15) ^ (in7 << 23);
        g3 = (in7 >> 9) ^ (in7 << 19);
        g4 = (in7 >> 13);
        g6 = (in7 << 12);
        g7 = (in7 >> 20);

        h1 = (in8 << 17);
        h2 = (in8 >> 15) ^ (in8 << 23);
        h3 = (in8 >> 9) ^ (in8 << 19);
        h4 = (in8 >> 13);
        h6 = (in8 << 12);
        h7 = (in8 >> 20);

        in9 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 8]) ^ next9 ^ b4 ^ c3 ^ d2 ^ e1;
        in10 = Z_U32_FROM_LE(input[i/sizeof(uint32_t) + 9]) ^ next10 ^ a6 ^ c4 ^ d3 ^ e2 ^ f1;

        i1 = (in9 << 17);
        i2 = (in9 >> 15) ^ (in9 << 23);
        i3 = (in9 >> 9) ^ (in9 << 19);
        i4 = (in9 >> 13);
        i6 = (in9 << 12);
        i7 = (in9 >> 20);

        j1 = (in10 << 17);
        j2 = (in10 >> 15) ^ (in10 << 23);
        j3 = (in10 >> 9) ^ (in10 << 19);
        j4 = (in10 >> 13);
        j6 = (in10 << 12);
        j7 = (in10 >> 20);

        out1 = a7 ^ b6 ^ d4 ^ e3 ^ f2 ^ g1;
        out2 = b7 ^ c6 ^ e4 ^ f3 ^ g2 ^ h1;
        out3 = c7 ^ d6 ^ f4 ^ g3 ^ h2 ^ i1;
        out4 = d7 ^ e6 ^ g4 ^ h3 ^ i2 ^ j1;
        out5 = e7 ^ f6 ^ h4 ^ i3 ^ j2;
        out6 = f7 ^ g6 ^ i4 ^ j3;
        out7 = g7 ^ h6 ^ j4;
        out8 = h7 ^ i6;
        out9 = i7 ^ j6;
        out10 = j7;

        next1 = out1;
        next2 = out2;
        next3 = out3;
        next4 = out4;
        next5 = out5;
        next6 = out6;
        next7 = out7;
        next8 = out8;
        next9 = out9;
        next10 = out10;

    }

    memcpy(final, input+(i/sizeof(uint32_t)), len-i);
    final[0] ^= Z_U32_TO_LE(next1);
    final[1] ^= Z_U32_TO_LE(next2);
    final[2] ^= Z_U32_TO_LE(next3);
    final[3] ^= Z_U32_TO_LE(next4);
    final[4] ^= Z_U32_TO_LE(next5);
    final[5] ^= Z_U32_TO_LE(next6);
    final[6] ^= Z_U32_TO_LE(next7);
    final[7] ^= Z_U32_TO_LE(next8);
    final[8] ^= Z_U32_TO_LE(next9);
    final[9] ^= Z_U32_TO_LE(next10);

    return crc32_braid(~crc, (uint8_t*)final, len-i);
}
#endif // OPTIMAL_CMP == 64

Z_INTERNAL uint32_t crc32_chorba(uint32_t crc, const uint8_t *buf, size_t len) {
    uintptr_t align_diff = ALIGN_DIFF(buf, 8);
    if (len <= align_diff + CHORBA_SMALL_THRESHOLD)
        return crc32_braid(crc, buf, len);

    if (align_diff) {
        crc = crc32_braid(crc, buf, align_diff);
        len -= align_diff;
        buf += align_diff;
    }
    if (len > CHORBA_LARGE_THRESHOLD)
        return crc32_chorba_118960_nondestructive(crc, (const z_word_t*)buf, len);
#if OPTIMAL_CMP == 64
    if (len > CHORBA_MEDIUM_LOWER_THRESHOLD && len <= CHORBA_MEDIUM_UPPER_THRESHOLD)
        return crc32_chorba_32768_nondestructive(crc, (const uint64_t*)buf, len);
    return crc32_chorba_small_nondestructive(crc, (const uint64_t*)buf, len);
#else
    return crc32_chorba_small_nondestructive_32bit(crc, (const uint32_t*)buf, len);
#endif
}

uint32_t crc32_copy_chorba(uint32_t crc, uint8_t *dst, const uint8_t *src, size_t len) {
    crc = crc32_chorba(crc, src, len);
    memcpy(dst, src, len);
    return crc;
}
