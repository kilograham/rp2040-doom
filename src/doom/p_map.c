//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard, Andrey Budko
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
//	Movement, collision handling.
//	Shooting and aiming.
//

#include <stdio.h>
#include <stdlib.h>

#include "deh_misc.h"

#include "m_bbox.h"
#include "m_random.h"
#include "i_system.h"

#include "doomdef.h"
#include "m_argv.h"
#include "m_misc.h"
#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"
// Data.
#include "sounds.h"

// Spechit overrun magic value.
//
// This is the value used by PrBoom-plus.  I think the value below is 
// actually better and works with more demos.  However, I think
// it's better for the spechits emulation to be compatible with
// PrBoom-plus, at least so that the big spechits emulation list
// on Doomworld can also be used with Chocolate Doom.

#define DEFAULT_SPECHIT_MAGIC 0x01C09C98

// This is from a post by myk on the Doomworld forums, 
// outputted from entryway's spechit_magic generator for
// s205n546.lmp.  The _exact_ value of this isn't too
// important; as long as it is in the right general
// range, it will usually work.  Otherwise, we can use
// the generator (hacked doom2.exe) and provide it 
// with -spechit.

//#define DEFAULT_SPECHIT_MAGIC 0x84f968e8


fixed_t		tmbbox[4];
mobj_t*		tmthing;
int		tmflags;
fixed_t		tmx;
fixed_t		tmy;


// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
boolean		floatok;

fixed_t		tmfloorz;
fixed_t		tmceilingz;
fixed_t		tmdropoffz;

// keep track of the line that lowers the ceiling,
// so missiles don't explode against sky hack walls
line_t*		ceilingline;

// keep track of special lines as they are hit,
// but don't process them until the move is proven valid

line_t*		spechit[MAXSPECIALCROSS];
int		numspechit;



//
// TELEPORT MOVE
// 

//
// PIT_StompThing
//
boolean PIT_StompThing (mobj_t* thing)
{
    fixed_t	blockdist;
		
    if (!(thing->flags & MF_SHOOTABLE) )
	return true;
		
    blockdist = mobj_radius(thing) + mobj_radius(tmthing);
    
    if (abs(thing->xy.x - tmx) >= blockdist
	 || abs(thing->xy.y - tmy) >= blockdist )
    {
	// didn't hit it
	return true;
    }
    
    // don't clip against self
    if (thing == tmthing)
	return true;
    
    // monsters don't stomp things except on boss level
    if ( !mobj_full(tmthing)->sp_player && gamemap != 30)
	return false;	
		
    P_DamageMobj (thing, tmthing, tmthing, 10000);
	
    return true;
}


//
// P_TeleportMove
//
boolean
P_TeleportMove
( mobj_t*	thing,
  fixed_t	x,
  fixed_t	y )
{
    int			xl;
    int			xh;
    int			yl;
    int			yh;
    int			bx;
    int			by;
    
    subsector_t*	newsubsec;
    
    // kill anything occupying the position
    tmthing = thing;
    tmflags = thing->flags;
	
    tmx = x;
    tmy = y;
	
    tmbbox[BOXTOP] = y + mobj_radius(tmthing);
    tmbbox[BOXBOTTOM] = y - mobj_radius(tmthing);
    tmbbox[BOXRIGHT] = x + mobj_radius(tmthing);
    tmbbox[BOXLEFT] = x - mobj_radius(tmthing);

    newsubsec = R_PointInSubsector (x,y);
    ceilingline = NULL;
    
    // The base floor/ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    const sector_t *sec = subsector_sector(newsubsec);
    tmfloorz = tmdropoffz = sector_floorheight(sec);
    tmceilingz = sector_ceilingheight(sec);
			
    validcount++;
    numspechit = 0;
    
    // stomp on any things contacted
    xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

    for (bx=xl ; bx<=xh ; bx++)
	for (by=yl ; by<=yh ; by++)
	    if (!P_BlockThingsIterator(bx,by,PIT_StompThing))
		return false;
    
    // the move is ok,
    // so link the thing into its new position
    mobj_full(thing)->floorz = tmfloorz;
    mobj_full(thing)->ceilingz = tmceilingz;
    P_ResetThingPosition (thing, x, y);
	
    return true;
}


//
// MOVEMENT ITERATOR FUNCTIONS
//

static void SpechitOverrun(line_t *ld);

//
// PIT_CheckLine
// Adjusts tmfloorz and tmceilingz as lines are contacted
//
boolean PIT_CheckLine (line_t* ld)
{
#if !USE_RAW_MAPLINEDEF
    if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
	|| tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
	|| tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
	|| tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP] )
	return true;
