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
//	Do all the WAD I/O, get map description,
//	set up initial state and misc. LUTs.
//



#include <math.h>

#include "z_zone.h"

#include "deh_main.h"
#include "i_swap.h"
#include "m_argv.h"
#include "m_bbox.h"

#include "g_game.h"

#include "i_system.h"
#include "w_wad.h"

#include "doomdef.h"
#include "p_local.h"

#include "s_sound.h"

#include "doomstat.h"


void	P_SpawnMapThing (spawnpoint_t spawnpoint);

#if USE_RAW_MAPTHING
const mapthing_t *mapthings;
cardinal_t nummapthings;
#endif

//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
cardinal_t		numvertexes;
vertex_t*	vertexes;

#if !WHD_SUPER_TINY
cardinal_t		numsegs;
#endif
seg_t*		segs;

cardinal_t		numsectors;
sector_t*	sectors;
#if LOAD_COMPRESSED || SAVE_COMPRESSED
whdsector_t *whd_sectors;
#endif


cardinal_t		numsubsectors;
subsector_t*	subsectors;

cardinal_t		numnodes;
node_t*		nodes;

cardinal_t		numlines;
#if WHD_SUPER_TINY
// this is size of line data / 5... used for line bitmap ... we divide
// the "line index" byte offset by 5 to get unique bitmap index (it wastes about 4% above numlines)
cardinal_t      numlines5;
#endif
line_t*		lines;


#if USE_WHD
uint8_t num_switched_sides = 0;
#if WHD_SUPER_TINY
const side_t*		sides_z;
uint16_t whd_sidemul;
uint16_t whd_vmul;
#else
const side_t*		sides;
cardinal_t		numsides;
#endif
#else
cardinal_t		numsides;
side_t*		sides;
#endif

#if !USE_INDEX_LINEBUFFER
line_t**		linebuffer;
#else
cardinal_t *	linebuffer;
#endif

static int      totallines;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
cardinal_t		bmapwidth;
cardinal_t		bmapheight;	// size in mapblocks
#if !USE_WHD
rowad_const short*		blockmap;	// int for larger maps
#else
byte *blockmap_whd;
#endif
// offsets in blockmap are from here
rowad_const short*		blockmaplump;
// origin of block map
fixed_t		bmaporgx;
fixed_t		bmaporgy;
// for thing chains
shortptr_t /*mobj_t**/*	blocklinks;


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
should_be_const byte*		rejectmatrix;


// Maintain single and multi player starting spots.
#define MAX_DEATHMATCH_STARTS	10

mapthing_t	deathmatchstarts[MAX_DEATHMATCH_STARTS];
mapthing_t*	deathmatch_p;
mapthing_t	playerstarts[MAXPLAYERS];
boolean     playerstartsingame[MAXPLAYERS];





//
// P_LoadVertexes
//
void P_LoadVertexes (int lump)
{
    int			i;
    mapvertex_t*	ml;
    vertex_t*		li;
    should_be_const byte*		data;

    // Determine number of lumps:
    //  total lump length / vertex record length.
    numvertexes = W_LumpLength (lump) / sizeof(mapvertex_t);

#if USE_RAW_MAPVERTEX
    static_assert(sizeof(mapvertex_t) == sizeof(vertex_t), "");
    static_assert(offsetof(mapvertex_t, y) == offsetof(vertex_t, y), "");
    data = W_CacheLumpNum (lump,PU_LEVEL);
    vertexes = (vertex_t *)data;
#if PRINT_LEVEL_SIZE
    printf("VERTEX LOAD map %d vertexes x 0x%03x : size = %08x\n", numvertexes, (int)sizeof(mapvertex_t ), numvertexes*(int)sizeof(mapvertex_t));
#endif
#else
    // Allocate zone memory for buffer.
    vertexes = Z_Malloc (numvertexes*sizeof(vertex_t),PU_LEVEL,0);
    printf("VERTEX SIZE %d\n", (int)(numvertexes*sizeof(vertex_t)));

    // Load data into cache.
    data = W_CacheLumpNum (lump, PU_STATIC);

    ml = (mapvertex_t *)data;
    li = (vertex_t *)vertexes;

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (i=0 ; i<numvertexes ; i++, li++, ml++)
    {
	li->x = SHORT(ml->x)<<FRACBITS;
	li->y = SHORT(ml->y)<<FRACBITS;
    }

    // Free buffer memory.
    W_ReleaseLumpNum(lump);
#endif
}

