/* chunkset_tpl.h -- inline functions to copy small data chunks.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* Copies a partial chunk, all bytes minus one */
static inline uint8_t* CSUFFIX(chunkcopy_partial)(uint8_t *out, uint8_t const *from, unsigned len) {
    Assert(len > 0, "chunkcopy length should never be zero");
#if CHUNK_SIZE > 16
    if (len & 16) {
        memcpy(out, from, 16);
        out += 16;
        from += 16;
    }
#endif
#if CHUNK_SIZE > 8
    if (len & 8) {
        memcpy(out, from, 8);
        out += 8;
        from += 8;
    }
#endif
    if (len & 4) {
        memcpy(out, from, 4);
        out += 4;
        from += 4;
    }
    if (len & 2) {
        memcpy(out, from, 2);
        out += 2;
        from += 2;
    }
    if (len & 1) {
        *out++ = *from++;
    }
    return out;
}

/* Returns the chunk size */
Z_INTERNAL uint32_t CSUFFIX(chunksize)(void) {
    return sizeof(chunk_t);
}

/* Behave like memcpy, but assume that it's OK to overwrite at least
   chunk_t bytes of output even if the length is shorter than this,
   that the length is non-zero, and that `from` lags `out` by at least
   sizeof chunk_t bytes (or that they don't overlap at all or simply that
   the distance is less than the length of the copy).

   Aside from better memory bus utilisation, this means that short copies
   (chunk_t bytes or fewer) will fall straight through the loop
   without iteration, which will hopefully make the branch prediction more
   reliable. */
static inline uint8_t* CSUFFIX(chunkcopy_static)(uint8_t *out, uint8_t const *from, unsigned len) {
    Assert(len > 0, "chunkcopy length should never be zero");
    chunk_t chunk;
    int32_t align = len % sizeof(chunk_t);
    loadchunk(from, &chunk);
    storechunk(out, &chunk);
    out += align;
    from += align;
    len /= sizeof(chunk_t);
    while (len > 0) {
        loadchunk(from, &chunk);
        storechunk(out, &chunk);
        out += sizeof(chunk_t);
        from += sizeof(chunk_t);
        --len;
    }
    return out;
}
Z_INTERNAL uint8_t* CSUFFIX(chunkcopy)(uint8_t *out, uint8_t const *from, unsigned len) {
    return CSUFFIX(chunkcopy_static)(out, from, len);
}

/* Behave like chunkcopy, but avoid writing beyond of legal output. */
static inline uint8_t* CSUFFIX(chunkcopy_safe_static)(uint8_t *out, uint8_t const *from, unsigned len, unsigned left) {
    len = MIN(len, left);
    if (len < sizeof(chunk_t))
        return CSUFFIX(chunkcopy_partial)(out, from, len);
    return CSUFFIX(chunkcopy_static)(out, from, len);
}
Z_INTERNAL uint8_t* CSUFFIX(chunkcopy_safe)(uint8_t *out, uint8_t const *from, unsigned len, unsigned left) {
    return CSUFFIX(chunkcopy_safe_static)(out, from, len, left);
}

/* Perform short copies until distance can be rewritten as being at least
   sizeof chunk_t.

   This assumes that it's OK to overwrite at least the first
   2*sizeof(chunk_t) bytes of output even if the copy is shorter than this.
   This assumption holds because inflate_fast() starts every iteration with at
   least 258 bytes of output space available (258 being the maximum length
   output from a single token; see inflate_fast()'s assumptions below). */
static inline uint8_t* CSUFFIX(chunkunroll_static)(uint8_t *out, unsigned *dist, unsigned *len) {
    unsigned char const *from = out - *dist;
    chunk_t chunk;
    while (*dist < *len && *dist < sizeof(chunk_t)) {
        loadchunk(from, &chunk);
        storechunk(out, &chunk);
        out += *dist;
        *len -= *dist;
        *dist += *dist;
    }
    return out;
}
Z_INTERNAL uint8_t* CSUFFIX(chunkunroll)(uint8_t *out, unsigned *dist, unsigned *len) {
    return CSUFFIX(chunkunroll_static)(out, dist, len);
}

/* Copy DIST bytes from OUT - DIST into OUT + DIST * k, for 0 <= k < LEN/DIST.
   Return OUT + LEN. */
static inline uint8_t* CSUFFIX(chunkmemset_static)(uint8_t *out, unsigned dist, unsigned len) {
    /* Debug performance related issues when len < sizeof(uint64_t):
       Assert(len >= sizeof(uint64_t), "chunkmemset should be called on larger chunks"); */
    Assert(dist > 0, "chunkmemset cannot have a distance 0");
    unsigned char *from = out - dist;
    chunk_t chunk;
    unsigned sz = sizeof(chunk);
    if (len < sz) {
        do {
            *out++ = *from++;
            --len;
        } while (len != 0);
        return out;
    }

#ifdef HAVE_CHUNKMEMSET_1
    if (dist == 1) {
        chunkmemset_1(from, &chunk);
    } else
#endif
#ifdef HAVE_CHUNKMEMSET_2
    if (dist == 2) {
        chunkmemset_2(from, &chunk);
    } else
#endif
#ifdef HAVE_CHUNKMEMSET_4
    if (dist == 4) {
        chunkmemset_4(from, &chunk);
    } else
#endif
#ifdef HAVE_CHUNKMEMSET_8
    if (dist == 8) {
        chunkmemset_8(from, &chunk);
    } else
#endif
    if (dist == sz) {
        loadchunk(from, &chunk);
    } else if (dist < sz) {
        while (len > dist) {
            out = CSUFFIX(chunkcopy_safe_static)(out, from, dist, dist);
            len -= dist;
        }
        if (len > 0) {
            out = CSUFFIX(chunkcopy_safe_static)(out, from, len, len);
        }
        return out;
    } else {
        out = CSUFFIX(chunkunroll_static)(out, &dist, &len);
        return CSUFFIX(chunkcopy_static)(out, out - dist, len);
    }

    unsigned rem = len % sz;
    len -= rem;
    while (len) {
        storechunk(out, &chunk);
        out += sz;
        len -= sz;
    }

    /* Last, deal with the case when LEN is not a multiple of SZ. */
    if (rem) {
        memcpy(out, from, rem);
        out += rem;
    }

    return out;
}
Z_INTERNAL uint8_t* CSUFFIX(chunkmemset)(uint8_t *out, unsigned dist, unsigned len) {
    return CSUFFIX(chunkmemset_static)(out, dist, len);
}

Z_INTERNAL uint8_t* CSUFFIX(chunkmemset_safe)(uint8_t *out, unsigned dist, unsigned len, unsigned left) {
    if (left < (unsigned)(3 * sizeof(chunk_t))) {
        unsigned char *from = out - dist;
        while (len > 0) {
            *out++ = *from++;
            --len;
        }
        return out;
    }
    return CSUFFIX(chunkmemset_static)(out, dist, len);
}
