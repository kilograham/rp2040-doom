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
//	Zone Memory Allocation. Neat.
//

#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "m_argv.h"

#include "z_zone.h"
#include "deh_str.h"
#include <assert.h>

// todo graham rework to use shortptr during alloc/free etc.

// todo we have perfectly good dumping
//#define USE_MEM_USE_TRACKING

#ifdef USE_MEM_USE_TRACKING
int32_t mem_used;
#endif

//
// ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// The rover can be left pointing at a non-empty block.
//
// It is of no value to free a cachable block,
//  because it will get overwritten automatically if needed.
// 
 
#define MEM_ALIGN sizeof(void *)
#define ZONEID	0x1d4a11

typedef struct memblock_s
{
    shortptr_t /*struct memblock_s*/	sp_next;
    shortptr_t /*struct memblock_s**/	sp_prev;
#if !DOOM_TINY
    int			size;	// including the header and possibly tiny fragments
#else
    uint16_t            size4;
#endif
#if !NO_Z_MALLOC_USER_PTR
    void**		user;
#endif
    uint8_t			tag;	// PU_FREE if this is free
#if !NO_Z_ZONE_ID
    int			id;	// should be ZONEID
#endif
#if Z_MALOOC_EXTRA_DATA
    uint8_t extra;
#endif
} memblock_t;

#if Z_MALOOC_EXTRA_DATA
uint8_t *Z_ObjectExtra(void *ptr) {
    memblock_t *block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
    return &block->extra;
}
#endif

#define memblock_next(mb) ((memblock_t *)shortptr_to_ptr((mb)->sp_next))
#define memblock_prev(mb) ((memblock_t *)shortptr_to_ptr((mb)->sp_prev))
#if !DOOM_TINY
#define memblock_size(mb) ((const int)(mb)->size)
#define set_memblock_size(mb, sz) (mb)->size=sz;
#else
#define memblock_size(mb) (((mb)->size4)<<2)
static inline void set_memblock_size(memblock_t *mb, int size) {
    assert(size < 65536*4);
    mb->size4 = (size + 3) / 4;
}
#if PICO_ON_DEVICE
static_assert(sizeof(memblock_t) == 8, "");
#endif
#endif
static inline shortptr_t memblock_to_shortptr(memblock_t *mb) {
    return ptr_to_shortptr(mb);
}

typedef struct
{
    // total bytes malloced, including header
    int		size;

    // start / end cap for linked list
    memblock_t	blocklist;
    
    memblock_t*	rover;
    
} memzone_t;



static memzone_t *mainzone;
#if !NO_ZONE_DEBUG
static boolean zero_on_free;
static boolean scan_on_free;
#else
#define zero_on_free false
#define scan_on_free false
#endif


//
// Z_Init
//
void Z_Init (void)
{
    memblock_t*	block;
    int		size;

    mainzone = (memzone_t *)I_ZoneBase (&size);
    mainzone->size = size;

    block = (memblock_t *)( (byte *)mainzone + sizeof(memzone_t) );
    // set the entire zone to one free block
    mainzone->blocklist.sp_next =
	mainzone->blocklist.sp_prev = memblock_to_shortptr(block);


#if !NO_Z_MALLOC_USER_PTR
    mainzone->blocklist.user = (void *)mainzone;
#endif
    mainzone->blocklist.tag = PU_STATIC;
    mainzone->rover = block;

    block->sp_prev = block->sp_next = memblock_to_shortptr(&mainzone->blocklist);

    // free block
    block->tag = PU_FREE;

    set_memblock_size(block, mainzone->size - sizeof(memzone_t));

#if !NO_ZONE_DEBUG
    // [Deliberately undocumented]
    // Zone memory debugging flag. If set, memory is zeroed after it is freed
    // to deliberately break any code that attempts to use it after free.
    //
    zero_on_free = M_ParmExists("-zonezero");

    // [Deliberately undocumented]
    // Zone memory debugging flag. If set, each time memory is freed, the zone
    // heap is scanned to look for remaining pointers to the freed block.
    //
    scan_on_free = M_ParmExists("-zonescan");
#endif
}

