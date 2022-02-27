//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2005, 2006 Andrey Budko
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
//	Movement/collision utility functions,
//	as used by function in p_map.c. 
//	BLOCKMAP Iterator functions,
//	and some PIT_* functions to use for iteration.
//



#include <stdlib.h>


#include "m_bbox.h"

#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"


// State.
#include "r_state.h"

//
// P_AproxDistance
// Gives an estimation of distance (not exact)
//

fixed_t
P_AproxDistance
( fixed_t	dx,
  fixed_t	dy )
{
    dx = abs(dx);
    dy = abs(dy);
    if (dx < dy)
	return dx+dy-(dx>>1);
    return dx+dy-(dy>>1);
}


//
// P_PointOnLineSide
// Returns 0 or 1
//
int
P_PointOnLineSide
( fixed_t	x,
  fixed_t	y,
  line_t*	line )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    if (line_is_vert(line))
    {
	if (x <= vertex_x(line_v1(line)))
	    return line_dy_gt_0(line);
	
	return line_dy_lt_0(line);
    }
    if (line_is_horiz(line))
    {
	if (y <= vertex_y(line_v1(line)))
	    return line_dx_lt_0(line);
	
	return line_dx_gt_0(line);
    }
	
    dx = (x - vertex_x(line_v1(line)));
    dy = (y - vertex_y(line_v1(line)));
	
    left = FixedMul ( line_dy(line)>>FRACBITS , dx );
    right = FixedMul ( dy , line_dx(line)>>FRACBITS );
	
    if (right < left)
	return 0;		// front side
    return 1;			// back side
}



//
// P_BoxOnLineSide
// Considers the line to be infinite
// Returns side 0 or 1, -1 if box crosses the line.
//
int
P_BoxOnLineSide
( fixed_t*	tmbox,
  line_t*	ld )
{
    int		p1 = 0;
    int		p2 = 0;

    switch (line_slopetype(ld))
    {
      case ST_HORIZONTAL:
	p1 = tmbox[BOXTOP] > vertex_y(line_v1(ld));
	p2 = tmbox[BOXBOTTOM] > vertex_y(line_v1(ld));
	if (line_dx_gt_0(ld))
	{
	    p1 ^= 1;
	    p2 ^= 1;
	}
	break;

      case ST_VERTICAL:
	p1 = tmbox[BOXRIGHT] < vertex_x(line_v1(ld));
	p2 = tmbox[BOXLEFT] < vertex_x(line_v1(ld));
	if (line_dy_lt_0(ld))
	{
	    p1 ^= 1;
	    p2 ^= 1;
	}
	break;
	
      case ST_POSITIVE:
	p1 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXTOP], ld);
	p2 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXBOTTOM], ld);
	break;
	
      case ST_NEGATIVE:
	p1 = P_PointOnLineSide (tmbox[BOXRIGHT], tmbox[BOXTOP], ld);
	p2 = P_PointOnLineSide (tmbox[BOXLEFT], tmbox[BOXBOTTOM], ld);
	break;
    }

    if (p1 == p2)
	return p1;
    return -1;
}


//
// P_PointOnDivlineSide
// Returns 0 or 1.
//
int
P_PointOnDivlineSide
( fixed_t	x,
  fixed_t	y,
  divline_t*	line )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;
	
    if (!line->dx)
    {
	if (x <= line->x)
	    return line->dy > 0;
	
	return line->dy < 0;
    }
    if (!line->dy)
    {
	if (y <= line->y)
	    return line->dx < 0;

	return line->dx > 0;
    }
	
    dx = (x - line->x);
    dy = (y - line->y);
	
    // try to quickly decide by looking at sign bits
    if ( (line->dy ^ line->dx ^ dx ^ dy)&0x80000000 )
    {
	if ( (line->dy ^ dx) & 0x80000000 )
	    return 1;		// (left is negative)
	return 0;
    }
	
    left = FixedMul ( line->dy>>8, dx>>8 );
    right = FixedMul ( dy>>8 , line->dx>>8 );
	
    if (right < left)
	return 0;		// front side
    return 1;			// back side
}



