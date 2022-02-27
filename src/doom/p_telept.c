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
//	Teleportation.
//




#include "doomdef.h"
#include "doomstat.h"

#include "s_sound.h"

#include "p_local.h"


// Data.
#include "sounds.h"

// State.
#include "r_state.h"



//
// TELEPORTATION
//
int
EV_Teleport
( line_t*	line,
  int		side,
  mobj_t*	thing )
{
    int		i;
    int		tag;
    mobj_t*	m;
    mobj_t*	fog;
    unsigned	an;
    thinker_t*	thinker;
    sector_t*	sector;
    fixed_t	oldx;
    fixed_t	oldy;
    fixed_t	oldz;

    // don't teleport missiles
    if (thing->flags & MF_MISSILE)
	return 0;		

    // Don't teleport if hit back of line,
    //  so you can get out of teleporter.
    if (side == 1)		
	return 0;	


    mobjfull_t *thingf = mobj_full(thing);

    tag = line_tag(line);
    for (i = 0; i < numsectors; i++)
    {
	if (sectors[ i ].tag == tag )
	{

	    for (thinker = thinker_next(&thinkercap);
		 thinker != &thinkercap;
		 thinker = thinker_next(thinker))
	    {
		// not a mobj
		if (thinker->function != ThinkF_P_MobjThinker)
		    continue;	

		m = (mobj_t *)thinker;
		
		// not a teleportman
		if (m->type != MT_TELEPORTMAN )
		    continue;		

		sector = mobj_sector(m);
		// wrong sector
		if (sector-sectors != i )
		    continue;	

		oldx = thing->xy.x;
		oldy = thing->xy.y;
		oldz = thing->z;
				
		if (!P_TeleportMove (thing, m->xy.x, m->xy.y))
		    return 0;

                // The first Final Doom executable does not set thing->z
                // when teleporting. This quirk is unique to this
                // particular version; the later version included in
                // some versions of the Id Anthology fixed this.

                if (gameversion != exe_final)
		    thing->z = thingf->floorz;

		if (thingf->sp_player)
		    mobj_player(thing)->viewz = thing->z+mobj_player(thing)->viewheight;

		// spawn teleport fog at source and destination
		fog = P_SpawnMobj (oldx, oldy, oldz, MT_TFOG);
		S_StartObjSound (fog, sfx_telept);
		an = mobj_full(m)->angle >> ANGLETOFINESHIFT;
		fog = P_SpawnMobj (m->xy.x+20*finecosine(an), m->xy.y + 20 * finesine(an)
				   , thing->z, MT_TFOG);

		// emit sound, where?
		S_StartObjSound (fog, sfx_telept);
		
		// don't move for a bit
		if (thingf->sp_player)
		    thingf->reactiontime = 18;

		thingf->angle = mobj_full(m)->angle;
		thingf->momx = thingf->momy = thingf->momz = 0;
		return 1;
	    }	
	}
    }
    return 0;
}

