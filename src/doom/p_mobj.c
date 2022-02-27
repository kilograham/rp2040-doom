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
//	Moving object handling. Spawn functions.
//

#include <stdio.h>

#include "i_system.h"
#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
#include "sounds.h"

#include "st_stuff.h"
#include "hu_stuff.h"

#include "s_sound.h"

#include "doomstat.h"


void G_PlayerReborn (int player);
void P_SpawnMapThing (spawnpoint_t 	spawnpoint);

//
// P_SetMobjState
// Returns true if the mobj is still present.
//
int test;

// Use a heuristic approach to detect infinite state cycles: Count the number
// of times the loop in P_SetMobjState() executes and exit with an error once
// an arbitrary very large limit is reached.

#define MOBJ_CYCLE_LIMIT 1000000

boolean
P_SetMobjState
( mobj_t*	mobj,
  statenum_t	state )
{
    should_be_const state_t*	st;
    int	cycle_counter = 0;

    do
    {
	if (state == S_NULL)
	{
#if !SHRINK_MOBJ
	    mobj->state = (state_t *) S_NULL;
#else
	    mobj->state_num = state;
#endif
	    P_RemoveMobj (mobj);
	    return false;
	}

	st = &states[state];
#if !SHRINK_MOBJ
	mobj->state = st;
#else
	mobj->state_num = state;
#endif
	mobj->tics = state_tics(st);
#if !SHRINK_MOBJ
	// ^ for shrunk, they are read from st->
	mobj->sprite = st->sprite;
	mobj->frame = st->frame;
#endif

	// Modified handling.
	// Call action functions when the state is set
	if (st->action.acp1)		
	    st->action.acp1(mobj);	
	
	state = st->nextstate;

	if (cycle_counter++ > MOBJ_CYCLE_LIMIT)
	{
	    I_Error("P_SetMobjState: Infinite state cycle detected!");
	}
    } while (!mobj->tics);
				
    return true;
}


//
// P_ExplodeMissile  
//
void P_ExplodeMissile (mobj_t* mo)
{
    mobj_full(mo)->momx = mobj_full(mo)->momy = mobj_full(mo)->momz = 0;

    P_SetMobjState (mo, mobjinfo[mo->type].deathstate);

    mo->tics -= P_Random()&3;

    if (mo->tics < 1)
	mo->tics = 1;

    mo->flags &= ~MF_MISSILE;

    if (mobj_info(mo)->deathsound)
	S_StartObjSound (mo, mobj_info(mo)->deathsound);
}


//
// P_XYMovement  
//
#define STOPSPEED		0x1000
#define FRICTION		0xe800

