/* test_data_p.h -- shared input data generators for tests and benchmarks
 * Copyright (C) 2025 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef TEST_DATA_P_H
#define TEST_DATA_P_H

static inline size_t append_raw(uint8_t *dest, size_t size, const void *src, size_t len) {
    if (len > size) len = size;
    if (len == 0)
        return 0;
    memcpy(dest, src, len);
    return len;
}
static inline size_t append_str(uint8_t *dest, size_t size, const char *src) {
    return append_raw(dest, size, src, strlen(src));
}
static inline size_t append_uint8_t(uint8_t *dest, size_t size, uint8_t src) {
    return append_raw(dest, size, &src, 1);
}

/* English-like text: words drawn Zipf-style from a small vocabulary, with
   occasional novel words mutated from vocabulary ones. Repeated words become
   short-to-medium matches at text-like distances; novel words and word boundaries
   leave scattered literals. */
static inline uint8_t *gen_text_data(size_t bufsize) {
    static const char letters[] = "etaoinshrdlucmfwypvbgk";
    uint8_t vocab[128][12];
    uint8_t vlen[128];
    uint32_t rng = 0x7e47da7a;
    uint8_t *buf = (uint8_t *)malloc(bufsize);
    if (buf == NULL)
        return NULL;

    for (int w = 0; w < 128; w++) {
        rng = rng * 1103515245u + 12345u;
        vlen[w] = (uint8_t)(3 + ((rng >> 16) % 8));
        for (int c = 0; c < vlen[w]; c++) {
            rng = rng * 1103515245u + 12345u;
            vocab[w][c] = (uint8_t)letters[(rng >> 16) % (sizeof(letters) - 1)];
        }
    }

    size_t pos = 0;
    uint32_t words = 0;
    while (pos < bufsize) {
        /* AND of two 7-bit draws biases toward low ranks (Zipf-like) */
        rng = rng * 1103515245u + 12345u;
        uint32_t w = ((rng >> 16) & 127) & ((rng >> 22) & 127);
        uint8_t word[12];
        uint8_t len = vlen[w];
        memcpy(word, vocab[w], len);
        rng = rng * 1103515245u + 12345u;
        if (((rng >> 16) % 6) == 0) {
            /* Novel word: mutate the tail into fresh literals */
            for (int c = len > 4 ? len - 4 : 1; c < len; c++) {
                rng = rng * 1103515245u + 12345u;
                word[c] = (uint8_t)letters[(rng >> 16) % (sizeof(letters) - 1)];
            }
        }
        pos += append_raw(buf + pos, bufsize - pos, word, len);
        rng = rng * 1103515245u + 12345u;
        words++;
        if ((words % 12) == 0)
            pos += append_str(buf + pos, bufsize - pos, ".\n");
        else if (((rng >> 16) % 16) == 0)
            pos += append_str(buf + pos, bufsize - pos, ", ");
        else
            pos += append_uint8_t(buf + pos, bufsize - pos, ' ');
    }
    if (bufsize > 0)
        buf[bufsize - 1] = 0;
    return buf;
}

/* A rotating pool of eight 3..8-byte random patterns emitted in random order.
   Re-emitted patterns become short back-references at small distances that
   frequently chain match-to-match; pool refreshes and separator bytes leave short
   literal runs; ~1/16 of iterations emit a dist=1 RLE run. */
