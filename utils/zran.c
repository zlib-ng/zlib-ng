/* zran.c -- example of deflate stream indexing and random access
 * Copyright (C) 2005, 2012, 2018, 2023, 2024, 2025 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 * Version 1.7  16 May 2025  Mark Adler */

// Illustrate the use of Z_BLOCK, inflatePrime(), and inflateSetDictionary()
// for random access of a compressed file. A file containing a raw deflate
// stream is provided on the command line. The compressed stream is decoded in
// its entirety, and an index built with access points about every SPAN bytes
// in the uncompressed output. The compressed file is left open, and can then
// be read randomly, having to decompress on the average SPAN/2 uncompressed
// bytes before getting to the desired block of data.
//
// An access point can be created at the start of any deflate block, by saving
// the starting file offset and bit of that block, and the 32K bytes of
// uncompressed data that precede that block. Also the uncompressed offset of
// that block is saved to provide a reference for locating a desired starting
// point in the uncompressed stream. deflate_index_build() decompresses the
// input raw deflate stream a block at a time, and at the end of each block
// decides if enough uncompressed data has gone by to justify the creation of a
// new access point. If so, that point is saved in a data structure that grows
// as needed to accommodate the points.
//
// To use the index, an offset in the uncompressed data is provided, for which
// the latest access point at or preceding that offset is located in the index.
// The input file is positioned to the specified location in the index, and if
// necessary the first few bits of the compressed data is read from the file.
// inflate is initialized with those bits and the 32K of uncompressed data, and
// decompression then proceeds until the desired offset in the file is reached.
// Then decompression continues to read the requested uncompressed data from
// the file.
//
// There is some fair bit of overhead to starting inflation for the random
// access, mainly copying the 32K byte dictionary. If small pieces of the file
// are being accessed, it would make sense to implement a cache to hold some
// lookahead to avoid many calls to deflate_index_extract() for small lengths.
//
// Another way to build an index would be to use inflateCopy(). That would not
// be constrained to have access points at block boundaries, but would require
// more memory per access point, and could not be saved to a file due to the
// use of pointers in the state. The approach here allows for storage of the
// index in a file.

#if !defined(_FILE_OFFSET_BITS) && (!defined(__ANDROID_API__) || __ANDROID_API__ > 23)
#  define _FILE_OFFSET_BITS 64
#endif

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#  include "zlib.h"
#else
#  include "zlib-ng.h"
#endif
#include "zran.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define WINSIZE 32768U      // sliding window size
#define CHUNK 16384         // file input buffer size

// See comments in zran.h.
void deflate_index_free(struct deflate_index *index) {
    if (index != NULL) {
        while (index->have)
            free(index->list[--index->have].window);
        free(index->list);
        index->list = NULL;
        PREFIX(inflateEnd)(&index->strm);
        free(index);
    }
}

// Add an access point to the list. If out of memory, return NULL. index->mode
// is temporarily the allocated number of access points, until it is time for
// deflate_index_build() to return. Then index->mode is set to the mode of
// inflation.
static struct deflate_index *add_point(struct deflate_index *index, z_off64_t in,
                                       z_off64_t out, z_off64_t beg,
                                       unsigned char *window) {
    if (index->have == index->mode) {
        // The list is full. Make it bigger.
        if (index->mode > INT_MAX / 2 || (size_t)(index->mode << 1) > SIZE_MAX / sizeof(point_t))
            return NULL; // Will overflow the int!
        index->mode = index->mode ? index->mode << 1 : 8;
        point_t *next = realloc(index->list, sizeof(point_t) * index->mode);
        if (next == NULL)
            return NULL;
        index->list = next;
    }

    // Fill in the access point and increment how many we have.
    point_t *next = (point_t *)(index->list) + index->have;
    next->out = out;
    next->in = in;
    next->bits = index->strm.data_type & 7;
    next->dict = (uint64_t)(out - beg) > WINSIZE ? WINSIZE : (unsigned)(out - beg);
    next->window = NULL;
    if (next->dict) {
        next->window = malloc(next->dict);
        if (next->window == NULL)
            return NULL;
        unsigned recent = WINSIZE - index->strm.avail_out;
        unsigned copy = recent > next->dict ? next->dict : recent;
        memcpy(next->window + next->dict - copy, window + recent - copy, copy);
        copy = next->dict - copy;
        memcpy(next->window, window + WINSIZE - copy, copy);
    }

    index->have++;

    // Return the index, which may have been newly allocated or destroyed.
    return index;
}

