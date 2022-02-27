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
//	Implements special effects:
//	Texture animation, height or lighting changes
//	 according to adjacent sectors, respective
//	 utility functions, etc.
//	Line Tag handling. Line and Sector triggers.
//


#include <stdlib.h>

#include "doomdef.h"
#include "doomstat.h"

#include "deh_main.h"
#include "i_system.h"
#include "z_zone.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_random.h"
#include "w_wad.h"

#include "r_local.h"
#include "p_local.h"

#include "g_game.h"

#include "s_sound.h"

// State.
#include "r_state.h"

// Data.
#include "sounds.h"


#if !USE_WHD // whd just using an integer for the equivalent of picnum
//
// Animating textures and planes
// There is another anim_t used in wi_stuff, unrelated.
//
typedef struct
{
    boolean	istexture;
    int		picnum;
    int		basepic;
    int		numpics;
    int		speed;
    
} anim_t;
#endif

//
//      source animation definition
//
typedef PACKED_STRUCT(
{
    textureorflatname_def_t endname;
    textureorflatname_def_t startname;
    uint8_t 	istexture;	// if false, it is a flat
    uint8_t		speed;
}) animdef_t;

#define FLATANIM_DEF(last,first,speed) { FLAT_NAME(last), FLAT_NAME(first), 0, speed }
#define TEXANIM_DEF(last,first,speed) { TEXTURE_NAME(last), TEXTURE_NAME(first), 1, speed }

//
// P_InitPicAnims
//

// Floor/ceiling animation sequences,
//  defined by first and last frame,
//  i.e. the flat (64x64 tile) name to
//  be used.
// The full animation sequence is given
//  using all the flats between the start
//  and end entry, in the order found in
//  the WAD file.
//
const animdef_t		animdefs[] =
{
    FLATANIM_DEF(NUKAGE3, NUKAGE1, 8),
    FLATANIM_DEF(FWATER4, FWATER1, 8),
    FLATANIM_DEF(SWATER4, SWATER1, 8),
    FLATANIM_DEF(LAVA4, LAVA1, 8),
    FLATANIM_DEF(BLOOD3, BLOOD1, 8),

#if !DEMO1_ONLY
    // DOOM II flat animations.
    FLATANIM_DEF(RROCK08, RROCK05, 8),
    FLATANIM_DEF(SLIME04, SLIME01, 8),
    FLATANIM_DEF(SLIME08, SLIME05, 8),
    FLATANIM_DEF(SLIME12, SLIME09, 8),
#endif

    TEXANIM_DEF(SLADRIP3, SLADRIP1, 8),

#if !DEMO1_ONLY
    TEXANIM_DEF(BLODGR4, BLODGR1, 8),
    TEXANIM_DEF(BLODRIP4, BLODRIP1, 8),
    TEXANIM_DEF(FIREWALL, FIREWALA, 8),
    TEXANIM_DEF(GSTFONT3, GSTFONT1, 8),
    TEXANIM_DEF(FIRELAVA, FIRELAV3, 8),
    TEXANIM_DEF(FIREMAG3, FIREMAG1, 8),
    TEXANIM_DEF(FIREBLU2, FIREBLU1, 8),
    TEXANIM_DEF(ROCKRED3, ROCKRED1, 8),

    TEXANIM_DEF(BFALL4, BFALL1, 8),
    TEXANIM_DEF(SFALL4, SFALL1, 8),
    TEXANIM_DEF(WFALL4, WFALL1, 8),
    TEXANIM_DEF(DBRAIN4, DBRAIN1, 8),
#endif
};

#if !USE_WHD
anim_t		anims[count_of(animdefs)];
anim_t*		lastanim;
#endif


void P_InitPicAnims (void)
{
#if !USE_WHD
    int		i;

    //	Init animation
    lastanim = anims;
    for (i=0 ; i < count_of(animdefs); i++)
    {
        const char *startname, *endname;
        startname = DEH_String(animdefs[i].startname);
        endname = DEH_String(animdefs[i].endname);

	if (animdefs[i].istexture)
	{
	    // different episode ?
	    if (R_CheckTextureNumForName(startname) == -1)
		continue;	

	    lastanim->picnum = R_TextureNumForName(endname);
	    lastanim->basepic = R_TextureNumForName(startname);
	}
	else
	{
	    if (W_CheckNumForName(startname) == -1)
		continue;

	    lastanim->picnum = R_FlatNumForName(endname);
	    lastanim->basepic = R_FlatNumForName(startname);
	}

	lastanim->istexture = animdefs[i].istexture;
	lastanim->numpics = lastanim->picnum - lastanim->basepic + 1;

	if (lastanim->numpics < 2)
	    I_Error ("P_InitPicAnims: bad cycle from %s to %s",
		     startname, endname);
	
	lastanim->speed = animdefs[i].speed;
	lastanim++;
    }

#endif
}



