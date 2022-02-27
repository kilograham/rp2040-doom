//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2021-2022 Graham Sanderson
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Simple basic typedefs, isolated here to make it easier
//	 separating modules.
//    


#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#include "config.h"

#if defined(_MSC_VER) && !defined(__cplusplus)
#define inline __inline
#endif

// #define macros to provide functions missing in Windows.
// Outside Windows, we use strings.h for str[n]casecmp.


#if !HAVE_DECL_STRCASECMP || !HAVE_DECL_STRNCASECMP

#include <string.h>
#if PICO_ON_DEVICE
extern int stricmp(const char *a, const char *b);
extern int strnicmp(const char *a, const char *b, size_t len);
#endif
#if !HAVE_DECL_STRCASECMP
#define strcasecmp stricmp
#endif
#if !HAVE_DECL_STRNCASECMP
#define strncasecmp strnicmp
#endif

#else

#include <strings.h>

#endif


//
// The packed attribute forces structures to be packed into the minimum 
// space necessary.  If this is not done, the compiler may align structure
// fields differently to optimize memory access, inflating the overall
// structure size.  It is important to use the packed attribute on certain
// structures where alignment is important, particularly data read/written
// to disk.
//

#ifdef __GNUC__

#if defined(_WIN32) && !defined(__clang__)
#define PACKEDATTR __attribute__((packed,gcc_struct))
#else
#define PACKEDATTR __attribute__((packed))
#endif

#define PRINTF_ATTR(fmt, first) __attribute__((format(printf, fmt, first)))
#define PRINTF_ARG_ATTR(x) __attribute__((format_arg(x)))
#define NORETURN __attribute__((noreturn))

#else
#define PACKEDATTR
#define PRINTF_ATTR(fmt, first)
#define PRINTF_ARG_ATTR(x)
#define NORETURN
#endif

#ifdef __WATCOMC__
#define PACKEDPREFIX _Packed
#else
#define PACKEDPREFIX
#endif

#define PACKED_STRUCT(...) PACKEDPREFIX struct __VA_ARGS__ PACKEDATTR

// C99 integer types; with gcc we just use this.  Other compilers
// should add conditional statements that define the C99 types.

// What is really wanted here is stdint.h; however, some old versions
// of Solaris don't have stdint.h and only have inttypes.h (the 
// pre-standardisation version).  inttypes.h is also in the C99 
// standard and defined to include stdint.h, so include this. 

#include <inttypes.h>

#if defined(__cplusplus) || defined(__bool_true_false_are_defined)

// Use builtin bool type with C++.

typedef bool boolean;

#else

typedef uint8_t boolean;
#define false 0
#define true 1
//typedef enum
//{
//    false,
//    true
//} boolean;

#endif

typedef uint8_t byte;
typedef uint8_t pixel_t;
typedef int16_t dpixel_t;

#if !DOOM_SMALL
typedef int isb_int8_t;
typedef int isb_int16_t;
typedef int isb_uint8_t;
#else
typedef int8_t isb_int8_t;
typedef int16_t isb_int16_t;
typedef uint8_t isb_uint8_t;
#endif
#include <limits.h>

#ifdef _WIN32

#define DIR_SEPARATOR '\\'
#define DIR_SEPARATOR_S "\\"
#define PATH_SEPARATOR ';'

#else

#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_S "/"
#define PATH_SEPARATOR ':'

#endif

#define arrlen(array) (sizeof(array) / sizeof(*array))

#if USE_FLAT_MAX_256
typedef uint8_t flatnum_t;
#else
typedef int flatnum_t;
#endif

#if !DOOM_SMALL
typedef int texnum_t;
#else
typedef short texnum_t;
#endif

#if DOOM_CONST
#define should_be_const const
#else
#define should_be_const
#endif

#if DOOM_SMALL
typedef uint8_t key_type_t;
typedef int8_t mouseb_type_t; // allow -1
#else
typedef int key_type_t;
typedef int mouseb_type_t;
#endif

#if DOOM_SMALL
typedef short lumpindex_t;
typedef uint16_t cardinal_t;
#else
typedef int lumpindex_t;
typedef int cardinal_t;
#endif
typedef const char * constcharstar;

#if !FLOOR_CEILING_CLIP_8BIT
#define FLOOR_CEILING_CLIP_OFFSET 0
typedef short floor_ceiling_clip_t;
#else
#define FLOOR_CEILING_CLIP_OFFSET 1
typedef uint8_t floor_ceiling_clip_t;
#endif

#if PICO_ON_DEVICE
#include <assert.h>
typedef uint16_t shortptr_t;
static inline void *shortptr_to_ptr(shortptr_t s) {
    return s ? (void *)(0x20000000 + s * 4) : NULL;
}
static inline shortptr_t ptr_to_shortptr(void *p) {
    if (!p) return 0;
    uintptr_t v = (uintptr_t)p;
    assert(v>=0x20000004 && v <= 0x20040000 && !(v&3));
    return (shortptr_t) ((v << 14u)>>16u);
}
#else
typedef void *shortptr_t;
#define shortptr_to_ptr(s) (s)
#define ptr_to_shortptr(s) (s)
#endif

#if DOOM_TINY
#define stderr_print(...) printf(__VA_ARGS__)
#else
#define stderr_print(...) fprintf(stderr, __VA_ARGS__)
#endif


#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#if DOOM_SMALL && !defined(USE_ROWAD)
#define USE_ROWAD 1
#endif

#if USE_ROWAD
#define rowad_const const
#define hack_rowad_p(type, instance, member) ((type *)instance)->member
#else
#define rowad_const
#define hack_rowad_p(type, instance, member) (instance)->member
#endif

#ifndef count_of
#define count_of(a) (sizeof(a)/sizeof((a)[0]))
#endif

#endif

