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
//  MapObj data. Map Objects or mobjs are actors, entities,
//  thinker, take-your-pick... anything that moves, acts, or
//  suffers state changes of more or less violent nature.
//


#ifndef __D_THINK__
#define __D_THINK__





//
// Experimental stuff.
// To compile this as "ANSI C with classes"
//  we will need to handle the various
//  action functions cleanly.
//
typedef  void (*actionf_v)();
typedef  void (*actionf_p1)( void* );
typedef  void (*actionf_p2)( void*, void* );

typedef union
{
  actionf_v	acv;
  actionf_p1	acp1;
  actionf_p2	acp2;

} actionf_t;





// Historically, "think_t" is yet another
//  function pointer to a routine to handle
//  an actor.
typedef enum {
    ThinkF_NULL = 0,
    ThinkF_T_MoveCeiling,
    ThinkF_T_VerticalDoor,
    ThinkF_T_PlatRaise,
    ThinkF_T_FireFlicker,
    ThinkF_T_LightFlash,
    ThinkF_T_StrobeFlash,
    ThinkF_T_MoveFloor,
    ThinkF_T_Glow,
    ThinkF_P_MobjThinker,
    ThinkF_INVALID, // only set during save game load, should be overwritten
    ThinkF_REMOVED,
    NUM_THINKF
} think_t_orig;
#include <assert.h>
static_assert(NUM_THINKF < 256, "");
typedef uint8_t think_t;

// linked list of actors.
typedef struct thinker_s
{
    // todo graham this can be an array index into an active thinker array
    shortptr_t /*struct thinker_s*/	sp_next;
    think_t		function;
#if DOOM_SMALL
    uint8_t pool_info; // 0xff if not in memory pool
#endif
} __attribute__((aligned(4))) thinker_t; // must be aligned for shortptr

#if DOOM_SMALL
#if PICO_ON_DEVICE
static_assert(sizeof(thinker_t) == 4, ""); // note z_zone requires this too to zero out
#endif
#endif
#define thinker_next(t) ((thinker_t *)shortptr_to_ptr((t)->sp_next))
static inline shortptr_t thinker_to_shortptr(thinker_t *thinker) {
    return ptr_to_shortptr(thinker);
}

#if !USE_THINKER_POOL
#include "z_zone.h"
static inline void *Z_ThinkMalloc(int size, int tag, void *user) {
    thinker_t *t = Z_Malloc(size, tag, user);
    memset(t, 0, size);
    return t;
}
#define Z_ThinkFree Z_Free
#else
#include "z_zone.h"
thinker_t *Z_ThinkMallocImpl(int size);
void Z_ThinkFree(thinker_t *thinker);
static inline void *Z_ThinkMalloc(int size, int tag, void *user) {
    assert(!user);
    assert(tag == PU_LEVEL);
    return Z_ThinkMallocImpl(size);
}
#endif



#endif