//
// GetSectorAtNullAddress
//
sector_t* GetSectorAtNullAddress(void)
{
    static boolean null_sector_is_initialized = false;
    static sector_t null_sector;

    if (!null_sector_is_initialized)
    {
        memset(&null_sector, 0, sizeof(null_sector));
        I_GetMemoryValue(0, &null_sector.rawfloorheight, sizeof(null_sector.rawfloorheight));
        I_GetMemoryValue(null_sector.rawfloorheight, &null_sector.rawceilingheight, null_sector.rawceilingheight);
        null_sector_is_initialized = true;
    }

    return &null_sector;
}

//
// P_LoadSegs
//
void P_LoadSegs (int lump)
{
    should_be_const byte*		data;
    int			i;
    mapseg_t*		ml;
    seg_t*		si;
    line_t*		ldef;
    int			linedef;
    int			side;
    int                 sidenum;
	
#if USE_RAW_MAPSEG
    data = W_CacheLumpNum (lump,PU_LEVEL);
#if !WHD_SUPER_TINY
    numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
#if PRINT_LEVEL_SIZE
    printf("SEG LOAD map %d segs x 0x%03x : size = %08x\n", numsegs, (int)sizeof(mapseg_t), numsegs*(int)sizeof(mapseg_t));
#endif
    static_assert(sizeof(seg_t) == sizeof(mapseg_t), "");
#else
#if PRINT_LEVEL_SIZE
    printf("SEG LOAD map segs : size = %08x\n", W_LumpLength(lump));
#endif
#endif
    segs = (seg_t *)data;

#else
    numsegs = W_LumpLength (lump) / sizeof(mapseg_t);
#if PRINT_LEVEL_SIZE
    printf("SEG LOAD alloc %d segs x 0x%03x : size = %08x\n", numsegs, (int)sizeof(seg_t), numsegs*(int)sizeof(seg_t));
#endif
    segs = Z_Malloc (numsegs*sizeof(seg_t),PU_LEVEL,0);
    // todo rowad
    si = (seg_t *)segs;
    memset (si, 0, numsegs*sizeof(seg_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    ml = (mapseg_t *)data;
    for (i=0 ; i<numsegs ; i++, si++, ml++)
    {
        si->v1 = &vertexes[SHORT(ml->v1)];
        si->v2 = &vertexes[SHORT(ml->v2)];

        si->angle = (SHORT(ml->angle)) << FRACBITS;
        si->offset = (SHORT(ml->offset)) << FRACBITS;
	linedef = SHORT(ml->linedef);
	ldef = &lines[linedef];
        si->linedef = ldef;
	side = SHORT(ml->side);

        // e6y: check for wrong indexes
        if ((unsigned)ldef->sidenum[side] >= (unsigned)numsides)
        {
            I_Error("P_LoadSegs: linedef %d for seg %d references a non-existent sidedef %d",
                    linedef, i, (unsigned)ldef->sidenum[side]);
        }

        si->sidedef = &sides[ldef->sidenum[side]];
        si->frontsector = sides[ldef->sidenum[side]].sector;

        if (ldef-> flags & ML_TWOSIDED)
        {
            sidenum = ldef->sidenum[side ^ 1];

            // If the sidenum is out of range, this may be a "glass hack"
            // impassible window.  Point at side #0 (this may not be
            // the correct Vanilla behavior; however, it seems to work for
            // OTTAWAU.WAD, which is the one place I've seen this trick
            // used).

            if (sidenum < 0 || sidenum >= numsides)
            {
                si->backsector = GetSectorAtNullAddress();
            }
            else
            {
                si->backsector = sides[sidenum].sector;
            }
        }
        else
        {
            si->backsector = 0;
        }
    }
	
    W_ReleaseLumpNum(lump);
#endif
}


//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
    should_be_const byte*		data;
    int			i;
    mapsubsector_t*	ms;
    subsector_t*	ss;
	
#if USE_WHD
    static_assert(sizeof(subsector_t) == 2, "");
    numsubsectors = W_LumpLength (lump) / 2;
#if PRINT_LEVEL_SIZE
    printf("SUBSECTOR LOAD map %d subsectors x 0x%03x : size = %08x\n", numsubsectors, (int)sizeof(subsector_t), numsubsectors*(int)sizeof(subsector_t));
#endif
    data = W_CacheLumpNum (lump,PU_LEVEL);
    subsectors = (subsector_t *)data;
#else
    numsubsectors = W_LumpLength (lump) / sizeof(mapsubsector_t);
#if PRINT_LEVEL_SIZE
    printf("SUBSECTOR LOAD alloc %d subsectors x 0x%03x : size = %08x\n", numsubsectors, (int)sizeof(subsector_t), numsubsectors*(int)sizeof(subsector_t));
#endif
    subsectors = Z_Malloc (numsubsectors*sizeof(subsector_t),PU_LEVEL,0);
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    ms = (mapsubsector_t *)data;
    memset (subsectors,0, numsubsectors*sizeof(subsector_t));
    ss = subsectors;
    
    for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
    {
	ss->numlines = SHORT(ms->numsegs);
	ss->firstline = SHORT(ms->firstseg);
    }
	
    W_ReleaseLumpNum(lump);
#endif
}



//
// P_LoadSectors
//
void P_LoadSectors (int lump)
{
    should_be_const byte*		data;
    int			i;
    sector_t*		ss;

    data = W_CacheLumpNum (lump,PU_STATIC);

#if !USE_WHD
    numsectors = W_LumpLength (lump) / sizeof(mapsector_t);
#if PRINT_LEVEL_SIZE
    printf("SECTOR LOAD alloc %d sectors x 0x%03x : size = %08x\n", numsectors, (int)sizeof(sector_t), numsectors*(int)sizeof(sector_t));
#endif
    sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);
    memset (sectors, 0, numsectors*sizeof(sector_t));
    mapsector_t *ms = (mapsector_t *)data;
    ss = sectors;
    for (i=0 ; i<numsectors ; i++, ss++, ms++)
    {
        sector_set_floorheight(ss, SHORT(ms->floorheight)<<FRACBITS);
        sector_set_ceilingheight(ss, SHORT(ms->ceilingheight)<<FRACBITS);
	ss->floorpic = R_FlatNumForName(ms->floorpic);
	ss->ceilingpic = R_FlatNumForName(ms->ceilingpic);
	ss->lightlevel = SHORT(ms->lightlevel);
	ss->special = SHORT(ms->special);
	ss->tag = SHORT(ms->tag);
	ss->thinglist = 0;
    }
#else
    numsectors = W_LumpLength (lump) / sizeof(whdsector_t);
#if PRINT_LEVEL_SIZE
    printf("SECTOR LOAD alloc %d sectors x 0x%03x : size = %08x\n", numsectors, (int)sizeof(sector_t), numsectors*(int)sizeof(sector_t));
#endif
    sectors = Z_Malloc (numsectors*sizeof(sector_t),PU_LEVEL,0);
    memset (sectors, 0, numsectors*sizeof(sector_t));
    whdsector_t *whss = (whdsector_t *)data;
#if !NO_USE_SAVE
    whd_sectors = whss;
#endif
    ss = sectors;
    for (i=0 ; i<numsectors ; i++, ss++, whss++)
    {
        sector_set_floorheight(ss, SHORT(whss->floorheight)<<FRACBITS);
        sector_set_ceilingheight(ss, SHORT(whss->ceilingheight)<<FRACBITS);
        ss->floorpic = whss->floorpic;
        ss->ceilingpic = whss->ceilingpic;
	ss->lightlevel = whss->lightlevel;
	ss->special = whss->special;
	ss->tag = whss->tag;
	ss->thinglist = 0;
    }
#endif
	
    W_ReleaseLumpNum(lump);
}