//
// UTILITIES
//



//
// getSide()
// Will return a side_t*
//  given the number of the current sector,
//  the line number, and the side (0/1) that you want.
//
side_t*
getSide
( int		currentSector,
  int		line,
  int		side )
{
    return sidenum_to_side( line_sidenum(sector_line(&sectors[currentSector], line), side));
}


//
// getSector()
// Will return a sector_t*
//  given the number of the current sector,
//  the line number and the side (0/1) that you want.
//
sector_t*
getSector
( int		currentSector,
  int		line,
  int		side )
{
    return side_sector(sidenum_to_side(line_sidenum(sector_line(&sectors[currentSector], line), side) ));
}


//
// twoSided()
// Given the sector number and the line number,
//  it will tell you whether the line is two-sided or not.
//
int
twoSided
( int	sector,
  int	line )
{
    return (line_flags(sector_line(&sectors[sector], line))) & ML_TWOSIDED;
}




//
// getNextSector()
// Return sector_t * of sector next to current.
// NULL if not two-sided line
//
sector_t*
getNextSector
( line_t*	line,
  sector_t*	sec )
{
    if (!(line_flags(line) & ML_TWOSIDED))
	return NULL;
		
    if (line_frontsector(line) == sec)
	return line_backsector(line);
	
    return line_frontsector(line);
}



//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t	P_FindLowestFloorSurrounding(sector_t* sec)
{
    int			i;
    line_t*		check;
    sector_t*		other;
    fixed_t		floor = sector_floorheight(sec);
	
    for (i=0 ;i < sec->linecount ; i++)
    {
	check = sector_line(sec, i);
	other = getNextSector(check,sec);

	if (!other)
	    continue;
	
	if (sector_floorheight(other) < floor)
	    floor = sector_floorheight(other);
    }
    return floor;
}



//
// P_FindHighestFloorSurrounding()
// FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t	P_FindHighestFloorSurrounding(sector_t *sec)
{
    int			i;
    line_t*		check;
    sector_t*		other;
    fixed_t		floor = -500*FRACUNIT;
	
    for (i=0 ;i < sec->linecount ; i++)
    {
	check = sector_line(sec, i);
	other = getNextSector(check,sec);
	
	if (!other)
	    continue;
	
	if (sector_floorheight(other) > floor)
	    floor = sector_floorheight(other);
    }
    return floor;
}



//
// P_FindNextHighestFloor
// FIND NEXT HIGHEST FLOOR IN SURROUNDING SECTORS
// Note: this should be doable w/o a fixed array.

// Thanks to entryway for the Vanilla overflow emulation.

// 20 adjoining sectors max!
#define MAX_ADJOINING_SECTORS     20