void P_XYMovement (mobj_t* mo)
{ 	
    fixed_t 	ptryx;
    fixed_t	ptryy;
    player_t*	player;
    fixed_t	xmove;
    fixed_t	ymove;

    mobjfull_t *mof = mobj_full(mo);
    if (!mof->momx && !mof->momy)
    {
	if (mo->flags & MF_SKULLFLY)
	{
	    // the skull slammed into something
	    mo->flags &= ~MF_SKULLFLY;
	    mof->momx = mof->momy = mof->momz = 0;

	    P_SetMobjState (mo, mobj_info(mo)->spawnstate);
	}
	return;
    }
	
    player = mobj_player(mo);
		
    if (mof->momx > MAXMOVE)
	mof->momx = MAXMOVE;
    else if (mof->momx < -MAXMOVE)
	mof->momx = -MAXMOVE;

    if (mof->momy > MAXMOVE)
	mof->momy = MAXMOVE;
    else if (mof->momy < -MAXMOVE)
	mof->momy = -MAXMOVE;
		
    xmove = mof->momx;
    ymove = mof->momy;
	
    do
    {
	if (xmove > MAXMOVE/2 || ymove > MAXMOVE/2)
	{
	    ptryx = mo->xy.x + xmove / 2;
	    ptryy = mo->xy.y + ymove / 2;
	    xmove >>= 1;
	    ymove >>= 1;
	}
	else
	{
	    ptryx = mo->xy.x + xmove;
	    ptryy = mo->xy.y + ymove;
	    xmove = ymove = 0;
	}
		
	if (!P_TryMove (mo, ptryx, ptryy))
	{
	    // blocked move
	    if (mof->sp_player)
	    {	// try to slide along it
		P_SlideMove (mo);
	    }
	    else if (mo->flags & MF_MISSILE)
	    {
		// explode a missile
		if (ceilingline &&
                line_backsector(ceilingline) &&
		    line_backsector(ceilingline)->ceilingpic == skyflatnum)
		{
		    // Hack to prevent missiles exploding
		    // against the sky.
		    // Does not handle sky floors.
		    P_RemoveMobj (mo);
		    return;
		}
		P_ExplodeMissile (mo);
	    }
	    else
		mof->momx = mof->momy = 0;
	}
    } while (xmove || ymove);
    
    // slow down
    if (player && player->cheats & CF_NOMOMENTUM)
    {
	// debug option for no sliding at all
	mof->momx = mof->momy = 0;
	return;
    }

    if (mo->flags & (MF_MISSILE | MF_SKULLFLY) )
	return; 	// no friction for missiles ever
		
    if (mo->z > mof->floorz)
	return;		// no friction when airborne

    if (mo->flags & MF_CORPSE)
    {
	// do not stop sliding
	//  if halfway off a step with some momentum
	if (mof->momx > FRACUNIT/4
	    || mof->momx < -FRACUNIT/4
	    || mof->momy > FRACUNIT/4
	    || mof->momy < -FRACUNIT/4)
	{
	    if (mof->floorz != sector_floorheight(mobj_sector(mo)))
		return;
	}
    }

    if (mof->momx > -STOPSPEED
	&& mof->momx < STOPSPEED
	&& mof->momy > -STOPSPEED
	&& mof->momy < STOPSPEED
	&& (!player
	    || (player->cmd.forwardmove== 0
		&& player->cmd.sidemove == 0 ) ) )
    {
	// if in a walking frame, stop moving
	if ( player&&(unsigned)(mobj_state_num(player->mo)- S_PLAY_RUN1) < 4)
	    P_SetMobjState (player->mo, S_PLAY);
	
	mof->momx = 0;
	mof->momy = 0;
    }
    else
    {
	mof->momx = FixedMul (mof->momx, FRICTION);
	mof->momy = FixedMul (mof->momy, FRICTION);
    }
}