#else
    // todo graham revisit
    if ((tmbbox[BOXRIGHT] <= vertex_x(line_v1(ld)) && tmbbox[BOXRIGHT] <= vertex_x(line_v2(ld)))
        || (tmbbox[BOXLEFT] >= vertex_x(line_v1(ld)) && tmbbox[BOXLEFT] >= vertex_x(line_v2(ld)))
        || (tmbbox[BOXTOP] <= vertex_y(line_v1(ld)) && tmbbox[BOXTOP] <= vertex_y(line_v2(ld)))
        || (tmbbox[BOXBOTTOM] >= vertex_y(line_v1(ld)) && tmbbox[BOXBOTTOM] >= vertex_y(line_v2(ld))))
        return true;
#endif

    if (P_BoxOnLineSide (tmbbox, ld) != -1)
	return true;
		
    // A line has been hit
    
    // The moving thing's destination position will cross
    // the given line.
    // If this should not be allowed, return false.
    // If the line is special, keep track of it
    // to process later if the move is proven ok.
    // NOTE: specials are NOT sorted by order,
    // so two special lines that are only 8 pixels apart
    // could be crossed in either order.
    
    if (!line_backsector(ld))
	return false;		// one sided line
		
    if (!(tmthing->flags & MF_MISSILE) )
    {
	if ( line_flags(ld) & ML_BLOCKING )
	    return false;	// explicitly blocking everything

	if ( !mobj_is_player(tmthing) && line_flags(ld) & ML_BLOCKMONSTERS )
	    return false;	// block monsters only
    }

    // set openrange, opentop, openbottom
    P_LineOpening (ld);	
	
    // adjust floor / ceiling heights
    if (opentop < tmceilingz)
    {
	tmceilingz = opentop;
	ceilingline = ld;
    }

    if (openbottom > tmfloorz)
	tmfloorz = openbottom;	

    if (lowfloor < tmdropoffz)
	tmdropoffz = lowfloor;
		
    // if contacted a special line, add it to the list
    if (line_special(ld))
    {
        spechit[numspechit] = ld;
	numspechit++;

        // fraggle: spechits overrun emulation code from prboom-plus
        if (numspechit > MAXSPECIALCROSS_ORIGINAL)
        {
            SpechitOverrun(ld);
        }
    }

    return true;
}

//
// PIT_CheckThing
//
boolean PIT_CheckThing (mobj_t* thing)
{
    fixed_t		blockdist;
    boolean		solid;
    int			damage;
		
    if (!(thing->flags & (MF_SOLID|MF_SPECIAL|MF_SHOOTABLE) ))
	return true;
    
    blockdist = mobj_radius(thing) + mobj_radius(tmthing);

    if (abs(thing->xy.x - tmx) >= blockdist
	 || abs(thing->xy.y - tmy) >= blockdist )
    {
	// didn't hit it
	return true;	
    }
    
    // don't clip against self
    if (thing == tmthing)
	return true;
    
    // check for skulls slamming into things
    if (tmthing->flags & MF_SKULLFLY)
    {
	damage = ((P_Random()%8)+1)*mobj_info(tmthing)->damage;
	
	P_DamageMobj (thing, tmthing, tmthing, damage);
	
	tmthing->flags &= ~MF_SKULLFLY;
	mobj_full(tmthing)->momx = mobj_full(tmthing)->momy = mobj_full(tmthing)->momz = 0;
	
	P_SetMobjState (tmthing, mobj_info(tmthing)->spawnstate);
	
	return false;		// stop moving
    }

    
    // missiles can hit other things
    if (tmthing->flags & MF_MISSILE)
    {
	// see if it went over / under
	if (tmthing->z > thing->z + mobj_height(thing))
	    return true;		// overhead
	if (tmthing->z+mobj_height(tmthing) < thing->z)
	    return true;		// underneath
		
	if (mobj_full(tmthing)->sp_target
         && (mobj_target(tmthing)->type == thing->type ||
	    (mobj_target(tmthing)->type == MT_KNIGHT && thing->type == MT_BRUISER)||
	    (mobj_target(tmthing)->type == MT_BRUISER && thing->type == MT_KNIGHT) ) )
	{
	    // Don't hit same species as originator.
	    if (thing == mobj_target(tmthing))
		return true;

            // sdh: Add deh_species_infighting here.  We can override the
            // "monsters of the same species cant hurt each other" behavior
            // through dehacked patches

	    if (thing->type != MT_PLAYER && !deh_species_infighting)
	    {
		// Explode, but do no damage.
		// Let players missile other players.
		return false;
	    }
	}
	
	if (! (thing->flags & MF_SHOOTABLE) )
	{
	    // didn't do any damage
	    return !(thing->flags & MF_SOLID);	
	}
	
	// damage / explode
	damage = ((P_Random()%8)+1)*mobj_info(tmthing)->damage;
	P_DamageMobj (thing, tmthing, mobj_target(tmthing), damage);

	// don't traverse any more
	return false;				
    }
    
    // check for special pickup
    if (thing->flags & MF_SPECIAL)
    {
	solid = (thing->flags & MF_SOLID) != 0;
	if (tmflags&MF_PICKUP)
	{
	    // can remove thing
	    P_TouchSpecialThing (thing, tmthing);
	}
	return !solid;
    }
	
    return !(thing->flags & MF_SOLID);
}


