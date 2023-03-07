#include "zbuild.h"

#include "zlib-ng.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int valid_wbits[] = {
    -9, -10, -11, -12, -13, -14, -15,
    8, 9, 10, 11, 12, 13, 14, 15,
    25, 26, 27, 28, 29, 30, 31,
};

const int valid_compression_levels[] = {
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9
};

const int valid_mem_levels[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9,
};

const int valid_strategies[] = {
    Z_FILTERED, 
    Z_HUFFMAN_ONLY,
    Z_RLE,
    Z_FIXED,
    Z_DEFAULT_STRATEGY,
};


int main (int argc, char *argv[])  {
    zng_stream zst_deflate;
    zng_stream zst_inflate;
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
    uint8_t data_in[4096];
    uint8_t compressed_out[8192];
    uint8_t decompressed_out[8192];
    FILE *in_file = fopen(argv[1], "r");   
    size_t read_size = fread(data_in, 1, 4096, in_file);
    fclose(in_file);
    if (!read_size) {
        fprintf(stderr, "File is empty or error: %s", argv[1]);
        return EXIT_FAILURE;
    }
    for (int wbits_index=0; wbits_index < 22; wbits_index+=1) {
        for (int complevel_index=0; complevel_index < 11; complevel_index+=1) {
            for (int memlevel_index=0; memlevel_index < 9; memlevel_index+=1) {
                for (int strat_index=0; strat_index < 5; strat_index+=1) {
                    memset(&zst_deflate, 0, sizeof(zng_stream));
                    memset(&zst_inflate, 0, sizeof(zng_stream));
                    int level = valid_compression_levels[complevel_index];
                    int wbits = valid_wbits[wbits_index];
                    int memlevel  = valid_mem_levels[memlevel_index];
                    int strategy = valid_strategies[strat_index];

                    int err = zng_deflateInit2(
                        &zst_deflate, level, Z_DEFLATED, wbits, memlevel, strategy);
                    if (err != Z_OK) {
                        fprintf(stderr, error_format, 
                            "deflateInit2", level, wbits, memlevel, strategy, 
                            zst_deflate.msg);
                        return EXIT_FAILURE; 
                    }
                    zst_deflate.next_in = data_in;
                    zst_deflate.avail_in = read_size;
                    zst_deflate.next_out = compressed_out;
                    zst_deflate.avail_out = 8192;
                    err = zng_deflate(&zst_deflate, Z_FINISH);
                    if (err != Z_STREAM_END) {
                        fprintf(stderr, error_format, 
                            "deflate", level, wbits, memlevel, strategy, 
                            zst_deflate.msg);
                        return EXIT_FAILURE; 
                    }
                    err = zng_inflateInit2(&zst_inflate, wbits);
                    if (err != Z_OK) {
                        fprintf(stderr, error_format, 
                            "inlateInit2", level, wbits, memlevel, strategy, err,
                            zst_inflate.msg);
                        return EXIT_FAILURE; 
                    }
                    zst_inflate.next_in = compressed_out;
                    zst_inflate.avail_in = 8192 - zst_deflate.avail_out;
                    zst_inflate.next_out = decompressed_out;
                    zst_inflate.avail_out = 8192;
                    err = zng_inflate(&zst_inflate, Z_FINISH);
                    if (err != Z_STREAM_END) {
                        fprintf(stderr, error_format, 
                            "inlate", level, wbits, memlevel, strategy, err,
                            zst_inflate.msg);
                        return EXIT_FAILURE; 
                    }
                    if (((8192 - zst_inflate.avail_out) != read_size) || 
                        memcmp(data_in, decompressed_out, read_size) !=0) 
                    {
                        fprintf(stderr, error_format, 
                            "Input and output is not the same", 
                            level, wbits, memlevel, strategy, 0, "");
                            fprintf(
                                stderr, 
                                "in bytes: %lu\n"
                                "out_bytes:%d\n"
                                "compare_result:%d", 
                                read_size, 8192-zst_inflate.avail_out, 
                                memcmp(data_in, decompressed_out, read_size));
                        return EXIT_FAILURE;
                    }
                    err = zng_deflateEnd(&zst_deflate);
                    if (err != Z_OK) {
                        printf("%d", zst_deflate.avail_out);
                        fprintf(stderr, error_format, 
                            "deflateEnd", level, wbits, memlevel, strategy, err,
                            zst_deflate.msg);
                        return EXIT_FAILURE; 
                    }
                    err = zng_inflateEnd(&zst_inflate);
                    if (err != Z_OK) {
                        fprintf(stderr, error_format, 
                            "inflateEnd", level, wbits, memlevel, strategy, err,
                            zst_inflate.msg);
                        return EXIT_FAILURE; 
                    }
                }
            }
        }
    }
    return EXIT_SUCCESS;
}