// Scan the zone heap for pointers within the specified range, and warn about
// any remaining pointers.
static void ScanForBlock(void *start, void *end)
{
    memblock_t *block;
    void **mem;
    int i, len, tag;

    block = memblock_next(&mainzone->blocklist);

    while (memblock_next(block) != &mainzone->blocklist)
    {
        tag = block->tag;

        if (tag == PU_STATIC || tag == PU_LEVEL || tag == PU_LEVSPEC)
        {
            // Scan for pointers on the assumption that pointers are aligned
            // on word boundaries (word size depending on pointer size):
            mem = (void **) ((byte *) block + sizeof(memblock_t));
            len = (memblock_size(block) - sizeof(memblock_t)) / sizeof(void *);

            for (i = 0; i < len; ++i)
            {
                if (start <= mem[i] && mem[i] <= end)
                {
                    stderr_print(
                            "%p has dangling pointer into freed block "
                            "%p (%p -> %p)\n",
                            mem, start, &mem[i], mem[i]);
                }
            }
        }

        block = memblock_next(block);
    }
}

//
// Z_Free
//
void Z_Free (void* ptr)
{
    memblock_t*		block;
    memblock_t*		other;

    block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));

#if !NO_Z_ZONE_ID
    if (block->id != ZONEID)
	I_Error ("Z_Free: freed a pointer without ZONEID");
#endif

#if !NO_Z_MALLOC_USER_PTR
    if (block->tag != PU_FREE && block->user != NULL)
    {
    	// clear the user's mark
	    *block->user = 0;
    }
#endif

#ifdef USE_MEM_USE_TRACKING
    mem_used -= memblock_size(block) + sizeof(memblock_t);
#endif

    // mark as free
    block->tag = PU_FREE;
#if !NO_Z_MALLOC_USER_PTR
    block->user = NULL;
#endif
#if !NO_Z_ZONE_ID
    block->id = 0;
#endif

    // If the -zonezero flag is provided, we zero out the block on free
    // to break code that depends on reading freed memory.
    if (zero_on_free)
    {
        memset(ptr, 0, memblock_size(block) - sizeof(memblock_t));
    }
    if (scan_on_free)
    {
        ScanForBlock(ptr,
                     (byte *) ptr + memblock_size(block) - sizeof(memblock_t));
    }

    other = memblock_prev(block);

    if (other->tag == PU_FREE)
    {
        // merge with previous free block
        set_memblock_size(other, memblock_size(other) + memblock_size(block));
        other->sp_next = block->sp_next;
        memblock_next(other)->sp_prev = memblock_to_shortptr(other);

        if (block == mainzone->rover)
            mainzone->rover = other;

        block = other;
    }

    other = memblock_next(block);
    if (other->tag == PU_FREE)
    {
        // merge the next free block onto the end
        set_memblock_size(block, memblock_size(other) + memblock_size(block));
        block->sp_next = other->sp_next;
        memblock_next(block)->sp_prev = memblock_to_shortptr(block);

        if (other == mainzone->rover)
            mainzone->rover = block;
    }
}



//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
#define MINFRAGMENT		64

#if !NO_Z_MALLOC_USER_PTR
void*
Z_Malloc
( int		size,
  int		tag,
  void*		user )
#else
void*
Z_MallocNoUser
( int		size,
  int		tag )