static inline uint8_t *gen_short_match_data(size_t bufsize) {
    uint8_t *buf = (uint8_t *)malloc(bufsize);
    if (buf == NULL)
        return NULL;

    uint32_t rng = 0xc001cafe;
    size_t i = 0;
    uint8_t pool[8][8];
    uint8_t plens[8];

    for (int s = 0; s < 8; s++) {
        rng = rng * 1103515245u + 12345u;
        plens[s] = (uint8_t)(3 + ((rng >> 16) % 6));
        for (int j = 0; j < plens[s]; j++) {
            rng = rng * 1103515245u + 12345u;
            pool[s][j] = (uint8_t)(rng >> 24);
        }
    }

    while (i < bufsize) {
        rng = rng * 1103515245u + 12345u;
        uint32_t r = (rng >> 16) & 0xF;
        uint32_t slot = (rng >> 20) & 7;
        if (r == 0) {
            /* RLE run: one byte repeated, matched at dist=1 */
            rng = rng * 1103515245u + 12345u;
            uint8_t b = (uint8_t)(rng >> 24);
            size_t run = 6 + ((rng >> 16) % 18);
            for (size_t j = 0; j < run && i < bufsize; j++)
                buf[i++] = b;
        } else if (r <= 2) {
            /* Refresh a pool slot with a fresh pattern and emit it: literals */
            rng = rng * 1103515245u + 12345u;
            plens[slot] = (uint8_t)(3 + ((rng >> 16) % 6));
            for (int j = 0; j < plens[slot]; j++) {
                rng = rng * 1103515245u + 12345u;
                pool[slot][j] = (uint8_t)(rng >> 24);
                if (i < bufsize)
                    buf[i++] = pool[slot][j];
            }
        } else if (r == 3) {
            /* Separator literal */
            rng = rng * 1103515245u + 12345u;
            buf[i++] = (uint8_t)(rng >> 24);
        } else {
            /* Re-emit a pool pattern: a short match, often chaining */
            for (int j = 0; j < plens[slot] && i < bufsize; j++)
                buf[i++] = pool[slot][j];
        }
    }
    return buf;
}

/* Random bytes over a 4-symbol (DNA base) alphabet. Nearly every 3-byte prefix
   collides, so hash chains grow very long while matches stay short. Stresses the
   longest_match short-match path with deep chain walks. */
static inline uint8_t *gen_dna_data(size_t bufsize) {
    static const uint8_t bases[4] = { 'A', 'C', 'G', 'T' };
    uint8_t *buf = (uint8_t *)malloc(bufsize);
    if (buf == NULL)
        return NULL;
    uint32_t rng = 0x01234567;
    for (size_t i = 0; i < bufsize; i++) {
        rng = rng * 1103515245u + 12345u;
        buf[i] = bases[(rng >> 24) & 3];
    }
    return buf;
}

static inline uint8_t clamp_uint8_t(int v) {
    return (uint8_t)(v < 0 ? 0 : (v > 0xFF ? 0xFF : v));
}

/* Pseudorandom incompressible bytes. Forces deflate into stored blocks, exercising
   the inflate literal-byte path with no chunk copies. */
static inline uint8_t *gen_random_data(size_t bufsize) {
    uint8_t *buf = (uint8_t *)malloc(bufsize);
    if (buf == NULL)
        return NULL;
    uint32_t rng = 0xdeadbeef;
    for (size_t i = 0; i < bufsize; i++) {
        rng = rng * 1103515245u + 12345u;
        buf[i] = (uint8_t)(rng >> 24);
    }
    return buf;
}

/* High-entropy literals from mostly the AND of two uniform bytes (~6.5 bits per
   byte). Deflate emits coded blocks (ratio ~1.1) with few matches. */
static inline uint8_t *gen_literals_data(size_t bufsize) {
    uint8_t *buf = (uint8_t *)malloc(bufsize);
    if (buf == NULL)
        return NULL;
    uint32_t rng = 0x600dd1ce;
    for (size_t i = 0; i < bufsize; i++) {
        rng = rng * 1103515245u + 12345u;
        uint8_t a = (uint8_t)(rng >> 24);
        rng = rng * 1103515245u + 12345u;
        /* 1/4 of bytes stay fully random to break up 3-byte repeats that
           deflate would otherwise turn into incidental matches at level 9 */
        buf[i] = ((rng >> 8) & 3) ? (uint8_t)(a & (rng >> 24)) : a;
    }
    return buf;
}

/* Object-archive-like binary interleaving machine-code-like flat literals,
   24-byte records that match at stride distances, and zero padding. Deflate emits
   coded blocks at ratio ~2.7 where ~80% of the symbols are literals. */