//
// MOVEMENT CLIPPING
//

//
// P_CheckPosition
// This is purely informative, nothing is modified
// (except things picked up).
// 
// in:
//  a mobj_t (can be valid or invalid)
//  a position to be checked
//   (doesn't need to be related to the mobj_t->x,y)
//
// during:
//  special things are touched if MF_PICKUP
//  early out on solid lines?
//
// out:
//  newsubsec
//  floorz
//  ceilingz
//  tmdropoffz
//   the lowest point contacted
//   (monsters won't move to a dropoff)
//  speciallines[]
//  numspeciallines
//
boolean
P_CheckPosition
( mobj_t*	thing,
  fixed_t	x,
  fixed_t	y )
{
    int			xl;
    int			xh;
    int			yl;
    int			yh;
    int			bx;
    int			by;
    subsector_t*	newsubsec;

    tmthing = thing;
    tmflags = thing->flags;
	
    tmx = x;
    tmy = y;
	
    tmbbox[BOXTOP] = y + mobj_radius(tmthing);
    tmbbox[BOXBOTTOM] = y - mobj_radius(tmthing);
    tmbbox[BOXRIGHT] = x + mobj_radius(tmthing);
    tmbbox[BOXLEFT] = x - mobj_radius(tmthing);

    newsubsec = R_PointInSubsector (x,y);
    ceilingline = NULL;
    
    // The base floor / ceiling is from the subsector
    // that contains the point.
    // Any contacted lines the step closer together
    // will adjust them.
    const sector_t *sec = subsector_sector(newsubsec);
    tmfloorz = tmdropoffz = sector_floorheight(sec);
    tmceilingz = sector_ceilingheight(sec);

    validcount++;
    numspechit = 0;

    if ( tmflags & MF_NOCLIP )
	return true;
    
    // Check things first, possibly picking things up.
    // The bounding box is extended by MAXRADIUS
    // because mobj_ts are grouped into mapblocks
    // based on their origin point, and can overlap
    // into adjacent blocks by up to MAXRADIUS units.
    xl = (tmbbox[BOXLEFT] - bmaporgx - MAXRADIUS)>>MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx + MAXRADIUS)>>MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy - MAXRADIUS)>>MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy + MAXRADIUS)>>MAPBLOCKSHIFT;

    for (bx=xl ; bx<=xh ; bx++)
	for (by=yl ; by<=yh ; by++)
	    if (!P_BlockThingsIterator(bx,by,PIT_CheckThing))
		return false;
    
    // check lines
    line_check_reset();
    xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
    xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
    yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
    yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

    for (bx=xl ; bx<=xh ; bx++)
	for (by=yl ; by<=yh ; by++)
	    if (!P_BlockLinesIterator (bx,by,PIT_CheckLine))
		return false;

    return true;
}


//
// P_TryMove
// Attempt to move to a new position,
// crossing special lines unless MF_TELEPORT is set.
//
boolean
P_TryMove
( mobj_t*	thing,
  fixed_t	x,
  fixed_t	y )
{
    fixed_t	oldx;
    fixed_t	oldy;
    int		side;
    int		oldside;
    line_t*	ld;

    floatok = false;
    if (!P_CheckPosition (thing, x, y))
	return false;		// solid wall or thing
    
    if ( !(thing->flags & MF_NOCLIP) )
    {
	if (tmceilingz - tmfloorz < mobj_height(thing))
	    return false;	// doesn't fit

	floatok = true;
	
	if ( !(thing->flags&MF_TELEPORT) 
	     &&tmceilingz - thing->z < mobj_height(thing))
	    return false;	// mobj must lower itself to fit

	if ( !(thing->flags&MF_TELEPORT)
	     && tmfloorz - thing->z > 24*FRACUNIT )
	    return false;	// too big a step up

	if ( !(thing->flags&(MF_DROPOFF|MF_FLOAT))
	     && tmfloorz - tmdropoffz > 24*FRACUNIT )
	    return false;	// don't stand over a dropoff
    }
    
    // the move is ok,
    // so link the thing into its new position
    oldx = thing->xy.x;
    oldy = thing->xy.y;
    mobj_full(thing)->floorz = tmfloorz;
    mobj_full(thing)->ceilingz = tmceilingz;
    P_ResetThingPosition (thing, x, y);
    
    // if any special lines were hit, do the effect
    if (! (thing->flags&(MF_TELEPORT|MF_NOCLIP)) )
    {
	while (numspechit--)
	{
	    // see if the line was crossed
	    ld = spechit[numspechit];
	    side = P_PointOnLineSide (thing->xy.x, thing->xy.y, ld);
	    oldside = P_PointOnLineSide (oldx, oldy, ld);
	    if (side != oldside)
	    {
		if (line_special(ld))
		    P_CrossSpecialLine (ld-lines, oldside, thing);
	    }
	}
    }

    return true;
}