//
// P_MakeDivline
//
void
P_MakeDivline
( line_t*	li,
  divline_t*	dl )
{
    dl->x = vertex_x(line_v1(li));
    dl->y = vertex_y(line_v1(li));
    dl->dx = line_dx(li);
    dl->dy = line_dy(li);
}



//
// P_InterceptVector
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings
// and addlines traversers.
//
fixed_t
P_InterceptVector
( divline_t*	v2,
  divline_t*	v1 )
{
#if 1
    fixed_t	frac;
    fixed_t	num;
    fixed_t	den;
	
    den = FixedMul (v1->dy>>8,v2->dx) - FixedMul(v1->dx>>8,v2->dy);

    if (den == 0)
	return 0;
    //	I_Error ("P_InterceptVector: parallel");
    
    num =
	FixedMul ( (v1->x - v2->x)>>8 ,v1->dy )
	+FixedMul ( (v2->y - v1->y)>>8, v1->dx );

    frac = FixedDiv (num , den);

    return frac;
#else	// UNUSED, float debug.
    float	frac;
    float	num;
    float	den;
    float	v1x;
    float	v1y;
    float	v1dx;
    float	v1dy;
    float	v2x;
    float	v2y;
    float	v2dx;
    float	v2dy;

    v1x = (float)v1->x/FRACUNIT;
    v1y = (float)v1->y/FRACUNIT;
    v1dx = (float)v1->dx/FRACUNIT;
    v1dy = (float)v1->dy/FRACUNIT;
    v2x = (float)v2->x/FRACUNIT;
    v2y = (float)v2->y/FRACUNIT;
    v2dx = (float)v2->dx/FRACUNIT;
    v2dy = (float)v2->dy/FRACUNIT;
	
    den = v1dy*v2dx - v1dx*v2dy;

    if (den == 0)
	return 0;	// parallel
    
    num = (v1x - v2x)*v1dy + (v2y - v1y)*v1dx;
    frac = num / den;

    return frac*FRACUNIT;
#endif
}


//
// P_LineOpening
// Sets opentop and openbottom to the window
// through a two sided line.
// OPTIMIZE: keep this precalculated
//
fixed_t opentop;
fixed_t openbottom;
fixed_t openrange;
fixed_t	lowfloor;


void P_LineOpening (line_t* linedef)
{
    sector_t*	front;
    sector_t*	back;
	
    if (line_onesided(linedef))
    {
	// single sided line
	openrange = 0;
	return;
    }
	 
    front = line_frontsector(linedef);
    back = line_backsector(linedef);
	
    if (front->rawceilingheight < back->rawceilingheight)
	opentop = sector_ceilingheight(front);
    else
	opentop = sector_ceilingheight(back);

    if (front->rawfloorheight > back->rawfloorheight)
    {
	openbottom = sector_floorheight(front);
	lowfloor = sector_floorheight(back);
    }
    else
    {
	openbottom = sector_floorheight(back);
	lowfloor = sector_floorheight(front);
    }
	
    openrange = opentop - openbottom;
}


//
// THING POSITION SETTING
//


