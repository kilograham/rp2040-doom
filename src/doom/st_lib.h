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
// 	The status bar widget code.
//

#ifndef __STLIB__
#define __STLIB__


// We are referring to patches.
#include "r_defs.h"

//
// Typedefs of widgets
//

// Number widget

typedef struct
{
    // upper right-hand corner
    //  of the number (right-justified)
    isb_int16_t 		x;
    isb_int16_t 		y;

    // max # of digits in number
    isb_int8_t width;

#if !DOOM_TINY
    // last number value
    int		oldnum;
#endif
    
    // pointer to current value
    int*	num;

    // pointer to boolean stating
    //  whether to update number
    boolean*	on;

    vpatch_sequence_t p;
#if DOOM_TINY
    int16_t cached;
#endif
#if !DOOM_TINY
    // user data
    int data;
#endif
    
} st_number_t;



// Percent widget ("child" of number widget,
//  or, more precisely, contains a number widget.)
typedef struct
{
    // number information
    st_number_t		n;

    // percent sign graphic
    vpatch_handle_small_t		p;
    
} st_percent_t;



// Multiple Icon widget
typedef struct
{
     // center-justified location of icons
    isb_int16_t 			x;
    isb_int16_t 			y;

#if !DOOM_TINY
    // last icon number
    int			oldinum;
#endif

    // pointer to current icon
    isb_int8_t*		inum;

    // pointer to boolean stating
    //  whether to update icon
    boolean*		on;

    // list of icons
    vpatch_handle_small_t*		p;

#if DOOM_TINY
    int8_t cached;
#endif
#if !DOOM_TINY
    // user data
    int			data;
#endif
    
} st_multicon_t;

// Binary Icon widget

typedef struct
{
    // center-justified location of icon
    isb_uint8_t 			x;
    isb_uint8_t 			y;
    vpatch_handle_small_t		p;	// icon

#if !DOOM_TINY
    // last icon value
    boolean		oldval;
#endif

    // pointer to current icon status
    boolean*		val;

    // pointer to boolean
    //  stating whether to update icon
    boolean*		on;

#if DOOM_TINY
    int8_t cached;
#endif

#if !DOOM_TINY
    int			data;   // user data
#endif

} st_binicon_t;

//
// Widget creation, access, and update routines
//

// Initializes widget library.
// More precisely, initialize STMINUS,
//  everything else is done somewhere else.
//
void STlib_init(void);



// Number widget routines
void
STlib_initNum
( st_number_t*		n,
  int			x,
  int			y,
  vpatch_sequence_t 		pl,
  int*			num,
  boolean*		on,
  int			width );

void
STlib_updateNum
( st_number_t*		n,
  boolean		refresh );


// Percent widget routines
void
STlib_initPercent
( st_percent_t*		p,
  int			x,
  int			y,
  vpatch_sequence_t 	pl,
  int*			num,
  boolean*		on,
  vpatch_handle_small_t		percent );


void
STlib_updatePercent
( st_percent_t*		per,
  int			refresh );


// Multiple Icon widget routines
void
STlib_initMultIcon
( st_multicon_t*	mi,
  int			x,
  int			y,
  vpatch_handle_small_t*		il,
  isb_int8_t*			inum,
  boolean*		on );


void
STlib_updateMultIcon
( st_multicon_t*	mi,
  boolean		refresh );

// Binary Icon widget routines

void
STlib_initBinIcon
( st_binicon_t*		b,
  int			x,
  int			y,
  vpatch_handle_small_t		i,
  boolean*		val,
  boolean*		on );

void
STlib_updateBinIcon
( st_binicon_t*		bi,
  boolean		refresh );

#endif
