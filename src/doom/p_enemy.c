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
//	Enemy thinking, AI.
//	Action Pointer Functions
//	that are associated with states/frames. 
//

#include <stdio.h>
#include <stdlib.h>

#include "m_random.h"
#include "i_system.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

#include "g_game.h"

// State.
#include "doomstat.h"
#include "r_state.h"

// Data.
#include "sounds.h"




typedef enum
{
    DI_EAST,
    DI_NORTHEAST,
    DI_NORTH,
    DI_NORTHWEST,
    DI_WEST,
    DI_SOUTHWEST,
    DI_SOUTH,
    DI_SOUTHEAST,
    DI_NODIR,
    NUMDIRS
    
} dirtype_t;


//
// P_NewChaseDir related LUT.
//
static const dirtype_t opposite[] =
{
  DI_WEST, DI_SOUTHWEST, DI_SOUTH, DI_SOUTHEAST,
  DI_EAST, DI_NORTHEAST, DI_NORTH, DI_NORTHWEST, DI_NODIR
};

static const dirtype_t diags[] =
{
    DI_NORTHWEST, DI_NORTHEAST, DI_SOUTHWEST, DI_SOUTHEAST
};

void A_Fall (mobj_t *actor);


//
// ENEMY THINKING
// Enemies are allways spawned
// with targetplayer = -1, threshold = 0
// Most monsters are spawned unaware of all players,
// but some can be made preaware
//


//
// Called by P_NoiseAlert.
// Recursively traverse adjacent sectors,
// sound blocking lines cut off traversal.
//

mobj_t*		soundtarget;

void
P_RecursiveSound
( sector_t*	sec,
  int		soundblocks )
{
    int		i;
    line_t*	check;
    sector_t*	other;

    assert(soundblocks <= 1);
    // wake up all monsters in this sector
    if (sector_validcount_update_check(sec, validcount) && sec->soundtraversed <= soundblocks+1) {
	return;		// already flooded
    }
    
    sec->soundtraversed = soundblocks+1;
    sec->soundtarget = mobj_to_shortptr(soundtarget);
	
    for (i=0 ;i<sec->linecount ; i++)
    {
	check = sector_line(sec, i);
	if (! (line_flags(check) & ML_TWOSIDED) )
	    continue;
	
	P_LineOpening (check);

	if (openrange <= 0)
	    continue;	// closed door
	
	if ( side_sector(sidenum_to_side(line_sidenum(check, 0))) == sec)
	    other = side_sector(sidenum_to_side(line_sidenum(check, 1)));
	else
	    other = side_sector(sidenum_to_side(line_sidenum(check, 0)));
	
	if (line_flags(check) & ML_SOUNDBLOCK)
	{
	    if (!soundblocks)
		P_RecursiveSound (other, 1);
	}
	else
	    P_RecursiveSound (other, soundblocks);
    }
}



//
// P_NoiseAlert
// If a monster yells at a player,
// it will alert other monsters to the player.
//
void
P_NoiseAlert
( mobj_t*	target,
  mobj_t*	emmiter )
{
    soundtarget = target;
    validcount++;
    sector_check_reset();
    P_RecursiveSound (mobj_sector(emmiter), 0);
}




//
// P_CheckMeleeRange
//
boolean P_CheckMeleeRange (mobj_t*	actor)
{
    mobj_t*	pl;
    fixed_t	dist;
	
    if (!mobj_full(actor)->sp_target)
	return false;
		
    pl = mobj_target(actor);
    dist = P_AproxDistance (pl->xy.x-actor->xy.x, pl->xy.y-actor->xy.y);

    if (dist >= MELEERANGE-20*FRACUNIT+mobj_info(pl)->radius)
	return false;
	
    if (! P_CheckSight (actor, pl))
	return false;
							
    return true;		
}

//
// P_CheckMissileRange
//
boolean P_CheckMissileRange (mobj_t* actor)
{
    fixed_t	dist;

    mobj_t*	pl = mobj_target(actor);
    if (! P_CheckSight (actor, pl) )
	return false;
	
    if ( actor->flags & MF_JUSTHIT )
    {
	// the target just hit the enemy,
	// so fight back!
	actor->flags &= ~MF_JUSTHIT;
	return true;
    }
	
    if (mobj_reactiontime(actor))
	return false;	// do not attack yet
		
    // OPTIMIZE: get this from a global checksight
    dist = P_AproxDistance ( actor->xy.x-pl->xy.x,
                             actor->xy.y - pl->xy.y) - 64 * FRACUNIT;
    
    if (!mobj_info(actor)->meleestate)
	dist -= 128*FRACUNIT;	// no melee attack, so fire more

    dist >>= FRACBITS;

    if (actor->type == MT_VILE)
    {
	if (dist > 14*64)	
	    return false;	// too far away
    }
	

    if (actor->type == MT_UNDEAD)
    {
	if (dist < 196)	
	    return false;	// close for fist attack
	dist >>= 1;
    }
	

    if (actor->type == MT_CYBORG
	|| actor->type == MT_SPIDER
	|| actor->type == MT_SKULL)
    {
	dist >>= 1;
    }
    
    if (dist > 200)
	dist = 200;
		
    if (actor->type == MT_CYBORG && dist > 160)
	dist = 160;
		
    if (P_Random () < dist)
	return false;
		
    return true;
}


//
// P_Move
// Move in the current direction,
// returns false if the move is blocked.
//
static const fixed_t	xspeed[8] = {FRACUNIT,47000,0,-47000,-FRACUNIT,-47000,0,47000};
static const fixed_t yspeed[8] = {0,47000,FRACUNIT,47000,0,-47000,-FRACUNIT,-47000};