//
// P_UnsetThingPosition
// Unlinks a thing from block map and sectors.
// On each position change, BLOCKMAP and other
// lookups maintaining lists ot things inside
// these structures need to be updated.
//
void P_UnsetThingPosition (mobj_t* thing)
{
    int		blockx;
    int		blocky;

    if ( ! (thing->flags & MF_NOSECTOR) )
    {
#if !SHRINK_MOBJ
	// inert things don't need to be in blockmap?
	// unlink from subsector
	if (thing->sp_snext)
            mobj_snext(thing)->sp_sprev = thing->sp_sprev;

	if (thing->sp_sprev)
	    mobj_sprev(thing)->sp_snext = thing->sp_snext;
	else
	    mobj_sector(thing)->thinglist = thing->sp_snext;
#else
	// removed back link, so start from beginning
	shortptr_t *prev = &mobj_sector(thing)->thinglist;
	//assert(*prev);
	while (*prev) {
	    mobj_t *mobj = shortptr_to_mobj(*prev);
	    if (mobj == thing) {
	        *prev = mobj->sp_snext;
	        break;
	    }
	    prev = &mobj->sp_snext;
	}
#endif
    }
	
    if ( ! (thing->flags & MF_NOBLOCKMAP) )
    {
#if !SHRINK_MOBJ
	// inert things don't need to be in blockmap
	// unlink from block map
	if (thing->sp_bnext)
	    mobj_bnext(thing)->sp_bprev = thing->sp_bprev;
	
	if (thing->sp_bprev)
	    mobj_bprev(thing)->sp_bnext = thing->sp_bnext;
	else
	{
	    blockx = (thing->xy.x - bmaporgx) >> MAPBLOCKSHIFT;
	    blocky = (thing->xy.y - bmaporgy) >> MAPBLOCKSHIFT;

	    if (blockx>=0 && blockx < bmapwidth
		&& blocky>=0 && blocky <bmapheight)
	    {
		blocklinks[blocky*bmapwidth+blockx] = thing->sp_bnext;
	    }
	}
#else
        blockx = (thing->xy.x - bmaporgx) >> MAPBLOCKSHIFT;
        blocky = (thing->xy.y - bmaporgy) >> MAPBLOCKSHIFT;
        if (blockx>=0 && blockx < bmapwidth
            && blocky>=0 && blocky <bmapheight) {
            shortptr_t *prev = &blocklinks[blocky*bmapwidth+blockx];
            //assert(*prev);
            while (*prev) {
                mobj_t *mobj = shortptr_to_mobj(*prev);
                if (mobj == thing) {
                    *prev = mobj->sp_bnext;
                    break;
                }
                prev = &mobj->sp_bnext;
            }
        } else {
            assert(!thing->sp_bnext);
        }
#endif
    }
}


//
// P_SetThingPosition
// Links a thing into both a block and a subsector
// based on it's x y.
// Sets thing->subsector properly
//
void
P_SetThingPosition (mobj_t* thing)
{
    subsector_t*	ss;
    sector_t*		sec;
    int			blockx;
    int			blocky;
    shortptr_t /*mobj_t**/*		link;

    
    // link into subsector
    ss = R_PointInSubsector (thing->xy.x, thing->xy.y);
#if !SHRINK_MOBJ
    thing->subsector = ss;
#else
    thing->sector_num = subsector_sector(ss) - sectors;
#endif
    
    if ( ! (thing->flags & MF_NOSECTOR) )
    {
	// invisible things don't go into the sector links
	sec = subsector_sector(ss);

#if !SHRINK_MOBJ
	thing->sp_sprev = 0;
#endif
	thing->sp_snext = sec->thinglist;

#if !SHRINK_MOBJ
	if (sec->thinglist)
	    shortptr_to_mobj(sec->thinglist)->sp_sprev = mobj_to_shortptr(thing);
#endif

	sec->thinglist = mobj_to_shortptr(thing);
    }

    
    // link into blockmap
    if ( ! (thing->flags & MF_NOBLOCKMAP) )
    {
	// inert things don't need to be in blockmap		
	blockx = (thing->xy.x - bmaporgx) >> MAPBLOCKSHIFT;
	blocky = (thing->xy.y - bmaporgy) >> MAPBLOCKSHIFT;

	if (blockx>=0
	    && blockx < bmapwidth
	    && blocky>=0
	    && blocky < bmapheight)
	{
	    link = &blocklinks[blocky*bmapwidth+blockx];
#if !SHRINK_MOBJ
	    thing->sp_bprev = 0;
#endif
	    thing->sp_bnext = *link;
#if !SHRINK_MOBJ
            if (*link)
		shortptr_to_mobj(*link)->sp_bprev = mobj_to_shortptr(thing);
#endif
	    *link = mobj_to_shortptr(thing);
	}
	else
	{
	    // thing is off the map
	    thing->sp_bnext = 0;
#if !SHRINK_MOBJ
	    thing->sp_bprev = 0;
#endif
	}
    }
}

