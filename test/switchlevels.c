/* Compresses a user-specified number of chunks from stdin into stdout as a single gzip stream.
 * Each chunk is compressed with a user-specified level.
 */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_all(unsigned char *buf, size_t size)
{
    for (size_t total_read = 0; total_read < size;) {
        size_t n_read = fread(buf + total_read, 1, size - total_read, stdin);
        if (ferror(stdin)) {
            perror("fread\n");
            return 1;
        }
        if (n_read == 0) {
            fprintf(stderr, "Premature EOF\n");
            return 1;
        }
        total_read += n_read;
    }
    return 0;
}

static int write_all(unsigned char *buf, size_t size)
{
    for (size_t total_written = 0; total_written < size;) {
        size_t n_written = fwrite(buf + total_written, 1, size - total_written, stdout);
        if (ferror(stdout)) {
            perror("fwrite\n");
            return 1;
        }
        total_written += n_written;
    }
    return 0;
}

static int compress_chunk(PREFIX3(stream) *strm, int level, int size, int last)
{
    int ret = 1;
    int err = PREFIX(deflateParams)(strm, level, Z_DEFAULT_STRATEGY);
    if (err != Z_OK) {
        fprintf(stderr, "deflateParams() failed with code %d\n", err);
        goto done;
    }
    unsigned long compsize = 100 + 2 * PREFIX(deflateBound)(strm, size);
    unsigned char *buf = malloc(size + compsize);
    if (buf == NULL) {
        fprintf(stderr, "Out of memory\n");
        goto done;
    }
    if (read_all(buf, size) != 0) {
        goto free_buf;
    }
    strm->next_in = buf;
    strm->avail_in = size;
    strm->next_out = buf + size;
    strm->avail_out = compsize;
    err = PREFIX(deflate)(strm, last ? Z_FINISH : Z_SYNC_FLUSH);
    if ((!last && err != Z_OK) || (last && err != Z_STREAM_END)) {
        fprintf(stderr, "deflate() failed with code %d\n", err);
        goto free_buf;
    }
    if (strm->avail_in != 0) {
        fprintf(stderr, "deflate() did not consume %d bytes of input\n", strm->avail_in);
        goto free_buf;
    }
    if (write_all(buf + size, compsize - strm->avail_out) != 0) {
        goto free_buf;
    }
    ret = 0;
free_buf:
    free(buf);
done:
    return ret;
}

/* ===========================================================================
 * Usage:  switchlevels level1 size1 [level2 size2 ...]
 */

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    PREFIX3(stream) strm;
    memset(&strm, 0, sizeof(strm));
    int err = PREFIX(deflateInit2)(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    if (err != Z_OK) {
        fprintf(stderr, "deflateInit() failed with code %d\n", err);
        goto done;
    }
    for (int i = 1; i < argc - 1; i += 2) {
        int level = atoi(argv[i]);
        int size = atoi(argv[i + 1]);
        if (compress_chunk(&strm, level, size, i + 2 >= argc - 1) != 0) {
            goto deflate_end;
        }
    }
    ret = EXIT_SUCCESS;
deflate_end:
    PREFIX(deflateEnd)(&strm);
done:
    return ret;
}