//
// P_ThingHeightClip
// Takes a valid thing and adjusts the thing->floorz,
// thing->ceilingz, and possibly thing->z.
// This is called for all nearby monsters
// whenever a sector changes height.
// If the thing doesn't fit,
// the z will be set to the lowest value
// and false will be returned.
//
boolean P_ThingHeightClip (mobj_t* thing)
{
    boolean		onfloor;
	
    onfloor = (thing->z == mobj_floorz(thing));
	
    P_CheckPosition (thing, thing->xy.x, thing->xy.y);
    // what about stranding a monster partially off an edge?

    if (!mobj_is_static(thing)) {
        mobj_full(thing)->floorz = tmfloorz;
        mobj_full(thing)->ceilingz = tmceilingz;
    } else {
        onfloor = !(thing->flags & MF_FLOAT);
    }
	
    if (onfloor)
    {
	// walking monsters rise and fall with the floor
	thing->z = mobj_floorz(thing);
    }
    else
    {
	// don't adjust a floating monster unless forced to
	if (thing->z+mobj_height(thing) > mobj_ceilingz(thing))
	    thing->z = mobj_ceilingz(thing) - mobj_height(thing);
    }
	
    if (mobj_ceilingz(thing) - mobj_floorz(thing) < mobj_height(thing))
	return false;
		
    return true;
}



//
// SLIDE MOVE
// Allows the player to slide along any angled walls.
//
fixed_t		bestslidefrac;
fixed_t		secondslidefrac;

line_t*		bestslideline;
line_t*		secondslideline;

mobj_t*		slidemo;

fixed_t		tmxmove;
fixed_t		tmymove;



//
// P_HitSlideLine
// Adjusts the xmove / ymove
// so that the next move will slide along the wall.
//
void P_HitSlideLine (line_t* ld)
{
    int			side;

    angle_t		lineangle;
    angle_t		moveangle;
    angle_t		deltaangle;
    
    fixed_t		movelen;
    fixed_t		newlen;
	
	
    if (line_is_horiz(ld))
    {
	tmymove = 0;
	return;
    }
    
    if (line_is_vert(ld))
    {
	tmxmove = 0;
	return;
    }
	
    side = P_PointOnLineSide (slidemo->xy.x, slidemo->xy.y, ld);

    // todo graham this is a fixed value
    lineangle = R_PointToAngle2 (0,0, line_dx(ld), line_dy(ld));

    if (side == 1)
	lineangle += ANG180;

    moveangle = R_PointToAngle2 (0,0, tmxmove, tmymove);
    deltaangle = moveangle-lineangle;

    if (deltaangle > ANG180)
	deltaangle += ANG180;
    //	I_Error ("SlideLine: ang>ANG180");

    lineangle >>= ANGLETOFINESHIFT;
    deltaangle >>= ANGLETOFINESHIFT;
	
    movelen = P_AproxDistance (tmxmove, tmymove);
    newlen = FixedMul (movelen, finecosine(deltaangle));

    tmxmove = FixedMul (newlen, finecosine(lineangle));
    tmymove = FixedMul (newlen, finesine(lineangle));
}


//
// PTR_SlideTraverse
//
boolean PTR_SlideTraverse (intercept_t* in)
{
    line_t*	li;
	
    if (!in->isaline)
	I_Error ("PTR_SlideTraverse: not a line?");
		
    li = in->d.line;
    
    if ( ! (line_flags(li) & ML_TWOSIDED) )
    {
	if (P_PointOnLineSide (slidemo->xy.x, slidemo->xy.y, li))
	{
	    // don't hit the back side
	    return true;		
	}
	goto isblocking;
    }

    // set openrange, opentop, openbottom
    P_LineOpening (li);
    
    if (openrange < mobj_height(slidemo))
	goto isblocking;		// doesn't fit
		
    if (opentop - slidemo->z < mobj_height(slidemo))
	goto isblocking;		// mobj is too high

    if (openbottom - slidemo->z > 24*FRACUNIT )
	goto isblocking;		// too big a step up

    // this line doesn't block movement
    return true;		
	
    // the line does block movement,
    // see if it is closer than best so far
  isblocking:		
    if (in->frac < bestslidefrac)
    {
	secondslidefrac = bestslidefrac;
	secondslideline = bestslideline;
	bestslidefrac = in->frac;
	bestslideline = li;
    }
	
    return false;	// stop
}