void P_ResetThingPosition (mobj_t* thing, fixed_t x, fixed_t y) {
    // arghh.. DoomII demo 3 relies on the oreder of the things in the blockmap...
    // don't think this is probably a huge speed concern, so reverting always to unset/set which moves the item
    // to the front.
#if true || !SHRINK_MOBJ
    P_UnsetThingPosition(thing);
    thing->xy.x = x;
    thing->xy.y = y;
    P_SetThingPosition(thing);
#else
    if ( ! (thing->flags & MF_NOSECTOR) ) {
        sector_t *oldsec = mobj_sector(thing);
        subsector_t *newsub = R_PointInSubsector(x, y);
#if !SHRINK_MOBJ
        thing->subsector = newsub;
#else
        thing->sector_num = subsector_sector(newsub) - sectors;
#endif
        sector_t *newsec = subsector_sector(newsub);
        if (oldsec != newsec) {
            // removed back link, so start from beginning
            shortptr_t *prev = &oldsec->thinglist;
            assert(*prev);
            while (*prev) {
                mobj_t *mobj = shortptr_to_mobj(*prev);
                if (mobj == thing) {
                    *prev = mobj->sp_snext;
                    break;
                }
                prev = &mobj->sp_snext;
            }
            thing->sp_snext = newsec->thinglist;
            newsec->thinglist = mobj_to_shortptr(thing);
        }
    }
    if ( ! (thing->flags & MF_NOBLOCKMAP) )
    {
        // inert things don't need to be in blockmap
        fixed_t oldblockx = (thing->xy.x - bmaporgx) >> MAPBLOCKSHIFT;
        fixed_t blockx = (x - bmaporgx) >> MAPBLOCKSHIFT;
        fixed_t oldblocky = (thing->xy.y - bmaporgy) >> MAPBLOCKSHIFT;
        fixed_t blocky = (y - bmaporgy) >> MAPBLOCKSHIFT;
        if (oldblockx != blockx || oldblocky != blocky) {
            if (oldblockx>=0 && oldblockx < bmapwidth
                && oldblocky>=0 && oldblocky <bmapheight) {
                shortptr_t *prev = &blocklinks[oldblocky * bmapwidth + oldblockx];
                assert(*prev);
                while (*prev) {
                    mobj_t *mobj = shortptr_to_mobj(*prev);
                    if (mobj == thing) {
                        *prev = mobj->sp_bnext;
                        break;
                    }
                    prev = &mobj->sp_bnext;
                }
            }
            if (blockx>=0
                && blockx < bmapwidth
                && blocky>=0
                && blocky < bmapheight)
            {
                thing->sp_bnext = blocklinks[blocky*bmapwidth+blockx];
                blocklinks[blocky*bmapwidth+blockx] = mobj_to_shortptr(thing);
            }
            else
            {
                // thing is off the map
                thing->sp_bnext = 0;
            }
        }
    }
    thing->xy.x = x;
    thing->xy.y = y;
#endif
}

//
// BLOCK MAP ITERATORS
// For each line/thing in the given mapblock,
// call the passed PIT_* function.
// If the function returns false,
// exit with false without checking anything else.
//


#if USE_WHD
// todo double in size for speed?
const uint8_t popcount8_table[128] = {
        0x10, 0x21, 0x21, 0x32, 0x21, 0x32, 0x32, 0x43, 0x21, 0x32, 0x32, 0x43, 0x32, 0x43, 0x43, 0x54,
        0x21, 0x32, 0x32, 0x43, 0x32, 0x43, 0x43, 0x54, 0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65,
        0x21, 0x32, 0x32, 0x43, 0x32, 0x43, 0x43, 0x54, 0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65,
        0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65, 0x43, 0x54, 0x54, 0x65, 0x54, 0x65, 0x65, 0x76,
        0x21, 0x32, 0x32, 0x43, 0x32, 0x43, 0x43, 0x54, 0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65,
        0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65, 0x43, 0x54, 0x54, 0x65, 0x54, 0x65, 0x65, 0x76,
        0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65, 0x43, 0x54, 0x54, 0x65, 0x54, 0x65, 0x65, 0x76,
        0x43, 0x54, 0x54, 0x65, 0x54, 0x65, 0x65, 0x76, 0x54, 0x65, 0x65, 0x76, 0x65, 0x76, 0x76, 0x87,
};
// todo half size for space?
// note this savus us about 1.5K in metadata over a 32 entry table
const uint8_t bitcount8_table[256] = {
        0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};
