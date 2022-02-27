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
//      Refresh/rendering module, shared data struct definitions.
//


#ifndef V_PATCH_H
#define V_PATCH_H

// Patches.
// A patch holds one or more columns.
// Patches are used for sprites and all masked pictures,
// and we compose textures from the TEXTURE1/2 lists
// of patches.

#if !USE_WHD
typedef PACKED_STRUCT (
{
    short		width;		// bounding box size
    short		height;
    // todo graham these aren't useful in texture patches
    //  also column offs not useful when opaque (well i guess compression ruins it anyway)
    short		leftoffset;	// pixels to the left of origin
    short		topoffset;	// pixels below the origin
    int			columnofs[8];	// only [width] used
    // the [0] is &columnofs[width]
}) patch_t;
#define patch_topoffset(p) SHORT((p)->topoffset)
#define patch_leftoffset(p) SHORT((p)->leftoffset)
#define patch_width(p) SHORT((p)->width)
#define patch_height(p) SHORT((p)->height)

#define vpatch_width(p) patch_width(p)
#define vpatch_height(p) patch_height(p)
#define vpatch_topoffset(p) SHORT((p)->topoffset)
#define vpatch_leftoffset(p) SHORT((p)->leftoffset)
#define patch_columnofs(p, col) LONG((p)->columnofs[col])
#else
#include "whddata.h"
typedef const uint8_t patch_t;
#define patch_topoffset(p) 0//crud//(p)->topoffset
#define patch_leftoffset(p) 0//crud//(p)->leftoffset
#define patch_width(p) ((p)[1] | ((((p)[2]&1) << 8)))
#define patch_decoder_size_needed(p) ((((p)[2]>>1)<<2)+3)
#define patch_is_wide(p) (((p)[2])&1)
#define patch_height(p) ((p)[3])
#define patch_columnofs(p, col) 0//crud//(p)->columnofs[col]
#define patch_byte_addressed(p) (((p)[0] & 4)!=0)
#define patch_fully_opaque(p) (((p)[0] & 2)!=0)
#define patch_has_extra(p) (((p)[0] & 1)!=0)

// vpatch style patches are stored differently
#define vpatch_width(p) ((p)[0] | (((p)[3]&0x2)<<7u))
#define vpatch_height(p) ((p)[1])
#define vpatch_colorcount(p) ((p)[2])
#define vpatch_type(p) ((p)[3]>>2)
#define vpatch_topoffset(p) ((int8_t)(p)[4])
#define vpatch_leftoffset(p) ((int8_t)(p)[5])
#define vpatch_palette(p) ((p)+6)
#define vpatch_has_shared_palette(p) ((p)[3]&1)
#define vpatch_shared_palette(p) ((vpatch_palette(p) + vpatch_colorcount(p))[0])
#define vpatch_data(p) (vpatch_palette(p) + vpatch_colorcount(p) + ((p)[3]&1))
#define NUM_SHARED_PALETTES 3
extern const uint8_t vpatch_for_shared_palette[NUM_SHARED_PALETTES];
#endif

// posts are runs of non masked source pixels
typedef PACKED_STRUCT (
{
    // todo graham this is iterated in lots of places, so painful when we change
    byte		topdelta;	// -1 is the last post in a column
    byte		length; 	// length data bytes follows
}) post_t;

// column_t is a list of 0 or more post_t, (byte)-1 terminated
typedef post_t	column_t;

#if USE_WHD
typedef uint8_t vpatch_handle_small_t;
typedef uint16_t vpatch_handle_large_t; // well 9 bits
typedef PACKED_STRUCT (
{
    union {
        // this is index 0 in a patchlist
        struct {
            uint16_t size; // includes header
            uint16_t max;
        } header;
        // this is index 1-> in a patchlist
        struct {
            uint32_t x:9;
            uint32_t repeat:6;
            uint32_t patch_handle:9;
            uint32_t y:8;
        } entry;
    };
}) vpatchlist_t;
static_assert(sizeof(vpatchlist_t)==4, "");
extern vpatchlist_t *vpatchlist;
#define VPATCH_HANDLE(x) x
typedef vpatch_handle_large_t vpatch_sequence_t; // consecutive numbers starting at the value
static inline vpatch_handle_large_t vpatch_n(vpatch_sequence_t seq, int n) {
    return seq + n;
}
#else
typedef should_be_const patch_t *vpatch_handle_small_t;
typedef should_be_const patch_t *vpatch_handle_large_t;
typedef vpatch_handle_large_t *vpatch_sequence_t;
#define VPATCH_HANDLE(x) W_CacheLumpName(DEH_String(x), PU_CACHE)
static inline vpatch_handle_large_t vpatch_n(vpatch_sequence_t seq, int n) {
    return seq[n];
}
#endif
#endif 

