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
//	Rendering of moving objects, sprites.
//


#ifndef __R_THINGS__
#define __R_THINGS__


#if !NO_VISSPRITES
#define MAXVISSPRITES  	128

extern vissprite_t	vissprites[MAXVISSPRITES];
extern vissprite_t*	vissprite_p;
extern vissprite_t	vsprsortedhead;
#endif

// Constant arrays used for psprite clipping
//  and initializing clipping.
// todo graham remove these

// vars for R_DrawMaskedColumn
extern floor_ceiling_clip_t		minfloorceilingcliparray[SCREENWIDTH];
extern floor_ceiling_clip_t		maxfloorceilingcliparray[SCREENWIDTH];
extern floor_ceiling_clip_t*		mfloorclip;
extern floor_ceiling_clip_t*		mceilingclip;

extern fixed_t		spryscale;
extern fixed_t		sprtopscreen;

extern fixed_t		pspritescale;
extern fixed_t		pspriteiscale;


void R_DrawMaskedColumn (maskedcolumn_t column);



void R_SortVisSprites (void);

void R_AddSprites (sector_t* sec);
void R_AddPSprites (void);
void R_DrawSprites (void);
void R_DrawSpriteEarly (vissprite_t* spr);
void R_InitSprites();
void R_ClearSprites (void);
void R_DrawMasked (void);
#if DOOM_TINY
void R_DrawVisSprite
        (vissprite_t *vis,
         int x1,
         int x2);
#endif

void
R_ClipVisSprite
( vissprite_t*		vis,
  int			xl,
  int			xh );


#endif
