//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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
//	Lookup tables.
//	Do not try to look them up :-).
//	In the order of appearance: 
//
//	int finetangent[4096]	- Tangens LUT.
//	 Should work with BAM fairly well (12 of 16bit,
//      effectively, by shifting).
//
//	int finesine[10240]		- Sine lookup.
//	 Guess what, serves as cosine, too.
//	 Remarkable thing is, how to use BAMs with this? 
//
//	int tantoangle[2049]	- ArcTan LUT,
//	  maps tan(angle) to angle fast. Gotta search.	
//    


#ifndef __TABLES__
#define __TABLES__

#include "doomtype.h"

#include "m_fixed.h"
	
#define FINEANGLES		8192
#define FINEMASK		(FINEANGLES-1)
#define FINEBITS        13


// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT	19		

#if !DOOM_TINY
// Effective size is 10240.
extern const fixed_t _finesine[5*FINEANGLES/4];

// Re-use data, is just PI/2 pahse shift.
extern const fixed_t *_finecosine;
// just for checking
#define finesine(x) (_finesine[x] / (1 << (16 - FRACBITS)))
#define finecosine(x) (_finecosine[x] / (1 << (16 - FRACBITS)))
#else
#include <assert.h>
static_assert(FINEANGLES == 1u << FINEBITS, "");
extern const uint16_t _finesine[5* FINEANGLES/4];
static inline fixed_t finesine(int x) {
    fixed_t rc = _finesine[x];
    // fix the top 16 bits based on the quadrant (they are either 0000 or ffff - this way without a branch)
    rc -= (x & (FINEANGLES >> 1)) << (16 - (FINEBITS - 1));
    return rc;
}

#define finecosine(x) finesine((x) + FINEANGLES/4)
#endif

// Effective size is 4096.
extern const fixed_t _finetangent[FINEANGLES/2];

#define finetangent(x) (_finetangent[x] / (1 << (16 - FRACBITS)))

// Gamma correction tables.
#if !DOOM_TINY
extern const byte gammatable[5][256];
#else
extern const byte gammatable[4][256];
#endif

// Binary Angle Measument, BAM.

#define ANG45           0x20000000
#define ANG90           0x40000000
#define ANG180          0x80000000
#define ANG270          0xc0000000
#define ANG_MAX         0xffffffff

#define ANG1            (ANG45 / 45)
#define ANG60           (ANG180 / 3)

// Heretic code uses this definition as though it represents one 
// degree, but it is not!  This is actually ~1.40 degrees.

#define ANG1_X          0x01000000

#define SLOPERANGE		2048
#define SLOPEBITS		11
#define DBITS			(FRACBITS-SLOPEBITS)

typedef unsigned int angle_t;


// Effective size is 2049;
// The +1 size is to handle the case when x==y
//  without additional checking.
extern const angle_t tantoangle[SLOPERANGE+1];


// Utility function,
//  called by R_PointToAngle.
//int SlopeDiv(unsigned int num, unsigned int den);


#endif