boolean P_Move (mobj_t*	actor)
{
    fixed_t	tryx;
    fixed_t	tryy;
    
    line_t*	ld;
    
    // warning: 'catch', 'throw', and 'try'
    // are all C++ reserved words
    boolean	try_ok;
    boolean	good;
		
    if (mobj_full(actor)->movedir == DI_NODIR)
	return false;
		
    if ((unsigned)mobj_full(actor)->movedir >= 8)
	I_Error ("Weird actor->movedir!");
		
    tryx = actor->xy.x + mobj_speed(actor) * xspeed[mobj_full(actor)->movedir];
    tryy = actor->xy.y + mobj_speed(actor) * yspeed[mobj_full(actor)->movedir];

    try_ok = P_TryMove (actor, tryx, tryy);

    if (!try_ok)
    {
	// open any specials
	if (actor->flags & MF_FLOAT && floatok)
	{
	    // must adjust height
	    if (actor->z < tmfloorz)
		actor->z += FLOATSPEED;
	    else
		actor->z -= FLOATSPEED;

	    actor->flags |= MF_INFLOAT;
	    return true;
	}
		
	if (!numspechit)
	    return false;

        mobj_full(actor)->movedir = DI_NODIR;
	good = false;
	while (numspechit--)
	{
	    ld = spechit[numspechit];
	    // if the special is not a door
	    // that can be opened,
	    // return false
	    if (P_UseSpecialLine (actor, ld,0))
		good = true;
	}
	return good;
    }
    else
    {
	actor->flags &= ~MF_INFLOAT;
    }
	
	
    if (! (actor->flags & MF_FLOAT) )	
	actor->z = mobj_full(actor)->floorz;
    return true; 
}


//
// TryWalk
// Attempts to move actor on
// in its current (ob->moveangle) direction.
// If blocked by either a wall or an actor
// returns FALSE
// If move is either clear or blocked only by a door,
// returns TRUE and sets...
// If a door is in the way,
// an OpenDoor call is made to start it opening.
//
boolean P_TryWalk (mobj_t* actor)
{	
    if (!P_Move (actor))
    {
	return false;
    }

    mobj_full(actor)->movecount = P_Random()&15;
    return true;
}




