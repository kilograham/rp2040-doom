/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "pico.h"

#include <string.h>

#if PICO_ON_DEVICE
static inline char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return c;
}

int strnicmp(const char *a, const char *b, size_t len) {
    int diff = 0;
    for(uint i=0; i<len && a[i]; i++) {
        diff = to_lower(a[i]) - to_lower(b[i]);
        if (diff) break;
    }
    return diff;
}

int stricmp(const char *a, const char *b) {
    return strnicmp(a, b, strlen(a));
}

#endif