//
// P_ZMovement
//
void P_ZMovement (mobj_t* mo)
{
    fixed_t	dist;
    fixed_t	delta;

    mobjfull_t *mof = mobj_full(mo);

    // check for smooth step up
    if (mof->sp_player && mo->z < mof->floorz)
    {
	mobj_player(mo)->viewheight -= mof->floorz-mo->z;

	mobj_player(mo)->deltaviewheight
	    = (VIEWHEIGHT - mobj_player(mo)->viewheight)>>3;
    }
    
    // adjust height
    mo->z += mof->momz;
	
    if ( mo->flags & MF_FLOAT
	 && mof->sp_target)
    {
	// float down towards target if too close
	if ( !(mo->flags & MF_SKULLFLY)
	     && !(mo->flags & MF_INFLOAT) )
	{
	    dist = P_AproxDistance (mo->xy.x - mobj_target(mo)->xy.x,
                                mo->xy.y - mobj_target(mo)->xy.y);
	    
	    delta =(mobj_target(mo)->z + (mof->height>>1)) - mo->z;

	    if (delta<0 && dist < -(delta*3) )
		mo->z -= FLOATSPEED;
	    else if (delta>0 && dist < (delta*3) )
		mo->z += FLOATSPEED;			
	}
	
    }
    
    // clip movement
    if (mo->z <= mof->floorz)
    {
	// hit the floor

	// Note (id):
	//  somebody left this after the setting momz to 0,
	//  kinda useless there.
	//
	// cph - This was the a bug in the linuxdoom-1.10 source which
	//  caused it not to sync Doom 2 v1.9 demos. Someone
	//  added the above comment and moved up the following code. So
	//  demos would desync in close lost soul fights.
	// Note that this only applies to original Doom 1 or Doom2 demos - not
	//  Final Doom and Ultimate Doom.  So we test demo_compatibility *and*
	//  gamemission. (Note we assume that Doom1 is always Ult Doom, which
	//  seems to hold for most published demos.)
        //  
        //  fraggle - cph got the logic here slightly wrong.  There are three
        //  versions of Doom 1.9:
        //
        //  * The version used in registered doom 1.9 + doom2 - no bounce
        //  * The version used in ultimate doom - has bounce
        //  * The version used in final doom - has bounce
        //
        // So we need to check that this is either retail or commercial
        // (but not doom2)
	
	int correct_lost_soul_bounce = gameversion >= exe_ultimate;

	if (correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
	{
	    // the skull slammed into something
	    mof->momz = -mof->momz;
	}
	
	if (mof->momz < 0)
	{
	    if (mof->sp_player
		&& mof->momz < -GRAVITY*8)
	    {
		// Squat down.
		// Decrease viewheight for a moment
		// after hitting the ground (hard),
		// and utter appropriate sound.
		mobj_player(mo)->deltaviewheight = mof->momz>>3;
		S_StartObjSound (mo, sfx_oof);
	    }
	    mof->momz = 0;
	}
	mo->z = mof->floorz;


	// cph 2001/05/26 -
	// See lost soul bouncing comment above. We need this here for bug
	// compatibility with original Doom2 v1.9 - if a soul is charging and
	// hit by a raising floor this incorrectly reverses its Y momentum.
	//

        if (!correct_lost_soul_bounce && mo->flags & MF_SKULLFLY)
            mof->momz = -mof->momz;

	if ( (mo->flags & MF_MISSILE)
	     && !(mo->flags & MF_NOCLIP) )
	{
	    P_ExplodeMissile (mo);
	    return;
	}
    }
    else if (! (mo->flags & MF_NOGRAVITY) )
    {
	if (mof->momz == 0)
	    mof->momz = -GRAVITY*2;
	else
	    mof->momz -= GRAVITY;
    }
	
    if (mo->z + mof->height > mof->ceilingz)
    {
	// hit the ceiling
	if (mof->momz > 0)
	    mof->momz = 0;
	{
	    mo->z = mof->ceilingz - mof->height;
	}

	if (mo->flags & MF_SKULLFLY)
	{	// the skull slammed into something
	    mof->momz = -mof->momz;
	}
	
	if ( (mo->flags & MF_MISSILE)
	     && !(mo->flags & MF_NOCLIP) )
	{
	    P_ExplodeMissile (mo);
	    return;
	}
    }
} 



//
// P_NightmareRespawn
//
void
P_NightmareRespawn (mobj_t* mobj)
{
    fixed_t		x;
    fixed_t		y;
    fixed_t		z; 
    subsector_t*	ss; 
    mobj_t*		mo;
    //mapthing_t*		mthing;
		
    x = mobj_spawnpoint(mobj).x << FRACBITS;
    y = mobj_spawnpoint(mobj).y << FRACBITS;

    // somthing is occupying it's position?
    if (!P_CheckPosition (mobj, x, y) ) 
	return;	// no respwan

    // spawn a teleport fog at old spot
    // because of removal of the body?
    mo = P_SpawnMobj (mobj->xy.x,
		      mobj->xy.y,
		      sector_floorheight(mobj_sector(mobj)) , MT_TFOG);
    // initiate teleport sound
    S_StartObjSound (mo, sfx_telept);

    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector (x,y); 

    mo = P_SpawnMobj (x, y, sector_floorheight(subsector_sector(ss)) , MT_TFOG);

    S_StartObjSound (mo, sfx_telept);

    // spawn the new monster
    const mapthing_t *mthing = &mobj_spawnpoint(mobj);
	
    // spawn it
    if (mobj_info(mobj)->flags & MF_SPAWNCEILING)
	z = ONCEILINGZ;
    else
	z = ONFLOORZ;

    // inherit attributes from deceased one
    mo = P_SpawnMobj (x,y,z, mobj->type);
    mo->spawnpoint = mobj->spawnpoint;
    mobj_full(mo)->angle = ANG45 * (mthing->angle/45);

    if (mthing->options & MTF_AMBUSH)
	mo->flags |= MF_AMBUSH;

    mobj_reactiontime(mo) = 18;
	
    // remove the old monster,
    P_RemoveMobj (mobj);
}

#if DEBUG_MOBJ
#define COUNTER_FROM 0
static int counter;
#endif

//
// P_MobjThinker
//
void P_MobjThinker (mobj_t* mobj)
{
#if DEBUG_MOBJ
    counter++;
#endif
    if (!mobj_is_static(mobj)) {
#if DEBUG_MOBJ
        mobjfull_t *mobjf = mobj_full(mobj);
        if (counter > COUNTER_FROM && mobj->type!=73 && mobj->type!=77) {
            printf("%d %d: ", mobj->debug_id, mobj->think_count);
            if (mobj->flags & MF_STATIC) {
                printf("S");
            }
            printf("MOBJ t %d dn %d fl %08x ti %d st %d %08x,%08x,%08x a=%08x r=%08x sec=%d\n", mobj->type, mobj_info(mobj)->doomednum, mobj->flags,  mobj->tics, (int)mobj_state_num(mobj), mobj->xy.x,
                   mobj->xy.y, mobj->z, mobj_angle(mobj), mobj_radius(mobj), (int)(mobj_sector(mobj)-sectors));
            printf("  mom %08x, %08x, %08x md %d fl:cl %08x:%08x trg? %d trc? %d\n", mobjf->momx, mobjf->momy, mobjf->momz,
                   mobjf->movedir, mobjf->floorz, mobjf->ceilingz, mobjf->sp_target != 0, mobjf->sp_tracer != 0);
            if (mobj->flags & MF_STATIC) {
                printf("   rt %d th %d\n", mobjf->reactiontime, mobjf->threshold);
            } else {
                printf("   rt %d ll %d th %d\n", mobjf->reactiontime, mobjf->lastlook, mobjf->threshold);
            }
            if (mobjf->sp_player) {
                printf("  player health=%d damage %d\n", mobj_player(mobj)->health, mobj_player(mobj)->damagecount);
            }
        }
#endif
        // momentum movement
        if (mobj_full(mobj)->momx
            || mobj_full(mobj)->momy
            || (mobj->flags & MF_SKULLFLY)) {
            P_XYMovement(mobj);

            // FIXME: decent NOP/NULL/Nil function pointer please.
            if (mobj->thinker.function == ThinkF_REMOVED)
                return;                // mobj was removed
        }
        if ((mobj->z != mobj_full(mobj)->floorz)
            || mobj_full(mobj)->momz) {
            P_ZMovement(mobj);

            // FIXME: decent NOP/NULL/Nil function pointer please.
            if (mobj->thinker.function == ThinkF_REMOVED)
                return;                // mobj was removed
        }
    } else {
#if DEBUG_MOBJ
        if (counter > COUNTER_FROM && mobj->type!=73 && mobj->type!=77) {
            printf("%d %d: ", mobj->debug_id, mobj->think_count);
            printf("SMOBJ t %d dn %d fl %08x ti %d st %d %08x,%08x,%08x a=%08x r=%08x sec=%d\n", mobj->type, mobj_info(mobj)->doomednum, mobj->flags,  mobj->tics, (int)mobj_state_num(mobj), mobj->xy.x,
                   mobj->xy.y, mobj->z, mobj_angle(mobj), mobj_radius(mobj), (int)(mobj_sector(mobj)-sectors));
            printf("  mom %08x, %08x, %08x md %d fl:cl %08x:%08x trg? %d trc? %d\n", 0, 0, 0,
                   0, mobj_floorz(mobj), mobj_ceilingz(mobj), 0, 0);
            printf("   rt %d th %d\n", mobj_info(mobj)->reactiontime, 0);
        }
#endif
#ifndef NDEBUG
        // todo graham not sure if this can happen... see if i can identify one; maybe just set it to floor? ah hah this happens in E3M1
        if (mobj->z != mobj_floorz(mobj)) {
            //I_Error("OOPS\n");
        }
#endif
    }

    
    // cycle through states,
    // calling action functions at transitions
    if (mobj->tics != -1)
    {
	mobj->tics--;
		
	// you can cycle through multiple states in a tic
	if (!mobj->tics)
	    if (!P_SetMobjState (mobj, mobj_state(mobj)->nextstate) )
		return;		// freed itself
    }
    else
    {
	// check for nightmare respawn
	if (! (mobj->flags & MF_COUNTKILL) )
	    return;

	if (!respawnmonsters)
	    return;

        mobj_full(mobj)->movecount++;

	if (mobj_full(mobj)->movecount < 12*TICRATE)
	    return;

	if ( leveltime&31 )
	    return;

	if (P_Random () > 4)
	    return;

	P_NightmareRespawn (mobj);
    }

}

//
// P_SpawnMobj
//
mobj_t*
P_SpawnMobj
( fixed_t	x,
  fixed_t	y,
  fixed_t	z,
  mobjtype_t	type )
{
    should_be_const state_t*	st;
    const mobjinfo_t*	info;

    info = &mobjinfo[type];
    int size = mobj_flags_is_static(info->flags) ? sizeof(mobj_t) : sizeof(mobjfull_t);
    mobj_t *mobj = Z_ThinkMalloc (size, PU_LEVEL, 0);

    mobj->type = type;
#if DEBUG_MOBJ
    static int debug_id;
    mobj->debug_id = ++debug_id;
#endif
#if !SHRINK_MOBJ
    mobj_info(mobj) = info;
#endif
    mobj->xy.x = x;
    mobj->xy.y = y;

    mobj->flags = info->flags;
    if (!mobj_is_static(mobj)) {
        mobj_full(mobj)->radius = info->radius;
        mobj_full(mobj)->health = info->spawnhealth;
        if (gameskill != sk_nightmare)
            mobj_full(mobj)->reactiontime = info->reactiontime;
        mobj_full(mobj)->height = info->height;
        mobj_full(mobj)->lastlook = P_Random () % MAXPLAYERS;
    } else {
        P_Random();
    }

    // do not set the state with P_SetMobjState,
    // because action routines can not be called yet
    st = &states[info->spawnstate];

#if !SHRINK_MOBJ
    mobj->state = st;
#else
    mobj->state_num = info->spawnstate;
#endif
    mobj->tics = state_tics(st);
#if !SHRINK_MOBJ
    // ^ for shrunk, they are read from st->
    mobj->sprite = st->sprite;
    mobj->frame = st->frame;
#endif

    // set subsector and/or block links
    P_SetThingPosition (mobj);

    if (!mobj_is_static(mobj)) {
        mobj_full(mobj)->floorz = sector_floorheight(mobj_sector(mobj));
        mobj_full(mobj)->ceilingz = sector_ceilingheight(mobj_sector(mobj));
    }

    if (z == ONFLOORZ)
	mobj->z = mobj_floorz(mobj);
    else if (z == ONCEILINGZ)
	mobj->z = mobj_ceilingz(mobj) - mobj_info(mobj)->height;
    else 
	mobj->z = z;

    mobj->thinker.function = ThinkF_P_MobjThinker;

    P_AddThinker (&mobj->thinker);

    return mobj;
}


//
// P_RemoveMobj
//
spawnpoint_t itemrespawnque[ITEMQUESIZE];
isb_int16_t 	itemrespawntime[ITEMQUESIZE];
isb_uint8_t 		iquehead;
isb_uint8_t 		iquetail;


void P_RemoveMobj (mobj_t* mobj)
{
    if ((mobj->flags & MF_SPECIAL)
	&& !(mobj->flags & MF_DROPPED)
	&& (mobj->type != MT_INV)
	&& (mobj->type != MT_INS))
    {
	itemrespawnque[iquehead] = mobj->spawnpoint;
	itemrespawntime[iquehead] = (isb_int16_t)leveltime;
	iquehead = (iquehead+1)&(ITEMQUESIZE-1);

	// lose one off the end?
	if (iquehead == iquetail)
	    iquetail = (iquetail+1)&(ITEMQUESIZE-1);
    }
	
    // unlink from sector and block lists
    P_UnsetThingPosition (mobj);
    
    // stop any playing sound
    S_StopObjSound (mobj);
    
    // free block
    P_RemoveThinker ((thinker_t*)mobj);
}




//
// P_RespawnSpecials
//
void P_RespawnSpecials (void)
{
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;
    
    subsector_t*	ss; 
    mobj_t*		mo;
    const mapthing_t*		mthing;
    
    int			i;

    // only respawn items in deathmatch
    if (deathmatch != 2)
	return;	// 

    // nothing left to respawn?
    if (iquehead == iquetail)
	return;		

    // wait at least 30 seconds
    if ((isb_int16_t)(leveltime - itemrespawntime[iquetail]) < 30*TICRATE)
	return;

#if !SHRINK_MOBJ
    mthing = &itemrespawnque[iquetail];
#else
    mthing = &mapthings[itemrespawnque[iquetail]];
#endif
	
    x = mthing->x << FRACBITS; 
    y = mthing->y << FRACBITS; 
	  
    // spawn a teleport fog at the new spot
    ss = R_PointInSubsector (x,y); 
    mo = P_SpawnMobj (x, y, sector_floorheight(subsector_sector(ss)) , MT_IFOG);
    S_StartObjSound (mo, sfx_itmbk);

    // find which type to spawn
    for (i=0 ; i< NUMMOBJTYPES ; i++)
    {
	if (mthing->type == mobjinfo[i].doomednum)
	    break;
    }

    if (i >= NUMMOBJTYPES)
    {
        I_Error("P_RespawnSpecials: Failed to find mobj type with doomednum "
                "%d when respawning thing. This would cause a buffer overrun "
                "in vanilla Doom", mthing->type);
    }

    // spawn it
    if (mobjinfo[i].flags & MF_SPAWNCEILING)
	z = ONCEILINGZ;
    else
	z = ONFLOORZ;

    mo = P_SpawnMobj (x,y,z, i);
    mo->spawnpoint = itemrespawnque[iquetail];
    mobj_full(mo)->angle = ANG45 * (mthing->angle/45);

    // pull it from the que
    iquetail = (iquetail+1)&(ITEMQUESIZE-1);
}




//
// P_SpawnPlayer
// Called when a player is spawned on the level.
// Most of the player structure stays unchanged
//  between levels.
//
void P_SpawnPlayer (const mapthing_t* mthing)
{
    player_t*		p;
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;

    mobj_t*		mobj;

    int			i;

    if (mthing->type == 0)
    {
        return;
    }

    // not playing?
    if (!playeringame[mthing->type-1])
	return;					
		
    p = &players[mthing->type-1];

    if (p->playerstate == PST_REBORN)
	G_PlayerReborn (mthing->type-1);

    x 		= mthing->x << FRACBITS;
    y 		= mthing->y << FRACBITS;
    z		= ONFLOORZ;
    mobj	= P_SpawnMobj (x,y,z, MT_PLAYER);

    // set color translations for player sprites
    if (mthing->type > 1)		
	mobj->flags |= (mthing->type-1)<<MF_TRANSSHIFT;

    mobj_full(mobj)->angle	= ANG45 * (mthing->angle/45);
    mobj_full(mobj)->sp_player = ptr_to_shortptr(p);
    mobj_full(mobj)->health = p->health;

    p->mo = mobj;
    p->playerstate = PST_LIVE;	
    p->refire = 0;
    p->message = NULL;
    p->damagecount = 0;
    p->bonuscount = 0;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->viewheight = VIEWHEIGHT;

    // setup gun psprite
    P_SetupPsprites (p);
    
    // give all cards in death match mode
    if (deathmatch)
	for (i=0 ; i<NUMCARDS ; i++)
	    p->cards[i] = true;
			
    if (mthing->type-1 == consoleplayer)
    {
	// wake up the status bar
	ST_Start ();
	// wake up the heads up text
	HU_Start ();		
    }
}


//
// P_SpawnMapThing
// The fields of the mapthing should
// already be in host byte order.
//
void P_SpawnMapThing (spawnpoint_t spawnpoint)
{
    int			i;
    int			bit;
    mobj_t*		mobj;
    fixed_t		x;
    fixed_t		y;
    fixed_t		z;

    const mapthing_t *mthing = &spawnpoint_mapthing(spawnpoint);
    // count deathmatch start positions
    if (mthing->type == 11)
    {
	if (deathmatch_p < &deathmatchstarts[10])
	{
	    memcpy (deathmatch_p, mthing, sizeof(*mthing));
	    deathmatch_p++;
	}
	return;
    }

    if (mthing->type <= 0)
    {
        // Thing type 0 is actually "player -1 start".  
        // For some reason, Vanilla Doom accepts/ignores this.

        return;
    }
	
    // check for players specially
    if (mthing->type <= 4)
    {
	// save spots for respawning in network games
	playerstarts[mthing->type-1] = *mthing;
	playerstartsingame[mthing->type-1] = true;
	if (!deathmatch)
	    P_SpawnPlayer (mthing);

	return;
    }

    // check for apropriate skill level
    if (!netgame && (mthing->options & 16) )
	return;
		
    if (gameskill == sk_baby)
	bit = 1;
    else if (gameskill == sk_nightmare)
	bit = 4;
    else
	bit = 1<<(gameskill-1);

    if (!(mthing->options & bit) )
	return;
	
    // find which type to spawn
    for (i=0 ; i< NUMMOBJTYPES ; i++)
	if (mthing->type == mobjinfo[i].doomednum)
	    break;
	
    if (i==NUMMOBJTYPES)
	I_Error ("P_SpawnMapThing: Unknown type %i at (%i, %i)",
		 mthing->type,
		 mthing->x, mthing->y);
		
    // don't spawn keycards and players in deathmatch
    if (deathmatch && mobjinfo[i].flags & MF_NOTDMATCH)
	return;
		
    // don't spawn any monsters if -nomonsters
    if (nomonsters
	&& ( i == MT_SKULL
	     || (mobjinfo[i].flags & MF_COUNTKILL)) )
    {
	return;
    }
    
    // spawn it
    x = mthing->x << FRACBITS;
    y = mthing->y << FRACBITS;

    if (mobjinfo[i].flags & MF_SPAWNCEILING)
	z = ONCEILINGZ;
    else
	z = ONFLOORZ;
    
    mobj = P_SpawnMobj (x,y,z, i);
    mobj->spawnpoint = spawnpoint;

    if (mobj->tics > 0)
	mobj->tics = 1 + (P_Random () % mobj->tics);
    if (mobj->flags & MF_COUNTKILL)
	totalkills++;
    if (mobj->flags & MF_COUNTITEM)
	totalitems++;

    if (!mobj_is_static(mobj)) // todo XXX temp?
        mobj_full(mobj)->angle = ANG45 * (mthing->angle/45);
    if (mthing->options & MTF_AMBUSH)
	    mobj->flags |= MF_AMBUSH;
}



//
// GAME SPAWN FUNCTIONS
//


//
// P_SpawnPuff
//
extern fixed_t attackrange;

void
P_SpawnPuff
( fixed_t	x,
  fixed_t	y,
  fixed_t	z )
{
    mobj_t*	th;
	
    z += (P_SubRandom() << 10);

    th = P_SpawnMobj (x,y,z, MT_PUFF);
    mobj_full(th)->momz = FRACUNIT;
    th->tics -= P_Random()&3;

    if (th->tics < 1)
	th->tics = 1;
	
    // don't make punches spark on the wall
    if (attackrange == MELEERANGE)
	P_SetMobjState (th, S_PUFF3);
}



//
// P_SpawnBlood
// 
void
P_SpawnBlood
( fixed_t	x,
  fixed_t	y,
  fixed_t	z,
  int		damage )
{
    mobj_t*	th;
	
    z += (P_SubRandom() << 10);
    th = P_SpawnMobj (x,y,z, MT_BLOOD);
    mobj_full(th)->momz = FRACUNIT*2;
    th->tics -= P_Random()&3;

    if (th->tics < 1)
	th->tics = 1;
		
    if (damage <= 12 && damage >= 9)
	P_SetMobjState (th,S_BLOOD2);
    else if (damage < 9)
	P_SetMobjState (th,S_BLOOD3);
}



//
// P_CheckMissileSpawn
// Moves the missile forward a bit
//  and possibly explodes it right there.
//
void P_CheckMissileSpawn (mobj_t* th)
{
    th->tics -= P_Random()&3;
    if (th->tics < 1)
	th->tics = 1;
    
    // move a little forward so an angle can
    // be computed if it immediately explodes
    th->xy.x += (mobj_full(th)->momx >> 1);
    th->xy.y += (mobj_full(th)->momy >> 1);
    th->z += (mobj_full(th)->momz>>1);

    if (!P_TryMove (th, th->xy.x, th->xy.y))
	P_ExplodeMissile (th);
}

// Certain functions assume that a mobj_t pointer is non-NULL,
// causing a crash in some situations where it is NULL.  Vanilla
// Doom did not crash because of the lack of proper memory 
// protection. This function substitutes NULL pointers for
// pointers to a dummy mobj, to avoid a crash.

mobj_t *P_SubstNullMobj(mobj_t *mobj)
{
    if (mobj == NULL)
    {
        static mobj_t dummy_mobj;

        dummy_mobj.xy.x = 0;
        dummy_mobj.xy.y = 0;
        dummy_mobj.z = 0;
        dummy_mobj.flags = 0;

        mobj = &dummy_mobj;
    }

    return mobj;
}

//
// P_SpawnMissile
//
mobj_t*
P_SpawnMissile
( mobj_t*	source,
  mobj_t*	dest,
  mobjtype_t	type )
{
    mobj_t*	th;
    angle_t	an;
    int		dist;

    th = P_SpawnMobj (source->xy.x,
		      source->xy.y,
		      source->z + 4*8*FRACUNIT, type);
    
    if (mobj_info(th)->seesound)
	S_StartObjSound (th, mobj_info(th)->seesound);

    mobj_full(th)->sp_target = mobj_to_shortptr(source);	// where it came from
    an = R_PointToAngle2 (source->xy.x, source->xy.y, dest->xy.x, dest->xy.y);

    // fuzzy player
    if (dest->flags & MF_SHADOW)
	an += P_SubRandom() << 20;

    mobj_full(th)->angle = an;
    an >>= ANGLETOFINESHIFT;
    int speed = mobj_speed(th);
    mobj_full(th)->momx = FixedMul (speed, finecosine(an));
    mobj_full(th)->momy = FixedMul (speed, finesine(an));
	
    dist = P_AproxDistance (dest->xy.x - source->xy.x, dest->xy.y - source->xy.y);
    dist = dist / speed;

    if (dist < 1)
	dist = 1;

    mobj_full(th)->momz = (dest->z - source->z) / dist;
    P_CheckMissileSpawn (th);
	
    return th;
}


//
// P_SpawnPlayerMissile
// Tries to aim at a nearby monster
//
void
P_SpawnPlayerMissile
( mobj_t*	source,
  mobjtype_t	type )
{
    mobj_t*	th;
    angle_t	an;
    
    fixed_t	x;
    fixed_t	y;
    fixed_t	z;
    fixed_t	slope;
    
    // see which target is to be aimed at
    an = mobj_full(source)->angle;
    slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
    
    if (!linetarget)
    {
	an += 1<<26;
	slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);

	if (!linetarget)
	{
	    an -= 2<<26;
	    slope = P_AimLineAttack (source, an, 16*64*FRACUNIT);
	}

	if (!linetarget)
	{
	    an = mobj_full(source)->angle;
	    slope = 0;
	}
    }
		
    x = source->xy.x;
    y = source->xy.y;
    z = source->z + 4*8*FRACUNIT;
	
    th = P_SpawnMobj (x,y,z, type);

    if (mobj_info(th)->seesound)
	S_StartObjSound (th, mobj_info(th)->seesound);

    mobj_full(th)->sp_target = mobj_to_shortptr(source);
    mobj_full(th)->angle = an;
    int speed = mobj_speed(th);
    mobj_full(th)->momx = FixedMul( speed,
			 finecosine(an>>ANGLETOFINESHIFT));
    mobj_full(th)->momy = FixedMul( speed,
			 finesine(an>>ANGLETOFINESHIFT));
    mobj_full(th)->momz = FixedMul( speed, slope);

    P_CheckMissileSpawn (th);
}

#if DOOM_CONST
int mobj_speed(mobj_t *mo) {
    const mobjinfo_t *inf = mobj_info(mo);
    int speed = inf->speed;
    if (nightmare_speeds && (mo->type == MT_BRUISERSHOT || mo->type == MT_HEADSHOT || mo->type == MT_TROOPSHOT)) {
        speed = 20 * FRACUNIT;
    }
    return speed;
}
#endif

