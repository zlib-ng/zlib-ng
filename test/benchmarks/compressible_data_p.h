/* compressible_data_p.h -- generate compressible data
 * Copyright (C) 2025 Hans Kristian Rosbach
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef COMPRESSIBLE_DATA_P_H
#define COMPRESSIBLE_DATA_P_H

// Alloc and initialize buffer with highly compressible data,
// interspersed with small amounts of random data and 3-byte matches.
static uint8_t *gen_compressible_data(int bufsize) {
    const char teststr1[42] = "Hello hello World broken Test tast mello.";
    const char teststr2[32] = "llollollollollo He Te me orld";
    const char teststr3[4] = "bro";
    int loops = 0;

    uint8_t *buffer = (uint8_t *)malloc(bufsize + 96); // Need extra space for init loop overrun
    if (buffer == NULL) {
        return NULL;
    }

    for (int pos = 0; pos < bufsize; ){
        pos += sprintf((char *)buffer+pos, "%s", teststr1);
        buffer[pos++] = (uint8_t)(rand() & 0xFF);
        // Every so often, add a few other little bits to break the pattern
        if (loops % 13 == 0) {
            pos += sprintf((char *)buffer+pos, "%s", teststr3);
            buffer[pos++] = (uint8_t)(rand() & 0xFF);
        }
        if (loops % 300 == 0) { // Only found once or twice per window
            pos += sprintf((char *)buffer+pos, "%s", teststr2);
        }
        loops++;
    }
    return buffer;
}
#endif