fixed_t
P_FindNextHighestFloor
( sector_t* sec,
  int       currentheight )
{
    int         i;
    int         h;
    int         min;
    line_t*     check;
    sector_t*   other;
    fixed_t     height = currentheight;
    fixed_t     heightlist[MAX_ADJOINING_SECTORS + 2];

    for (i=0, h=0; i < sec->linecount; i++)
    {
        check = sector_line(sec, i);
        other = getNextSector(check,sec);

        if (!other)
            continue;
        
        if (sector_floorheight(other) > height)
        {
            // Emulation of memory (stack) overflow
            if (h == MAX_ADJOINING_SECTORS + 1)
            {
                height = sector_floorheight(other);
            }
            else if (h == MAX_ADJOINING_SECTORS + 2)
            {
                // Fatal overflow: game crashes at 22 sectors
                I_Error("Sector with more than 22 adjoining sectors. "
                        "Vanilla will crash here");
            }

            heightlist[h++] = sector_floorheight(other);
        }
    }
    
    // Find lowest height in list
    if (!h)
    {
        return currentheight;
    }
        
    min = heightlist[0];
    
    // Range checking? 
    for (i = 1; i < h; i++)
    {
        if (heightlist[i] < min)
        {
            min = heightlist[i];
        }
    }

    return min;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t
P_FindLowestCeilingSurrounding(sector_t* sec)
{
    int			i;
    line_t*		check;
    sector_t*		other;
    fixed_t		height = INT_MAX;
	
    for (i=0 ;i < sec->linecount ; i++)
    {
	check = sector_line(sec, i);
	other = getNextSector(check,sec);

	if (!other)
	    continue;

	if (sector_ceilingheight(other) < height)
	    height = sector_ceilingheight(other);
    }
    return height;
}


//
// FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t	P_FindHighestCeilingSurrounding(sector_t* sec)
{
    int		i;
    line_t*	check;
    sector_t*	other;
    fixed_t	height = 0;
	
    for (i=0 ;i < sec->linecount ; i++)
    {
	check = sector_line(sec, i);
	other = getNextSector(check,sec);

	if (!other)
	    continue;

	if (sector_ceilingheight(other) > height)
	    height = sector_ceilingheight(other);
    }
    return height;
}



//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//
int
P_FindSectorFromLineTag
( line_t*	line,
  int		start )
{
    int	i;

    int tag = line_tag(line);
    for (i=start+1;i<numsectors;i++)
	if (sectors[i].tag == tag)
	    return i;
    
    return -1;
}




//
// Find minimum light from an adjacent sector
//
int
P_FindMinSurroundingLight
( sector_t*	sector,
  int		max )
{
    int		i;
    int		min;
    line_t*	line;
    sector_t*	check;
	
    min = max;
    for (i=0 ; i < sector->linecount ; i++)
    {
	line = sector_line(sector, i);
	check = getNextSector(line,sector);

	if (!check)
	    continue;

	if (check->lightlevel < min)
	    min = check->lightlevel;
    }
    return min;
}



//
// EVENTS
// Events are operations triggered by using, crossing,
// or shooting special lines, or by timed thinkers.
//

//
// P_CrossSpecialLine - TRIGGER
// Called every time a thing origin is about
//  to cross a line with a non 0 special.
//
void
P_CrossSpecialLine
( int		linenum,
  int		side,
  mobj_t*	thing )
{
    line_t*	line;
    int		ok;

    line = &lines[linenum];
    
    //	Triggers that other things can activate
    if (!mobj_full(thing)->sp_player)
    {
	// Things that should NOT trigger specials...
	switch(thing->type)
	{
	  case MT_ROCKET:
	  case MT_PLASMA:
	  case MT_BFG:
	  case MT_TROOPSHOT:
	  case MT_HEADSHOT:
	  case MT_BRUISERSHOT:
	    return;
	    break;
	    
	  default: break;
	}
		
	ok = 0;
	switch(line_special(line))
	{
	  case 39:	// TELEPORT TRIGGER
	  case 97:	// TELEPORT RETRIGGER
	  case 125:	// TELEPORT MONSTERONLY TRIGGER
	  case 126:	// TELEPORT MONSTERONLY RETRIGGER
	  case 4:	// RAISE DOOR
	  case 10:	// PLAT DOWN-WAIT-UP-STAY TRIGGER
	  case 88:	// PLAT DOWN-WAIT-UP-STAY RETRIGGER
	    ok = 1;
	    break;
	}
	if (!ok)
	    return;
    }


    // Note: could use some const's here.
    switch (line_special(line))
    {
	// TRIGGERS.
	// All from here to RETRIGGERS.
      case 2:
	// Open Door
	EV_DoDoor(line,vld_open);
            clear_line_special(line);
	break;

      case 3:
	// Close Door
	EV_DoDoor(line,vld_close);
            clear_line_special(line);
	break;

      case 4:
	// Raise Door
	EV_DoDoor(line,vld_normal);
            clear_line_special(line);
	break;
	
      case 5:
	// Raise Floor
	EV_DoFloor(line,raiseFloor);
            clear_line_special(line);
	break;
	
      case 6:
	// Fast Ceiling Crush & Raise
	EV_DoCeiling(line,fastCrushAndRaise);
            clear_line_special(line);
	break;
	
      case 8:
	// Build Stairs
	EV_BuildStairs(line,build8);
            clear_line_special(line);
	break;
	
      case 10:
	// PlatDownWaitUp
	EV_DoPlat(line,downWaitUpStay,0);
            clear_line_special(line);
	break;
	
      case 12:
	// Light Turn On - brightest near
	EV_LightTurnOn(line,0);
            clear_line_special(line);
	break;
	
      case 13:
	// Light Turn On 255
	EV_LightTurnOn(line,255);
            clear_line_special(line);
	break;
	
      case 16:
	// Close Door 30
	EV_DoDoor(line,vld_close30ThenOpen);
            clear_line_special(line);
	break;
	
      case 17:
	// Start Light Strobing
	EV_StartLightStrobing(line);
            clear_line_special(line);
	break;
	
      case 19:
	// Lower Floor
	EV_DoFloor(line,lowerFloor);
            clear_line_special(line);
	break;
	
      case 22:
	// Raise floor to nearest height and change texture
	EV_DoPlat(line,raiseToNearestAndChange,0);
            clear_line_special(line);
	break;
	
      case 25:
	// Ceiling Crush and Raise
	EV_DoCeiling(line,crushAndRaise);
            clear_line_special(line);
	break;
	
      case 30:
	// Raise floor to shortest texture height
	//  on either side of lines.
	EV_DoFloor(line,raiseToTexture);
            clear_line_special(line);
	break;
	
      case 35:
	// Lights Very Dark
	EV_LightTurnOn(line,35);
            clear_line_special(line);
	break;
	
      case 36:
	// Lower Floor (TURBO)
	EV_DoFloor(line,turboLower);
            clear_line_special(line);
	break;
	
      case 37:
	// LowerAndChange
	EV_DoFloor(line,lowerAndChange);
            clear_line_special(line);
	break;
	
      case 38:
	// Lower Floor To Lowest
	EV_DoFloor( line, lowerFloorToLowest );
            clear_line_special(line);
	break;
	
      case 39:
	// TELEPORT!
	EV_Teleport( line, side, thing );
            clear_line_special(line);
	break;

      case 40:
	// RaiseCeilingLowerFloor
	EV_DoCeiling( line, raiseToHighest );
	EV_DoFloor( line, lowerFloorToLowest );
            clear_line_special(line);
	break;
	
      case 44:
	// Ceiling Crush
	EV_DoCeiling( line, lowerAndCrush );
            clear_line_special(line);
	break;
	
      case 52:
	// EXIT!
	G_ExitLevel ();
	break;
	
      case 53:
	// Perpetual Platform Raise
	EV_DoPlat(line,perpetualRaise,0);
            clear_line_special(line);
	break;
	
      case 54:
	// Platform Stop
	EV_StopPlat(line);
            clear_line_special(line);
	break;

      case 56:
	// Raise Floor Crush
	EV_DoFloor(line,raiseFloorCrush);
            clear_line_special(line);
	break;

      case 57:
	// Ceiling Crush Stop
	EV_CeilingCrushStop(line);
            clear_line_special(line);
	break;
	
      case 58:
	// Raise Floor 24
	EV_DoFloor(line,raiseFloor24);
            clear_line_special(line);
	break;

      case 59:
	// Raise Floor 24 And Change
	EV_DoFloor(line,raiseFloor24AndChange);
            clear_line_special(line);
	break;
	
      case 104:
	// Turn lights off in sector(tag)
	EV_TurnTagLightsOff(line);
            clear_line_special(line);
	break;
	
      case 108:
	// Blazing Door Raise (faster than TURBO!)
	EV_DoDoor (line,vld_blazeRaise);
            clear_line_special(line);
	break;
	
      case 109:
	// Blazing Door Open (faster than TURBO!)
	EV_DoDoor (line,vld_blazeOpen);
            clear_line_special(line);
	break;
	
      case 100:
	// Build Stairs Turbo 16
	EV_BuildStairs(line,turbo16);
            clear_line_special(line);
	break;
	
      case 110:
	// Blazing Door Close (faster than TURBO!)
	EV_DoDoor (line,vld_blazeClose);
            clear_line_special(line);
	break;

      case 119:
	// Raise floor to nearest surr. floor
	EV_DoFloor(line,raiseFloorToNearest);
            clear_line_special(line);
	break;
	
      case 121:
	// Blazing PlatDownWaitUpStay
	EV_DoPlat(line,blazeDWUS,0);
            clear_line_special(line);
	break;
	
      case 124:
	// Secret EXIT
	G_SecretExitLevel ();
	break;
		
      case 125:
	// TELEPORT MonsterONLY
	if (!mobj_full(thing)->sp_player)
	{
	    EV_Teleport( line, side, thing );
        clear_line_special(line);
	}
	break;
	
      case 130:
	// Raise Floor Turbo
	EV_DoFloor(line,raiseFloorTurbo);
            clear_line_special(line);
	break;
	
      case 141:
	// Silent Ceiling Crush & Raise
	EV_DoCeiling(line,silentCrushAndRaise);
            clear_line_special(line);
	break;
	
	// RETRIGGERS.  All from here till end.
      case 72:
	// Ceiling Crush
	EV_DoCeiling( line, lowerAndCrush );
	break;

      case 73:
	// Ceiling Crush and Raise
	EV_DoCeiling(line,crushAndRaise);
	break;

      case 74:
	// Ceiling Crush Stop
	EV_CeilingCrushStop(line);
	break;
	
      case 75:
	// Close Door
	EV_DoDoor(line,vld_close);
	break;
	
      case 76:
	// Close Door 30
	EV_DoDoor(line,vld_close30ThenOpen);
	break;
	
      case 77:
	// Fast Ceiling Crush & Raise
	EV_DoCeiling(line,fastCrushAndRaise);
	break;
	
      case 79:
	// Lights Very Dark
	EV_LightTurnOn(line,35);
	break;
	
      case 80:
	// Light Turn On - brightest near
	EV_LightTurnOn(line,0);
	break;
	
      case 81:
	// Light Turn On 255
	EV_LightTurnOn(line,255);
	break;
	
      case 82:
	// Lower Floor To Lowest
	EV_DoFloor( line, lowerFloorToLowest );
	break;
	
      case 83:
	// Lower Floor
	EV_DoFloor(line,lowerFloor);
	break;

      case 84:
	// LowerAndChange
	EV_DoFloor(line,lowerAndChange);
	break;

      case 86:
	// Open Door
	EV_DoDoor(line,vld_open);
	break;
	
      case 87:
	// Perpetual Platform Raise
	EV_DoPlat(line,perpetualRaise,0);
	break;
	
      case 88:
	// PlatDownWaitUp
	EV_DoPlat(line,downWaitUpStay,0);
	break;
	
      case 89:
	// Platform Stop
	EV_StopPlat(line);
	break;
	
      case 90:
	// Raise Door
	EV_DoDoor(line,vld_normal);
	break;
	
      case 91:
	// Raise Floor
	EV_DoFloor(line,raiseFloor);
	break;
	
      case 92:
	// Raise Floor 24
	EV_DoFloor(line,raiseFloor24);
	break;
	
      case 93:
	// Raise Floor 24 And Change
	EV_DoFloor(line,raiseFloor24AndChange);
	break;
	
      case 94:
	// Raise Floor Crush
	EV_DoFloor(line,raiseFloorCrush);
	break;
	
      case 95:
	// Raise floor to nearest height
	// and change texture.
	EV_DoPlat(line,raiseToNearestAndChange,0);
	break;
	
      case 96:
	// Raise floor to shortest texture height
	// on either side of lines.
	EV_DoFloor(line,raiseToTexture);
	break;
	
      case 97:
	// TELEPORT!
	EV_Teleport( line, side, thing );
	break;
	
      case 98:
	// Lower Floor (TURBO)
	EV_DoFloor(line,turboLower);
	break;

      case 105:
	// Blazing Door Raise (faster than TURBO!)
	EV_DoDoor (line,vld_blazeRaise);
	break;
	
      case 106:
	// Blazing Door Open (faster than TURBO!)
	EV_DoDoor (line,vld_blazeOpen);
	break;

      case 107:
	// Blazing Door Close (faster than TURBO!)
	EV_DoDoor (line,vld_blazeClose);
	break;

      case 120:
	// Blazing PlatDownWaitUpStay.
	EV_DoPlat(line,blazeDWUS,0);
	break;
	
      case 126:
	// TELEPORT MonsterONLY.
	if (!mobj_full(thing)->sp_player)
	    EV_Teleport( line, side, thing );
	break;
	
      case 128:
	// Raise To Nearest Floor
	EV_DoFloor(line,raiseFloorToNearest);
	break;
	
      case 129:
	// Raise Floor Turbo
	EV_DoFloor(line,raiseFloorTurbo);
	break;
    }
}



//
// P_ShootSpecialLine - IMPACT SPECIALS
// Called when a thing shoots a special line.
//
void
P_ShootSpecialLine
( mobj_t*	thing,
  line_t*	line )
{
    int		ok;
    
    //	Impacts that other things can activate.
    if (!mobj_full(thing)->sp_player)
    {
	ok = 0;
	switch(line_special(line))
	{
	  case 46:
	    // OPEN DOOR IMPACT
	    ok = 1;
	    break;
	}
	if (!ok)
	    return;
    }

    switch(line_special(line))
    {
      case 24:
	// RAISE FLOOR
	EV_DoFloor(line,raiseFloor);
	P_ChangeSwitchTexture(line,0);
	break;
	
      case 46:
	// OPEN DOOR
	EV_DoDoor(line,vld_open);
	P_ChangeSwitchTexture(line,1);
	break;
	
      case 47:
	// RAISE FLOOR NEAR AND CHANGE
	EV_DoPlat(line,raiseToNearestAndChange,0);
	P_ChangeSwitchTexture(line,0);
	break;
    }
}



//
// P_PlayerInSpecialSector
// Called every tic frame
//  that the player origin is in a special sector
//
void P_PlayerInSpecialSector (player_t* player)
{
    sector_t*	sector;
	
    sector = mobj_sector(player->mo);

    // Falling, not all the way down yet?
    if (player->mo->z != sector_floorheight(sector))
	return;	

    // Has hitten ground.
    switch (sector->special)
    {
      case 5:
	// HELLSLIME DAMAGE
	if (!player->powers[pw_ironfeet])
	    if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 10);
	break;
	
      case 7:
	// NUKAGE DAMAGE
	if (!player->powers[pw_ironfeet])
	    if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 5);
	break;
	
      case 16:
	// SUPER HELLSLIME DAMAGE
      case 4:
	// STROBE HURT
	if (!player->powers[pw_ironfeet]
	    || (P_Random()<5) )
	{
	    if (!(leveltime&0x1f))
		P_DamageMobj (player->mo, NULL, NULL, 20);
	}
	break;
			
      case 9:
	// SECRET SECTOR
	player->secretcount++;
	sector->special = 0;
	break;
			
      case 11:
	// EXIT SUPER DAMAGE! (for E1M8 finale)
	player->cheats &= ~CF_GODMODE;

	if (!(leveltime&0x1f))
	    P_DamageMobj (player->mo, NULL, NULL, 20);

	if (player->health <= 10)
	    G_ExitLevel();
	break;
			
      default:
	I_Error ("P_PlayerInSpecialSector: "
		 "unknown special %i",
		 sector->special);
	break;
    };
}