#endif
{
    int		extra;
    memblock_t*	start;
    memblock_t* rover;
    memblock_t* newblock;
    memblock_t*	base;
    void *result;

    size = (size + MEM_ALIGN - 1) & ~(MEM_ALIGN - 1);
    
    // scan through the block list,
    // looking for the first free block
    // of sufficient size,
    // throwing out any purgable blocks along the way.

    // account for size of block header
    size += sizeof(memblock_t);
    
    // if there is a free block behind the rover,
    //  back up over them
    base = mainzone->rover;
    
    if (memblock_prev(base)->tag == PU_FREE)
        base = memblock_prev(base);
	
    rover = base;
    start = memblock_prev(base);
	
    do
    {
        if (rover == start)
        {
            // scanned all the way around the list
#if DOOM_TINY
            panic("out of memory");
#else
            I_Error ("Z_Malloc: failed on allocation of %i bytes", size);
#endif
        }
	
        if (rover->tag != PU_FREE)
        {
            if (rover->tag < PU_PURGELEVEL)
            {
                // hit a block that can't be purged,
                // so move base past it
                base = rover = memblock_next(rover);
            }
            else
            {
                // free the rover block (adding the size to base)

                // the rover can be the base block
                base = memblock_prev(base);
                Z_Free ((byte *)rover+sizeof(memblock_t));
                base = memblock_next(base);
                rover = memblock_next(base);
            }
        }
        else
        {
            rover = memblock_next(rover);
        }

    } while (base->tag != PU_FREE || memblock_size(base) < size);

    
    // found a block big enough
    extra = memblock_size(base) - size;
    
    if (extra >  MINFRAGMENT)
    {
        // there will be a free fragment after the allocated block
        newblock = (memblock_t *) ((byte *)base + size );
        set_memblock_size(newblock, extra);
	
        newblock->tag = PU_FREE;
#if !NO_Z_MALLOC_USER_PTR
        newblock->user = NULL;
#endif
        newblock->sp_prev = memblock_to_shortptr(base);
        newblock->sp_next = base->sp_next;
        memblock_next(newblock)->sp_prev = memblock_to_shortptr(newblock);

        base->sp_next = memblock_to_shortptr(newblock);
        set_memblock_size(base, size);
    }

#if !NO_Z_MALLOC_USER_PTR
	if (user == NULL && tag >= PU_PURGELEVEL)
	    I_Error ("Z_Malloc: an owner is required for purgable blocks");

    base->user = user;
#else
    assert( tag < PU_PURGELEVEL);
#endif
    base->tag = tag;

    result  = (void *) ((byte *)base + sizeof(memblock_t));

#if !NO_Z_MALLOC_USER_PTR
    if (base->user)
    {
        *base->user = result;
    }
#endif

    // next allocation will start looking here
    mainzone->rover = memblock_next(base);

#if !NO_Z_ZONE_ID
    base->id = ZONEID;
#endif

#ifdef USE_MEM_USE_TRACKING
    mem_used += memblock_size(base) + sizeof(memblock_t);
    static int8_t pants;
    if (0 == (0xf & pants++))
        printf("Mem used %d\n", (int)mem_used);
#endif
#if DOOM_SMALL
    assert(size - sizeof(memblock_t) >= 4);
    *(uint32_t *)result = 0; // if this is a thinker_t we have zeroed out the
#if !PICO_ON_DEVICE
    if (size - sizeof(memblock_t) >= 12) {
        ((uint32_t *)result)[1] = 0; // if this is a thinker_t we have zeroed out the pool_info field
        ((uint32_t *)result)[2] = 0; // if this is a thinker_t we have zeroed out the pool_info field
    }
#endif
#endif
    return result;
}

//
// Z_FreeTags
//
void
Z_FreeTags
( int		lowtag,
  int		hightag )
{
    memblock_t*	block;
    memblock_t*	next;
	
    for (block = memblock_next(&mainzone->blocklist) ;
	 block != &mainzone->blocklist ;
	 block = next)
    {
	// get link before freeing
	next = memblock_next(block);

	// free block?
	if (block->tag == PU_FREE)
	    continue;
	
	if (block->tag >= lowtag && block->tag <= hightag)
	    Z_Free ( (byte *)block+sizeof(memblock_t));
    }
}



//
// Z_DumpHeap
// Note: TFileDumpHeap( stdout ) ?
//
void
Z_DumpHeap
( int		lowtag,
  int		hightag )
{
    memblock_t*	block;
	
    printf ("zone size: %i  location: %p\n",
	    mainzone->size,mainzone);
    
    printf ("tag range: %i to %i\n",
	    lowtag, hightag);
	
    for (block = memblock_next(&mainzone->blocklist) ; ; block = memblock_next(block))
    {
	if (block->tag >= lowtag && block->tag <= hightag)
#if !NO_Z_MALLOC_USER_PTR
    printf ("block:%p    size:%7i    user:%p    tag:%3i\n",
		    block, memblock_size(block), block->user, block->tag);
#else
            printf ("block:%p    size:%7i    tag:%3i\n",
                    block, memblock_size(block), block->tag);
#endif
		
	if (memblock_next(block) == &mainzone->blocklist)
	{
	    // all blocks have been hit
	    break;
	}
	
	if ( (byte *)block + memblock_size(block) != (byte *)memblock_next(block))
	    printf ("ERROR: block size does not touch the next block\n");

	if ( memblock_prev(memblock_next(block))!= block)
	    printf ("ERROR: next block doesn't have proper back link\n");

	if (block->tag == PU_FREE && memblock_next(block)->tag == PU_FREE)
	    printf ("ERROR: two consecutive free blocks\n");
    }
}