//
// P_LoadNodes
//
void P_LoadNodes (int lump)
{
    should_be_const byte*	data;
    int		i;
    int		j;
    int		k;
    mapnode_t*	mn;
    node_t*	no;

#if !WHD_SUPER_TINY
    numnodes = W_LumpLength (lump) / sizeof(mapnode_t);
#else
    numnodes = W_LumpLength (lump) / sizeof(whdnode_t);
#endif

#if USE_RAW_MAPNODE
    data = W_CacheLumpNum (lump,PU_LEVEL);
    nodes = (node_t *)data;
#else
    data = W_CacheLumpNum (lump,PU_STATIC);
    mn = (mapnode_t *)data;
    // todo rowad
    nodes = Z_Malloc (numnodes*sizeof(node_t),PU_LEVEL,0);
    no = (node_t *)nodes;
    
    for (i=0 ; i<numnodes ; i++, no++, mn++)
    {
	no->x = int16_to_node_coord(SHORT(mn->x));
	no->y = int16_to_node_coord(SHORT(mn->y));
	no->dx = int16_to_node_coord(mn->dx);
	no->dy = int16_to_node_coord(mn->dy);
	for (j=0 ; j<2 ; j++)
	{
	    no->children[j] = SHORT(mn->children[j]);
	    for (k=0 ; k<4 ; k++)
		no->bbox[j][k] = int16_to_node_coord(mn->bbox[j][k]);
	}
    }

    W_ReleaseLumpNum(lump);
#endif
}


