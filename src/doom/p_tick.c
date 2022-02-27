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
//	Archiving: SaveGame I/O.
//	Thinker, Ticker.
//


#include <i_system.h>
#include "z_zone.h"
#include "p_local.h"

#include "doomstat.h"


int	leveltime;

//
// THINKERS
// All thinkers should be allocated by Z_Malloc
// so they can be operated on uniformly.
// The actual structures will vary in size,
// but the first element must be thinker_t.
//



// Both the head and tail of the thinker list.
thinker_t thinkercap;
thinker_t *thinkertail;

#if USE_THINKER_POOL
#define MAX_THINKER_POOLS 2
static shortptr_t thinker_pool[MAX_THINKER_POOLS];
#endif

//
// P_InitThinkers
//
void P_InitThinkers (void)
{
    thinkercap.sp_next = thinker_to_shortptr(&thinkercap);
    thinkertail = &thinkercap;
#if USE_THINKER_POOL
    memset(thinker_pool, 0, sizeof(thinker_pool));
#endif
}




//
// P_AddThinker
// Adds a new thinker at the end of the list.
//
void P_AddThinker (thinker_t* thinker)
{
    assert(thinker_next(thinkertail) == &thinkercap);
    thinker->sp_next = thinkertail->sp_next;
    thinkertail->sp_next = thinker_to_shortptr(thinker);
    thinkertail = thinker;
}

//
// P_RemoveThinker
// Deallocation is lazy -- it will not actually be freed
// until its thinking turn comes up.
//
void P_RemoveThinker (thinker_t* thinker)
{
  // FIXME: NOP.
  thinker->function = ThinkF_REMOVED;
}



//
// P_AllocateThinker
// Allocates memory and adds a new thinker at the end of the list.
//
void P_AllocateThinker (thinker_t*	thinker)
{
}



//
// P_RunThinkers
//
void P_RunThinkers (void)
{
    thinker_t *prevthinker, *currentthinker;

    prevthinker = &thinkercap;
    currentthinker = thinker_next(prevthinker);
    while (currentthinker != &thinkercap) {
        if (currentthinker->function == ThinkF_REMOVED) {
            // time to remove it
            prevthinker->sp_next = currentthinker->sp_next;
            if (prevthinker->sp_next == ptr_to_shortptr(&thinkercap)) {
                thinkertail = prevthinker;
            }
            Z_ThinkFree(currentthinker);
        } else {
            switch (currentthinker->function) {
                case ThinkF_NULL:
                    break;
                case ThinkF_T_MoveCeiling:
                    T_MoveCeiling((ceiling_t *) currentthinker);
                    break;
                case ThinkF_T_VerticalDoor:
                    T_VerticalDoor((vldoor_t *) currentthinker);
                    break;
                case ThinkF_T_PlatRaise:
                    T_PlatRaise((plat_t *) currentthinker);
                    break;
                case ThinkF_T_FireFlicker:
                    T_FireFlicker((fireflicker_t *) currentthinker);
                    break;
                case ThinkF_T_LightFlash:
                    T_LightFlash((lightflash_t *) currentthinker);
                    break;
                case ThinkF_T_StrobeFlash:
                    T_StrobeFlash((strobe_t *) currentthinker);
                    break;
                case ThinkF_T_MoveFloor:
                    T_MoveFloor((floormove_t *) currentthinker);
                    break;
                case ThinkF_T_Glow:
                    T_Glow((glow_t *) currentthinker);
                    break;
                case ThinkF_P_MobjThinker:
                    P_MobjThinker((mobj_t *) currentthinker);
                    break;
                default:
                    I_Error("Unexpected thinker");
            }
            prevthinker = currentthinker;
        }
        currentthinker = thinker_next(prevthinker);
    }
}



//
// P_Ticker
//

void P_Ticker (void)
{
    int		i;
    
    // run the tic
    if (paused)
	return;
		
    // pause if in menu and at least one tic has been run
    if ( !netgame
	 && menuactive
	 && !demoplayback
	 && players[consoleplayer].viewz != 1)
    {
	return;
    }
    
		
    for (i=0 ; i<MAXPLAYERS ; i++)
	if (playeringame[i])
	    P_PlayerThink (&players[i]);
			
    P_RunThinkers ();
    P_UpdateSpecials ();
    P_RespawnSpecials ();

    // for par times
    leveltime++;	
}

// =================================================================
// thinker_t objects are the most common dynamically allocated things
// and include our mobj_t (and mobjfull_t). We therefore try to minimize
// the Z_Zone malloc overhead (8 bytes) by simple pooling.
//
// We use one memory object allocation "block" to store up to 8 slots of the same
// size. (we only do this for known sizes).
//
// There is actually a padding byte spare in the malloc header (which we use
// for a 8 entry bit set for which of the block's slots are free)
//
// There is a byte spare in the thinker_t (which we call pool_info) which
// is used to identity a slot entry rather than a raw object, and to locate
// the enclosing block if this is indeed a slot object.
//
// pool_info is two nibbles of the form tttt:1sss where t is an object type
// which identifies the size of each slot, and sss is the 0-7 slot number.
// the pool_info is 0 for non slot thinker_t objects.
//
// We keep a linked list of partially full blocks which starts in the thinker_pool
// array below. The forward link from each block to the next partially full block
// is stored in the "sp_next" thinker_t field of the highest numbered
// free slot of the block
// =================================================================

