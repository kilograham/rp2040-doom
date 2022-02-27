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
//	Plats (i.e. elevator platforms) code, raising/lowering.
//

#include <stdio.h>

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"


shortptr_t /*plat_t* */		activeplats[MAXPLATS];



//
// Move a plat up and down
//
void T_PlatRaise(plat_t* plat)
{
    result_e	res;
	
    switch(plat->status)
    {
      case up:
	res = T_MovePlane(plat->sector,
			  plat->speed,
			  plat->high,
			  plat->crush,0,1);
					
	if (plat->type == raiseAndChange
	    || plat->type == raiseToNearestAndChange)
	{
	    if (!(leveltime&7))
		S_StartSound(&plat->sector->soundorg, sfx_stnmov);
	}
	
				
	if (res == crushed && (!plat->crush))
	{
	    plat->count = plat->wait;
	    plat->status = down;
	    S_StartSound(&plat->sector->soundorg, sfx_pstart);
	}
	else
	{
	    if (res == pastdest)
	    {
		plat->count = plat->wait;
		plat->status = waiting;
		S_StartSound(&plat->sector->soundorg, sfx_pstop);

		switch(plat->type)
		{
		  case blazeDWUS:
		  case downWaitUpStay:
		    P_RemoveActivePlat(plat);
		    break;
		    
		  case raiseAndChange:
		  case raiseToNearestAndChange:
		    P_RemoveActivePlat(plat);
		    break;
		    
		  default:
		    break;
		}
	    }
	}
	break;
	
      case	down:
	res = T_MovePlane(plat->sector,plat->speed,plat->low,false,0,-1);

	if (res == pastdest)
	{
	    plat->count = plat->wait;
	    plat->status = waiting;
	    S_StartSound(&plat->sector->soundorg,sfx_pstop);
	}
	break;
	
      case	waiting:
	if (!--plat->count)
	{
	    if (sector_floorheight(plat->sector) == plat->low)
		plat->status = up;
	    else
		plat->status = down;
	    S_StartSound(&plat->sector->soundorg,sfx_pstart);
	}
      case	in_stasis:
	break;
    }
}


//
// Do Platforms
//  "amount" is only used for SOME platforms.
//
int
EV_DoPlat
( line_t*	line,
  plattype_e	type,
  int		amount )
{
    plat_t*	plat;
    int		secnum;
    int		rtn;
    sector_t*	sec;
	
    secnum = -1;
    rtn = 0;

    
    //	Activate all <type> plats that are in_stasis
    switch(type)
    {
      case perpetualRaise:
	P_ActivateInStasis(line_tag(line));
	break;
	
      default:
	break;
    }
	
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	sec = &sectors[secnum];

	if (sec->specialdata)
	    continue;
	
	// Find lowest & highest floors around sector
	rtn = 1;
	plat = Z_Malloc( sizeof(*plat), PU_LEVSPEC, 0);
	P_AddThinker(&plat->thinker);
		
	plat->type = type;
	plat->sector = sec;
	plat->sector->specialdata = ptr_to_shortptr(plat);
	plat->thinker.function = ThinkF_T_PlatRaise;
	plat->crush = false;
	plat->tag = line_tag(line);
	
	switch(type)
	{
	  case raiseToNearestAndChange:
	    plat->speed = PLATSPEED/2;
	    sec->floorpic = side_sector(sidenum_to_side(line_sidenum(line, 0)))->floorpic;
	    plat->high = P_FindNextHighestFloor(sec,sector_floorheight(sec));
	    plat->wait = 0;
	    plat->status = up;
	    // NO MORE DAMAGE, IF APPLICABLE
	    sec->special = 0;		

	    S_StartSound(&sec->soundorg,sfx_stnmov);
	    break;
	    
	  case raiseAndChange:
	    plat->speed = PLATSPEED/2;
	    sec->floorpic = side_sector(sidenum_to_side(line_sidenum(line, 0)))->floorpic;
	    plat->high = sector_floorheight(sec) + amount*FRACUNIT;
	    plat->wait = 0;
	    plat->status = up;

	    S_StartSound(&sec->soundorg,sfx_stnmov);
	    break;
	    
	  case downWaitUpStay:
	    plat->speed = PLATSPEED * 4;
	    plat->low = P_FindLowestFloorSurrounding(sec);

	    if (plat->low > sector_floorheight(sec))
		plat->low = sector_floorheight(sec);

	    plat->high = sector_floorheight(sec);
	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = down;
	    S_StartSound(&sec->soundorg,sfx_pstart);
	    break;
	    
	  case blazeDWUS:
	    plat->speed = PLATSPEED * 8;
	    plat->low = P_FindLowestFloorSurrounding(sec);

	    if (plat->low > sector_floorheight(sec))
		plat->low = sector_floorheight(sec);

	    plat->high = sector_floorheight(sec);
	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = down;
	    S_StartSound(&sec->soundorg,sfx_pstart);
	    break;
	    
	  case perpetualRaise:
	    plat->speed = PLATSPEED;
	    plat->low = P_FindLowestFloorSurrounding(sec);

	    if (plat->low > sector_floorheight(sec))
		plat->low = sector_floorheight(sec);

	    plat->high = P_FindHighestFloorSurrounding(sec);

	    if (plat->high < sector_floorheight(sec))
		plat->high = sector_floorheight(sec);

	    plat->wait = TICRATE*PLATWAIT;
	    plat->status = P_Random()&1;

	    S_StartSound(&sec->soundorg,sfx_pstart);
	    break;
	}
	P_AddActivePlat(plat);
    }
    return rtn;
}



void P_ActivateInStasis(int tag)
{
    int		i;
	
    for (i = 0;i < MAXPLATS;i++) {
	if (activeplats[i]) {
	    plat_t *p = shortptr_to_plat(activeplats[i]);
	    if (p->tag == tag && p->status == in_stasis)
	    {
                p->status = p->oldstatus;
                p->thinker.function = ThinkF_T_PlatRaise;
	    }
	}
    }
}

void EV_StopPlat(line_t* line)
{
    int		j;
	
    for (j = 0;j < MAXPLATS;j++) {
	if (activeplats[j]) {
	    plat_t *p = shortptr_to_plat(activeplats[j]);
	    if (p->status != in_stasis && (p->tag == line_tag(line))) {
                p->oldstatus = p->status;
                p->status = in_stasis;
                p->thinker.function = ThinkF_NULL;
            }
	}
    }
}

void P_AddActivePlat(plat_t* plat)
{
    int		i;
    
    for (i = 0;i < MAXPLATS;i++)
	if (!activeplats[i])
	{
	    activeplats[i] = plat_to_shortptr(plat);
	    return;
	}
    I_Error ("P_AddActivePlat: no more plats!");
}

void P_RemoveActivePlat(plat_t* plat)
{
    int		i;
    for (i = 0;i < MAXPLATS;i++) {
        if (plat == shortptr_to_plat(activeplats[i]))
        {
            plat->sector->specialdata = 0;
            P_RemoveThinker(&plat->thinker);
            activeplats[i] = 0;
	    
            return;
        }
    }
    I_Error ("P_RemoveActivePlat: can't find plat!");
}