#endif
//
// P_BlockLinesIterator
// The validcount flags are used to avoid checking lines
// that are marked in multiple mapblocks,
// so increment validcount before the first call
// to P_BlockLinesIterator, then make one or more calls
// to it.
//
boolean
P_BlockLinesIterator
( int			x,
  int			y,
  boolean(*func)(line_t*) )
{
    int			offset;
    rowad_const short*		list;
    line_t*		ld;
	
    if (x<0
	|| y<0
	|| x>=bmapwidth
	|| y>=bmapheight)
    {
	return true;
    }

#if !USE_WHD
    offset = y*bmapwidth+x;
	
    offset = *(blockmap+offset);

    for ( list = blockmaplump+offset ; *list != -1 ; list++)
    {
	ld = &lines[*list];

	if (line_validcount_update_check(ld, validcount))
	    continue; 	// line has already been checked

	if ( !func(ld) )
	    return false;
    }
#else
    // handle 0
    if (!line_validcount_update_check(&lines[0], validcount) && !func(&lines[0])) return false;

    uint row_header_offset = y * (2 + (bmapwidth + 7) / 8);
    uint bmx = x / 8;

    if (blockmap_whd[row_header_offset + 2 + bmx] & (1u << (x & 7))) {
        uint data_offset = blockmap_whd[row_header_offset] | (blockmap_whd[row_header_offset + 1] << 8u);
        uint cell_metadata_index = popcount8(blockmap_whd[row_header_offset + 2 + bmx] & ((1u << (x & 7)) - 1));
        for(int xx = 0; xx < bmx; xx++) {
            cell_metadata_index += popcount8(blockmap_whd[row_header_offset + 2 + xx]);
        }
        uint cell_metadata = blockmap_whd[data_offset + cell_metadata_index * 2] | (blockmap_whd[data_offset + cell_metadata_index * 2 + 1] << 8u);
        if ((cell_metadata & 0xf000) == 0xf000) {
            const line_t *line = &lines[cell_metadata & 0xfffu];
            if (!line_validcount_update_check(line, validcount) && !func(line)) return false;
        } else {
            uint count = (cell_metadata >> 10u) + 1;
            uint element_offset = data_offset + (cell_metadata & 0x3ff);
            uint last = 0;
            while (count--) {
                uint b = blockmap_whd[element_offset++];
                if (b & 0x80) {
                    b = ((b & 0x7f) << 8) + blockmap_whd[element_offset++];
                }
                last += b + 1;
                const line_t *line = &lines[last];
                if (!line_validcount_update_check(line, validcount) && !func(line)) return false;
            }
        }
    }
#endif
    return true;	// everything was checked
}


//
// P_BlockThingsIterator
//
boolean
P_BlockThingsIterator
( int			x,
  int			y,
  boolean(*func)(mobj_t*) )
{
    // todo graham we can make do with a smaller hashtable I'm sure
    mobj_t*		mobj;
	
    if ( x<0
	 || y<0
	 || x>=bmapwidth
	 || y>=bmapheight)
    {
	return true;
    }


    for (mobj = shortptr_to_mobj(blocklinks[y*bmapwidth+x]) ;
	 mobj ;
	 mobj = mobj_bnext(mobj))
    {
	if (!func( mobj ) )
	    return false;
    }
    return true;
}



//
// INTERCEPT ROUTINES
//
intercept_t	intercepts[MAXINTERCEPTS];
intercept_t*	intercept_p;

divline_t 	trace;
boolean 	earlyout;
int		ptflags;

static void InterceptsOverrun(int num_intercepts, intercept_t *intercept);