//
// P_UpdateSpecials
// Animate planes, scroll walls, etc.
//
boolean		levelTimer;
int		levelTimeCount;

void P_UpdateSpecials (void)
{
    int		i;
    line_t*	line;

    
    //	LEVEL TIMER
    if (levelTimer == true)
    {
	levelTimeCount--;
	if (!levelTimeCount)
	    G_ExitLevel();
    }

    //	ANIMATE FLATS AND TEXTURES GLOBALLY
#if !USE_WHD
    for (anim_t *anim = anims ; anim < lastanim ; anim++)
    {
	for (i=anim->basepic ; i<anim->basepic+anim->numpics ; i++)
	{
	    int pic = anim->basepic + ( (leveltime/anim->speed + i)%anim->numpics );
	    if (anim->istexture)
		texturetranslation[i] = pic;
	    else
		flattranslation[i] = pic;
	}
    }
#else
    for (int a=0;a<count_of(animdefs);a++)
    {
        const animdef_t *anim = animdefs + a;
        for (i=anim->startname ; i<=anim->endname ; i++)
        {
            textureorflatname_t pic = anim->startname + ( (leveltime/anim->speed + i)%(anim->endname + 1 - anim->startname));
            if (anim->istexture) {
                assert(i < count_of(whd_texturetranslation));
                whd_texturetranslation[i] = pic;
            } else {
                assert(i < count_of(whd_flattranslation));
                whd_flattranslation[i] = pic;
            }
        }
    }
#endif

#if !USE_WHD
    //	ANIMATE LINE SPECIALS
    for (i = 0; i < numlinespecials; i++)
    {
	line = linespeciallist[i];
	switch(line_special(line))
	{
	  case 48:
	    // EFFECT FIRSTCOL SCROLL +
        side_settextureoffset16(sidenum_to_side(line_sidenum(line, 0)), side_textureoffset16(sidenum_to_side(line_sidenum(line, 0)))+1);
	    break;
	}
    }
#else
    // single global variable
    linespecialoffset++;
#endif

    
    //	DO BUTTONS
    for (i = 0; i < MAXBUTTONS; i++)
	if (buttonlist[i].btimer)
	{
	    buttonlist[i].btimer--;
	    if (!buttonlist[i].btimer)
	    {
		switch(buttonlist[i].where)
		{
		  case top:
		    side_settoptexture(sidenum_to_side(line_sidenum(buttonlist[i].line, 0)), buttonlist[i].btexture);
		    break;
		    
		  case middle:
		    side_setmidtexture(sidenum_to_side(line_sidenum(buttonlist[i].line, 0)), buttonlist[i].btexture);
		    break;
		    
		  case bottom:
		    side_setbottomtexture(sidenum_to_side(line_sidenum(buttonlist[i].line, 0)), buttonlist[i].btexture);
		    break;
		}
		S_StartSound(buttonlist[i].soundorg,sfx_swtchn);
		memset(&buttonlist[i],0,sizeof(button_t));
	    }
	}
}


