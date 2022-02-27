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
//      Refresh/rendering module, shared data struct definitions.
//


#ifndef __R_DEFS__
#define __R_DEFS__


// Screenwidth.
#include "doomdef.h"

// Some more or less basic data types
// we depend on.
#include "m_fixed.h"
#include "tables.h"

// We rely on the thinker data struct
// to handle sound origins in sectors.
#include "d_think.h"
// SECTORS do store MObjs anyway.
#include "doomdata.h"
#include "whddata.h"

#include "i_video.h"

#include "v_patch.h"

// xy position, but also noting that the address of this can
// be used to identify an object which has an XY position
typedef struct xy_positioned_s {
    fixed_t x, y;
} xy_positioned_t;

#if !NO_DRAWSEGS
// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE		0
#define SIL_BOTTOM		1
#define SIL_TOP			2
#define SIL_BOTH		3

#define MAXDRAWSEGS		256
#endif


//
// INTERNAL MAP TYPES
//  used by play and refresh
//

//
// Your plain vanilla vertex.
// Note: transformed values not buffered locally,
//  like some DOOM-alikes ("wt", "WebView") did.
//
#if !USE_RAW_MAPVERTEX
typedef struct
{
    fixed_t x;
    fixed_t y;
} vertex_t;
#else
typedef const mapvertex_t vertex_t;
#endif

// Forward of LineDefs, for Sectors.
struct line_s;

typedef struct mobj_s mobj_t;

// floor/ceiling height
#if DOOM_SMALL
typedef int16_t sectorheight_t;
#else
typedef fixed_t sectorheight_t;
#endif
//
// The SECTORS record, at runtime.
// Stores things/mobjs.
//
typedef	struct
{
    sectorheight_t 	rawfloorheight; // mutable (sometimes)
    sectorheight_t 	rawceilingheight; // mutable (sometimes)
    short	lightlevel; // mutable // todo seems like this could be 4 bits
    // 0 = untraversed, 1,2 = sndlines -1
#if !DOOM_SMALL
int		soundtraversed; // temporary used by P_RecurseiveSound
#else
uint8_t     soundtraversed:2; // actually i think it is only ever 0 or 1
#endif
#if !DOOM_SMALL
    short	special;
#else
    uint8_t     special:5;
#endif

#if !DOOM_SMALL
    short	floorpic; // mutable (sometimes)
    short	ceilingpic; // seems not to be mutable
#else
    uint8_t     floorpic;
    uint8_t     ceilingpic; // (CAN BE CONST)
#endif

    short	tag; // immutable; how big? (CAN BE CONST)
    cardinal_t 	linecount; // seems unlikely to be more than 8 bit really (CAN BE CONST)

    // thing that made a sound (or null)
    shortptr_t /*mobj_t*/	soundtarget;

#if !DOOM_SMALL
    // mapblock bounding box for height changes
    int		blockbox[4];
#else
    uint8_t     blockbox[4]; // (CAN BE CONST)
#endif

    // list of mobjs in sector
    shortptr_t /*mobj_t*/thinglist;

    // thinker_t for reversable actions
    shortptr_t /*void*/	specialdata;

#if USE_INDEX_LINEBUFFER
    cardinal_t	line_index;	// within linebuffer of first line (CAN BE CONST)
#else
    rowad_const struct line_s**	lines;	// [linecount] size
#endif

    // origin for any sounds played by the sector
    xy_positioned_t 	soundorg; // middle of bbox (CAN BE CONST)

#if !DOOM_SMALL
    // if == validcount, already checked
    int		validcount;
#endif
} sector_t;
#if !DOOM_SMALL
#define sector_check_reset() ((void)0)

static inline boolean sector_validcount_update_check(should_be_const sector_t *s, int validcount) {
    if (validcount == s->validcount) {
        return true;
    }
    hack_rowad_p(sector_t, s, validcount) = validcount;
    return false;
}
#else
extern uint32_t *line_sector_check_bitmap; // shared by both line and sector checking - todo actually just alloc on the fly
void sector_check_reset(void);
boolean sector_validcount_update_check_impl(should_be_const  sector_t *s);
#define sector_validcount_update_check(s, vc) sector_validcount_update_check_impl(s)
#endif
#if !DOOM_SMALL
#define sector_floorheight(s) ((s)->rawfloorheight)
#define sector_ceilingheight(s) ((s)->rawceilingheight)
#define sector_set_floorheight(s, h) (s)->rawfloorheight = h
#define sector_set_ceilingheight(s, h) (s)->rawceilingheight = h
#define sector_delta_floorheight(s, d) (s)->rawfloorheight += d
#define sector_delta_ceilingheight(s, d) (s)->rawceilingheight += d
#else
#define SECTORHEIGHT_SHIFT 14
#define SECTORHEIGHT_ZERO_MASK (1u << (SECTORHEIGHT_SHIFT - 1))
#define sector_floorheight(s) ((s)->rawfloorheight << SECTORHEIGHT_SHIFT)
#define sector_ceilingheight(s) ((s)->rawceilingheight << SECTORHEIGHT_SHIFT)
static inline void sector_set_floorheight(sector_t *s, fixed_t h) {
    assert(!(h & SECTORHEIGHT_ZERO_MASK));
    s->rawfloorheight = h >> SECTORHEIGHT_SHIFT;
}
static inline void sector_delta_floorheight(sector_t *s, fixed_t d) {
    assert(!(d & SECTORHEIGHT_ZERO_MASK));
    s->rawfloorheight += d >> SECTORHEIGHT_SHIFT;
}
static inline void sector_set_ceilingheight(sector_t *s, fixed_t h) {
    assert(!(h & SECTORHEIGHT_ZERO_MASK));
    s->rawceilingheight = h >> SECTORHEIGHT_SHIFT;
}
static inline void sector_delta_ceilingheight(sector_t *s, fixed_t d) {
    assert(!(d & SECTORHEIGHT_ZERO_MASK));
    s->rawceilingheight += d >> SECTORHEIGHT_SHIFT;
}
#endif