//
// PIT_AddLineIntercepts.
// Looks for lines in the given block
// that intercept the given trace
// to add to the intercepts list.
//
// A line is crossed if its endpoints
// are on opposite sides of the trace.
// Returns true if earlyout and a solid line hit.
//
boolean
PIT_AddLineIntercepts (line_t* ld)
{
    int			s1;
    int			s2;
    fixed_t		frac;
    divline_t		dl;
	
    // avoid precision problems with two routines
    if ( trace.dx > FRACUNIT*16
	 || trace.dy > FRACUNIT*16
	 || trace.dx < -FRACUNIT*16
	 || trace.dy < -FRACUNIT*16)
    {
	s1 = P_PointOnDivlineSide (vertex_x(line_v1(ld)), vertex_y(line_v1(ld)), &trace);
	s2 = P_PointOnDivlineSide (vertex_x(line_v2(ld)), vertex_y(line_v2(ld)), &trace);
    }
    else
    {
	s1 = P_PointOnLineSide (trace.x, trace.y, ld);
	s2 = P_PointOnLineSide (trace.x+trace.dx, trace.y+trace.dy, ld);
    }
    
    if (s1 == s2)
	return true;	// line isn't crossed
    
    // hit the line
    P_MakeDivline (ld, &dl);
    frac = P_InterceptVector (&trace, &dl);

    if (frac < 0)
	return true;	// behind source
	
    // try to early out the check
    if (earlyout
	&& frac < FRACUNIT
	&& !line_backsector(ld))
    {
	return false;	// stop checking
    }
    
#if NO_INTERCEPTS_OVERRUN
    if (intercept_p - intercepts == MAXINTERCEPTS) return false;
#endif

    intercept_p->frac = frac;
    intercept_p->isaline = true;
    intercept_p->d.line = ld;
#if !NO_INTERCEPTS_OVERRUN
    InterceptsOverrun(intercept_p - intercepts, intercept_p);
#endif
    intercept_p++;

    return true;	// continue
}



//
// PIT_AddThingIntercepts
//
boolean PIT_AddThingIntercepts (mobj_t* thing)
{
    fixed_t		x1;
    fixed_t		y1;
    fixed_t		x2;
    fixed_t		y2;
    
    int			s1;
    int			s2;
    
    boolean		tracepositive;

    divline_t		dl;
    
    fixed_t		frac;
	
    tracepositive = (trace.dx ^ trace.dy)>0;
		
    // check a corner to corner crossection for hit
    if (tracepositive)
    {
	x1 = thing->xy.x - mobj_radius(thing);
	y1 = thing->xy.y + mobj_radius(thing);
		
	x2 = thing->xy.x + mobj_radius(thing);
	y2 = thing->xy.y - mobj_radius(thing);
    }
    else
    {
	x1 = thing->xy.x - mobj_radius(thing);
	y1 = thing->xy.y - mobj_radius(thing);
		
	x2 = thing->xy.x + mobj_radius(thing);
	y2 = thing->xy.y + mobj_radius(thing);
    }
    
    s1 = P_PointOnDivlineSide (x1, y1, &trace);
    s2 = P_PointOnDivlineSide (x2, y2, &trace);

    if (s1 == s2)
	return true;		// line isn't crossed
	
    dl.x = x1;
    dl.y = y1;
    dl.dx = x2-x1;
    dl.dy = y2-y1;
    
    frac = P_InterceptVector (&trace, &dl);

    if (frac < 0)
	return true;		// behind source

    intercept_p->frac = frac;
    intercept_p->isaline = false;
    intercept_p->d.thing = thing;
    InterceptsOverrun(intercept_p - intercepts, intercept_p);
    intercept_p++;

    return true;		// keep going
}


