/* compressible_data_p.h -- generate compressible data
 * Copyright (C) 2025 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef COMPRESSIBLE_DATA_P_H
#define COMPRESSIBLE_DATA_P_H


static inline size_t append_raw(uint8_t *dest, size_t size, const void *src, size_t len) {
    if (len > size) len = size;
    if (len == 0) return 0;
    memcpy(dest, src, len);
    return len;
}
static inline size_t append_str(uint8_t *dest, size_t size, const char *src) {
    return append_raw(dest, size, src, strlen(src));
}
static inline size_t append_uint8_t(uint8_t *dest, size_t size, uint8_t src) {
    return append_raw(dest, size, &src, 1);
}

// Alloc and initialize buffer with highly compressible data,
// interspersed with small amounts of random data and 3-byte matches.
static uint8_t *gen_compressible_data(size_t bufsize) {
    const char teststr1[42] = "Hello hello World broken Test tast mello.";
    const char teststr2[32] = "llollollollollo He Te me orld";
    const char teststr3[4] = "bro";
    int loops = 0;

    uint8_t *buffer = (uint8_t *)malloc(bufsize);
    if (buffer == NULL) {
        return NULL;
    }

    for (size_t pos = 0; pos < bufsize; ) {
        pos += append_str(buffer+pos, bufsize-pos, teststr1);
        pos += append_uint8_t(buffer+pos, bufsize-pos, (uint8_t)(rand() & 0xFF));
        // Every so often, add a few other little bits to break the pattern
        if (loops % 13 == 0) {
            pos += append_str(buffer+pos, bufsize-pos, teststr3);
            pos += append_uint8_t(buffer+pos, bufsize-pos, (uint8_t)(rand() & 0xFF));
        }
        if (loops % 300 == 0) { // Only found once or twice per window
            pos += append_str(buffer+pos, bufsize-pos, teststr2);
        }
        loops++;
    }
    if (bufsize > 0) {
        buffer[bufsize-1] = 0;
    }
    return buffer;
}
#endif