//
// Donut overrun emulation
//
// Derived from the code from PrBoom+.  Thanks go to Andrey Budko (entryway)
// as usual :-)
//

#define DONUT_FLOORHEIGHT_DEFAULT 0x00000000
#define DONUT_FLOORPIC_DEFAULT 0x16

static void DonutOverrun(fixed_t *s3_floorheight, short *s3_floorpic,
                         line_t *line, sector_t *pillar_sector)
{
    static isb_int8_t first = 1;
    static int tmp_s3_floorheight;
    static int tmp_s3_floorpic;

    extern int numflats;

    if (first)
    {
        int p;

        // This is the first time we have had an overrun.
        first = 0;

        // Default values
        tmp_s3_floorheight = DONUT_FLOORHEIGHT_DEFAULT;
        tmp_s3_floorpic = DONUT_FLOORPIC_DEFAULT;

        //!
        // @category compat
        // @arg <x> <y>
        //
        // Use the specified magic values when emulating behavior caused
        // by memory overruns from improperly constructed donuts.
        // In Vanilla Doom this can differ depending on the operating
        // system.  The default (if this option is not specified) is to
        // emulate the behavior when running under Windows 98.

#if !NO_USE_ARGS
        p = M_CheckParmWithArgs("-donut", 2);

        if (p > 0)
        {
            // Dump of needed memory: (fixed_t)0000:0000 and (short)0000:0008
            //
            // C:\>debug
            // -d 0:0
            //
            // DOS 6.22:
            // 0000:0000    (57 92 19 00) F4 06 70 00-(16 00)
            // DOS 7.1:
            // 0000:0000    (9E 0F C9 00) 65 04 70 00-(16 00)
            // Win98:
            // 0000:0000    (00 00 00 00) 65 04 70 00-(16 00)
            // DOSBox under XP:
            // 0000:0000    (00 00 00 F1) ?? ?? ?? 00-(07 00)

            M_StrToInt(myargv[p + 1], &tmp_s3_floorheight);
            M_StrToInt(myargv[p + 2], &tmp_s3_floorpic);

            if (tmp_s3_floorpic >= numflats)
            {
                stderr_print(
                        "DonutOverrun: The second parameter for \"-donut\" "
                        "switch should be greater than 0 and less than number "
                        "of flats (%d). Using default value (%d) instead. \n",
                        numflats, DONUT_FLOORPIC_DEFAULT);
                tmp_s3_floorpic = DONUT_FLOORPIC_DEFAULT;
            }
        }
#endif
    }

    /*
    stderr_print(
            "Linedef: %d; Sector: %d; "
            "New floor height: %d; New floor pic: %d\n",
            line->iLineID, pillar_sector->iSectorID,
            tmp_s3_floorheight >> 16, tmp_s3_floorpic);
     */

    *s3_floorheight = (fixed_t) tmp_s3_floorheight;
    *s3_floorpic = (short) tmp_s3_floorpic;
}