void P_NewChaseDir (mobj_t*	actor)
{
    fixed_t	deltax;
    fixed_t	deltay;
    
    dirtype_t	d[3];
    
    int		tdir;
    dirtype_t	olddir;
    
    dirtype_t	turnaround;

    if (!mobj_full(actor)->sp_target)
	I_Error ("P_NewChaseDir: called with no target");
		
    olddir = mobj_full(actor)->movedir;
    turnaround=opposite[olddir];

    mobj_t *pl = mobj_target(actor);
    deltax = pl->xy.x - actor->xy.x;
    deltay = pl->xy.y - actor->xy.y;

    if (deltax>10*FRACUNIT)
	d[1]= DI_EAST;
    else if (deltax<-10*FRACUNIT)
	d[1]= DI_WEST;
    else
	d[1]=DI_NODIR;

    if (deltay<-10*FRACUNIT)
	d[2]= DI_SOUTH;
    else if (deltay>10*FRACUNIT)
	d[2]= DI_NORTH;
    else
	d[2]=DI_NODIR;

    // try direct route
    if (d[1] != DI_NODIR
	&& d[2] != DI_NODIR)
    {
	mobj_full(actor)->movedir = diags[((deltay<0)<<1)+(deltax>0)];
	if (mobj_full(actor)->movedir != (int) turnaround && P_TryWalk(actor))
	    return;
    }

    // try other directions
    if (P_Random() > 200
	||  abs(deltay)>abs(deltax))
    {
	tdir=d[1];
	d[1]=d[2];
	d[2]=tdir;
    }

    if (d[1]==turnaround)
	d[1]=DI_NODIR;
    if (d[2]==turnaround)
	d[2]=DI_NODIR;
	
    if (d[1]!=DI_NODIR)
    {
	mobj_full(actor)->movedir = d[1];
	if (P_TryWalk(actor))
	{
	    // either moved forward or attacked
	    return;
	}
    }

    if (d[2]!=DI_NODIR)
    {
	mobj_full(actor)->movedir =d[2];

	if (P_TryWalk(actor))
	    return;
    }

    // there is no direct path to the player,
    // so pick another direction.
    if (olddir!=DI_NODIR)
    {
	mobj_full(actor)->movedir =olddir;

	if (P_TryWalk(actor))
	    return;
    }

    // randomly determine direction of search
    if (P_Random()&1) 	
    {
	for ( tdir=DI_EAST;
	      tdir<=DI_SOUTHEAST;
	      tdir++ )
	{
	    if (tdir != (int) turnaround)
	    {
		mobj_full(actor)->movedir =tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }
    else
    {
	for ( tdir=DI_SOUTHEAST;
	      tdir != (DI_EAST-1);
	      tdir-- )
	{
	    if (tdir != (int) turnaround)
	    {
		mobj_full(actor)->movedir = tdir;
		
		if ( P_TryWalk(actor) )
		    return;
	    }
	}
    }

    if (turnaround !=  DI_NODIR)
    {
	mobj_full(actor)->movedir =turnaround;
	if ( P_TryWalk(actor) )
	    return;
    }

    mobj_full(actor)->movedir = DI_NODIR;	// can not move
}



//
// P_LookForPlayers
// If allaround is false, only look 180 degrees in front.
// Returns true if a player is targeted.
//
boolean
P_LookForPlayers
( mobj_t*	actor,
  boolean	allaround )
{
    int		c;
    int		stop;
    player_t*	player;
    angle_t	an;
    fixed_t	dist;

    c = 0;
    stop = (mobj_full(actor)->lastlook-1)&3;
	
    for ( ; ; mobj_full(actor)->lastlook = (mobj_full(actor)->lastlook+1)&3 )
    {
	if (!playeringame[mobj_full(actor)->lastlook])
	    continue;
			
	if (c++ == 2
	    || mobj_full(actor)->lastlook == stop)
	{
	    // done looking
	    return false;	
	}
	
	player = &players[mobj_full(actor)->lastlook];

	if (player->health <= 0)
	    continue;		// dead

	if (!P_CheckSight (actor, player->mo))
	    continue;		// out of sight
			
	if (!allaround)
	{
	    an = R_PointToAngle2 (actor->xy.x,
				  actor->xy.y,
				  player->mo->xy.x,
				  player->mo->xy.y)
		- mobj_full(actor)->angle;
	    
	    if (an > ANG90 && an < ANG270)
	    {
		dist = P_AproxDistance (player->mo->xy.x - actor->xy.x,
                                player->mo->xy.y - actor->xy.y);
		// if real close, react anyway
		if (dist > MELEERANGE)
		    continue;	// behind back
	    }
	}
		
	mobj_full(actor)->sp_target = mobj_to_shortptr(player->mo);
	return true;
    }

    return false;
}


//
// A_KeenDie
// DOOM II special, map 32.
// Uses special tag 666.
//
void A_KeenDie (mobj_t* mo)
{
    thinker_t*	th;
    mobj_t*	mo2;
    fake_line_t	junk;

    A_Fall (mo);
    
    // scan the remaining thinkers
    // to see if all Keens are dead
    for (th = thinker_next(&thinkercap) ; th != &thinkercap ; th=thinker_next(th))
    {
	if (th->function != ThinkF_P_MobjThinker)
	    continue;

	mo2 = (mobj_t *)th;
	if (mo2 != mo
	    && mo2->type == mo->type
	    && mobj_full(mo2)->health > 0)
	{
	    // other Keen not dead
	    return;		
	}
    }

    init_fake_line(&junk, TAG_666);
    EV_DoDoor(fakeline_to_line(&junk), vld_open);
}


//
// ACTION ROUTINES
//

//
// A_Look
// Stay in state until a player is sighted.
//
void A_Look (mobj_t* actor)
{
    mobj_t*	targ;
	
    mobj_full(actor)->threshold = 0;	// any shot will wake up
    targ = shortptr_to_mobj(mobj_sector(actor)->soundtarget);

    if (targ
	&& (targ->flags & MF_SHOOTABLE) )
    {
	mobj_full(actor)->sp_target = mobj_to_shortptr(targ);

	if ( actor->flags & MF_AMBUSH )
	{
	    if (P_CheckSight (actor, mobj_target(actor)))
		goto seeyou;
	}
	else
	    goto seeyou;
    }
	
	
    if (!P_LookForPlayers (actor, false) )
	return;
		
    // go into chase state
  seeyou:
    if (mobj_info(actor)->seesound)
    {
	int		sound;
		
	switch (mobj_info(actor)->seesound)
	{
	  case sfx_posit1:
	  case sfx_posit2:
	  case sfx_posit3:
	    sound = sfx_posit1+P_Random()%3;
	    break;

	  case sfx_bgsit1:
	  case sfx_bgsit2:
	    sound = sfx_bgsit1+P_Random()%2;
	    break;

	  default:
	    sound = mobj_info(actor)->seesound;
	    break;
	}

	if (actor->type==MT_SPIDER
	    || actor->type == MT_CYBORG)
	{
	    // full volume
	    S_StartSound (NULL, sound);
	}
	else
	    S_StartObjSound (actor, sound);
    }

    P_SetMobjState (actor, mobj_info(actor)->seestate);
}


//
// A_Chase
// Actor has a melee attack,
// so it tries to close as fast as possible
//
void A_Chase (mobj_t*	actor)
{
    int		delta;

    if (mobj_reactiontime(actor))
	mobj_reactiontime(actor)--;
				

    // modify target threshold
    if  (mobj_full(actor)->threshold)
    {
	if (!mobj_full(actor)->sp_target
	    || mobj_full(mobj_target(actor))->health <= 0)
	{
	    mobj_full(actor)->threshold = 0;
	}
	else
	    mobj_full(actor)->threshold--;
    }
    
    // turn towards movement direction if not there yet
    if (mobj_full(actor)->movedir < 8)
    {
	mobj_full(actor)->angle &= (7<<29);
	delta = mobj_full(actor)->angle - (mobj_full(actor)->movedir << 29);
	
	if (delta > 0)
	    mobj_full(actor)->angle -= ANG90/2;
	else if (delta < 0)
	    mobj_full(actor)->angle += ANG90/2;
    }

    if (!mobj_full(actor)->sp_target
	|| !(mobj_target(actor)->flags&MF_SHOOTABLE))
    {
	// look for a new target
	if (P_LookForPlayers(actor,true))
	    return; 	// got a new target
	
	P_SetMobjState (actor, mobj_info(actor)->spawnstate);
	return;
    }
    
    // do not attack twice in a row
    if (actor->flags & MF_JUSTATTACKED)
    {
	actor->flags &= ~MF_JUSTATTACKED;
	if (gameskill != sk_nightmare && !fastparm)
	    P_NewChaseDir (actor);
	return;
    }
    
    // check for melee attack
    if (mobj_info(actor)->meleestate
	&& P_CheckMeleeRange (actor))
    {
	if (mobj_info(actor)->attacksound)
	    S_StartObjSound (actor, mobj_info(actor)->attacksound);

	P_SetMobjState (actor, mobj_info(actor)->meleestate);
	return;
    }
    
    // check for missile attack
    if (mobj_info(actor)->missilestate)
    {
	if (gameskill < sk_nightmare
	    && !fastparm && mobj_full(actor)->movecount)
	{
	    goto nomissile;
	}
	
	if (!P_CheckMissileRange (actor))
	    goto nomissile;
	
	P_SetMobjState (actor, mobj_info(actor)->missilestate);
	actor->flags |= MF_JUSTATTACKED;
	return;
    }

    // ?
  nomissile:
    // possibly choose another target
    if (netgame
	&& !mobj_full(actor)->threshold
	&& !P_CheckSight (actor, mobj_target(actor)) )
    {
	if (P_LookForPlayers(actor,true))
	    return;	// got a new target
    }
    
    // chase towards player
    if (--mobj_full(actor)->movecount<0
	|| !P_Move (actor))
    {
	P_NewChaseDir (actor);
    }
    
    // make active sound
    if (mobj_info(actor)->activesound
	&& P_Random () < 3)
    {
	S_StartObjSound (actor, mobj_info(actor)->activesound);
    }
}


//
// A_FaceTarget
//
void A_FaceTarget (mobj_t* actor)
{	
    if (!mobj_full(actor)->sp_target)
	return;
    
    mobj_t *target = mobj_target(actor);
    
    actor->flags &= ~MF_AMBUSH;
	
    mobj_full(actor)->angle = R_PointToAngle2 (actor->xy.x,
				    actor->xy.y,
				    target->xy.x,
				    target->xy.y);
    
    if (target->flags & MF_SHADOW)
	mobj_full(actor)->angle += P_SubRandom() << 21;
}


//
// A_PosAttack
//
void A_PosAttack (mobj_t* actor)
{
    int		angle;
    int		damage;
    int		slope;
	
    if (!mobj_full(actor)->sp_target)
	return;
		
    A_FaceTarget (actor);
    angle = mobj_full(actor)->angle;
    slope = P_AimLineAttack (actor, angle, MISSILERANGE);

    S_StartObjSound (actor, sfx_pistol);
    angle += P_SubRandom() << 20;
    damage = ((P_Random()%5)+1)*3;
    P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void A_SPosAttack (mobj_t* actor)
{
    int		i;
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!mobj_full(actor)->sp_target)
	return;

    S_StartObjSound (actor, sfx_shotgn);
    A_FaceTarget (actor);
    bangle = mobj_full(actor)->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

    for (i=0 ; i<3 ; i++)
    {
	angle = bangle + (P_SubRandom() << 20);
	damage = ((P_Random()%5)+1)*3;
	P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
    }
}

void A_CPosAttack (mobj_t* actor)
{
    int		angle;
    int		bangle;
    int		damage;
    int		slope;
	
    if (!mobj_full(actor)->sp_target)
	return;

    S_StartObjSound (actor, sfx_shotgn);
    A_FaceTarget (actor);
    bangle = mobj_full(actor)->angle;
    slope = P_AimLineAttack (actor, bangle, MISSILERANGE);

    angle = bangle + (P_SubRandom() << 20);
    damage = ((P_Random()%5)+1)*3;
    P_LineAttack (actor, angle, MISSILERANGE, slope, damage);
}

void A_CPosRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 40)
	return;

    if (!mobj_full(actor)->sp_target
	|| mobj_full(mobj_target(actor))->health <= 0
	|| !P_CheckSight (actor, mobj_target(actor)) )
    {
	P_SetMobjState (actor, mobj_info(actor)->seestate);
    }
}