#if PICO_ON_DEVICE
static_assert(sizeof(sector_t) == 0x24, "");
#endif

//
// The SideDef.
//

#if !USE_WHD
typedef struct
{
    // add this to the calculated texture column
    fixed_t	textureoffset;
    
    // add this to the calculated texture top
    fixed_t	rowoffset;

    // Texture indices.
    // We do not maintain names here. 
    short	toptexture;
    short	bottomtexture;
    short	midtexture;

    // Sector the SideDef is facing.
    sector_t*	sector;
} side_t;
#else
#if WHD_SUPER_TINY
typedef const uint8_t side_t; // these are compressed
#else
typedef const whdsidedef_t side_t;
#endif
#endif



//
// Move clipping aid for LineDefs.
//
typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE
} slopetype_t;

#if !USE_RAW_MAPLINEDEF
typedef struct line_s
{
    // Vertices, from v1 to v2.
    const vertex_t*	v1;
    const vertex_t*	v2;

    // Precalculated v2 - v1 for side checking.
    fixed_t	dx;
    fixed_t	dy;

    // Animation related.
    short	flags;
    short	special;
    short	tag;

    // Visual appearance: SideDefs.
    //  sidenum[1] will be -1 if one sided
    short	sidenum[2];			

    // Neat. Another bounding box, for the extent
    //  of the LineDef.
    fixed_t	bbox[4];

    // To aid move clipping.
    slopetype_t	slopetype;

    // Front and back sector.
    // Note: redundant? Can be retrieved from SideDefs.
    sector_t*	frontsector;
    sector_t*	backsector;

    // if == validcount, already checked
    int		validcount; // ACK_MUTATED */

#if !DOOM_SMALL
    // thinker_t for reversable actions
    void*	specialdata;
#endif
} line_t;
typedef line_t fake_line_t;
#define fakeline_to_line(fake) (fake)
static inline void init_fake_line(fake_line_t *line, int tag) { line->tag = tag; }
#else
#if WHD_SUPER_TINY
typedef const uint8_t line_t;
typedef struct {
    uint8_t data[6];
} fake_line_t;
static inline void init_fake_line(fake_line_t *line, int tag) {
    line->data[1] = ML_HAS_TAG >> 8;
    assert(tag >=0 && tag <256);
    line->data[5] = tag;
}
#define fakeline_to_line(fake) (fake)->data
#else
typedef const maplinedef_t line_t;
typedef maplinedef_t fake_line_t; // just used for a temp junk instance
#define fakeline_to_line(fake) (fake)
static inline void init_fake_line(fake_line_t *line, int tag) { line->tag = tag; }
#endif
#endif

//
// A SubSector.
// References a Sector.
// Basically, this is a list of LineSegs,
//  indicating the visible walls that define
//  (all or some) sides of a convex BSP leaf.
//
#if !USE_WHD
typedef struct subsector_s
{
    sector_t*	sector;
    short	numlines;
    short	firstline;
} subsector_t;
#else
typedef const uint16_t subsector_t;
#endif

//
// The LineSeg.
//
#if !USE_RAW_MAPSEG
typedef struct
{
    const vertex_t*	v1;
    const vertex_t*	v2;
    
    fixed_t	offset;

    angle_t	angle;

    side_t*	sidedef;
    line_t*	linedef;

    // Sector references.
    // Could be retrieved from linedef, too.
    // backsector is NULL for one sided lines
    sector_t*	frontsector;
    sector_t*	backsector;
} seg_t;
#else
#if !WHD_SUPER_TINY
typedef const mapseg_t seg_t;
#else
typedef const uint8_t seg_t;
#endif
#endif

//
// BSP node.
//
#if !USE_RAW_MAPNODE
typedef fixed_t node_coord_t;
typedef struct
{
    // Partition line.
    node_coord_t	x;
    node_coord_t	y;
    node_coord_t	dx;
    node_coord_t	dy;

    // Bounding box for each child.
    node_coord_t	bbox[2][4];

    // If NF_SUBSECTOR its a subsector.
    unsigned short children[2];

} node_t;

