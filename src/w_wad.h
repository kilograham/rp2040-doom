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
//	WAD I/O functions.
//


#ifndef __W_WAD__
#define __W_WAD__

#include <stdio.h>

#include "doomtype.h"
#include "w_file.h"


//
// TYPES
//

//
// WADFILE I/O related stuff.
//

#if !USE_WHD
typedef struct lumpinfo_s lumpinfo_t;
#else
typedef uint32_t lumpinfo_t;
#endif

#if !USE_WHD
struct lumpinfo_s
{
#if !USE_MEMMAP_ONLY
    char	name[8];
    wad_file_t *wad_file;
    int		position;
    int		size;
    void       *cache;
#else
    const  char *name;
    const void  *mem;
    int         size;
#endif
#if PRINT_TOUCHED_LUMPS
    int8_t       touched;
#endif

    // Used for hash table lookups
#if DOOM_SMALL
    short next;
#else
    lumpindex_t next;
#endif
};
#endif

#if !USE_WHD
extern lumpinfo_t **lumpinfo;
static inline should_be_const lumpinfo_t *lump_info(int lump) {
    return lumpinfo[lump];
}
#else
extern const lumpinfo_t *lump_offsets;
static inline const lumpinfo_t *lump_info(int lump) {
    return &lump_offsets[lump];
}
#endif
extern unsigned int numlumps;

wad_file_t *W_AddFile(const char *filename);
void W_Reload(void);

lumpindex_t W_CheckNumForName(const char *name);
lumpindex_t W_GetNumForName(const char *name);

int W_LumpLength(lumpindex_t lump);
void W_ReadLump(lumpindex_t lump, void *dest);

#if USE_MEMMAP_ONLY
extern const uint8_t *whd_map_base;
static inline should_be_const uint8_t *lump_data(const lumpinfo_t *lump) {
#if !USE_WHD
    return (const uint8_t *)lump->mem;
#else
    return whd_map_base + ((*lump)&0xffffffu);
#endif
}
#endif

#if !DOOM_TINY
should_be_const void *W_CacheLumpNum(lumpindex_t lump, int tag);
#else
static inline should_be_const void *W_CacheLumpNum(lumpindex_t lumpnum, int tag)
{
    return lump_data(lump_info(lumpnum));
}
#endif
should_be_const void *W_CacheLumpName(const char *name, int tag);

void W_GenerateHashTable(void);

extern unsigned int W_LumpNameHash(const char *s);

void W_ReleaseLumpNum(lumpindex_t lump);
void W_ReleaseLumpName(const char *name);

const char *W_WadNameForLump(const lumpinfo_t *lump);
boolean W_IsIWADLump(const lumpinfo_t *lump);

#endif