// Decompression modes. These are the inflateInit2() windowBits parameter.
#define RAW -15
#define ZLIB 15
#define GZIP 31

// See comments in zran.h.
int deflate_index_build(FILE *in, z_off64_t span, struct deflate_index **built) {
    // If this returns with an error, any attempt to use the index will cleanly
    // return an error.
    *built = NULL;

    // Create and initialize the index list.
    struct deflate_index *index = malloc(sizeof(struct deflate_index));
    if (index == NULL)
        return Z_MEM_ERROR;
    index->have = 0;
    index->mode = 0;            // entries in index->list allocation
    index->list = NULL;
    index->strm.state = NULL; // so inflateEnd() can work
    index->strm.zalloc = NULL;
    index->strm.zfree = NULL;
    index->strm.opaque = NULL;

    // Set up the inflation state.
    index->strm.avail_in = 0;
    index->strm.avail_out = 0;
    unsigned char *buf;        // input buffer
    unsigned char *win;        // output sliding window
    z_off64_t totin = 0;       // total bytes read from input
    z_off64_t totout = 0;      // total bytes uncompressed
    z_off64_t beg = 0;         // starting offset of last history reset
    int mode = 0;              // mode: RAW, ZLIB, or GZIP (0 => not set yet)

    buf = malloc(CHUNK);
    if (buf == NULL) {
        deflate_index_free(index);
        return Z_MEM_ERROR;
    }

    win = malloc(WINSIZE);
    if (win == NULL) {
       free(buf);
       deflate_index_free(index);
       return Z_MEM_ERROR;
    }

    memset(win, 0, WINSIZE);

    // Decompress from in, generating access points along the way.
    int ret = 0;                // the return value from zlib, or Z_ERRNO
    z_off64_t last = 0;         // last access point uncompressed offset
    do {
        // Assure available input, at least until reaching EOF.
        if (index->strm.avail_in == 0) {
            index->strm.avail_in = (uint32_t)fread(buf, 1, CHUNK, in);
            totin += index->strm.avail_in;
            index->strm.next_in = buf;
            if (index->strm.avail_in < CHUNK && ferror(in)) {
                ret = Z_ERRNO;
                break;
            }

            if (mode == 0) {
                // At the start of the input -- determine the type. Assume raw
                // if it is neither zlib nor gzip. This could in theory result
                // in a false positive for zlib, but in practice the fill bits
                // after a stored block are always zeros, so a raw stream won't
                // start with an 8 in the low nybble.
                mode = index->strm.avail_in == 0 ? RAW :    // will fail
                       (index->strm.next_in[0] & 0xf) == 8 ? ZLIB :
                       index->strm.next_in[0] == 0x1f ? GZIP :
                       /* else */ RAW;
                index->strm.zalloc = Z_NULL;
                index->strm.zfree = Z_NULL;
                index->strm.opaque = Z_NULL;
                ret = PREFIX(inflateInit2)(&index->strm, mode);
                if (ret != Z_OK)
                    break;
            }
        }

        // Assure available output. This rotates the output through, for use as
        // a sliding window on the uncompressed data.
        if (index->strm.avail_out == 0) {
            index->strm.avail_out = WINSIZE;
            index->strm.next_out = win;
        }

        if (mode == RAW && index->have == 0)
            // We skip the inflate() call at the start of raw deflate data in
            // order generate an access point there. Set data_type to imitate
            // the end of a header.
            index->strm.data_type = 0x80;
        else {
            // Inflate and update the number of uncompressed bytes.
            unsigned before = index->strm.avail_out;
            ret = PREFIX(inflate)(&index->strm, Z_BLOCK);
            totout += before - index->strm.avail_out;
        }

        if ((index->strm.data_type & 0xc0) == 0x80 &&
            (index->have == 0 || totout - last >= span)) {
            // We are at the end of a header or a non-last deflate block, so we
            // can add an access point here. Furthermore, we are either at the
            // very start for the first access point, or there has been span or
            // more uncompressed bytes since the last access point, so we want
            // to add an access point here.
            struct deflate_index *_index = add_point(index, totin - index->strm.avail_in, totout, beg, win);
            if (_index == NULL) {
                ret = Z_MEM_ERROR;
                break;
            }
            index = _index;
            last = totout;
        }

        if (ret == Z_STREAM_END && mode == GZIP) {
            if (index->strm.avail_in || ungetc(getc(in), in) != EOF) {
                // There is more input after the end of a gzip member. Reset the
                // inflate state to read another gzip member. On success, this will
                // set ret to Z_OK to continue decompressing.
                ret = PREFIX(inflateReset2)(&index->strm, GZIP);
                beg = totout;           // reset history
            } else if (ferror(in))
                ret = Z_ERRNO;
        }

        // Keep going until Z_STREAM_END or error. If the compressed data ends
        // prematurely without a file read error, Z_BUF_ERROR is returned.
    } while (ret == Z_OK);

    if (ret != Z_STREAM_END) {
        // An error was encountered. Discard the index and return a negative
        // error code.
        free(win);
        free(buf);
        deflate_index_free(index);
        return ret == Z_NEED_DICT ? Z_DATA_ERROR : ret;
    }

    free(win);
    free(buf);

    // Return the index.
    index->mode = mode;
    index->length = totout;
    *built = index;
    return index->have;
}