void A_SpidRefire (mobj_t* actor)
{	
    // keep firing unless target got out of sight
    A_FaceTarget (actor);

    if (P_Random () < 10)
	return;

    if (!mobj_full(actor)->sp_target
	|| mobj_full(mobj_target(actor))->health <= 0
	|| !P_CheckSight (actor, mobj_target(actor)) )
    {
	P_SetMobjState (actor, mobj_info(actor)->seestate);
    }
}

void A_BspiAttack (mobj_t *actor)
{	
    if (!mobj_full(actor)->sp_target)
	return;
		
    A_FaceTarget (actor);

    // launch a missile
    P_SpawnMissile (actor, mobj_target(actor), MT_ARACHPLAZ);
}


//
// A_TroopAttack
//
void A_TroopAttack (mobj_t* actor)
{
    int		damage;
	
    if (!mobj_full(actor)->sp_target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	S_StartObjSound (actor, sfx_claw);
	damage = (P_Random()%8+1)*3;
	P_DamageMobj (mobj_target(actor), actor, actor, damage);
	return;
    }

    
    // launch a missile
    P_SpawnMissile (actor, mobj_target(actor), MT_TROOPSHOT);
}


void A_SargAttack (mobj_t* actor)
{
    int		damage;

    if (!mobj_full(actor)->sp_target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	damage = ((P_Random()%10)+1)*4;
	P_DamageMobj (mobj_target(actor), actor, actor, damage);
    }
}

void A_HeadAttack (mobj_t* actor)
{
    int		damage;
	
    if (!mobj_full(actor)->sp_target)
	return;
		
    A_FaceTarget (actor);
    if (P_CheckMeleeRange (actor))
    {
	damage = (P_Random()%6+1)*10;
	P_DamageMobj (mobj_target(actor), actor, actor, damage);
	return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, mobj_target(actor), MT_HEADSHOT);
}

void A_CyberAttack (mobj_t* actor)
{	
    if (!mobj_full(actor)->sp_target)
	return;
		
    A_FaceTarget (actor);
    P_SpawnMissile (actor, mobj_target(actor), MT_ROCKET);
}


void A_BruisAttack (mobj_t* actor)
{
    int		damage;
	
    if (!mobj_full(actor)->sp_target)
	return;
		
    if (P_CheckMeleeRange (actor))
    {
	S_StartObjSound (actor, sfx_claw);
	damage = (P_Random()%8+1)*10;
	P_DamageMobj (mobj_target(actor), actor, actor, damage);
	return;
    }
    
    // launch a missile
    P_SpawnMissile (actor, mobj_target(actor), MT_BRUISERSHOT);
}


//
// A_SkelMissile
//
void A_SkelMissile (mobj_t* actor)
{	
    mobj_t*	mo;
	
    if (!mobj_full(actor)->sp_target)
	return;
		
    A_FaceTarget (actor);
    actor->z += 16*FRACUNIT;	// so missile spawns higher
    mo = P_SpawnMissile (actor, mobj_target(actor), MT_TRACER);
    actor->z -= 16*FRACUNIT;	// back to normal

    mo->xy.x += mobj_full(mo)->momx;
    mo->xy.y += mobj_full(mo)->momy;
    mobj_full(mo)->sp_tracer = mobj_full(actor)->sp_target;
}

//int	TRACEANGLE = 0xc000000;
#define TRACEANGLE 0xc000000

void A_Tracer (mobj_t* actor)
{
    angle_t	exact;
    fixed_t	dist;
    fixed_t	slope;
    mobj_t*	dest;
    mobj_t*	th;
		
    if (gametic & 3)
	return;
    
    // spawn a puff of smoke behind the rocket		
    P_SpawnPuff (actor->xy.x, actor->xy.y, actor->z);
	
    th = P_SpawnMobj (actor->xy.x - mobj_full(actor)->momx,
                      actor->xy.y - mobj_full(actor)->momy,
		      actor->z, MT_SMOKE);

    mobj_full(th)->momz = FRACUNIT;
    th->tics -= P_Random()&3;
    if (th->tics < 1)
	th->tics = 1;
    
    // adjust direction
    dest = mobj_tracer(actor);
	
    if (!dest || mobj_full(dest)->health <= 0)
	return;
    
    // change angle	
    exact = R_PointToAngle2 (actor->xy.x,
			     actor->xy.y,
			     dest->xy.x,
			     dest->xy.y);

    if (exact != mobj_full(actor)->angle)
    {
	if (exact - mobj_full(actor)->angle > 0x80000000)
	{
            mobj_full(actor)->angle -= TRACEANGLE;
	    if (exact - mobj_full(actor)->angle < 0x80000000)
                mobj_full(actor)->angle = exact;
	}
	else
	{
            mobj_full(actor)->angle += TRACEANGLE;
	    if (exact - mobj_full(actor)->angle > 0x80000000)
                mobj_full(actor)->angle = exact;
	}
    }
	
    exact = mobj_full(actor)->angle>>ANGLETOFINESHIFT;
    int speed = mobj_speed(actor);
    mobj_full(actor)->momx = FixedMul (speed, finecosine(exact));
    mobj_full(actor)->momy = FixedMul (speed, finesine(exact));
    
    // change slope
    dist = P_AproxDistance (dest->xy.x - actor->xy.x,
                            dest->xy.y - actor->xy.y);
    
    dist = dist / speed;

    if (dist < 1)
	dist = 1;
    slope = (dest->z+40*FRACUNIT - actor->z) / dist;

    if (slope < mobj_full(actor)->momz)
        mobj_full(actor)->momz -= FRACUNIT/8;
    else
        mobj_full(actor)->momz += FRACUNIT/8;
}