//
// P_LoadThings
//
void P_LoadThings (int lump)
{
    should_be_const byte               *data;
    int			i;
    const mapthing_t         *mt;
    mapthing_t          spawnthing;
    int			numthings;
    boolean		spawn;

    data = W_CacheLumpNum (lump,PU_STATIC);
    numthings = W_LumpLength (lump) / sizeof(mapthing_t);

    mt = (mapthing_t *)data;
#if USE_RAW_MAPTHING
    mapthings = mt;
    nummapthings = numthings;
#endif
    for (i=0 ; i<numthings ; i++, mt++)
    {
	spawn = true;

	// Do not spawn cool, new monsters if !commercial
	if (gamemode != commercial)
	{
	    switch (SHORT(mt->type))
	    {
	      case 68:	// Arachnotron
	      case 64:	// Archvile
	      case 88:	// Boss Brain
	      case 89:	// Boss Shooter
	      case 69:	// Hell Knight
	      case 67:	// Mancubus
	      case 71:	// Pain Elemental
	      case 65:	// Former Human Commando
	      case 66:	// Revenant
	      case 84:	// Wolf SS
		spawn = false;
		break;
	    }
	}
	if (spawn == false)
	    break;

#if !USE_RAW_MAPTHING
    // Do spawn all other stuff.
	spawnthing.x = SHORT(mt->x);
	spawnthing.y = SHORT(mt->y);
	spawnthing.angle = SHORT(mt->angle);
	spawnthing.type = SHORT(mt->type);
	spawnthing.options = SHORT(mt->options);
#endif

#if !SHRINK_MOBJ
	P_SpawnMapThing(spawnthing);
#else
    P_SpawnMapThing(i);
#endif

    }

    if (!deathmatch)
    {
        for (i = 0; i < MAXPLAYERS; i++)
        {
            if (playeringame[i] && !playerstartsingame[i])
            {
                I_Error("P_LoadThings: Player %d start missing (vanilla crashes here)", i + 1);
            }
            playerstartsingame[i] = false;
        }
    }

#if !USE_RAW_MAPTHING
    W_ReleaseLumpNum(lump);
#endif
}


