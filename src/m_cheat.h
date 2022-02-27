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
//	Cheat code checking.
//


#ifndef __M_CHEAT__
#define __M_CHEAT__

//
// CHEAT SEQUENCE PACKAGE
//

// declaring a cheat

#if !DOOM_TINY
#define CHEAT(value, parameters) \
    { value, sizeof(value) - 1, parameters, 0, 0, "" }
#else
#define CHEAT(value, parameters) \
{ value, sizeof(value) - 1, parameters, 0, 0 }
#endif

#define MAX_CHEAT_LEN 25
#define MAX_CHEAT_PARAMS 5

typedef struct
{
    // settings for this cheat

    //char sequence[MAX_CHEAT_LEN];
    should_be_const char *sequence;
    isb_uint8_t sequence_len;
    isb_uint8_t parameter_chars;

    // state used during the game

    isb_uint8_t chars_read;
    isb_int8_t param_chars_read;
#ifndef DOOM_TINY // just use a shared one - don't think multiple cheats could be into parameters at the same time
    char parameter_buf[MAX_CHEAT_PARAMS];
#endif
} cheatseq_t;

int
cht_CheckCheat
( cheatseq_t*		cht,
  char			key );


void
cht_GetParam
( cheatseq_t*		cht,
  char*			buffer );


#endif