void A_SkelWhoosh (mobj_t*	actor)
{
    if (!mobj_full(actor)->sp_target)
	return;
    A_FaceTarget (actor);
    S_StartObjSound (actor,sfx_skeswg);
}

void A_SkelFist (mobj_t*	actor)
{
    int		damage;

    if (!mobj_full(actor)->sp_target)
	return;
		
    A_FaceTarget (actor);
	
    if (P_CheckMeleeRange (actor))
    {
	damage = ((P_Random()%10)+1)*6;
	S_StartObjSound (actor, sfx_skepch);
	P_DamageMobj (mobj_target(actor), actor, actor, damage);
    }
}



//
// PIT_VileCheck
// Detect a corpse that could be raised.
//
mobj_t*		corpsehit;
mobj_t*		vileobj;
fixed_t		viletryx;
fixed_t		viletryy;

boolean PIT_VileCheck (mobj_t*	thing)
{
    int		maxdist;
    boolean	check;
	
    if (!(thing->flags & MF_CORPSE) )
	return true;	// not a monster
    
    if (thing->tics != -1)
	return true;	// not lying still yet
    
    if (mobj_info(thing)->raisestate == S_NULL)
	return true;	// monster doesn't have a raise state
    
    maxdist = mobj_info(thing)->radius + mobjinfo[MT_VILE].radius;
	
    if (abs(thing->xy.x - viletryx) > maxdist
	 || abs(thing->xy.y - viletryy) > maxdist )
	return true;		// not actually touching
		
    corpsehit = thing;
    mobj_full(corpsehit)->momx = mobj_full(corpsehit)->momy = 0;
    mobj_full(corpsehit)->height <<= 2;
    check = P_CheckPosition (corpsehit, corpsehit->xy.x, corpsehit->xy.y);
    mobj_full(corpsehit)->height >>= 2;

    if (!check)
	return true;		// doesn't fit here
		
    return false;		// got one, so stop checking
}



//
// A_VileChase
// Check for ressurecting a body
//
void A_VileChase (mobj_t* actor)
{
    int			xl;
    int			xh;
    int			yl;
    int			yh;
    
    int			bx;
    int			by;

    const mobjinfo_t*		info;
    shortptr_t		temp;
	
    if (mobj_full(actor)->movedir != DI_NODIR)
    {
        int speed = mobj_speed(actor);
	// check for corpses to raise
	viletryx =
            actor->xy.x + speed * xspeed[mobj_full(actor)->movedir];
	viletryy =
            actor->xy.y + speed * yspeed[mobj_full(actor)->movedir];

	xl = (viletryx - bmaporgx - MAXRADIUS*2)>>MAPBLOCKSHIFT;
	xh = (viletryx - bmaporgx + MAXRADIUS*2)>>MAPBLOCKSHIFT;
	yl = (viletryy - bmaporgy - MAXRADIUS*2)>>MAPBLOCKSHIFT;
	yh = (viletryy - bmaporgy + MAXRADIUS*2)>>MAPBLOCKSHIFT;
	
	vileobj = actor;
	for (bx=xl ; bx<=xh ; bx++)
	{
	    for (by=yl ; by<=yh ; by++)
	    {
		// Call PIT_VileCheck to check
		// whether object is a corpse
		// that canbe raised.
		if (!P_BlockThingsIterator(bx,by,PIT_VileCheck))
		{
		    // got one!
		    temp = mobj_full(actor)->sp_target;
                    mobj_full(actor)->sp_target = mobj_to_shortptr(corpsehit);
		    A_FaceTarget (actor);
                    mobj_full(actor)->sp_target = temp;
					
		    P_SetMobjState (actor, S_VILE_HEAL1);
		    S_StartObjSound (corpsehit, sfx_slop);
		    info = mobj_info(corpsehit);
		    
		    P_SetMobjState (corpsehit,info->raisestate);
                    mobj_full(corpsehit)->height <<= 2;
		    corpsehit->flags = info->flags;
                    mobj_full(corpsehit)->health = info->spawnhealth;
                    mobj_full(corpsehit)->sp_target = 0;

		    return;
		}
	    }
	}
    }

    // Return to normal attack.
    A_Chase (actor);
}


//
// A_VileStart
//
void A_VileStart (mobj_t* actor)
{
    S_StartObjSound (actor, sfx_vilatk);
}


//
// A_Fire
// Keep fire in front of player unless out of sight
//
void A_Fire (mobj_t* actor);

void A_StartFire (mobj_t* actor)
{
    S_StartObjSound(actor,sfx_flamst);
    A_Fire(actor);
}

void A_FireCrackle (mobj_t* actor)
{
    S_StartObjSound(actor,sfx_flame);
    A_Fire(actor);
}

void A_Fire (mobj_t* actor)
{
    mobj_t*	dest;
    mobj_t*     target;
    unsigned	an;
		
    dest = mobj_tracer(actor);
    if (!dest)
	return;

    target = P_SubstNullMobj(mobj_target(actor));
		
    // don't move it if the vile lost sight
    if (!P_CheckSight (target, dest) )
	return;

    an = mobj_full(dest)->angle >> ANGLETOFINESHIFT;

    P_ResetThingPosition (actor, dest->xy.x + FixedMul (24 * FRACUNIT, finecosine(an)), dest->xy.y + FixedMul (24 * FRACUNIT, finesine(an)));
    actor->z = dest->z;
}



//
// A_VileTarget
// Spawn the hellfire
//
void A_VileTarget (mobj_t*	actor)
{
    mobj_t*	fog;
	
    if (!mobj_full(actor)->sp_target)
	return;

    A_FaceTarget (actor);

    mobj_t *target = mobj_target(actor);
    fog = P_SpawnMobj (target->xy.x,
		       target->xy.x,
		       target->z, MT_FIRE);

    mobj_full(actor)->sp_tracer = mobj_to_shortptr(fog);
    mobj_full(fog)->sp_target = mobj_to_shortptr(actor);
    mobj_full(fog)->sp_tracer = mobj_to_shortptr(target);
    A_Fire (fog);
}




