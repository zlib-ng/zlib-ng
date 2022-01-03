/* test_gzio.c - Test read/write of .gz files */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_shared.h"

#define TESTFILE "foo.gz"

int main(int argc, char *argv[]) {
#ifdef NO_GZCOMPRESS
    fprintf(stderr, "NO_GZCOMPRESS -- gz* functions cannot compress\n");
#else
    const char *fname = (argc > 1 ? argv[1] : TESTFILE);
    uint8_t compr[128], uncompr[128];
    uint32_t compr_len = sizeof(compr), uncompr_len = sizeof(uncompr);
    size_t read;
    int64_t pos;
    gzFile file;
    int err;

    /* Write gz file with test data */
    file = PREFIX(gzopen)(fname, "wb");
    if (file == NULL)
        error("gzopen error\n");
    /* Write hello, hello! using gzputs and gzprintf */
    PREFIX(gzputc)(file, 'h');
    if (PREFIX(gzputs)(file, "ello") != 4)
        error("gzputs err: %s\n", PREFIX(gzerror)(file, &err));
    if (PREFIX(gzprintf)(file, ", %s!", "hello") != 8)
        error("gzprintf err: %s\n", PREFIX(gzerror)(file, &err));
    /* Write string null-teriminator using gzseek */
    if (PREFIX(gzseek)(file, 1L, SEEK_CUR) < 0)
        error("gzseek error, gztell=%ld\n", (long)PREFIX(gztell)(file));
    /* Write hello, hello! using gzfwrite using best compression level */
    if (PREFIX(gzsetparams)(file, Z_BEST_COMPRESSION, Z_DEFAULT_STRATEGY) != Z_OK)
        error("gzsetparams err: %s\n", PREFIX(gzerror)(file, &err));
    if (PREFIX(gzfwrite)(hello, hello_len, 1, file) == 0)
        error("gzfwrite err: %s\n", PREFIX(gzerror)(file, &err));
    /* Flush compressed bytes to file */
    if (PREFIX(gzflush)(file, Z_SYNC_FLUSH) != Z_OK)
        error("gzflush err: %s\n", PREFIX(gzerror)(file, &err));
    compr_len = (uint32_t)PREFIX(gzoffset)(file);
    if (compr_len <= 0)
        error("gzoffset err: %s\n", PREFIX(gzerror)(file, &err));
    PREFIX(gzclose)(file);

    /* Open gz file we previously wrote */
    file = PREFIX(gzopen)(fname, "rb");
    if (file == NULL)
        error("gzopen error\n");

    /* Read uncompressed data - hello, hello! string twice */
    strcpy((char*)uncompr, "garbages");
    if (PREFIX(gzread)(file, uncompr, (unsigned)uncompr_len) != (int)(hello_len + hello_len))
        error("gzread err: %s\n", PREFIX(gzerror)(file, &err));
    if (strcmp((char*)uncompr, hello))
        error("bad gzread: %s\n", (char*)uncompr);
    else
        printf("gzread(): %s\n", (char*)uncompr);
    /* Check position at the end of the gz file */
    if (PREFIX(gzeof)(file) != 1)
        error("gzeof err: not reporting end of stream\n");

    /* Seek backwards mid-string and check char reading with gzgetc and gzungetc */
    pos = PREFIX(gzseek)(file, -22L, SEEK_CUR);
    if (pos != 6 || PREFIX(gztell)(file) != pos)
        error("gzseek error, pos=%ld, gztell=%ld\n", (long)pos, (long)PREFIX(gztell)(file));
    if (PREFIX(gzgetc)(file) != ' ')
        error("gzgetc error\n");
    if (PREFIX(gzungetc)(' ', file) != ' ')
        error("gzungetc error\n");
    /* Read first hello, hello! string with gzgets */
    strcpy((char*)uncompr, "garbages");
    PREFIX(gzgets)(file, (char*)uncompr, (int)uncompr_len);
    if (strlen((char*)uncompr) != 7) /* " hello!" */
        error("gzgets err after gzseek: %s\n", PREFIX(gzerror)(file, &err));
    if (strcmp((char*)uncompr, hello + 6))
        error("bad gzgets after gzseek\n");
    else
        printf("gzgets() after gzseek: %s\n", (char*)uncompr);
    /* Seek to second hello, hello! string */
    pos = PREFIX(gzseek)(file, 14L, SEEK_SET);
    if (pos != 14 || PREFIX(gztell)(file) != pos)
        error("gzseek error, pos=%ld, gztell=%ld\n", (long)pos, (long)PREFIX(gztell)(file));
    /* Check position not at end of file */
    if (PREFIX(gzeof)(file) != 0)
        error("gzeof err: reporting end of stream\n");
    /* Read first hello, hello! string with gzfread */
    strcpy((char*)uncompr, "garbages");
    read = PREFIX(gzfread)(uncompr, uncompr_len, 1, file);
    if (strcmp((const char *)uncompr, hello) != 0)
        error("bad gzgets\n");
    else
        printf("gzgets(): %s\n", (char*)uncompr);
    pos = PREFIX(gzoffset)(file);
    if (pos < 0 || pos != (compr_len + 10))
        error("gzoffset err: wrong offset at end\n");
    /* Trigger an error and clear it with gzclearerr */
    PREFIX(gzfread)(uncompr, (size_t)-1, (size_t)-1, file);
    PREFIX(gzerror)(file, &err);
    if (err == 0)
        error("gzerror err: no error returned\n");
    PREFIX(gzclearerr)(file);
    PREFIX(gzerror)(file, &err);
    if (err != 0)
        error("gzclearerr err: not zero %d\n", err);

    PREFIX(gzclose)(file);

    if (PREFIX(gzclose)(NULL) != Z_STREAM_ERROR)
        error("gzclose unexpected return when handle null\n");
    Z_UNUSED(read);
#endif
    return 0;
}