// See comments in zran.h.
ptrdiff_t deflate_index_extract(FILE *in, struct deflate_index *index,
                                z_off64_t offset, unsigned char *buf, size_t len) {
    // Do a quick sanity check on the index.
    if (index == NULL || index->have < 1 || index->list[0].out != 0 ||
        index->strm.state == Z_NULL)
        return Z_STREAM_ERROR;

    // If nothing to extract, return zero bytes extracted.
    if (len == 0 || offset < 0 || offset >= index->length)
        return 0;

    // Find the access point closest to but not after offset.
    int lo = -1, hi = index->have;
    point_t *point = index->list;
    while (hi - lo > 1) {
        int mid = (lo + hi) >> 1;
        if (offset < point[mid].out)
            hi = mid;
        else
            lo = mid;
    }
    point += lo;

    // Initialize the input file and prime the inflate engine to start there.
#ifdef _MSC_VER
    int ret = _fseeki64(in, point->in - (point->bits ? 1 : 0), SEEK_SET);
#else
    int ret = fseeko(in, point->in - (point->bits ? 1 : 0), SEEK_SET);
#endif
    if (ret != 0)
        return Z_ERRNO;
    int ch = 0;
    if (point->bits && (ch = getc(in)) == EOF)
        return ferror(in) ? Z_ERRNO : Z_BUF_ERROR;
    index->strm.avail_in = 0;
    ret = PREFIX(inflateReset2)(&index->strm, RAW);
    if (ret != Z_OK)
        return ret;
    if (point->bits)
        PREFIX(inflatePrime)(&index->strm, point->bits, ch >> (8 - point->bits));
    if (point->dict)
        PREFIX(inflateSetDictionary)(&index->strm, point->window, point->dict);

    // Skip uncompressed bytes until offset reached, then satisfy request.
    unsigned char *input;
    unsigned char *discard;

    input = malloc(CHUNK);
    if (input == NULL)
        return Z_MEM_ERROR;

    discard = malloc(WINSIZE);
    if (discard == NULL) {
       free(input);
       return Z_MEM_ERROR;
    }

    offset -= point->out;       // number of bytes to skip to get to offset
    size_t left = len;          // number of bytes left to read after offset
    do {
        if (offset) {
            // Discard up to offset uncompressed bytes.
            index->strm.avail_out = (uint64_t)offset < WINSIZE ? (unsigned)offset : WINSIZE;
            index->strm.next_out = discard;
        }
        else {
            // Uncompress up to left bytes into buf.
            index->strm.avail_out = left < (unsigned)-1 ? (unsigned)left :
                                                          (unsigned)-1;
            index->strm.next_out = buf + len - left;
        }

        // Uncompress, setting got to the number of bytes uncompressed.
        if (index->strm.avail_in == 0) {
            // Assure available input.
            index->strm.avail_in = (uint32_t)fread(input, 1, CHUNK, in);
            if (index->strm.avail_in < CHUNK && ferror(in)) {
                ret = Z_ERRNO;
                break;
            }
            index->strm.next_in = input;
        }
        unsigned got = index->strm.avail_out;
        ret = PREFIX(inflate)(&index->strm, Z_NO_FLUSH);
        got -= index->strm.avail_out;

        // Update the appropriate count.
        if (offset)
            offset -= got;
        else {
            left -= got;
            if (left == 0)
                // Request satisfied.
                break;
        }

        // If we're at the end of a gzip member and there's more to read,
        // continue to the next gzip member.
        if (ret == Z_STREAM_END && index->mode == GZIP) {
            // Discard the gzip trailer.
            unsigned drop = 8;              // length of gzip trailer
            if (index->strm.avail_in >= drop) {
                index->strm.avail_in -= drop;
                index->strm.next_in += drop;
            }
            else {
                // Read and discard the remainder of the gzip trailer.
                drop -= index->strm.avail_in;
                index->strm.avail_in = 0;
                do {
                    if (getc(in) == EOF) {
                        // The input does not have a complete trailer.
                        free(discard);
                        free(input);
                        return ferror(in) ? Z_ERRNO : Z_BUF_ERROR;
                    }
                } while (--drop);
            }

            ch = index->strm.avail_in ? 0 : getc(in);
            if (index->strm.avail_in ||
                (ch != EOF && ungetc(ch, in) != EOF)) {
                // There's more after the gzip trailer. Use inflate to skip the
                // gzip header and resume the raw inflate there.
                PREFIX(inflateReset2)(&index->strm, GZIP);
                do {
                    if (index->strm.avail_in == 0) {
                        index->strm.avail_in = (uint32_t)fread(input, 1, CHUNK, in);
                        if (index->strm.avail_in < CHUNK && ferror(in)) {
                            ret = Z_ERRNO;
                            break;
                        }
                        index->strm.next_in = input;
                    }
                    index->strm.avail_out = WINSIZE;
                    index->strm.next_out = discard;
                    ret = PREFIX(inflate)(&index->strm, Z_BLOCK);  // stop after header
                } while (ret == Z_OK && (index->strm.data_type & 0x80) == 0);
                if (ret != Z_OK)
                    break;
                PREFIX(inflateReset2)(&index->strm, RAW);
            } else if (ferror(in)) {
                ret = Z_ERRNO;
                break;
            }
        }

        // Continue until we have the requested data, the deflate data has
        // ended, or an error is encountered.
    } while (ret == Z_OK);

    free(discard);
    free(input);
    // Return the number of uncompressed bytes read into buf, or the error.
    return ret == Z_OK || ret == Z_STREAM_END ? (ptrdiff_t)(len - left) : ret;
}

