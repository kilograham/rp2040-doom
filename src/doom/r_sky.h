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
//	Sky rendering.
//


#ifndef __R_SKY__
#define __R_SKY__



// SKY, store the number for name.
#define			SKYFLATNAME  "F_SKY1"

// The sky map is 256*128*4 maps.
#define ANGLETOSKYSHIFT		22

extern  texnum_t 		skytexture;
#if USE_WHD
extern framedrawable_t *skytexture_fd;
#endif
#if DOOM_TINY
extern lumpindex_t   skytexture_patch;
#endif
extern int		skytexturemid;

// Called whenever the view size changes.
void R_InitSkyMap (void);

#endif
