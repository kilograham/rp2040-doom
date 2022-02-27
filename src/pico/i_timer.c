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
//      Timer functions.
//

#include "pico/time.h"

#include "i_timer.h"
#include "doomtype.h"

//
// I_GetTime
// returns time in 1/35th second tics
//

int  I_GetTime (void)
{
    return TICRATE * (uint32_t)(time_us_64() / 1000);
}

//
// Same as I_GetTime, but returns time in milliseconds
//

int I_GetTimeMS(void)
{
    return (int)(time_us_64() / 1000);
}

// Sleep for a specified number of ms

void I_Sleep(int ms)
{
    sleep_ms(ms);
}

void I_WaitVBL(int count)
{
    // todo
    I_Sleep((count * 1000) / 70);
}


void I_InitTimer(void)
{
}