//
// Special Stuff that can not be categorized
//
int EV_DoDonut(line_t*	line)
{
    sector_t*		s1;
    sector_t*		s2;
    sector_t*		s3;
    int			secnum;
    int			rtn;
    int			i;
    floormove_t*	floor;
    fixed_t s3_floorheight;
    short s3_floorpic;

    secnum = -1;
    rtn = 0;
    while ((secnum = P_FindSectorFromLineTag(line,secnum)) >= 0)
    {
	s1 = &sectors[secnum];

	// ALREADY MOVING?  IF SO, KEEP GOING...
	if (s1->specialdata)
	    continue;

	rtn = 1;
	s2 = getNextSector(sector_line(s1, 0),s1);

        // Vanilla Doom does not check if the linedef is one sided.  The
        // game does not crash, but reads invalid memory and causes the
        // sector floor to move "down" to some unknown height.
        // DOSbox prints a warning about an invalid memory access.
        //
        // I'm not sure exactly what invalid memory is being read.  This
        // isn't something that should be done, anyway.
        // Just print a warning and return.

        if (s2 == NULL)
        {
            stderr_print(
                    "EV_DoDonut: linedef had no second sidedef! "
                    "Unexpected behavior may occur in Vanilla Doom. \n");
	    break;
        }

	for (i = 0; i < s2->linecount; i++)
	{
	    s3 = line_backsector(sector_line(s2, i));

	    if (s3 == s1)
		continue;

            if (s3 == NULL)
            {
                // e6y
                // s3 is NULL, so
                // s3->floorheight is an int at 0000:0000
                // s3->floorpic is a short at 0000:0008
                // Trying to emulate

                stderr_print(
                        "EV_DoDonut: WARNING: emulating buffer overrun due to "
                        "NULL back sector. "
                        "Unexpected behavior may occur in Vanilla Doom.\n");

                DonutOverrun(&s3_floorheight, &s3_floorpic, line, s1);
            }
            else
            {
                s3_floorheight = sector_floorheight(s3);
                s3_floorpic = s3->floorpic;
            }

	    //	Spawn rising slime
	    floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
	    P_AddThinker (&floor->thinker);
	    s2->specialdata = ptr_to_shortptr(floor);
	    floor->thinker.function = ThinkF_T_MoveFloor;
	    floor->type = donutRaise;
	    floor->crush = false;
	    floor->direction = 1;
	    floor->sector = s2;
	    floor->speed = FLOORSPEED / 2;
	    floor->texture = s3_floorpic;
	    floor->newspecial = 0;
	    floor->floordestheight = s3_floorheight;
	    
	    //	Spawn lowering donut-hole
	    floor = Z_Malloc (sizeof(*floor), PU_LEVSPEC, 0);
	    P_AddThinker (&floor->thinker);
	    s1->specialdata = ptr_to_shortptr(floor);
        floor->thinker.function = ThinkF_T_MoveFloor;
	    floor->type = lowerFloor;
	    floor->crush = false;
	    floor->direction = -1;
	    floor->sector = s1;
	    floor->speed = FLOORSPEED / 2;
	    floor->floordestheight = s3_floorheight;
	    break;
	}
    }
    return rtn;
}



