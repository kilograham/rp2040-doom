/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
// not sure we need anything, but doom headers expect it

void __attribute__((noreturn)) fail(const char *msg, ...);

//#define SAVE_PNG 1
#define VERIFY_ENCODING 1 // extra work => warm fuzzy feeling
#define USE_MUSX 1

//#define MUS_PER_EVENT_GAP 1
#define MUS_GROUP_SIZE_CODE 1

#define TEXTURE_PIXEL_STATS 1
//#define PRETEND_COMPRESS 1
// todo sounds a bit better with 5, but slower
#ifndef NDEBUG
#define LOOKAHEAD 3
#else
#define LOOKAHEAD 5
#endif

#define CONSIDER_SEQUENCES 0
#define SEQUENCE 10

//#define SAVE_PNG 1