//
// P_TraverseIntercepts
// Returns true if the traverser function returns true
// for all lines.
// 
boolean
P_TraverseIntercepts
( traverser_t	func,
  fixed_t	maxfrac )
{
    int			count;
    fixed_t		dist;
    intercept_t*	scan;
    intercept_t*	in;
	
    count = intercept_p - intercepts;
    
    in = 0;			// shut up compiler warning
	
    while (count--)
    {
	dist = INT_MAX;
	for (scan = intercepts ; scan<intercept_p ; scan++)
	{
	    if (scan->frac < dist)
	    {
		dist = scan->frac;
		in = scan;
	    }
	}
	
	if (dist > maxfrac)
	    return true;	// checked everything in range		

#if 0  // UNUSED
    {
	// don't check these yet, there may be others inserted
	in = scan = intercepts;
	for ( scan = intercepts ; scan<intercept_p ; scan++)
	    if (scan->frac > maxfrac)
		*in++ = *scan;
	intercept_p = in;
	return false;
    }
#endif

        if ( !func (in) )
	    return false;	// don't bother going farther

	in->frac = INT_MAX;
    }
	
    return true;		// everything was traversed
}

extern fixed_t bulletslope;

// Intercepts Overrun emulation, from PrBoom-plus.
// Thanks to Andrey Budko (entryway) for researching this and his 
// implementation of Intercepts Overrun emulation in PrBoom-plus
// which this is based on.

typedef struct
{
    int len;
    void *addr;
    boolean int16_array;
} intercepts_overrun_t;

// Intercepts memory table.  This is where various variables are located
// in memory in Vanilla Doom.  When the intercepts table overflows, we
// need to write to them.
//
// Almost all of the values to overwrite are 32-bit integers, except for
// playerstarts, which is effectively an array of 16-bit integers and
// must be treated differently.

static intercepts_overrun_t intercepts_overrun[] =
{
    {4,   NULL,                          false},
    {4,   NULL, /* &earlyout, */         false},
    {4,   NULL, /* &intercept_p, */      false},
    {4,   &lowfloor,                     false},
    {4,   &openbottom,                   false},
    {4,   &opentop,                      false},
    {4,   &openrange,                    false},
    {4,   NULL,                          false},
    {120, NULL, /* &activeplats, */      false},
    {8,   NULL,                          false},
    {4,   &bulletslope,                  false},
    {4,   NULL, /* &swingx, */           false},
    {4,   NULL, /* &swingy, */           false},
    {4,   NULL,                          false},
    {40,  &playerstarts,                 true},
    {4,   NULL, /* &blocklinks, */       false},
    {4,   &bmapwidth,                    false},
    {4,   NULL, /* &blockmap, */         false},
    {4,   &bmaporgx,                     false},
    {4,   &bmaporgy,                     false},
    {4,   NULL, /* &blockmaplump, */     false},
    {4,   &bmapheight,                   false},
    {0,   NULL,                          false},
};

// Overwrite a specific memory location with a value.

static void InterceptsMemoryOverrun(int location, int value)
{
    int i, offset;
    int index;
    void *addr;

    i = 0;
    offset = 0;

    // Search down the array until we find the right entry

    while (intercepts_overrun[i].len != 0)
    {
        if (offset + intercepts_overrun[i].len > location)
        {
            addr = intercepts_overrun[i].addr;

            // Write the value to the memory location.
            // 16-bit and 32-bit values are written differently.

            if (addr != NULL)
            {
                if (intercepts_overrun[i].int16_array)
                {
                    index = (location - offset) / 2;
                    ((short *) addr)[index] = value & 0xffff;
                    ((short *) addr)[index + 1] = (value >> 16) & 0xffff;
                }
                else
                {
                    index = (location - offset) / 4;
                    ((int *) addr)[index] = value;
                }
            }

            break;
        }

        offset += intercepts_overrun[i].len;
        ++i;
    }
}

// Emulate overruns of the intercepts[] array.

static void InterceptsOverrun(int num_intercepts, intercept_t *intercept)
{
    int location;

    if (num_intercepts <= MAXINTERCEPTS_ORIGINAL)
    {
        // No overrun

        return;
    }

    location = (num_intercepts - MAXINTERCEPTS_ORIGINAL - 1) * 12;

    // Overwrite memory that is overwritten in Vanilla Doom, using
    // the values from the intercept structure.
    //
    // Note: the ->d.{thing,line} member should really have its
    // address translated into the correct address value for 
    // Vanilla Doom.

    InterceptsMemoryOverrun(location, intercept->frac);
    InterceptsMemoryOverrun(location + 4, intercept->isaline);
    InterceptsMemoryOverrun(location + 8, (intptr_t) intercept->d.thing);
}