//
// A_VileAttack
//
void A_VileAttack (mobj_t* actor)
{	
    mobj_t*	fire;
    int		an;
	
    if (!mobj_full(actor)->sp_target)
	return;
    
    A_FaceTarget (actor);

    mobj_t *target = mobj_target(actor);
    if (!P_CheckSight (actor, target) )
	return;

    S_StartObjSound (actor, sfx_barexp);
    P_DamageMobj (target, actor, actor, 20);
    mobj_full(target)->momz = 1000*FRACUNIT/mobj_info(target)->mass;
	
    an = mobj_full(actor)->angle >> ANGLETOFINESHIFT;

    fire = mobj_tracer(actor);

    if (!fire)
	return;
		
    // move the fire between the vile and the player
    fire->xy.x = target->xy.x - FixedMul (24 * FRACUNIT, finecosine(an));
    fire->xy.y = target->xy.y - FixedMul (24 * FRACUNIT, finesine(an));
    P_RadiusAttack (fire, actor, 70 );
}




//
// Mancubus attack,
// firing three missiles (bruisers)
// in three different directions?
// Doesn't look like it. 
//
#define	FATSPREAD	(ANG90/8)

void A_FatRaise (mobj_t *actor)
{
    A_FaceTarget (actor);
    S_StartObjSound (actor, sfx_manatk);
}


void A_FatAttack1 (mobj_t* actor)
{
    mobj_t*	mo;
    mobj_t*     target;
    int		an;

    A_FaceTarget (actor);

    // Change direction  to ...
    mobj_full(actor)->angle += FATSPREAD;
    target = P_SubstNullMobj(mobj_target(actor));
    P_SpawnMissile (actor, target, MT_FATSHOT);

    mo = P_SpawnMissile (actor, target, MT_FATSHOT);
    mobj_full(mo)->angle += FATSPREAD;
    an = mobj_full(mo)->angle >> ANGLETOFINESHIFT;
    int speed = mobj_speed(mo);
    mobj_full(mo)->momx = FixedMul (speed, finecosine(an));
    mobj_full(mo)->momy = FixedMul (speed, finesine(an));
}

void A_FatAttack2 (mobj_t* actor)
{
    mobj_t*	mo;
    mobj_t*     target;
    int		an;

    A_FaceTarget (actor);
    // Now here choose opposite deviation.
    mobj_full(actor)->angle -= FATSPREAD;
    target = P_SubstNullMobj(mobj_target(actor));
    P_SpawnMissile (actor, target, MT_FATSHOT);

    mo = P_SpawnMissile (actor, target, MT_FATSHOT);
    mobj_full(mo)->angle -= FATSPREAD*2;
    an = mobj_full(mo)->angle >> ANGLETOFINESHIFT;
    int speed = mobj_speed(mo);
    mobj_full(mo)->momx = FixedMul (speed, finecosine(an));
    mobj_full(mo)->momy = FixedMul (speed, finesine(an));
}

void A_FatAttack3 (mobj_t*	actor)
{
    mobj_t*	mo;
    mobj_t*     target;
    int		an;

    A_FaceTarget (actor);

    target = P_SubstNullMobj(mobj_target(actor));
    
    mo = P_SpawnMissile (actor, target, MT_FATSHOT);
    mobj_full(mo)->angle -= FATSPREAD/2;
    an = mobj_full(mo)->angle >> ANGLETOFINESHIFT;
    int speed = mobj_speed(mo);
    mobj_full(mo)->momx = FixedMul (speed, finecosine(an));
    mobj_full(mo)->momy = FixedMul (speed, finesine(an));

    mo = P_SpawnMissile (actor, target, MT_FATSHOT);
    mobj_full(mo)->angle += FATSPREAD/2;
    speed = mobj_speed(mo);
    an = mobj_full(mo)->angle >> ANGLETOFINESHIFT;
    mobj_full(mo)->momx = FixedMul (speed, finecosine(an));
    mobj_full(mo)->momy = FixedMul (speed, finesine(an));
}


//
// SkullAttack
// Fly at the player like a missile.
//
#define	SKULLSPEED		(20*FRACUNIT)

void A_SkullAttack (mobj_t* actor)
{
    mobj_t*		dest;
    angle_t		an;
    int			dist;

    if (!mobj_full(actor)->sp_target)
	return;
		
    dest = mobj_target(actor);
    actor->flags |= MF_SKULLFLY;

    S_StartObjSound (actor, mobj_info(actor)->attacksound);
    A_FaceTarget (actor);
    an = mobj_full(actor)->angle >> ANGLETOFINESHIFT;
    mobj_full(actor)->momx = FixedMul (SKULLSPEED, finecosine(an));
    mobj_full(actor)->momy = FixedMul (SKULLSPEED, finesine(an));
    dist = P_AproxDistance (dest->xy.x - actor->xy.x, dest->xy.y - actor->xy.y);
    dist = dist / SKULLSPEED;
    
    if (dist < 1)
	dist = 1;
    mobj_full(actor)->momz = (dest->z+(mobj_height(dest)>>1) - actor->z) / dist;
}


//
// A_PainShootSkull
// Spawn a lost soul and launch it at the target
//
void
A_PainShootSkull
( mobj_t*	actor,
  angle_t	angle )
{
    fixed_t	x;
    fixed_t	y;
    fixed_t	z;
    
    mobj_t*	newmobj;
    angle_t	an;
    int		prestep;
    int		count;
    thinker_t*	currentthinker;

    // count total number of skull currently on the level
    count = 0;

    currentthinker = thinker_next(&thinkercap);
    while (currentthinker != &thinkercap)
    {
	if (   (currentthinker->function == ThinkF_P_MobjThinker)
	    && ((mobj_t *)currentthinker)->type == MT_SKULL)
	    count++;
	currentthinker = thinker_next(currentthinker);
    }

    // if there are allready 20 skulls on the level,
    // don't spit another one
    if (count > 20)
	return;


    // okay, there's playe for another one
    an = angle >> ANGLETOFINESHIFT;
    
    prestep =
	4*FRACUNIT
	+ 3*(mobj_info(actor)->radius + mobjinfo[MT_SKULL].radius)/2;
    
    x = actor->xy.x + FixedMul (prestep, finecosine(an));
    y = actor->xy.y + FixedMul (prestep, finesine(an));
    z = actor->z + 8*FRACUNIT;
		
    newmobj = P_SpawnMobj (x , y, z, MT_SKULL);

    // Check for movements.
    if (!P_TryMove (newmobj, newmobj->xy.x, newmobj->xy.y))
    {
	// kill it immediately
	P_DamageMobj (newmobj,actor,actor,10000);	
	return;
    }
		
    mobj_full(newmobj)->sp_target = mobj_full(actor)->sp_target;
    A_SkullAttack (newmobj);
}