//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//
void P_LoadLineDefs (int lump)
{
    should_be_const byte*		data;
    int			i;
    maplinedef_t*	mld;
    line_t*		ld;
    const vertex_t*		v1;
    const vertex_t*		v2;
	
#if USE_RAW_MAPLINEDEF
    data = W_CacheLumpNum (lump,PU_LEVEL);
#if !WHD_SUPER_TINY
    numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
    static_assert(sizeof(line_t) == sizeof(maplinedef_t), "");
#if PRINT_LEVEL_SIZE
    printf("LINE LOAD map %d linedefs x 0x%03x : size = %08x\n", numlines, (int)sizeof(maplinedef_t), numlines*(int)sizeof(maplinedef_t));
#endif
    lines = (line_t *) data;
    unsigned int bitmap_size = (numlines + 31) / 8;
#else
    numlines = ((uint16_t *)data)[0];
    whd_sidemul = ((uint16_t *)data)[1];
    whd_vmul = ((uint16_t *)data)[2];
    lines = (line_t *) (data + 6);
    numlines5 = line_bitmap_index(W_LumpLength(lump) - 6);
#if PRINT_LEVEL_SIZE
    printf("LINE LOAD map %d linedefs : size = %08x\n", numlines, W_LumpLength(lump));
#endif
    unsigned int bitmap_size = (numlines5 + 31) / 8;
#endif
    line_sector_check_bitmap = Z_Malloc(bitmap_size, PU_LEVEL,0);
    assert(numlines >= numsectors); // would seem to make sense
    line_mapped_bitmap = Z_Malloc(bitmap_size, PU_LEVEL,0);
    memset(line_mapped_bitmap, 0, bitmap_size);
    line_special_cleared_bitmap = Z_Malloc(bitmap_size, PU_LEVEL,0);
    memset(line_special_cleared_bitmap, 0, bitmap_size);
#else
    numlines = W_LumpLength (lump) / sizeof(maplinedef_t);
#if PRINT_LEVEL_SIZE
    printf("LINE LOAD alloc %d linedefs x 0x%03x : size = %08x\n", numlines, (int)sizeof(line_t), numlines*(int)sizeof(line_t));
#endif
    lines = Z_Malloc (numlines*sizeof(line_t),PU_LEVEL,0);
    // todo rowad
    memset ((line_t *)lines, 0, numlines*sizeof(line_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    mld = (maplinedef_t *)data;
    // todo rowad
    ld = (line_t*)lines;
    for (i=0 ; i<numlines ; i++, mld++, ld++)
    {
	ld->flags = SHORT(mld->flags);
	ld->special = SHORT(mld->special);
	ld->tag = SHORT(mld->tag);
	v1 = ld->v1 = &vertexes[SHORT(mld->v1)];
	v2 = ld->v2 = &vertexes[SHORT(mld->v2)];
	ld->dx = vertex_x(v2) - vertex_x(v1);
	ld->dy = vertex_y(v2) - vertex_y(v1);
	
	if (!ld->dx)
	    ld->slopetype = ST_VERTICAL;
	else if (!ld->dy)
	    ld->slopetype = ST_HORIZONTAL;
	else
	{
	    if (FixedDiv (ld->dy , ld->dx) > 0)
		ld->slopetype = ST_POSITIVE;
	    else
		ld->slopetype = ST_NEGATIVE;
	}
		
	if (vertex_x(v1) < vertex_x(v2))
	{
	    ld->bbox[BOXLEFT] = vertex_x(v1);
	    ld->bbox[BOXRIGHT] = vertex_x(v2);
	}
	else
	{
	    ld->bbox[BOXLEFT] = vertex_x(v2);
	    ld->bbox[BOXRIGHT] = vertex_x(v1);
	}

	if (vertex_y(v1) < vertex_y(v2))
	{
	    ld->bbox[BOXBOTTOM] = vertex_y(v1);
	    ld->bbox[BOXTOP] = vertex_y(v2);
	}
	else
	{
	    ld->bbox[BOXBOTTOM] = vertex_y(v2);
	    ld->bbox[BOXTOP] = vertex_y(v1);
	}

	ld->sidenum[0] = SHORT(mld->sidenum[0]);
	ld->sidenum[1] = SHORT(mld->sidenum[1]);

	if (ld->sidenum[0] != -1)
	    ld->frontsector = sides[ld->sidenum[0]].sector;
	else
	    ld->frontsector = 0;

	if (ld->sidenum[1] != -1)
	    ld->backsector = sides[ld->sidenum[1]].sector;
	else
	    ld->backsector = 0;
    }

    W_ReleaseLumpNum(lump);
#endif
}

//
// P_LoadSideDefs
//
void P_LoadSideDefs (int lump)
{
#if USE_WHD
    num_switched_sides = 0;
#if WHD_SUPER_TINY
#if PRINT_LEVEL_SIZE
    printf("SIDEDEF LOAD map ? sides size = %08x\n", W_LumpLength(lump));
#endif
    sides_z = (const side_t *) W_CacheLumpNum (lump,PU_LEVEL);
#else
    static_assert(sizeof(side_t)==12, "");
    numsides = W_LumpLength (lump) / sizeof(side_t); // todo should really be packed?
#if PRINT_LEVEL_SIZE
    printf("SIDEDEF LOAD map %d sides x 0x%03x : size = %08x\n", numsides, (int)sizeof(side_t), numsides * (int)sizeof(side_t));
#endif
    sides = (const side_t *) W_CacheLumpNum (lump,PU_LEVEL);
#endif
#else
    should_be_const byte*		data;
    int			i;
    mapsidedef_t*	msd;
    side_t*		sd;
	
    numsides = W_LumpLength (lump) / sizeof(mapsidedef_t);
#if PRINT_LEVEL_SIZE
    printf("SIDEDEF ** HAS NAMES ** LOAD alloc %d sides x 0x%03x : size = %08x\n", numsides, (int)sizeof(side_t), numsides * (int)sizeof(side_t));
#endif
    sides = Z_Malloc (numsides*sizeof(side_t),PU_LEVEL,0);
    memset (sides, 0, numsides*sizeof(side_t));
    data = W_CacheLumpNum (lump,PU_STATIC);
	
    msd = (mapsidedef_t *)data;
    sd = sides;
    for (i=0 ; i<numsides ; i++, msd++, sd++)
    {
        side_settextureoffset16(sd, SHORT(msd->textureoffset));
        side_setrowoffset16(sd, SHORT(msd->rowoffset));
        side_settoptexture(sd, R_TextureNumForName(msd->toptexture));
        side_setbottomtexture(sd, R_TextureNumForName(msd->bottomtexture));
	    side_setmidtexture(sd, R_TextureNumForName(msd->midtexture));
	    sd->sector = &sectors[SHORT(msd->sector)];
    }

    W_ReleaseLumpNum(lump);
#endif
}


//
// P_LoadBlockMap
//
void P_LoadBlockMap (int lump)
{
    int i;
    int count;


    // Swap all short integers to native byte ordering.

#if !USE_ROWAD
    int lumplen = W_LumpLength(lump);
    count = lumplen / 2;
    blockmaplump = Z_Malloc(lumplen, PU_LEVEL, 0);
    W_ReadLump(lump, blockmaplump);

    for (i=0; i<count; i++)
    {
	blockmaplump[i] = SHORT(blockmaplump[i]);
    }
#else
#ifdef SYS_BIG_ENDIAN
#error wrong format
#endif
    blockmaplump = W_CacheLumpNum(lump, PU_LEVEL);
#endif
#if !USE_WHD
    blockmap = blockmaplump + 4;
#else
    blockmap_whd = (uint8_t *)(blockmaplump + 4);
#endif

    // Read the header

    bmaporgx = blockmaplump[0]<<FRACBITS;
    bmaporgy = blockmaplump[1]<<FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];

#if PRINT_LEVEL_SIZE
#if !USE_WHD
#if USE_ROWAD
    printf("BLOCKMAP load map %d blocks x 0x%03x : size = %08x\n", count, (int)sizeof(short ), count*(int)sizeof(short));
#else
    printf("BLOCKMAP load alloc %d vertexes x 0x%03x : size = %08x\n", count, (int)sizeof(short ), count*(int)sizeof(short));
#endif
#else
    printf("BLOCKMAP load map %dx%d size = %08x\n", bmapwidth, bmapheight, W_LumpLength(lump));
#endif
#endif

    // Clear out mobj chains

    count = sizeof(*blocklinks) * bmapwidth * bmapheight;
#if PRINT_LEVEL_SIZE
    printf("BLOCKMAP %dx%d\n", bmapwidth, bmapheight);
    printf("BLOCKLINKS alloc %d links x 0x%03x : size = %08x\n", bmapwidth * bmapheight, (int)sizeof(*blocklinks), count);
#endif

    blocklinks = Z_Malloc(count, PU_LEVEL, 0);
    memset(blocklinks, 0, count);
}



//
// P_lLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void P_GroupLines (void)
{
    int			i;
    int			j;
    line_t*		li;
    sector_t*		sector;
    subsector_t*	ss;
    seg_t*		seg;
    fixed_t		bbox[4];
    int			block;

#if !USE_WHD
    // look up sector number for each subsector
    ss = subsectors;
    for (i=0 ; i<numsubsectors ; i++, ss++)
    {
	seg = &segs[ss->firstline];
	ss->sector = seg_sidedef(seg)->sector;
    }
#endif

    // count number of lines in each sector
    li = lines;
    totallines = 0;
    for (i = 0; i < numlines; i++) {
        totallines++;
        line_frontsector(li)->linecount++;

        if (line_backsector(li) && line_backsector(li) != line_frontsector(li)) {
            line_backsector(li)->linecount++;
            totallines++;
        }
        li += line_next_step(li);
    }

    // build line tables for each sector
#if !USE_INDEX_LINEBUFFER
#if PRINT_LEVEL_SIZE
    printf("LINEBUFFER alloc %d total lines x 0x%03x : size = %08x\n", totallines, (int)sizeof(line_t *), totallines*(int)sizeof(line_t *));
#endif
    linebuffer = Z_Malloc (totallines*sizeof(line_t *), PU_LEVEL, 0);
#else
#if PRINT_LEVEL_SIZE
    printf("LINEBUFFER alloc %d total lines x 0x%03x : size = %08x\n", totallines, (int)sizeof(short), totallines*(int)sizeof(short));
#endif
    linebuffer = Z_Malloc (totallines*sizeof(cardinal_t), PU_LEVEL, 0);
#endif

#if USE_INDEX_LINEBUFFER
    totallines=0;
#endif
    for (i=0; i<numsectors; ++i)
    {
        // Assign the line buffer for this sector

#if !USE_INDEX_LINEBUFFER
        sectors[i].lines = linebuffer;
        linebuffer += sectors[i].linecount;
#else
        sectors[i].line_index = totallines;
        totallines += sectors[i].linecount;
#endif

        // Reset linecount to zero so in the next stage we can count
        // lines into the list.

        sectors[i].linecount = 0;
    }

    // Assign lines to sectors
    li = lines;
    for (i=0; i<numlines; ++i)
    { 
        if (line_frontsector(li) != NULL)
        {
            sector = line_frontsector(li);

#if !USE_INDEX_LINEBUFFER
            sector->lines[sector->linecount] = li;
#else
            linebuffer[sector->line_index + sector->linecount] = li - lines;
#endif
            ++sector->linecount;
        }

        if (line_backsector(li) != NULL && line_frontsector(li) != line_backsector(li))
        {
            sector = line_backsector(li);

#if !USE_INDEX_LINEBUFFER
            sector->lines[sector->linecount] = li;
#else
            linebuffer[sector->line_index + sector->linecount] = li - lines;
#endif
            ++sector->linecount;
        }
        li += line_next_step(li);
    }
    
    // Generate bounding boxes for sectors
	
    sector = sectors;
    for (i=0 ; i<numsectors ; i++, sector++)
    {
	M_ClearBox (bbox);

	for (j=0 ; j<sector->linecount; j++)
	{
            li = sector_line(sector, j);

            M_AddToBox (bbox, vertex_x(line_v1(li)), vertex_y(line_v1(li)));
            M_AddToBox (bbox, vertex_x(line_v2(li)), vertex_y(line_v2(li)));
	}

	// set the degenmobj_t to the middle of the bounding box
	sector->soundorg.x = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;
	sector->soundorg.y = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;
		
	// adjust bounding box to map blocks
	block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapheight ? bmapheight-1 : block;
	sector->blockbox[BOXTOP]=block;

	block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXBOTTOM]=block;

	block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapwidth ? bmapwidth-1 : block;
	sector->blockbox[BOXRIGHT]=block;

	block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXLEFT]=block;
    }
	
}