#if USE_THINKER_POOL
static int thinker_pool_type(int size) {
    switch (size) {
        case sizeof(mobj_t):
            return 0;
        case sizeof(mobjfull_t):
            return 1;
        default:
            return -1;
    }
}

static int thinker_pool_object_size(int type) {
    switch (type) {
        case 0:
            return sizeof(mobj_t);
        case 1:
            return sizeof(mobjfull_t);
        default:
            I_Error("what?");
            return 0;
    }
}

static inline thinker_t *thinker_n(void *obj, int n, int size) {
    assert(!(size & 3));
    return (thinker_t *)(obj + n * size);
}

thinker_t *Z_ThinkMallocImpl(int size) {
    int type = thinker_pool_type(size);
    if (type < 0) {
        // not pooled, so just malloc
        thinker_t *thinker = Z_Malloc(size, PU_LEVEL, 0);
        memset(thinker, 0, size);
        return thinker;
    }
    if (!thinker_pool[type]) {
        // we don't have any partial pools, so allocate into new pool
        void *block = Z_Malloc(size * 8, PU_LEVEL, 0);
        thinker_pool[type] = ptr_to_shortptr(block);
        uint8_t *bitset = Z_ObjectExtra(block);
        // all free but first
        *bitset = 0xfe;
        thinker_t *thinker = (thinker_t *)block;
        memset(thinker, 0, size);
        thinker->pool_info = (type << 4u) | 0x8;
        // pointer to next free pool (none) in the last free thinker
        thinker_n(block, 7, size)->sp_next = 0;
//        printf("Pool %d @ %p, allocating new block slot %d %02x (free) = %p pi %02x\n", size, block, 0, *bitset, thinker, thinker->pool_info);
        return thinker;
    }
    void *block = shortptr_to_ptr(thinker_pool[type]);
    uint8_t *bitset = Z_ObjectExtra(block);
    int slot = __builtin_ctz(*bitset);
    assert(slot >= 0 && slot < 8);
    thinker_t *thinker = thinker_n(block, slot, size);
    memset(thinker, 0, size);
    // indicate that this thinker is in a pool (and where)
    thinker->pool_info = (type << 4u) | 0x8 | slot;
    *bitset &= ~(1u << slot);
//    printf("Pool %d @ %p, allocating slot %d %02x (free) = %p pi %02x\n", size, block, slot, *bitset, thinker, thinker->pool_info);
    if (!*bitset) {
        // if the pool is now full, so follow the link from the last
        // allocated thinker (us)
//        printf("  unlinking full block from head\n");
        thinker_pool[type] = thinker->sp_next;
    }
    return thinker;
}

void dump_chain(int type, int size) {
#if 1
    printf("P-chain(%d): ", size);
    void *cur_block = shortptr_to_ptr(thinker_pool[type]);
    while (cur_block) {
        uint8_t *current_bitset = Z_ObjectExtra(cur_block);
        printf("-> %p(%02x) ", cur_block, *current_bitset);
        int highest_free_slot = 31 - __builtin_clz(*current_bitset);
        assert(highest_free_slot >=0 && highest_free_slot < 8);
        cur_block = shortptr_to_ptr(thinker_n(cur_block, highest_free_slot, size)->sp_next);
    }
    printf("\n");
#endif
}

void Z_ThinkFree(thinker_t *thinker) {
    if (!thinker->pool_info) {
        // not in a pool
        Z_Free(thinker);
        return;
    }

    assert(thinker->pool_info & 0x8);
    int type = thinker->pool_info >> 4;
    int slot = thinker->pool_info & 0x7;
    assert(type <= MAX_THINKER_POOLS);

    int size = thinker_pool_object_size(type);
    void *block = ((void *)thinker) - slot * size;
    uint8_t *bitset = Z_ObjectExtra(block);
    assert(!(*bitset & (1u << slot)));
//    printf("Pool %d @ %p, freeing slot %d %02x (free) = %p pi %02x\n", size, block, slot, *bitset, thinker, thinker->pool_info);

    shortptr_t next_pool;
    if (!*bitset) {
        // was full before
//        printf("  from full pool, so inserting %p as head\n", block);
        next_pool = thinker_pool[type];
        // new addition to partial block list, so stick us at the front
        thinker_pool[type] = ptr_to_shortptr(block);
    } else {
        int highest_free_slot = 31 - __builtin_clz(*bitset);
        next_pool = thinker_n(block, highest_free_slot, size)->sp_next;
    }
    *bitset |= 1u << slot;
    if (*bitset == 0xff) {
        // we need to unlink
//        *bitset &= ~(1u << slot);
//        dump_chain(type, size);
//        *bitset |= 1u << slot;
//        printf("  block %p now empty, so unlinking and freeing\n", block);
        shortptr_t *pprev = &thinker_pool[type];
        void *cur_block = shortptr_to_ptr(*pprev);
        while (cur_block) {
            if (cur_block == block) {
                *pprev = next_pool;
                break;
            }
            uint8_t *cur_bitset = Z_ObjectExtra(cur_block);
            int highest_free_slot = 31 - __builtin_clz(*cur_bitset);
            pprev = &thinker_n(cur_block, highest_free_slot, size)->sp_next;
            cur_block = shortptr_to_ptr(*pprev);
        }
        Z_Free(block);
    } else {
        if (*bitset < (2u << slot)) {
            // we are now the highest free bit, and should hold the forward pointer
//            printf("  slot %d is topmost free slot, so updating next link to %p\n", slot, next_pool);
            thinker_n(block, slot, size)->sp_next = next_pool;
        }
    }
//    dump_chain(type, size);
}
#endif