//
// A_PainAttack
// Spawn a lost soul and launch it at the target
// 
void A_PainAttack (mobj_t* actor)
{
    if (!mobj_full(actor)->sp_target)
	return;

    A_FaceTarget (actor);
    A_PainShootSkull (actor, mobj_full(actor)->angle);
}


void A_PainDie (mobj_t* actor)
{
    A_Fall (actor);
    A_PainShootSkull (actor, mobj_full(actor)->angle+ANG90);
    A_PainShootSkull (actor, mobj_full(actor)->angle+ANG180);
    A_PainShootSkull (actor, mobj_full(actor)->angle+ANG270);
}






void A_Scream (mobj_t* actor)
{
    int		sound;
	
    switch (mobj_info(actor)->deathsound)
    {
      case 0:
	return;
		
      case sfx_podth1:
      case sfx_podth2:
      case sfx_podth3:
	sound = sfx_podth1 + P_Random ()%3;
	break;
		
      case sfx_bgdth1:
      case sfx_bgdth2:
	sound = sfx_bgdth1 + P_Random ()%2;
	break;
	
      default:
	sound = mobj_info(actor)->deathsound;
	break;
    }

    // Check for bosses.
    if (actor->type==MT_SPIDER
	|| actor->type == MT_CYBORG)
    {
	// full volume
	S_StartSound (NULL, sound);
    }
    else
	S_StartObjSound (actor, sound);
}


void A_XScream (mobj_t* actor)
{
    S_StartObjSound (actor, sfx_slop);
}

void A_Pain (mobj_t* actor)
{
    if (mobj_info(actor)->painsound)
	S_StartObjSound (actor, mobj_info(actor)->painsound);
}



void A_Fall (mobj_t *actor)
{
    // actor is on ground, it can be walked over
    actor->flags &= ~MF_SOLID;

    // So change this if corpse objects
    // are meant to be obstacles.
}


//
// A_Explode
//
void A_Explode (mobj_t* thingy)
{
    P_RadiusAttack(thingy, mobj_target(thingy), 128);
}

// Check whether the death of the specified monster type is allowed
// to trigger the end of episode special action.
//
// This behavior changed in v1.9, the most notable effect of which
// was to break uac_dead.wad

static boolean CheckBossEnd(mobjtype_t motype)
{
    if (gameversion < exe_ultimate)
    {
        if (gamemap != 8)
        {
            return false;
        }

        // Baron death on later episodes is nothing special.

        if (motype == MT_BRUISER && gameepisode != 1)
        {
            return false;
        }

        return true;
    }
    else
    {
        // New logic that appeared in Ultimate Doom.
        // Looks like the logic was overhauled while adding in the
        // episode 4 support.  Now bosses only trigger on their
        // specific episode.

	switch(gameepisode)
	{
            case 1:
                return gamemap == 8 && motype == MT_BRUISER;

            case 2:
                return gamemap == 8 && motype == MT_CYBORG;

            case 3:
                return gamemap == 8 && motype == MT_SPIDER;

	    case 4:
                return (gamemap == 6 && motype == MT_CYBORG)
                    || (gamemap == 8 && motype == MT_SPIDER);

            default:
                return gamemap == 8;
	}
    }
}

//
// A_BossDeath
// Possibly trigger special effects
// if on first boss level
//
void A_BossDeath (mobj_t* mo)
{
    thinker_t*	th;
    mobj_t*	mo2;
    fake_line_t	junk;
    int		i;
		
    if ( gamemode == commercial)
    {
	if (gamemap != 7)
	    return;
		
	if ((mo->type != MT_FATSO)
	    && (mo->type != MT_BABY))
	    return;
    }
    else
    {
        if (!CheckBossEnd(mo->type))
        {
            return;
        }
    }

    // make sure there is a player alive for victory
    for (i=0 ; i<MAXPLAYERS ; i++)
	if (playeringame[i] && players[i].health > 0)
	    break;
    
    if (i==MAXPLAYERS)
	return;	// no one left alive, so do not end game
    
    // scan the remaining thinkers to see
    // if all bosses are dead
    for (th = thinker_next(&thinkercap) ; th != &thinkercap ; th=thinker_next(th))
    {
	if (th->function != ThinkF_P_MobjThinker)
	    continue;
	
	mo2 = (mobj_t *)th;
	if (mo2 != mo
	    && mo2->type == mo->type
	    && mobj_full(mo2)->health > 0)
	{
	    // other boss not dead
	    return;
	}
    }
	
    // victory!
    if ( gamemode == commercial)
    {
	if (gamemap == 7)
	{
	    if (mo->type == MT_FATSO)
	    {
                init_fake_line(&junk, TAG_666);
		EV_DoFloor(fakeline_to_line(&junk),lowerFloorToLowest);
		return;
	    }
	    
	    if (mo->type == MT_BABY)
	    {
	        init_fake_line(&junk, TAG_667);
		EV_DoFloor(fakeline_to_line(&junk),raiseToTexture);
		return;
	    }
	}
    }
    else
    {
	switch(gameepisode)
	{
	  case 1:
            init_fake_line(&junk, TAG_666);
	    EV_DoFloor (fakeline_to_line(&junk), lowerFloorToLowest);
	    return;
	    break;
	    
	  case 4:
	    switch(gamemap)
	    {
	      case 6:
               init_fake_line(&junk, TAG_666);
		EV_DoDoor (fakeline_to_line(&junk), vld_blazeOpen);
		return;
		break;
		
	      case 8:
                init_fake_line(&junk, TAG_666);
		EV_DoFloor (fakeline_to_line(&junk), lowerFloorToLowest);
		return;
		break;
	    }
	}
    }
	
    G_ExitLevel ();
}


void A_Hoof (mobj_t* mo)
{
    S_StartObjSound (mo, sfx_hoof);
    A_Chase (mo);
}

void A_Metal (mobj_t* mo)
{
    S_StartObjSound (mo, sfx_metal);
    A_Chase (mo);
}

void A_BabyMetal (mobj_t* mo)
{
    S_StartObjSound (mo, sfx_bspwlk);
    A_Chase (mo);
}

void
A_OpenShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartObjSound (player->mo, sfx_dbopn);
}