// Pad the REJECT lump with extra data when the lump is too small,
// to simulate a REJECT buffer overflow in Vanilla Doom.

static void PadRejectArray(byte *array, unsigned int len)
{
    unsigned int i;
    unsigned int byte_num;
    byte *dest;
    unsigned int padvalue;

    // Values to pad the REJECT array with:

    unsigned int rejectpad[4] =
    {
        0,                                    // Size
        0,                                    // Part of z_zone block header
        50,                                   // PU_LEVEL
        0x1d4a11                              // DOOM_CONST_ZONEID
    };

    rejectpad[0] = ((totallines * 4 + 3) & ~3) + 24;

    // Copy values from rejectpad into the destination array.

    dest = array;

    for (i=0; i<len && i<sizeof(rejectpad); ++i)
    {
        byte_num = i % 4;
        *dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
        ++dest;
    }

    // We only have a limited pad size.  Print a warning if the
    // REJECT lump is too small.

    if (len > sizeof(rejectpad))
    {
        stderr_print("PadRejectArray: REJECT lump too short to pad! (%i > %i)\n",
                        len, (int) sizeof(rejectpad));

        // Pad remaining space with 0 (or 0xff, if specified on command line).

        if (M_CheckParm("-reject_pad_with_ff"))
        {
            padvalue = 0xff;
        }
        else
        {
            padvalue = 0xf00;
        }

        memset(array + sizeof(rejectpad), padvalue, len - sizeof(rejectpad));
    }
}

