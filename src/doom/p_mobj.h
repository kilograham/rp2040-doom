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
//	Map Objects, MObj, definition and handling.
//


#ifndef __P_MOBJ__
#define __P_MOBJ__

// Basics.
#include "tables.h"
#include "m_fixed.h"

// We need the thinker_t stuff.
#include "d_think.h"

// We need the WAD data structure for Map things,
// from the THINGS lump.
#include "doomdata.h"

// States are tied to finite states are
//  tied to animation frames.
// Needs precompiled tables/data structures.
#include "info.h"
#include "r_defs.h"
//
// NOTES: mobj_t
//
// mobj_ts are used to tell the refresh where to draw an image,
// tell the world simulation when objects are contacted,
// and tell the sound driver how to position a sound.
//
// The refresh uses the next and prev links to follow
// lists of things in sectors as they are being drawn.
// The sprite, frame, and angle elements determine which patch_t
// is used to draw the sprite if it is visible.
// The sprite and frame values are allmost allways set
// from state_t structures.
// The statescr.exe utility generates the states.h and states.c
// files that contain the sprite/frame numbers from the
// statescr.txt source file.
// The xyz origin point represents a point at the bottom middle
// of the sprite (between the feet of a biped).
// This is the default origin position for patch_ts grabbed
// with lumpy.exe.
// A walking creature will have its z equal to the floor
// it is standing on.
//
// The sound code uses the x,y, and subsector fields
// to do stereo positioning of any sound effited by the mobj_t.
//
// The play simulation uses the blocklinks, x,y,z, radius, height
// to determine when mobj_ts are touching each other,
// touching lines in the map, or hit by trace lines (gunshots,
// lines of sight, etc).
// The mobj_t->flags element has various bit flags
// used by the simulation.
//
// Every mobj_t is linked into a single sector
// based on its origin coordinates.
// The subsector_t is found with R_PointInSubsector(x,y),
// and the sector_t can be found with subsector->sector.
// The sector links are only used by the rendering code,
// the play simulation does not care about them at all.
//
// Any mobj_t that needs to be acted upon by something else
// in the play world (block movement, be shot, etc) will also
// need to be linked into the blockmap.
// If the thing has the MF_NOBLOCK flag set, it will not use
// the block links. It can still interact with other things,
// but only as the instigator (missiles will run into other
// things, but nothing can run into a missile).
// Each block in the grid is 128*128 units, and knows about
// every line_t that it contains a piece of, and every
// interactable mobj_t that has its origin contained.  
//
// A valid mobj_t is a mobj_t that has the proper subsector_t
// filled in for its xy coordinates and is linked into the
// sector from which the subsector was made, or has the
// MF_NOSECTOR flag set (the subsector_t needs to be valid
// even if MF_NOSECTOR is set), and is linked into a blockmap
// block or has the MF_NOBLOCKMAP flag set.
// Links should only be modified by the P_[Un]SetThingPosition()
// functions.
// Do not change the MF_NO? flags while a thing is valid.
//
// Any questions?
//

