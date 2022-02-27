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
//	LineOfSight/Visibility checks, uses REJECT Lookup Table.
//



#include "doomdef.h"

#include "i_system.h"
#include "p_local.h"

// State.
#include "r_state.h"

//
// P_CheckSight
//
fixed_t		sightzstart;		// eye z of looker
fixed_t		topslope;
fixed_t		bottomslope;		// slopes to top and bottom of target

divline_t	strace;			// from t1 to t2
fixed_t		t2x;
fixed_t		t2y;

int		sightcounts[2];


//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
int
P_DivlineSide
( fixed_t	x,
  fixed_t	y,
  const divline_t*	node )
{
    fixed_t	dx;
    fixed_t	dy;
    fixed_t	left;
    fixed_t	right;

    if (!node->dx)
    {
	if (x==node->x)
	    return 2;
	
	if (x <= node->x)
	    return node->dy > 0;

	return node->dy < 0;
    }
    
    if (!node->dy)
    {
	if (x==node->y)
	    return 2;

	if (y <= node->y)
	    return node->dx < 0;

	return node->dx > 0;
    }
	
    dx = (x - node->x);
    dy = (y - node->y);

    left =  (node->dy>>FRACBITS) * (dx>>FRACBITS);
    right = (dy>>FRACBITS) * (node->dx>>FRACBITS);
	
    if (right < left)
	return 0;	// front side
    
    if (left == right)
	return 2;
    return 1;		// back side
}


//
// P_InterceptVector2
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings and addlines traversers.
//
fixed_t
P_InterceptVector2
( divline_t*	v2,
  divline_t*	v1 )
{
    fixed_t	frac;
    fixed_t	num;
    fixed_t	den;
	
    den = FixedMul (v1->dy>>8,v2->dx) - FixedMul(v1->dx>>8,v2->dy);

    if (den == 0)
	return 0;
    //	I_Error ("P_InterceptVector: parallel");
    
    num = FixedMul ( (v1->x - v2->x)>>8 ,v1->dy) + 
	FixedMul ( (v2->y - v1->y)>>8 , v1->dx);
    frac = FixedDiv (num , den);

    return frac;
}

//
// P_CrossSubsector
// Returns true
//  if strace crosses the given subsector successfully.
//
boolean P_CrossSubsector (int num)
{
    seg_t*		seg;
    seg_t*		seg_end;
    line_t*		line;
    int			s1;
    int			s2;
    subsector_t*	sub;
    sector_t*		front;
    sector_t*		back;
    fixed_t		opentop;
    fixed_t		openbottom;
    divline_t		divl;
    const vertex_t*		v1;
    const vertex_t*		v2;
    fixed_t		frac;
    fixed_t		slope;
	
#ifdef RANGECHECK
    if (num>=numsubsectors)
	I_Error ("P_CrossSubsector: ss %i with numss = %i",
		 num,
		 numsubsectors);
#endif

    sub = &subsectors[num];
    
    // check lines
    seg_end = subsector_linelimit(sub);
    seg = subsector_firstline(sub);

    for ( ; seg != seg_end; seg += seg_next_step(seg))
    {
	line = seg_linedef(seg);

	// allready checked other side?
	if (line_validcount_update_check(line, validcount))
	    continue;

	v1 = line_v1(line);
	v2 = line_v2(line);
	s1 = P_DivlineSide (vertex_x(v1), vertex_y(v1), &strace);
	s2 = P_DivlineSide (vertex_x(v2), vertex_y(v2), &strace);

	// line isn't crossed?
	if (s1 == s2)
	    continue;
	
	divl.x = vertex_x(v1);
	divl.y = vertex_y(v1);
	divl.dx = vertex_x(v2) - vertex_x(v1);
	divl.dy = vertex_y(v2) - vertex_y(v1);
	s1 = P_DivlineSide (strace.x, strace.y, &divl);
	s2 = P_DivlineSide (t2x, t2y, &divl);

	// line isn't crossed?
	if (s1 == s2)
	    continue;	

        // Backsector may be NULL if this is an "impassible
        // glass" hack line.

        if (line_backsector(line) == NULL)
        {
            return false;
        }

	// stop because it is not two sided anyway
	// might do this after updating validcount?
	if ( !(line_flags(line) & ML_TWOSIDED) )
	    return false;
	
	// crosses a two sided line
	front = seg_frontsector(seg);
	back = seg_backsector(seg);

	// no wall to block sight with?
	if (front->rawfloorheight == back->rawfloorheight
	&& front->rawceilingheight == back->rawceilingheight)
	    continue;	

	// possible occluder
	// because of ceiling height differences
	if (front->rawceilingheight < back->rawceilingheight)
	    opentop = sector_ceilingheight(front);
	else
	    opentop = sector_ceilingheight(back);

	// because of ceiling height differences
	if (front->rawfloorheight > back->rawfloorheight)
	    openbottom = sector_floorheight(front);
	else
	    openbottom = sector_floorheight(back);
		
	// quick test for totally closed doors
	if (openbottom >= opentop)	
	    return false;		// stop
	
	frac = P_InterceptVector2 (&strace, &divl);
		
	if (front->rawfloorheight != back->rawfloorheight)
	{
	    slope = FixedDiv (openbottom - sightzstart , frac);
	    if (slope > bottomslope)
		bottomslope = slope;
	}
		
	if (front->rawceilingheight != back->rawceilingheight)
	{
	    slope = FixedDiv (opentop - sightzstart , frac);
	    if (slope < topslope)
		topslope = slope;
	}
		
	if (topslope <= bottomslope)
	    return false;		// stop				
    }
    // passed the subsector ok
    return true;		
}