#define SPAN 1048576L       // desired distance between access points
#define LEN 16384           // number of bytes to extract

// Demonstrate the use of deflate_index_build() and deflate_index_extract() by
// indexing the file given on the command line, then extracting `length`
// uncompressed bytes starting at `offset` and writing them to stdout.
int main(int argc, char **argv) {
    // Open the input file.
    if (argc != 4) {
        fprintf(stderr, "usage: zran file.raw offset length\n");
        return 1;
    }
    SET_BINARY_MODE(stdout);
    FILE *in = fopen(argv[1], "rb");
    if (in == NULL) {
        fprintf(stderr, "zran: could not open %s for reading\n", argv[1]);
        return 1;
    }

    // Get offset.
    z_off64_t offset;
    char *end;
    errno = 0;
    offset = strtoll(argv[2], &end, 10);
    if (*end || end == argv[2] || offset < 0 || errno == ERANGE) {
        fclose(in);
        fprintf(stderr, "zran: %s is not a valid offset\n", argv[2]);
        return 1;
    }

    // Get length
    size_t length = 0;
    unsigned long long _length = 0; // Temporary to handle 32-bit targets
    errno = 0;
    if (strtoll(argv[3], &end, 10) > 0) { // Check for negative values first
       errno = 0;
       _length = strtoull(argv[3], &end, 10);
    }
    if (*end || end == argv[3] || _length == 0 || errno == ERANGE || _length > SIZE_MAX) {
        fclose(in);
        fprintf(stderr, "zran: %s is not a valid length\n", argv[3]);
        return 1;
    }
    length = (size_t)_length;

    // Build index.
    struct deflate_index *index = NULL;
    int len = deflate_index_build(in, SPAN, &index);
    if (len < 0) {
        fclose(in);
        switch (len) {
        case Z_MEM_ERROR:
            fprintf(stderr, "zran: out of memory\n");
            break;
        case Z_BUF_ERROR:
            fprintf(stderr, "zran: %s ended prematurely\n", argv[1]);
            break;
        case Z_DATA_ERROR:
            fprintf(stderr, "zran: compressed data error in %s\n", argv[1]);
            break;
        case Z_ERRNO:
            fprintf(stderr, "zran: read error on %s\n", argv[1]);
            break;
        default:
            fprintf(stderr, "zran: error %d while building index\n", len);
        }
        return 1;
    }
    fprintf(stderr, "zran: built index with %d access points\n", len);

    // Use index by reading requested bytes from specified offset.
    if (index == NULL || offset >= index->length) {
        fprintf(stderr, "zran: %" PRId64 " is not a valid offset\n", (int64_t)offset);
        deflate_index_free(index);
        fclose(in);
        return 1;
    }
    size_t requested = length;
    if ((uint64_t)(index->length - offset) < length)
        length = (size_t)(index->length - offset);
    unsigned char *buf = malloc(length);
    if (buf == NULL) {
       fprintf(stderr, "zran: out of memory\n");
       deflate_index_free(index);
       fclose(in);
       return 1;
    }
    ptrdiff_t got = deflate_index_extract(in, index, offset, buf, length);
    int status = 0;
    if (got < 0) {
        fprintf(stderr, "zran: extraction failed: %s error\n",
                got == Z_MEM_ERROR ? "out of memory" :
                got == Z_ERRNO ? "read" :
                got == Z_DATA_ERROR || got == Z_BUF_ERROR ? "input corrupted" :
                "internal");
        status = 1;
    } else {
        size_t written = fwrite(buf, 1, got, stdout);
        if (written == (size_t)got && fflush(stdout) == 0) {
           fprintf(stderr, "zran: extracted %" PRId64 " of %" PRIu64 " bytes at %" PRId64 "\n", (int64_t)got, (uint64_t)requested, (int64_t)offset);
           if ((size_t)got != requested)
               status = 1;
        } else {
           fprintf(stderr, "zran: write error\n");
           status = 1;
        }
    }

    // Clean up and exit.
    free(buf);
    deflate_index_free(index);
    if (fclose(in) != 0)
        status = 1;
    return status;
}
