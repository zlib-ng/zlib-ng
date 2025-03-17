/* parameterized_tester.c - Test deflate() and inflate() on a chunk of the
 * input data file, combining many deflate() settings to improve testing coverage.
 */
#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdio.h>

/*
 * WITH_EXTRA_TESTS results in 9450 permutations per input file.
 * Normal mode results in 540.
 *
 * WITH_EXTRA_TESTS also tests a bigger chunk of the files.
 */

#ifdef WITH_EXTRA_TESTS
const int valid_wbits[] = {
    -9, -10, -11, -12, -13, -14, -15,
     9,  10,  11,  12,  13,  14,  15,
#  ifdef WITH_GZFILEOP
    25,  26,  27,  28,  29,  30,  31,
#  endif
};

const int valid_compression_levels[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9
};

const int valid_mem_levels[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9,
};

#define MAX_FILESIZE_KB 4096

#  ifdef WITH_GZFILEOP
#    define NUM_W 21
#  else
#    define NUM_W 14
#  endif
#  define NUM_C 10
#  define NUM_M 9

#else // !WITH_EXTRA_TESTS

const int valid_wbits[] = {
    -9, -15,
     9,  15,
#  ifdef WITH_GZFILEOP
    25,  31,
#  endif
};

const int valid_compression_levels[] = {
    0, 1, 2, 4, 6, 9
};

const int valid_mem_levels[] = {
    1, 8, 9,
};

#define MAX_FILESIZE_KB 128

#  ifdef WITH_GZFILEOP
#    define NUM_W 6
#  else
#    define NUM_W 4
#  endif
#  define NUM_C 6
#  define NUM_M 3
#endif

#define MAX_FILESIZE MAX_FILESIZE_KB * 1024
#define MAX_OUTSIZE MAX_FILESIZE_KB * (1024 + 128)

const int valid_strategies[] = {
    Z_FILTERED,
    Z_HUFFMAN_ONLY,
    Z_RLE,
    Z_FIXED,
    Z_DEFAULT_STRATEGY,
};

int main (int argc, char *argv[])  {
    PREFIX3(stream) c_stream;
    PREFIX3(stream) d_stream;
    uint8_t data_in[MAX_FILESIZE];
    uint8_t compressed_out[MAX_OUTSIZE];
    uint8_t decompressed_out[MAX_OUTSIZE];
    const char *error_format = "Error at %s\n"
                               "level: %d\n"
                               "wbits: %d\n"
                               "memLevel: %d\n"
                               "strategy: %d\n"
                               "exit: %d\n"
                               "message: %s\n";

    if (argc != 2) {
        fputs("A data input must be given as argument", stderr);
        return EXIT_FAILURE;
    }

    if ((argc == 2 && strcmp(argv[1], "--help") == 0)) {
        printf("Usage: test_parameterized <FILENAME>\n\n");
        return 0;
    }

    FILE *in_file = fopen(argv[1], "r");
    uint32_t read_size = (uint32_t)fread(data_in, 1, MAX_FILESIZE, in_file);
    fclose(in_file);

    if (!read_size) {
        fprintf(stderr, "File is empty or error: %s", argv[1]);
        return EXIT_FAILURE;
    }

    for (int wbits_index = 0; wbits_index < NUM_W; wbits_index += 1) {
        for (int complevel_index = 0; complevel_index < NUM_C; complevel_index += 1) {
            for (int memlevel_index = 0; memlevel_index < NUM_M; memlevel_index += 1) {
                for (int strat_index = 0; strat_index < 5; strat_index += 1) {

                    int level = valid_compression_levels[complevel_index];
                    int wbits = valid_wbits[wbits_index];
                    int memlevel  = valid_mem_levels[memlevel_index];
                    int strategy = valid_strategies[strat_index];
                    memset(&c_stream, 0, sizeof(PREFIX3(stream)));
                    memset(&d_stream, 0, sizeof(PREFIX3(stream)));

                    int err = PREFIX(deflateInit2)(&c_stream, level, Z_DEFLATED, wbits, memlevel, strategy);
                    if (err != Z_OK) {
                        fprintf(stderr, error_format, "deflateInit2", level, wbits, memlevel, strategy, c_stream.msg);
                        return EXIT_FAILURE;
                    }

                    c_stream.next_in = data_in;
                    c_stream.avail_in = read_size;
                    c_stream.next_out = compressed_out;
                    c_stream.avail_out = MAX_OUTSIZE;

                    err = PREFIX(deflate)(&c_stream, Z_FINISH);
                    if (err != Z_STREAM_END) {
                        fprintf(stderr, error_format, "deflate", level, wbits, memlevel, strategy, c_stream.msg);
                        return EXIT_FAILURE;
                    }

                    err = PREFIX(inflateInit2)(&d_stream, wbits);
                    if (err != Z_OK) {
                        fprintf(stderr, error_format, "inflateInit2", level, wbits, memlevel, strategy, err, d_stream.msg);
                        return EXIT_FAILURE;
                    }

                    d_stream.next_in = compressed_out;
                    d_stream.avail_in = MAX_OUTSIZE - c_stream.avail_out;
                    d_stream.next_out = decompressed_out;
                    d_stream.avail_out = MAX_OUTSIZE;

                    err = PREFIX(inflate)(&d_stream, Z_FINISH);
                    if (err != Z_STREAM_END) {
                        fprintf(stderr, error_format, "inflate", level, wbits, memlevel, strategy, err, d_stream.msg);
                        return EXIT_FAILURE;
                    }

                    if (((MAX_OUTSIZE - d_stream.avail_out) != read_size) || memcmp(data_in, decompressed_out, read_size) !=0) {
                        fprintf(stderr, error_format, "Input and output is not the same", level, wbits, memlevel, strategy, 0, "");
                        fprintf(stderr,
                                "in bytes: %u\n"
                                "out_bytes:%u\n"
                                "compare_result:%d\n",
                                read_size, MAX_OUTSIZE - d_stream.avail_out,
                                memcmp(data_in, decompressed_out, read_size));
                        return EXIT_FAILURE;
                    }

                    err = PREFIX(deflateEnd)(&c_stream);
                    if (err != Z_OK) {
                        printf("%d", c_stream.avail_out);
                        fprintf(stderr, error_format, "deflateEnd", level, wbits, memlevel, strategy, err, c_stream.msg);
                        return EXIT_FAILURE;
                    }

                    err = PREFIX(inflateEnd)(&d_stream);
                    if (err != Z_OK) {
                        fprintf(stderr, error_format, "inflateEnd", level, wbits, memlevel, strategy, err, d_stream.msg);
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