//
// P_CrossBSPNode
// Returns true
//  if strace crosses the given node successfully.
//
boolean P_CrossBSPNode (int bspnum)
{
    node_t*	bsp;
    int		side;

    if (bspnum & NF_SUBSECTOR)
    {
	if (bspnum == -1)
	    return P_CrossSubsector (0);
	else
	    return P_CrossSubsector (bspnum&(~NF_SUBSECTOR));
    }
		
    bsp = &nodes[bspnum];
    
    // decide which side the start point is on
#if USE_RAW_MAPNODE
    // these functions seem identical anyway
    side = R_PointOnSide(strace.x, strace.y, bsp);
#else
    side = P_DivlineSide (strace.x, strace.y, (divline_t *)bsp);
#endif
    if (side == 2)
	side = 0;	// an "on" should cross both sides

    // cross the starting side
    if (!P_CrossBSPNode (bsp_child(bspnum, side)) )
	return false;
	
    // the partition plane is crossed here
#if USE_RAW_MAPNODE
    // these functions seem identical anyway
    if (side == R_PointOnSide(t2x, t2y, bsp))
#else
    if (side == P_DivlineSide (t2x, t2y,(divline_t *)bsp))
#endif
    {
	// the line doesn't touch the other side
	return true;
    }
    
    // cross the ending side		
    return P_CrossBSPNode (bsp_child(bspnum, side^1));
}


//
// P_CheckSight
// Returns true
//  if a straight line between t1 and t2 is unobstructed.
// Uses REJECT.
//
boolean
P_CheckSight
( mobj_t*	t1,
  mobj_t*	t2 )
{
    int		s1;
    int		s2;
    int		pnum;
    int		bytenum;
    int		bitnum;
    
    // First check for trivial rejection.

    // Determine subsector entries in REJECT table.
    s1 = (mobj_sector(t1) - sectors);
    s2 = (mobj_sector(t2) - sectors);
    pnum = s1*numsectors + s2;
    bytenum = pnum>>3;
    bitnum = 1 << (pnum&7);

    // Check in REJECT table.
    if (rejectmatrix[bytenum]&bitnum)
    {
	sightcounts[0]++;

	// can't possibly be connected
	return false;	
    }

    // An unobstructed LOS is possible.
    // Now look from eyes of t1 to any part of t2.
    sightcounts[1]++;

    line_check_reset();
    validcount++;
	
    sightzstart = t1->z + mobj_height(t1) - (mobj_height(t1)>>2);
    topslope = (t2->z+mobj_height(t2)) - sightzstart;
    bottomslope = (t2->z) - sightzstart;
	
    strace.x = t1->xy.x;
    strace.y = t1->xy.y;
    t2x = t2->xy.x;
    t2y = t2->xy.y;
    strace.dx = t2->xy.x - t1->xy.x;
    strace.dy = t2->xy.y - t1->xy.y;

    // the head node is the last node output
    return P_CrossBSPNode (numnodes-1);	
}


