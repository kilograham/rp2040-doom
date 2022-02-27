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
//      Zone Memory Allocation, perhaps NeXT ObjectiveC inspired.
//	Remark: this was the only stuff that, according
//	 to John Carmack, might have been useful for
//	 Quake.
//



#ifndef __Z_ZONE__
#define __Z_ZONE__

#include <stdio.h>

//
// ZONE MEMORY
// PU - purge tags.

enum
{
    PU_STATIC = 1,                  // static entire execution time
    PU_SOUND,                       // static while playing
    PU_MUSIC,                       // static while playing
    PU_FREE,                        // a free block
    PU_LEVEL,                       // static until level exited
    PU_LEVSPEC,                     // a special thinker in a level
    
    // Tags >= PU_PURGELEVEL are purgable whenever needed.

    PU_PURGELEVEL,
    PU_CACHE,

    // Total number of different tag types

    PU_NUM_TAGS
};
        

void	Z_Init (void);
#if !NO_Z_MALLOC_USER_PTR
void*	Z_Malloc (int size, int tag, void *ptr);
#else
#include <assert.h>
void*	Z_MallocNoUser (int size, int tag);
#define Z_Malloc(s, t, p) ({ static_assert(!(p), ""); Z_MallocNoUser(s,t); })
#endif
void    Z_Free (void *ptr);
void    Z_FreeTags (int lowtag, int hightag);
void    Z_DumpHeap (int lowtag, int hightag);
void    Z_FileDumpHeap (FILE *f);
void    Z_CheckHeap (void);
void    Z_ChangeTag2 (void *ptr, int tag, const char *file, int line);
void    Z_ChangeUser(void *ptr, void **user);
int     Z_FreeMemory (void);
unsigned int Z_ZoneSize(void);

#if Z_MALOOC_EXTRA_DATA
unsigned char *Z_ObjectExtra(void *ptr);
#endif
//
// This is used to get the local FILE:LINE info from CPP
// prior to really call the function in question.
//
#define Z_ChangeTag(p,t)                                       \
    Z_ChangeTag2((p), (t), __FILE__, __LINE__)


#endif