//
// SPECIAL SPAWNING
//

//
// P_SpawnSpecials
// After the map has been loaded, scan for specials
//  that spawn thinkers
//
short		numlinespecials;
#if !USE_WHD
line_t*		linespeciallist[MAXLINEANIMS];
#else
uint16_t linespecialoffsetlist[MAXLINEANIMS];
uint16_t linespecialoffset;
#endif


// Parses command line parameters.
void P_SpawnSpecials (void)
{
    sector_t*	sector;
    int		i;

    // See if -TIMER was specified.

    if (timelimit > 0 && deathmatch)
    {
        levelTimer = true;
        levelTimeCount = timelimit * 60 * TICRATE;
    }
    else
    {
	levelTimer = false;
    }

    //	Init special SECTORs.
    sector = sectors;
    for (i=0 ; i<numsectors ; i++, sector++)
    {
	if (!sector->special)
	    continue;
	
	switch (sector->special)
	{
	  case 1:
	    // FLICKERING LIGHTS
	    P_SpawnLightFlash (sector);
	    break;

	  case 2:
	    // STROBE FAST
	    P_SpawnStrobeFlash(sector,FASTDARK,0);
	    break;
	    
	  case 3:
	    // STROBE SLOW
	    P_SpawnStrobeFlash(sector,SLOWDARK,0);
	    break;
	    
	  case 4:
	    // STROBE FAST/DEATH SLIME
	    P_SpawnStrobeFlash(sector,FASTDARK,0);
	    sector->special = 4;
	    break;
	    
	  case 8:
	    // GLOWING LIGHT
	    P_SpawnGlowingLight(sector);
	    break;
	  case 9:
	    // SECRET SECTOR
	    totalsecret++;
	    break;
	    
	  case 10:
	    // DOOR CLOSE IN 30 SECONDS
	    P_SpawnDoorCloseIn30 (sector);
	    break;
	    
	  case 12:
	    // SYNC STROBE SLOW
	    P_SpawnStrobeFlash (sector, SLOWDARK, 1);
	    break;

	  case 13:
	    // SYNC STROBE FAST
	    P_SpawnStrobeFlash (sector, FASTDARK, 1);
	    break;

	  case 14:
	    // DOOR RAISE IN 5 MINUTES
	    P_SpawnDoorRaiseIn5Mins (sector, i);
	    break;
	    
	  case 17:
	    P_SpawnFireFlicker(sector);
	    break;
	}
    }

    
    //	Init line EFFECTs
    numlinespecials = 0;
    line_t *li = lines;
    for (i = 0;i < numlines; i++)
    {
	switch(line_special(li))
	{
	  case 48:
            if (numlinespecials >= MAXLINEANIMS)
            {
                I_Error("Too many scrolling wall linedefs! "
                        "(Vanilla limit is 64)");
            }
	    // EFFECT FIRSTCOL SCROLL+
#if !USE_WHD
	    linespeciallist[numlinespecials] = li;
#else
            assert(li - lines < 65536);
            linespecialoffsetlist[numlinespecials] = li - lines;
#endif
	    numlinespecials++;
	    break;
	}
	li += line_next_step(li);

    }

    
    //	Init other misc stuff
    for (i = 0;i < MAXCEILINGS;i++)
	activeceilings[i] = 0;

    for (i = 0;i < MAXPLATS;i++)
	activeplats[i] = 0;
    
    for (i = 0;i < MAXBUTTONS;i++)
	memset(&buttonlist[i],0,sizeof(button_t));

    // UNUSED: no horizonal sliders.
    //	P_InitSlidingDoorFrames();
}