#if !NO_FILE_ACCESS
//
// Z_FileDumpHeap
//
void Z_FileDumpHeap (FILE* f)
{
    memblock_t*	block;
	
    fprintf (f,"zone size: %i  location: %p\n",mainzone->size,mainzone);
	
    for (block = memblock_next(&mainzone->blocklist) ; ; block = memblock_next(block))
    {
#if !NO_Z_MALLOC_USER_PTR
	fprintf (f,"block:%p    size:%7i    user:%p    tag:%3i\n",
		 block, memblock_size(block), block->user, block->tag);
#else
        fprintf (f,"block:%p    size:%7i    tag:%3i\n",
                 block, memblock_size(block), block->tag);
#endif
		
	if (memblock_next(block) == &mainzone->blocklist)
	{
	    // all blocks have been hit
	    break;
	}
	
	if ( (byte *)block + memblock_size(block) != (byte *)memblock_next(block))
	    fprintf (f,"ERROR: block size does not touch the next block\n");

	if ( memblock_prev(memblock_next(block)) != block)
	    fprintf (f,"ERROR: next block doesn't have proper back link\n");

	if (block->tag == PU_FREE && memblock_next(block)->tag == PU_FREE)
	    fprintf (f,"ERROR: two consecutive free blocks\n");
    }
}
#endif


//
// Z_CheckHeap
//
void Z_CheckHeap (void)
{
#if !NO_ZONE_DEBUG
    memblock_t*	block;
	
    for (block = memblock_next(&mainzone->blocklist) ; ; block = memblock_next(block))
    {
	if (memblock_next(block) == &mainzone->blocklist)
	{
	    // all blocks have been hit
	    break;
	}
	
	if ( (byte *)block + memblock_size(block) != (byte *)memblock_next(block))
	    I_Error ("Z_CheckHeap: block size does not touch the next block\n");

	if ( memblock_prev(memblock_next(block)) != block)
	    I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");

	if (block->tag == PU_FREE && memblock_next(block)->tag == PU_FREE)
	    I_Error ("Z_CheckHeap: two consecutive free blocks\n");
    }
#endif
}




//
// Z_ChangeTag
//
void Z_ChangeTag2(void *ptr, int tag, const char *file, int line)
{
    memblock_t*	block;
	
    block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));

#if !NO_Z_ZONE_ID
    if (block->id != ZONEID)
        I_Error("%s:%i: Z_ChangeTag: block without a ZONEID!",
                file, line);
#endif

#if !NO_Z_MALLOC_USER_PTR
    if (tag >= PU_PURGELEVEL && block->user == NULL)
        I_Error("%s:%i: Z_ChangeTag: an owner is required "
                "for purgable blocks", file, line);
#endif

    block->tag = tag;
}

#if !NO_Z_MALLOC_USER_PTR
void Z_ChangeUser(void *ptr, void **user)
{
    memblock_t*	block;

    block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));

#if !NO_Z_ZONE_ID
    if (block->id != ZONEID)
    {
        I_Error("Z_ChangeUser: Tried to change user for invalid block!");
    }
#endif

    block->user = user;
    *user = ptr;
}
#endif


//
// Z_FreeMemory
//
int Z_FreeMemory (void)
{
    memblock_t*		block;
    int			free;
	
    free = 0;

    for (block = memblock_next(&mainzone->blocklist) ;
         block != &mainzone->blocklist;
         block = memblock_next(block))
    {
        if (block->tag == PU_FREE || block->tag >= PU_PURGELEVEL)
            free += memblock_size(block);
    }

    return free;
}

unsigned int Z_ZoneSize(void)
{
    return mainzone->size;
}