//
// Misc. mobj flags
//
typedef enum
{
    // Call P_SpecialThing when touched.
    MF_SPECIAL		= 1,
    // Blocks.
    MF_SOLID		= 2,
    // Can be hit.
    MF_SHOOTABLE	= 4,
    // Don't use the sector links (invisible but touchable).
    MF_NOSECTOR		= 8,
    // Don't use the blocklinks (inert but displayable)
    MF_NOBLOCKMAP	= 16,                    

    // Not to be activated by sound, deaf monster.
    MF_AMBUSH		= 32,
    // Will try to attack right back.
    MF_JUSTHIT		= 64,
    // Will take at least one step before attacking.
    MF_JUSTATTACKED	= 128,
    // On level spawning (initial position),
    //  hang from ceiling instead of stand on floor.
    MF_SPAWNCEILING	= 256,
    // Don't apply gravity (every tic),
    //  that is, object will float, keeping current height
    //  or changing it actively.
    MF_NOGRAVITY	= 512,

    // Movement flags.
    // This allows jumps from high places.
    MF_DROPOFF		= 0x400,
    // For players, will pick up items.
    MF_PICKUP		= 0x800,
    // Player cheat. ???
    MF_NOCLIP		= 0x1000,
    // Player: keep info about sliding along walls.
    MF_SLIDE		= 0x2000,
    // Allow moves to any height, no gravity.
    // For active floaters, e.g. cacodemons, pain elementals.
    MF_FLOAT		= 0x4000,
    // Don't cross lines
    //   ??? or look at heights on teleport.
    MF_TELEPORT		= 0x8000,
    // Don't hit same species, explode on block.
    // Player missiles as well as fireballs of various kinds.
    MF_MISSILE		= 0x10000,	
    // Dropped by a demon, not level spawned.
    // E.g. ammo clips dropped by dying former humans.
    MF_DROPPED		= 0x20000,
    // Use fuzzy draw (shadow demons or spectres),
    //  temporary player invisibility powerup.
    MF_SHADOW		= 0x40000,
    // Flag: don't bleed when shot (use puff),
    //  barrels and shootable furniture shall not bleed.
    MF_NOBLOOD		= 0x80000,
    // Don't stop moving halfway off a step,
    //  that is, have dead bodies slide down all the way.
    MF_CORPSE		= 0x100000,
    // Floating to a height for a move, ???
    //  don't auto float to target's height.
    MF_INFLOAT		= 0x200000,

    // On kill, count this enemy object
    //  towards intermission kill total.
    // Happy gathering.
    MF_COUNTKILL	= 0x400000,
    
    // On picking up, count this item object
    //  towards intermission item total.
    MF_COUNTITEM	= 0x800000,

    // Special handling: skull in flight.
    // Neither a cacodemon nor a missile.
    MF_SKULLFLY		= 0x1000000,

    // Don't spawn this object
    //  in death match mode (e.g. key cards).
    MF_NOTDMATCH    	= 0x2000000,

    // Player sprites in multiplayer modes are modified
    //  using an internal color lookup table for re-indexing.
    // If 0x4 0x8 or 0xc,
    //  use a translation table for player colormaps
    MF_TRANSLATION  	= 0xc000000,
    // Hmm ???.
    MF_TRANSSHIFT	= 26,

    MF_DECORATION = 0x10000000,
    MF_STATIC  = (MF_DECORATION | MF_SPECIAL)	// Objects with these flags go in the static mobj zone, and occupy less RAM
} mobjflag_t;

#if !SHRINK_MOBJ
typedef mapthing_t spawnpoint_t;
#define spawnpoint_mapthing(s) (s)
#define mobj_info(o) ((o)->info)
#define mobj_subsector(o) ((o)->subsector)
#define mobj_sector(o) subsector_sector(mobj_subsector(o))
#define mobj_state(o) ((o)->state)
#define mobj_state_num(o) (mobj_state(o) - states)
#define mobj_sprite(o) ((o)->sprite)
#define mobj_frame(o) ((o)->frame)
#else

extern const mapthing_t *mapthings;
extern uint16_t nummapthings;
typedef uint16_t spawnpoint_t;
#define spawnpoint_mapthing(s) mapthings[s]
#define mobj_info(o) (&mobjinfo[(o)->type])
#define mobj_sector(o) (&sectors[(o)->sector_num])
#define mobj_state_num(o) ((o)->state_num)
#define mobj_state(o) (&states[mobj_state_num(o)])
#define mobj_sprite(o) (mobj_state(o)->sprite)
#define mobj_frame(o) (mobj_state(o)->frame)
#endif
#define mobj_spawnpoint(o) spawnpoint_mapthing((o)->spawnpoint)
#define mobj_snext(o) ((mobj_t *)shortptr_to_ptr((o)->sp_snext))
#define mobj_sprev(o) ((mobj_t *)shortptr_to_ptr((o)->sp_sprev))
#define mobj_bnext(o) ((mobj_t *)shortptr_to_ptr((o)->sp_bnext))
#define mobj_bprev(o) ((mobj_t *)shortptr_to_ptr((o)->sp_bprev))

// Map Object definition.
typedef struct mobj_s
{
    // List: thinker links.
    thinker_t		thinker;

#if DEBUG_MOBJ
    int debug_id;
    int think_count;
#endif
#if !SHRINK_MOBJ
    int			tics;	// state tic counter
#else
    int8_t tics;
#endif

#if !SHRINK_MOBJ
    spritenum_t		sprite;	// used to find patch_t and flip value
    int		frame;	// might be ORed with FF_FULLBRIGHT
#endif

    mobjtype_t		type;
#if !SHRINK_MOBJ
    const mobjinfo_t*		info;	// &mobjinfo[mobj->type]
#endif

#if !SHRINK_MOBJ
    should_be_const state_t*		state;
#else
    statenum_t state_num; // probably only 10 bits
#endif

    // ===== 16 bit fields (when SHRUNK) todo collapse furher once we know everything======
#if !SHRINK_MOBJ
    subsector_t*	subsector;
#else
    uint16_t sector_num;
#endif

    spawnpoint_t spawnpoint;

    // Info for drawing: position.
    xy_positioned_t xy;
    fixed_t		z;

    int			flags;

    // More list: links in sector (if needed)
    shortptr_t/*struct mobj_s*/	sp_snext;
#if !SHRINK_MOBJ
    shortptr_t/*struct mobj_s*/	sp_sprev;
#endif

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    shortptr_t/*struct mobj_s*/	sp_bnext;
#if !SHRINK_MOBJ
    shortptr_t/*struct mobj_s*/	sp_bprev;
#endif

} mobj_t;