static void P_LoadReject(int lumpnum)
{
    int minlength;
    int lumplen;

    // Calculate the size that the REJECT lump *should* be.

    minlength = (numsectors * numsectors + 7) / 8;

    // If the lump meets the minimum length, it can be loaded directly.
    // Otherwise, we need to allocate a buffer of the correct size
    // and pad it with appropriate data.

    lumplen = W_LumpLength(lumpnum);

    if (lumplen >= minlength)
    {
        rejectmatrix = W_CacheLumpNum(lumpnum, PU_LEVEL);
    }
    else
    {
#if !USE_WHD
        byte* tmp = Z_Malloc(minlength, PU_LEVEL, &rejectmatrix);
        W_ReadLump(lumpnum, tmp);

        PadRejectArray(tmp + lumplen, minlength - lumplen);
        rejectmatrix = tmp;
#else
        // we don't care about invalid reject lumps (laos we don't handle z_malloc user ptr
        assert(false);
#endif
    }
}

// pointer to the current map lump info struct
should_be_const lumpinfo_t *maplumpinfo;

//
// P_SetupLevel
//
void
P_SetupLevel
( int		episode,
  int		map,
  int		playermask,
  skill_t	skill)
{
    int		i;
    char	lumpname[9];
    int		lumpnum;

#if PICO_DOOM_INFO
    printf("SETUP LEVEL E%dM%d\n", episode, map);
#endif
	
    totalkills = totalitems = totalsecret = wminfo.maxfrags = 0;
    wminfo.partime = 180;
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	players[i].killcount = players[i].secretcount 
	    = players[i].itemcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].viewz = 1; 

    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start ();			

    Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);

    // UNUSED W_Profile ();
    P_InitThinkers ();

