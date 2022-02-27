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
//	Refresh, visplane stuff (floor, ceilings).
//


#ifndef __R_PLANE__
#define __R_PLANE__


#include "r_data.h"


// Visplane related.

#if !NO_DRAWSEGS
extern  short*		lastopening;
#endif


typedef void (*planefunction_t) (int top, int bottom);

extern planefunction_t	floorfunc;
extern planefunction_t	ceilingfunc_t;

extern floor_ceiling_clip_t floorclip[SCREENWIDTH];
extern floor_ceiling_clip_t ceilingclip[SCREENWIDTH];

#if !DOOM_TINY
extern fixed_t		yslope[SCREENHEIGHT];
#else
extern fixed_t		yslope[MAIN_VIEWHEIGHT];
#endif
#if !FIXED_SCREENWIDTH
extern fixed_t		_distscale[SCREENWIDTH];
#define distscale(x) _distscale[x]
#else
extern const uint16_t		_distscale[SCREENWIDTH];
#define distscale(x) (_distscale[x] | (fixed_t)0x10000)
#endif
extern fixed_t      basexscale;
extern fixed_t      baseyscale;

void R_InitPlanes (void);
void R_ClearPlanes (void);

void
R_MapPlane
( int		y,
  int		x1,
  int		x2 );

void
R_MakeSpans
( int		x,
  int		t1,
  int		b1,
  int		t2,
  int		b2 );

#if !NO_VISPLANES
void R_DrawPlanes (void);

visplane_t*
R_FindPlane
( fixed_t	height,
  int		picnum,
  int		lightlevel );

visplane_t*
R_CheckPlane
( visplane_t*	pl,
  int		start,
  int		stop );
#endif


#endif