#define node_coord_to_fixed(n) (n)
#define node_coord_to_int16(n) ((n) >> FRACBITS)
#define int16_to_node_coord(n) ((n) << FRACBITS)
#else
typedef int16_t node_coord_t;
#if !WHD_SUPER_TINY
typedef const mapnode_t node_t;
#else
typedef const whdnode_t node_t;
#endif
#define node_coord_to_fixed(n) (((fixed_t)(n)) << FRACBITS)
#define node_coord_to_int16(n) (n)
#define int16_to_node_coord(n) (n)
#endif
// PC direct to screen pointers
//B UNUSED - keep till detailshift in r_draw.c resolved
//extern byte*	destview;
//extern byte*	destscreen;





//
// OTHER TYPES
//

// This could be wider for >8 bit display.
// Indeed, true color support is posibble
//  precalculating 24bpp lightmap/colormap LUT.
//  from darkening PLAYPAL to all black.
// Could even us emore than 32 levels.
typedef pixel_t		lighttable_t;



#ifndef NO_DRAWSEGS
//
// ?
//
typedef struct drawseg_s
{
    seg_t*		curline;
    int			x1;
    int			x2;

    fixed_t		scale1;
    fixed_t		scale2;
    fixed_t		scalestep;

    // 0=none, 1=bottom, 2=top, 3=both
    int			silhouette;

    // do not clip sprites above this
    fixed_t		bsilheight;

    // do not clip sprites below this
    fixed_t		tsilheight;
    
    // Pointers to lists for sprite clipping,
    //  all three adjusted so [x1] is first value.
    short*		sprtopclip;		
    short*		sprbottomclip;	
    short*		maskedtexturecol;
    
} drawseg_t;
#endif


// A vissprite_t is a thing
//  that will be drawn during a refresh.
// I.e. a sprite object that is partly visible.
typedef struct vissprite_s
{
    // Doubly linked list.
    struct vissprite_s*	prev;
    struct vissprite_s*	next;
    
    int			x1;
    int			x2;

#if !DOOM_TINY
    // for line side calculation
    fixed_t		gx;
    fixed_t		gy;		

    // global bottom / top for silhouette clipping
    fixed_t		gz;
    fixed_t		gzt;
#endif

    // horizontal position of x1
    fixed_t		startfrac;
    
    fixed_t		scale;
    
    // negative if flipped
    fixed_t		xiscale;	

    fixed_t		texturemid;
    int			patch;

    // for color translation and shadow draw,
    //  maxbright frames as well
#if !USE_LIGHTMAP_INDEXES
    const lighttable_t*	colormap;
#else
    int8_t colormap;
#endif
   
    int			mobjflags;
    
} vissprite_t;

//	
// Sprites are patches with a special naming convention
//  so they can be recognized by R_InitSprites.
// The base name is NNNNFx or NNNNFxFx, with
//  x indicating the rotation, x = 0, 1-7.
// The sprite and frame specified by a thing_t
//  is range checked at run time.
// A sprite is a patch_t that is assumed to represent
//  a three dimensional object and may have multiple
//  rotations pre drawn.
// Horizontal flipping is used to save space,
//  thus NNNNF2F5 defines a mirrored patch.
// Some sprites will only have one picture used
// for all views: NNNNF0
//
#if !USE_WHD
typedef struct
{
    // Lump to use for view angles 0-7.
    // todo graham, seems like this is actually a spritenum
    lumpindex_t 	lump[8];

    // If false use 0 for any position.
    // Note: as eight entries are available,
    //  we might as well insert the same name eight times.
    // todo graham actually lets use a small spriteframe instead
    boolean	rotate;

    #if !DOOM_SMALL
    // Flip bit (1 = flip) to use for view angles 0-7.
    byte	flip[8];
    #else
    byte flips;
    #endif
    
} spriteframe_t;
typedef spriteframe_t *spriteframeref_t;
#else
typedef uint16_t spriteframeref_t;
#endif

#if !USE_WHD
//
// A sprite definition:
//  a number of animation frames.
//
typedef struct
{
    int			numframes;
    spriteframe_t*	spriteframes;
} spritedef_t;
#endif

#if PICODOOM_RENDER_BABY
extern uint8_t render_frame_index;
#endif

#if !NO_VISPLANES

//
// Now what is a visplane, anyway?
// 
typedef struct
{
  fixed_t		height;
  isb_int16_t 		lightlevel;
  flatnum_t     	picnum;
#if PICODOOM_RENDER_BABY
  uint8_t               used_by;
#endif
#if !NO_VISPLANE_GUTS
  int			minx;
  int			maxx;
  
  // leave pads for [minx-1]/[maxx+1]
  
  byte		pad1;
  // Here lies the rub for all
  //  dynamic resize/change of resolution.
  byte		top[SCREENWIDTH];
  byte		pad2;
  byte		pad3;
  // See above.
  byte		bottom[SCREENWIDTH];
  byte		pad4;
#endif
} visplane_t;
#endif



#endif