#if !NO_USE_RELOAD
    // if working with a devlopment map, reload it
    W_Reload ();
#endif

    // find map name
    if ( gamemode == commercial)
    {
	if (map<10)
	    DEH_snprintf(lumpname, 9, "map0%i", map);
	else
	    DEH_snprintf(lumpname, 9, "map%i", map);
    }
    else
    {
	lumpname[0] = 'E';
	lumpname[1] = '0' + episode;
	lumpname[2] = 'M';
	lumpname[3] = '0' + map;
	lumpname[4] = 0;
    }

    lumpnum = W_GetNumForName (lumpname);
	
    maplumpinfo = lump_info(lumpnum);

    leveltime = 0;

    // note: most of this ordering is important
    P_LoadBlockMap (lumpnum+ML_BLOCKMAP);
    P_LoadVertexes (lumpnum+ML_VERTEXES);
    P_LoadSectors (lumpnum+ML_SECTORS);
    P_LoadSideDefs (lumpnum+ML_SIDEDEFS);

    P_LoadLineDefs (lumpnum+ML_LINEDEFS);
    P_LoadSubsectors (lumpnum+ML_SSECTORS);
    P_LoadNodes (lumpnum+ML_NODES);
    P_LoadSegs (lumpnum+ML_SEGS);

    P_GroupLines ();
    P_LoadReject (lumpnum+ML_REJECT);

    bodyqueslot = 0;
    deathmatch_p = deathmatchstarts;
    P_LoadThings (lumpnum+ML_THINGS);
    
    // if deathmatch, randomly spawn the active players
    if (deathmatch)
    {
	for (i=0 ; i<MAXPLAYERS ; i++)
	    if (playeringame[i])
	    {
		players[i].mo = NULL;
		G_DeathMatchSpawnPlayer (i);
	    }
			
    }

    // clear special respawning que
    iquehead = iquetail = 0;		
	
    // set up world state
    P_SpawnSpecials ();
	
    // build subsector connect matrix
    //	UNUSED P_ConnectSubsectors ();

    // preload graphics
    if (precache)
	R_PrecacheLevel ();

    //printf ("free memory: 0x%x\n", Z_FreeMemory());

}



//
// P_Init
//
void P_Init (void)
{
    P_InitSwitchList ();
    P_InitPicAnims ();
    R_InitSprites ();
}