static inline uint8_t *gen_mixed_data(size_t bufsize) {
    uint8_t *buf = (uint8_t *)malloc(bufsize);
    if (buf == NULL)
        return NULL;
    uint32_t rng = 0xb1a5b1a5;
    size_t pos = 0;
    uint8_t rec[24];
    for (int i = 0; i < 24; i++) {
        rng = rng * 1103515245u + 12345u;
        rec[i] = (uint8_t)(rng >> 16);
    }
    while (pos < bufsize) {
        rng = rng * 1103515245u + 12345u;
        uint32_t kind = (rng >> 16) % 100;
        if (kind < 86) {
            /* Code-like: flat literal run, then sometimes a short self-copy.
               Run lengths roughly follow the run histogram of real machine
               code (head at 1-2 plus a 17-64 tail, mean ~6). */
            rng = rng * 1103515245u + 12345u;
            uint32_t r = (rng >> 16) & 15;
            size_t run = r < 4 ? 1 + ((rng >> 20) & 1)
                       : r < 8 ? 3 + ((rng >> 20) & 1)
                       : r < 12 ? 5 + ((rng >> 20) & 3)
                       : r < 14 ? 9 + ((rng >> 20) & 7)
                       : 17 + ((rng >> 20) % 48);
            for (size_t j = 0; j < run && pos < bufsize; j++) {
                rng = rng * 1103515245u + 12345u;
                uint8_t a = (uint8_t)(rng >> 16);
                rng = rng * 1103515245u + 12345u;
                buf[pos++] = (uint8_t)(a & (rng >> 16));
            }
            rng = rng * 1103515245u + 12345u;
            if (pos < 16 || pos >= bufsize || ((rng >> 16) & 1))
                continue;
            rng = rng * 1103515245u + 12345u;
            size_t length = 4 + ((rng >> 16) % 14);
            rng = rng * 1103515245u + 12345u;
            size_t maxd = pos < 4096 ? pos : 4096;
            size_t dist = maxd > 8 ? 8 + ((rng >> 16) % (maxd - 7)) : pos;
            size_t start = pos - dist;
            for (size_t i = 0; i < length && pos < bufsize; i++, pos++)
                buf[pos] = buf[start + i];
        } else if (kind < 96) {
            /* Symbol-table-like: 24-byte records, a few varying bytes each */
            rng = rng * 1103515245u + 12345u;
            size_t nrec = 2 + ((rng >> 16) % 4);
            for (size_t r2 = 0; r2 < nrec && pos + 24 <= bufsize; r2++) {
                memcpy(buf + pos, rec, 24);
                rng = rng * 1103515245u + 12345u;
                size_t nvary = 3 + ((rng >> 16) & 1);
                for (size_t k = 0; k < nvary; k++) {
                    rng = rng * 1103515245u + 12345u;
                    buf[pos + ((rng >> 16) % 24)] = (uint8_t)(rng >> 8);
                }
                pos += 24;
            }
        } else {
            /* Zero padding, long dist-1 runs */
            rng = rng * 1103515245u + 12345u;
            size_t z = 64 + ((rng >> 16) % 384);
            if (z > bufsize - pos) z = bufsize - pos;
            memset(buf + pos, 0, z);
            pos += z;
        }
    }
    return buf;
}

/* RGB photo-like pixels at a fixed row width: smooth gradients with per-pixel
   noise. Yields short matches at dist=3 (RGB stride) and longer inter-row matches;
   deflate emits scattered literals between them. */
static inline uint8_t *gen_realistic_rgb_data(size_t bufsize) {
    uint8_t *buf = (uint8_t *)malloc(bufsize);
    if (buf == NULL)
        return NULL;

    size_t pixels = bufsize / 3;
    uint32_t width = (uint32_t)(pixels >= 256 ? 256 : pixels);
    uint32_t height = (uint32_t)(width > 0 ? pixels / width : 0);
    if (height == 0) {
        memset(buf, 0, bufsize);
        return buf;
    }

    uint32_t seed = 0x12345678;
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            size_t idx = ((size_t)y * width + x) * 3;
            /* Diagonal gradient as base color */
            uint8_t base_r = (uint8_t)((x + y) * 179 / (width + height));
            uint8_t base_g = (uint8_t)((x * 2 + y) * 131 / (width + height));
            uint8_t base_b = (uint8_t)(y * 241 / height);
            /* Simple xorshift noise, +/- 15 levels */
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            int noise = (int)(seed & 0x1F) - 15;
            buf[idx]     = clamp_uint8_t(base_r + noise);
            buf[idx + 1] = clamp_uint8_t(base_g + (noise >> 1));
            buf[idx + 2] = clamp_uint8_t(base_b - noise);
        }
    }
    size_t filled = (size_t)width * height * 3;
    if (filled < bufsize) memset(buf + filled, 0, bufsize - filled);
    return buf;
}