// ===== NON-STATIC object fields =====

typedef struct mobjfull_s
{
    mobj_t core;

    // Movement direction, movement generation (zig-zagging).
    int8_t		movedir;	// 0-7

#if !SHRINK_MOBJ
    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    int			reactiontime;
#else
    uint8_t reactiontime;
#endif

    // Player number last looked for.
#if !SHRINK_MOBJ
    int			lastlook;
#else
    int8_t			lastlook;
#endif

#if !SHRINK_MOBJ
    // If >0, the target will be chased
    // no matter what (even if shot)
    int			threshold;
#else
    int8_t			threshold;
#endif

#if !SHRINK_MOBJ
    int			health;
#else
    int16_t     health;
#endif

    int			movecount;	// when 0, select a new dir

    //More drawing info: to determine current sprite.
    angle_t		angle;	// orientation

    // The closest interval over all contacted Sectors.
    fixed_t		floorz;
    fixed_t		ceilingz;

    // For movement checking.
    fixed_t		radius; // mostly readonly (set to 0 at some point)
    fixed_t		height;	

    // Momentums, used to update position.
    fixed_t		momx;
    fixed_t		momy;
    fixed_t		momz;

    // If == validcount, already checked.
    //int			validcount;

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    shortptr_t /*struct mobj_s*/	sp_target;

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    shortptr_t /*struct player_s*/	sp_player;

    // Thing being chased/attacked for tracers.
    shortptr_t /*struct mobj_s*/	sp_tracer;

} mobjfull_t;
#define mobj_target(o) ((mobj_t *)shortptr_to_ptr(mobj_full(o)->sp_target))
#define mobj_player(o) ((struct player_s *)shortptr_to_ptr(mobj_full(o)->sp_player))
struct player_s;
static inline shortptr_t player_to_shortptr(struct player_s *p) {
    return ptr_to_shortptr(p);
}
#define mobj_tracer(o) ((mobj_t *)shortptr_to_ptr(mobj_full(o)->sp_tracer))
static inline shortptr_t mobj_to_shortptr(mobj_t *o) {
    return ptr_to_shortptr(o);
}
#define shortptr_to_mobj(s) ((mobj_t *)shortptr_to_ptr(s))

#if !DOOM_SMALL
#define mobj_flags_is_static(i) 0
#else
#define mobj_flags_is_static(i) ((i) & MF_STATIC)
#endif
#define mobj_is_static(o) (mobj_flags_is_static((o)->flags))

static inline mobjfull_t *mobj_full(mobj_t *mobj) {
    assert(!mobj_is_static(mobj));
    return (mobjfull_t *)mobj;
}

static inline int mobj_radius(mobj_t *mobj) {
    return mobj_is_static(mobj) ? mobj_info(mobj)->radius : mobj_full(mobj)->radius;
}

static inline int mobj_height(mobj_t *mobj) {
    return mobj_is_static(mobj) ? mobj_info(mobj)->height : mobj_full(mobj)->height;
}

static inline int mobj_is_player(mobj_t *mobj) {
    return !mobj_is_static(mobj) && mobj_full(mobj)->sp_player;
}

#if !DOOM_CONST
#define mobj_speed(o) (mobj_info(o)->speed)
#else
int mobj_speed(mobj_t *mo);
#endif

// todo we assume static objects are not crossing sectors; is this true?
#define mobj_floorz(mobj) (mobj_is_static(mobj) ? sector_floorheight(mobj_sector(mobj)) : mobj_full(mobj)->floorz)
#define mobj_ceilingz(mobj) (mobj_is_static(mobj) ? sector_ceilingheight(mobj_sector(mobj)) : mobj_full(mobj)->ceilingz)
//#define mobj_radius(o) mobj_radius(o)

// there is a reaction time in the info struct, however I've not seen it used on a static object
#define mobj_reactiontime(o) mobj_full(o)->reactiontime

static inline angle_t mobj_angle(mobj_t *mobj) {
    if (mobj_is_static(mobj)) {
        return ANG45 * (spawnpoint_mapthing(mobj->spawnpoint).angle / 45);
    } else {
        return mobj_full(mobj)->angle;
    }
}
#if PICO_BUILD && !DEBUG_MOBJ
#include "pico.h"
#include <assert.h>
#if PICO_ON_DEVICE
static_assert(sizeof(mobj_t)==0x20, "");
static_assert(sizeof(mobjfull_t)==0x54, "");
#else
static_assert(sizeof(mobj_t)==0x38, "");
#endif
#endif



#endif