//
// P_PathTraverse
// Traces a line from x1,y1 to x2,y2,
// calling the traverser function for each.
// Returns true if the traverser function returns true
// for all lines.
//
boolean
P_PathTraverse
( fixed_t		x1,
  fixed_t		y1,
  fixed_t		x2,
  fixed_t		y2,
  int			flags,
  boolean (*trav) (intercept_t *))
{
    fixed_t	xt1;
    fixed_t	yt1;
    fixed_t	xt2;
    fixed_t	yt2;
    
    fixed_t	xstep;
    fixed_t	ystep;
    
    fixed_t	partial;
    
    fixed_t	xintercept;
    fixed_t	yintercept;
    
    int		mapx;
    int		mapy;
    
    int		mapxstep;
    int		mapystep;

    int		count;
		
    earlyout = (flags & PT_EARLYOUT) != 0;

    line_check_reset();
    validcount++;
    intercept_p = intercepts;
	
    if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
	x1 += FRACUNIT;	// don't side exactly on a line
    
    if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
	y1 += FRACUNIT;	// don't side exactly on a line

    trace.x = x1;
    trace.y = y1;
    trace.dx = x2 - x1;
    trace.dy = y2 - y1;

    x1 -= bmaporgx;
    y1 -= bmaporgy;
    xt1 = x1>>MAPBLOCKSHIFT;
    yt1 = y1>>MAPBLOCKSHIFT;

    x2 -= bmaporgx;
    y2 -= bmaporgy;
    xt2 = x2>>MAPBLOCKSHIFT;
    yt2 = y2>>MAPBLOCKSHIFT;

    if (xt2 > xt1)
    {
	mapxstep = 1;
	partial = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
	ystep = FixedDiv (y2-y1,abs(x2-x1));
    }
    else if (xt2 < xt1)
    {
	mapxstep = -1;
	partial = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
	ystep = FixedDiv (y2-y1,abs(x2-x1));
    }
    else
    {
	mapxstep = 0;
	partial = FRACUNIT;
	ystep = 256*FRACUNIT;
    }	

    yintercept = (y1>>MAPBTOFRAC) + FixedMul (partial, ystep);

	
    if (yt2 > yt1)
    {
	mapystep = 1;
	partial = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
	xstep = FixedDiv (x2-x1,abs(y2-y1));
    }
    else if (yt2 < yt1)
    {
	mapystep = -1;
	partial = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
	xstep = FixedDiv (x2-x1,abs(y2-y1));
    }
    else
    {
	mapystep = 0;
	partial = FRACUNIT;
	xstep = 256*FRACUNIT;
    }	
    xintercept = (x1>>MAPBTOFRAC) + FixedMul (partial, xstep);
    
    // Step through map blocks.
    // Count is present to prevent a round off error
    // from skipping the break.
    mapx = xt1;
    mapy = yt1;
	
    for (count = 0 ; count < 64 ; count++)
    {
	if (flags & PT_ADDLINES)
	{
	    if (!P_BlockLinesIterator (mapx, mapy,PIT_AddLineIntercepts))
		return false;	// early out
	}
	
	if (flags & PT_ADDTHINGS)
	{
	    if (!P_BlockThingsIterator (mapx, mapy,PIT_AddThingIntercepts))
		return false;	// early out
	}
		
	if (mapx == xt2
	    && mapy == yt2)
	{
	    break;
	}
	
	if ( (yintercept >> FRACBITS) == mapy)
	{
	    yintercept += ystep;
	    mapx += mapxstep;
	}
	else if ( (xintercept >> FRACBITS) == mapx)
	{
	    xintercept += xstep;
	    mapy += mapystep;
	}
		
    }
    // go through the sorted list
    return P_TraverseIntercepts ( trav, FRACUNIT );
}