void
A_LoadShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartObjSound (player->mo, sfx_dbload);
}

void
A_ReFire
( player_t*	player,
  pspdef_t*	psp );

void
A_CloseShotgun2
( player_t*	player,
  pspdef_t*	psp )
{
    S_StartObjSound (player->mo, sfx_dbcls);
    A_ReFire(player,psp);
}



static shortptr_t /*mobj_t **/	braintargets[32];
static isb_int8_t 	numbraintargets;
static isb_int8_t 	braintargeton = 0;

void A_BrainAwake (mobj_t* mo)
{
    thinker_t*	thinker;
    mobj_t*	m;
	
    // find all the target spots
    numbraintargets = 0;
    braintargeton = 0;

    for (thinker = thinker_next(&thinkercap) ;
	 thinker != &thinkercap ;
	 thinker = thinker_next(thinker))
    {
	if (thinker->function != ThinkF_P_MobjThinker)
	    continue;	// not a mobj

	m = (mobj_t *)thinker;

	if (m->type == MT_BOSSTARGET )
	{
	    braintargets[numbraintargets] = mobj_to_shortptr(m);
	    numbraintargets++;
	}
    }
	
    S_StartSound (NULL,sfx_bossit);
}


void A_BrainPain (mobj_t*	mo)
{
    S_StartSound (NULL,sfx_bospn);
}


void A_BrainScream (mobj_t*	mo)
{
    int		x;
    int		y;
    int		z;
    mobj_t*	th;
	
    for (x= mo->xy.x - 196 * FRACUNIT ; x < mo->xy.x + 320 * FRACUNIT ; x+= FRACUNIT * 8)
    {
	y = mo->xy.y - 320 * FRACUNIT;
	z = 128 + P_Random()*2*FRACUNIT;
	th = P_SpawnMobj (x,y,z, MT_ROCKET);
	mobj_full(th)->momz = P_Random()*512;

	P_SetMobjState (th, S_BRAINEXPLODE1);

	th->tics -= P_Random()&7;
	if (th->tics < 1)
	    th->tics = 1;
    }
	
    S_StartSound (NULL,sfx_bosdth);
}



void A_BrainExplode (mobj_t* mo)
{
    int		x;
    int		y;
    int		z;
    mobj_t*	th;
	
    x = mo->xy.x + P_SubRandom() * 2048;
    y = mo->xy.y;
    z = 128 + P_Random()*2*FRACUNIT;
    th = P_SpawnMobj (x,y,z, MT_ROCKET);
    mobj_full(th)->momz = P_Random()*512;

    P_SetMobjState (th, S_BRAINEXPLODE1);

    th->tics -= P_Random()&7;
    if (th->tics < 1)
	th->tics = 1;
}


void A_BrainDie (mobj_t*	mo)
{
    G_ExitLevel ();
}

void A_BrainSpit (mobj_t*	mo)
{
    mobj_t*	targ;
    mobj_t*	newmobj;
    
    static int	easy = 0;
	
    easy ^= 1;
    if (gameskill <= sk_easy && (!easy))
	return;
		
    // shoot a cube at current target
    targ = shortptr_to_mobj(braintargets[braintargeton]);
    if (numbraintargets == 0)
    {
        I_Error("A_BrainSpit: numbraintargets was 0 (vanilla crashes here)");
    }
    braintargeton = (isb_int8_t)((braintargeton+1)%numbraintargets);

    // spawn brain missile
    newmobj = P_SpawnMissile (mo, targ, MT_SPAWNSHOT);
    mobj_full(newmobj)->sp_target = mobj_to_shortptr(targ);
    int reactiontime =((targ->xy.y - mo->xy.y) / mobj_full(newmobj)->momy) / state_tics(mobj_state(newmobj));
#if DOOM_SMALL && !PICO_ON_DEVICE
    // todo graham no idea if this can happen
    if (reactiontime > 255) {
        printf("WARNING: reaction time > 255"); // this is doom II onyl anyway
        reactiontime = 255;
    }
#endif
    mobj_reactiontime(newmobj) = reactiontime;

    S_StartUnpositionedSound( sfx_bospit);
}



void A_SpawnFly (mobj_t* mo);

// travelling cube sound
void A_SpawnSound (mobj_t* mo)	
{
    S_StartObjSound (mo,sfx_boscub);
    A_SpawnFly(mo);
}

void A_SpawnFly (mobj_t* mo)
{
    mobj_t*	newmobj;
    mobj_t*	fog;
    mobj_t*	targ;
    int		r;
    mobjtype_t	type;
	
    if (--mobj_reactiontime(mo))
	return;	// still flying
	
    targ = P_SubstNullMobj(mobj_target(mo));

    // First spawn teleport fog.
    fog = P_SpawnMobj (targ->xy.x, targ->xy.y, targ->z, MT_SPAWNFIRE);
    S_StartObjSound (fog, sfx_telept);

    // Randomly select monster to spawn.
    r = P_Random ();

    // Probability distribution (kind of :),
    // decreasing likelihood.
    if ( r<50 )
	type = MT_TROOP;
    else if (r<90)
	type = MT_SERGEANT;
    else if (r<120)
	type = MT_SHADOWS;
    else if (r<130)
	type = MT_PAIN;
    else if (r<160)
	type = MT_HEAD;
    else if (r<162)
	type = MT_VILE;
    else if (r<172)
	type = MT_UNDEAD;
    else if (r<192)
	type = MT_BABY;
    else if (r<222)
	type = MT_FATSO;
    else if (r<246)
	type = MT_KNIGHT;
    else
	type = MT_BRUISER;		

    newmobj	= P_SpawnMobj (targ->xy.x, targ->xy.y, targ->z, type);
    if (P_LookForPlayers (newmobj, true) )
	P_SetMobjState (newmobj, mobj_info(newmobj)->seestate);
	
    // telefrag anything in this spot
    P_TeleportMove (newmobj, newmobj->xy.x, newmobj->xy.y);

    // remove self (i.e., cube).
    P_RemoveMobj (mo);
}



void A_PlayerScream (mobj_t* mo)
{
    // Default death sound.
    int		sound = sfx_pldeth;
	
    if ( (gamemode == commercial)
	&& 	(mobj_full(mo)->health < -50))
    {
	// IF THE PLAYER DIES
	// LESS THAN -50% WITHOUT GIBBING
	sound = sfx_pdiehi;
    }
    
    S_StartObjSound (mo, sound);
}