/* RGB pixels arranged as three solid R/G/B stripes. Yields long matches at dist=3
   within each stripe and large back-references across the stripe boundaries. */
static inline uint8_t *gen_striped_rgb_data(size_t bufsize) {
    uint8_t *buf = (uint8_t *)malloc(bufsize);
    if (buf == NULL)
        return NULL;

    size_t pixels = bufsize / 3;
    size_t red_stop = pixels / 3;
    size_t blue_stop = 2 * pixels / 3;
    size_t i = 0;

    for (size_t x = 0; i < red_stop; x += 3, ++i) {
        buf[x] = 255; buf[x + 1] = 0; buf[x + 2] = 0;
    }
    for (size_t x = 3 * i; i < blue_stop; x += 3, ++i) {
        buf[x] = 0; buf[x + 1] = 255; buf[x + 2] = 0;
    }
    for (size_t x = 3 * i; i < pixels; x += 3, ++i) {
        buf[x] = 0; buf[x + 1] = 0; buf[x + 2] = 255;
    }
    size_t filled = pixels * 3;
    if (filled < bufsize) memset(buf + filled, 0, bufsize - filled);
    return buf;
}

/* Each variant targets a distinct shape of deflate stream. */
enum test_data_type {
    TEST_DATA_TEXT = 0,         /* mixed literals + short/medium matches */
    TEST_DATA_SHORT_MATCH,      /* many short back-references */
    TEST_DATA_DNA,              /* 4-symbol alphabet, dense chains + short matches */
    TEST_DATA_RANDOM,           /* incompressible, deflate uses stored blocks */
    TEST_DATA_LITERALS,         /* high-entropy literals, coded blocks, few matches */
    TEST_DATA_MIXED,            /* binary-like literal runs + medium matches */
    TEST_DATA_REALISTIC_RGB,    /* RGB photo, short matches at dist=3 */
    TEST_DATA_STRIPED_RGB,      /* solid R/G/B stripes, long dist=3 matches */
    TEST_DATA_COUNT
};

static inline const char *test_data_type_name(int data_type) {
    switch (data_type) {
        case TEST_DATA_TEXT:           return "text";
        case TEST_DATA_SHORT_MATCH:    return "short_match";
        case TEST_DATA_DNA:            return "dna";
        case TEST_DATA_RANDOM:         return "random";
        case TEST_DATA_LITERALS:       return "literals";
        case TEST_DATA_MIXED:          return "mixed";
        case TEST_DATA_REALISTIC_RGB:  return "realistic_rgb";
        case TEST_DATA_STRIPED_RGB:    return "striped_rgb";
    }
    return NULL;
}

static inline uint8_t *gen_test_data(enum test_data_type data_type, size_t bufsize) {
    switch (data_type) {
        case TEST_DATA_TEXT:           return gen_text_data(bufsize);
        case TEST_DATA_SHORT_MATCH:    return gen_short_match_data(bufsize);
        case TEST_DATA_DNA:            return gen_dna_data(bufsize);
        case TEST_DATA_RANDOM:         return gen_random_data(bufsize);
        case TEST_DATA_LITERALS:       return gen_literals_data(bufsize);
        case TEST_DATA_MIXED:          return gen_mixed_data(bufsize);
        case TEST_DATA_REALISTIC_RGB:  return gen_realistic_rgb_data(bufsize);
        case TEST_DATA_STRIPED_RGB:    return gen_striped_rgb_data(bufsize);
        case TEST_DATA_COUNT:          break;
    }
    return NULL;
}

#endif