//
// P_SlideMove
// The momx / momy move is bad, so try to slide
// along a wall.
// Find the first line hit, move flush to it,
// and slide along it
//
// This is a kludgy mess.
//
void P_SlideMove (mobj_t* mo)
{
    fixed_t		leadx;
    fixed_t		leady;
    fixed_t		trailx;
    fixed_t		traily;
    fixed_t		newx;
    fixed_t		newy;
    int			hitcount;
		
    slidemo = mo;
    hitcount = 0;
    
  retry:
    if (++hitcount == 3)
	goto stairstep;		// don't loop forever

    
    // trace along the three leading corners
    if (mobj_full(mo)->momx > 0)
    {
	leadx = mo->xy.x + mobj_radius(mo);
	trailx = mo->xy.x - mobj_radius(mo);
    }
    else
    {
	leadx = mo->xy.x - mobj_radius(mo);
	trailx = mo->xy.x + mobj_radius(mo);
    }
	
    if (mobj_full(mo)->momy > 0)
    {
	leady = mo->xy.y + mobj_radius(mo);
	traily = mo->xy.y - mobj_radius(mo);
    }
    else
    {
	leady = mo->xy.y - mobj_radius(mo);
	traily = mo->xy.y + mobj_radius(mo);
    }
		
    bestslidefrac = FRACUNIT+1;
	
    P_PathTraverse ( leadx, leady, leadx+mobj_full(mo)->momx, leady+mobj_full(mo)->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    P_PathTraverse ( trailx, leady, trailx+mobj_full(mo)->momx, leady+mobj_full(mo)->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    P_PathTraverse ( leadx, traily, leadx+mobj_full(mo)->momx, traily+mobj_full(mo)->momy,
		     PT_ADDLINES, PTR_SlideTraverse );
    
    // move up to the wall
    if (bestslidefrac == FRACUNIT+1)
    {
	// the move most have hit the middle, so stairstep
      stairstep:
	if (!P_TryMove (mo, mo->xy.x, mo->xy.y + mobj_full(mo)->momy))
	    P_TryMove (mo, mo->xy.x + mobj_full(mo)->momx, mo->xy.y);
	return;
    }

    // fudge a bit to make sure it doesn't hit
    bestslidefrac -= 0x800;	
    if (bestslidefrac > 0)
    {
	newx = FixedMul (mobj_full(mo)->momx, bestslidefrac);
	newy = FixedMul (mobj_full(mo)->momy, bestslidefrac);
	
	if (!P_TryMove (mo, mo->xy.x + newx, mo->xy.y + newy))
	    goto stairstep;
    }
    
    // Now continue along the wall.
    // First calculate remainder.
    bestslidefrac = FRACUNIT-(bestslidefrac+0x800);
    
    if (bestslidefrac > FRACUNIT)
	bestslidefrac = FRACUNIT;
    
    if (bestslidefrac <= 0)
	return;
    
    tmxmove = FixedMul (mobj_full(mo)->momx, bestslidefrac);
    tmymove = FixedMul (mobj_full(mo)->momy, bestslidefrac);

    P_HitSlideLine (bestslideline);	// clip the moves

    mobj_full(mo)->momx = tmxmove;
    mobj_full(mo)->momy = tmymove;
		
    if (!P_TryMove (mo, mo->xy.x + tmxmove, mo->xy.y + tmymove))
    {
	goto retry;
    }
}


//
// P_LineAttack
//
mobj_t*		linetarget;	// who got hit (or NULL)
mobj_t*		shootthing;

// Height if not aiming up or down
// ???: use slope for monsters?
fixed_t		shootz;	

int		la_damage;
fixed_t		attackrange;

fixed_t		aimslope;

// slopes to top and bottom of target
extern fixed_t	topslope;
extern fixed_t	bottomslope;	


//
// PTR_AimTraverse
// Sets linetaget and aimslope when a target is aimed at.
//
boolean
PTR_AimTraverse (intercept_t* in)
{
    line_t*		li;
    mobj_t*		th;
    fixed_t		slope;
    fixed_t		thingtopslope;
    fixed_t		thingbottomslope;
    fixed_t		dist;
		
    if (in->isaline)
    {
	li = in->d.line;
	
	if ( !(line_flags(li) & ML_TWOSIDED) )
	    return false;		// stop
	
	// Crosses a two sided line.
	// A two sided line will restrict
	// the possible target ranges.
	P_LineOpening (li);
	
	if (openbottom >= opentop)
	    return false;		// stop
	
	dist = FixedMul (attackrange, in->frac);

        if (line_backsector(li) == NULL
         || line_frontsector(li)->rawfloorheight != line_backsector(li)->rawfloorheight)
	{
	    slope = FixedDiv (openbottom - shootz , dist);
	    if (slope > bottomslope)
		bottomslope = slope;
	}
		
	if (line_backsector(li) == NULL
         || line_frontsector(li)->rawceilingheight != line_backsector(li)->rawceilingheight)
	{
	    slope = FixedDiv (opentop - shootz , dist);
	    if (slope < topslope)
		topslope = slope;
	}
		
	if (topslope <= bottomslope)
	    return false;		// stop
			
	return true;			// shot continues
    }
    
    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
	return true;			// can't shoot self
    
    if (!(th->flags&MF_SHOOTABLE))
	return true;			// corpse or something

    // check angles to see if the thing can be aimed at
    dist = FixedMul (attackrange, in->frac);
    thingtopslope = FixedDiv (th->z+mobj_height(th) - shootz , dist);

    if (thingtopslope < bottomslope)
	return true;			// shot over the thing

    thingbottomslope = FixedDiv (th->z - shootz, dist);

    if (thingbottomslope > topslope)
	return true;			// shot under the thing
    
    // this thing can be hit!
    if (thingtopslope > topslope)
	thingtopslope = topslope;
    
    if (thingbottomslope < bottomslope)
	thingbottomslope = bottomslope;

    aimslope = (thingtopslope+thingbottomslope)/2;
    linetarget = th;

    return false;			// don't go any farther
}


//
// PTR_ShootTraverse
//
boolean PTR_ShootTraverse (intercept_t* in)
{
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;
    fixed_t		frac;
    
    line_t*		li;
    
    mobj_t*		th;

    fixed_t		slope;
    fixed_t		dist;
    fixed_t		thingtopslope;
    fixed_t		thingbottomslope;
		
    if (in->isaline)
    {
	li = in->d.line;
	
	if (line_special(li))
	    P_ShootSpecialLine (shootthing, li);

	if ( !(line_flags(li) & ML_TWOSIDED) )
	    goto hitline;
	
	// crosses a two sided line
	P_LineOpening (li);
		
	dist = FixedMul (attackrange, in->frac);

        // e6y: emulation of missed back side on two-sided lines.
        // backsector can be NULL when emulating missing back side.

        if (line_backsector(li) == NULL)
        {
            slope = FixedDiv (openbottom - shootz , dist);
            if (slope > aimslope)
                goto hitline;

            slope = FixedDiv (opentop - shootz , dist);
            if (slope < aimslope)
                goto hitline;
        }
        else
        {
            if (line_frontsector(li)->rawfloorheight != line_backsector(li)->rawfloorheight)
            {
                slope = FixedDiv (openbottom - shootz , dist);
                if (slope > aimslope)
                    goto hitline;
            }

            if (line_frontsector(li)->rawceilingheight != line_backsector(li)->rawceilingheight)
            {
                slope = FixedDiv (opentop - shootz , dist);
                if (slope < aimslope)
                    goto hitline;
            }
        }

	// shot continues
	return true;
	
	
	// hit line
      hitline:
	// position a bit closer
	frac = in->frac - FixedDiv (4*FRACUNIT,attackrange);
	x = trace.x + FixedMul (trace.dx, frac);
	y = trace.y + FixedMul (trace.dy, frac);
	z = shootz + FixedMul (aimslope, FixedMul(frac, attackrange));

	if (line_frontsector(li)->ceilingpic == skyflatnum)
	{
	    // don't shoot the sky!
	    if (z > sector_ceilingheight(line_frontsector(li)))
		return false;
	    
	    // it's a sky hack wall
	    if	(line_backsector(li) && line_backsector(li)->ceilingpic == skyflatnum)
		return false;		
	}

	// Spawn bullet puffs.
	P_SpawnPuff (x,y,z);
	
	// don't go any farther
	return false;	
    }
    
    // shoot a thing
    th = in->d.thing;
    if (th == shootthing)
	return true;		// can't shoot self
    
    if (!(th->flags&MF_SHOOTABLE))
	return true;		// corpse or something
		
    // check angles to see if the thing can be aimed at
    dist = FixedMul (attackrange, in->frac);
    thingtopslope = FixedDiv (th->z+mobj_height(th) - shootz , dist);

    if (thingtopslope < aimslope)
	return true;		// shot over the thing

    thingbottomslope = FixedDiv (th->z - shootz, dist);

    if (thingbottomslope > aimslope)
	return true;		// shot under the thing

    
    // hit thing
    // position a bit closer
    frac = in->frac - FixedDiv (10*FRACUNIT,attackrange);

    x = trace.x + FixedMul (trace.dx, frac);
    y = trace.y + FixedMul (trace.dy, frac);
    z = shootz + FixedMul (aimslope, FixedMul(frac, attackrange));

    // Spawn bullet puffs or blod spots,
    // depending on target type.
    if (in->d.thing->flags & MF_NOBLOOD)
	P_SpawnPuff (x,y,z);
    else
	P_SpawnBlood (x,y,z, la_damage);

    if (la_damage)
	P_DamageMobj (th, shootthing, shootthing, la_damage);

    // don't go any farther
    return false;
	
}


//
// P_AimLineAttack
//
fixed_t
P_AimLineAttack
( mobj_t*	t1,
  angle_t	angle,
  fixed_t	distance )
{
    fixed_t	x2;
    fixed_t	y2;

    t1 = P_SubstNullMobj(t1);
	
    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    
    x2 = t1->xy.x + (distance >> FRACBITS) * finecosine(angle);
    y2 = t1->xy.y + (distance >> FRACBITS) * finesine(angle);
    shootz = t1->z + (mobj_height(t1)>>1) + 8*FRACUNIT;

    // can't shoot outside view angles
    topslope = (SCREENHEIGHT/2)*FRACUNIT/(SCREENWIDTH/2);	
    bottomslope = -(SCREENHEIGHT/2)*FRACUNIT/(SCREENWIDTH/2);
    
    attackrange = distance;
    linetarget = NULL;
	
    P_PathTraverse (t1->xy.x, t1->xy.y,
                    x2, y2,
		     PT_ADDLINES|PT_ADDTHINGS,
                    PTR_AimTraverse );
		
    if (linetarget)
	return aimslope;

    return 0;
}
 

//
// P_LineAttack
// If damage == 0, it is just a test trace
// that will leave linetarget set.
//
void
P_LineAttack
( mobj_t*	t1,
  angle_t	angle,
  fixed_t	distance,
  fixed_t	slope,
  int		damage )
{
    fixed_t	x2;
    fixed_t	y2;
	
    angle >>= ANGLETOFINESHIFT;
    shootthing = t1;
    la_damage = damage;
    x2 = t1->xy.x + (distance >> FRACBITS) * finecosine(angle);
    y2 = t1->xy.y + (distance >> FRACBITS) * finesine(angle);
    shootz = t1->z + (mobj_height(t1)>>1) + 8*FRACUNIT;
    attackrange = distance;
    aimslope = slope;
		
    P_PathTraverse (t1->xy.x, t1->xy.y,
                    x2, y2,
		     PT_ADDLINES|PT_ADDTHINGS,
                    PTR_ShootTraverse );
}
 


//
// USE LINES
//
mobj_t*		usething;

boolean	PTR_UseTraverse (intercept_t* in)
{
    int		side;
	
    if (!line_special(in->d.line))
    {
	P_LineOpening (in->d.line);
	if (openrange <= 0)
	{
	    S_StartObjSound (usething, sfx_noway);
	    
	    // can't use through a wall
	    return false;	
	}
	// not a special line, but keep checking
	return true ;		
    }
	
    side = 0;
    if (P_PointOnLineSide (usething->xy.x, usething->xy.y, in->d.line) == 1)
	side = 1;
    
    //	return false;		// don't use back side
	
    P_UseSpecialLine (usething, in->d.line, side);

    // can't use for than one special line in a row
    return false;
}


//
// P_UseLines
// Looks for special lines in front of the player to activate.
//
void P_UseLines (player_t*	player) 
{
    int		angle;
    fixed_t	x1;
    fixed_t	y1;
    fixed_t	x2;
    fixed_t	y2;
	
    usething = player->mo;
		
    angle = mobj_full(player->mo)->angle >> ANGLETOFINESHIFT;

    x1 = player->mo->xy.x;
    y1 = player->mo->xy.y;
    x2 = x1 + (USERANGE>>FRACBITS)*finecosine(angle);
    y2 = y1 + (USERANGE>>FRACBITS)*finesine(angle);
	
    P_PathTraverse ( x1, y1, x2, y2, PT_ADDLINES, PTR_UseTraverse );
}


//
// RADIUS ATTACK
//
mobj_t*		bombsource;
mobj_t*		bombspot;
isb_uint8_t 	bombdamage;


//
// PIT_RadiusAttack
// "bombsource" is the creature
// that caused the explosion at "bombspot".
//
boolean PIT_RadiusAttack (mobj_t* thing)
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	dist;
	
    if (!(thing->flags & MF_SHOOTABLE) )
	return true;

    // Boss spider and cyborg
    // take no damage from concussion.
    if (thing->type == MT_CYBORG
	|| thing->type == MT_SPIDER)
	return true;	
		
    dx = abs(thing->xy.x - bombspot->xy.x);
    dy = abs(thing->xy.y - bombspot->xy.y);
    
    dist = dx>dy ? dx : dy;
    dist = (dist - mobj_radius(thing)) >> FRACBITS;

    if (dist < 0)
	dist = 0;

    if (dist >= bombdamage)
	return true;	// out of range

    if ( P_CheckSight (thing, bombspot) )
    {
	// must be in direct path
	P_DamageMobj (thing, bombspot, bombsource, ((fixed_t)bombdamage) - dist);
    }
    
    return true;
}


//
// P_RadiusAttack
// Source is the creature that caused the explosion at spot.
//
void
P_RadiusAttack
( mobj_t*	spot,
  mobj_t*	source,
  int		damage )
{
    int		x;
    int		y;
    
    int		xl;
    int		xh;
    int		yl;
    int		yh;
    
    fixed_t	dist;
	
    dist = (damage+MAXRADIUS)<<FRACBITS;
    yh = (spot->xy.y + dist - bmaporgy) >> MAPBLOCKSHIFT;
    yl = (spot->xy.y - dist - bmaporgy) >> MAPBLOCKSHIFT;
    xh = (spot->xy.x + dist - bmaporgx) >> MAPBLOCKSHIFT;
    xl = (spot->xy.x - dist - bmaporgx) >> MAPBLOCKSHIFT;
    bombspot = spot;
    bombsource = source;
    bombdamage = damage;
	
    for (y=yl ; y<=yh ; y++)
	for (x=xl ; x<=xh ; x++)
	    P_BlockThingsIterator (x, y, PIT_RadiusAttack );
}



//
// SECTOR HEIGHT CHANGING
// After modifying a sectors floor or ceiling height,
// call this routine to adjust the positions
// of all things that touch the sector.
//
// If anything doesn't fit anymore, true will be returned.
// If crunch is true, they will take damage
//  as they are being crushed.
// If Crunch is false, you should set the sector height back
//  the way it was and call P_ChangeSector again
//  to undo the changes.
//
boolean		crushchange;
boolean		nofit;


//
// PIT_ChangeSector
//
boolean PIT_ChangeSector (mobj_t*	thing)
{
    mobj_t*	mo;
	
    if (P_ThingHeightClip (thing))
    {
	// keep checking
	return true;
    }
    

    // crunch bodies to giblets
    if (!mobj_is_static(thing) && mobj_full(thing)->health <= 0)
    {
	P_SetMobjState (thing, S_GIBS);

	thing->flags &= ~MF_SOLID;
	mobj_full(thing)->height = 0;
	mobj_full(thing)->radius = 0;

	// keep checking
	return true;		
    }

    // crunch dropped items
    if (thing->flags & MF_DROPPED)
    {
	P_RemoveMobj (thing);
	
	// keep checking
	return true;		
    }

    if (! (thing->flags & MF_SHOOTABLE) )
    {
	// assume it is bloody gibs or something
	return true;			
    }
    
    nofit = true;

    if (crushchange && !(leveltime&3) )
    {
	P_DamageMobj(thing,NULL,NULL,10);

	// spray blood in a random direction
	mo = P_SpawnMobj (thing->xy.x,
			  thing->xy.y,
			  thing->z + mobj_height(thing)/2, MT_BLOOD);
	
	mobj_full(mo)->momx = P_SubRandom() << 12;
	mobj_full(mo)->momy = P_SubRandom() << 12;
    }

    // keep checking (crush other things)	
    return true;	
}



//
// P_ChangeSector
//
boolean
P_ChangeSector
( sector_t*	sector,
  boolean	crunch )
{
    int		x;
    int		y;
	
    nofit = false;
    crushchange = crunch;
	
    // re-check heights for all things near the moving sector
    for (x=sector->blockbox[BOXLEFT] ; x<= sector->blockbox[BOXRIGHT] ; x++)
	for (y=sector->blockbox[BOXBOTTOM];y<= sector->blockbox[BOXTOP] ; y++)
	    P_BlockThingsIterator (x, y, PIT_ChangeSector);
	
	
    return nofit;
}

// Code to emulate the behavior of Vanilla Doom when encountering an overrun
// of the spechit array.  This is by Andrey Budko (e6y) and comes from his
// PrBoom plus port.  A big thanks to Andrey for this.

static void SpechitOverrun(line_t *ld)
{
    static unsigned int baseaddr = 0;
    unsigned int addr;
   
    if (baseaddr == 0)
    {
        int p;

        // This is the first time we have had an overrun.  Work out
        // what base address we are going to use.
        // Allow a spechit value to be specified on the command line.

        //!
        // @category compat
        // @arg <n>
        //
        // Use the specified magic value when emulating spechit overruns.
        //

#if !NO_USE_ARGS
        p = M_CheckParmWithArgs("-spechit", 1);
        
        if (p > 0)
        {
            M_StrToInt(myargv[p+1], (int *) &baseaddr);
        }
        else
#endif
        {
            baseaddr = DEFAULT_SPECHIT_MAGIC;
        }
    }
    
    // Calculate address used in doom2.exe

    addr = baseaddr + (ld - lines) * 0x3E;

    switch(numspechit)
    {
        case 9: 
        case 10:
        case 11:
        case 12:
            tmbbox[numspechit-9] = addr;
            break;
        case 13: 
            crushchange = addr; 
            break;
        case 14: 
            nofit = addr; 
            break;
        default:
            stderr_print( "SpechitOverrun: Warning: unable to emulate"
                            "an overrun where numspechit=%i\n",
                            numspechit);
            break;
    }
}

