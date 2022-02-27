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
//	Refresh/render internal state variables (global).
//


#ifndef __R_STATE__
#define __R_STATE__

#if USE_WHD
#include "whddata.h"
extern const uint8_t popcount8_table[128];
extern const uint8_t bitcount8_table[256];
static inline uint8_t popcount8(uint8_t v) {
    return (v & 1) ? (popcount8_table[v/2] >> 4) : (popcount8_table[v/2] & 0xf);
}
static inline uint8_t bitcount8(uint8_t v) {
    return bitcount8_table[v];
}
#endif

// Need data structure definitions.
#include "d_player.h"
#include "r_data.h"

extern int		viewwidth;
extern int		viewheight;
extern int		scaledviewwidth;

extern const lighttable_t*	colormaps;

extern lumpindex_t 	firstflat;
// replacement mappings for for global animation
#if !USE_WHD
extern flatnum_t*	flattranslation;
extern texnum_t*		texturetranslation;
#define flat_translation(f) flattranslation[f]
#define texture_translation(t) texturetranslation[t]
#else
// note these are actually smaller types the flatnum_t/textnum_t as all the translatable flats/textures
// have been moved to the beginning (i.e. < 256)
extern flatname_t	whd_flattranslation[NUM_SPECIAL_FLATS];
extern const uint8_t *whd_specialtoflat;
#define whd_flattospecial (&whd_specialtoflat[NUM_SPECIAL_FLATS])
extern texturename_t 	whd_texturetranslation[NUM_SPECIAL_TEXTURES];
#define flat_translation(f) ((f)<NUM_SPECIAL_FLATS?whd_flattranslation[f]:(f))
#define texture_translation(t) ((t)<NUM_SPECIAL_TEXTURES?whd_texturetranslation[t]:(t))
#endif

// texture info
#if !USE_WHD
// needed for texture pegging
extern fixed_t*		textureheight;
#define texture_height(t) textureheight[t]
#else
extern cardinal_t numtextures;
extern const whdtexture_t *whd_textures;
#define texture_width(t) whd_textures[t].width
#define texture_height(t) (whd_textures[t].height << FRACBITS)
#endif

// Sprite....
#if !USE_WHD
extern fixed_t*		spritewidth;
extern fixed_t*		spriteoffset;
extern fixed_t*		spritetopoffset;
#define sprite_width(s) spritewidth[s]
#define sprite_offset(s) spriteoffset[s]
#define sprite_topoffset(s) spritetopoffset[s]
extern spritedef_t*	sprites;
#define sprite_sprdef(s) (&sprites[s])
#define sprite_numframes(s) sprite_sprdef(s)->numframes
#define sprite_frame(s,n) (&sprite_sprdef(s)->spriteframes[n])
#define spriteframe_rotates(f) (f)->rotate
#define spriteframe_unrotated_pic(f) (f)->lump[0]
#define spriteframe_rotated_pic(f,n) (f)->lump[n]
#if !DOOM_SMALL
#define spriteframe_rotated_flipped(f, n) (f)->flip[n]
#define spriteframe_unrotated_flipped(f) (f)->flip[0]
#else
#define spriteframe_rotated_flipped(f, n) (((f)->flips & (1u << (n))) != 0)
#define spriteframe_unrotated_flipped(f) spriteframe_rotated_flipped(f, 0)
#endif
#else
extern const int32_t *whd_sprite_meta;
extern const uint16_t *whd_sprite_frame_meta;
#define sprite_width(s) ((whd_sprite_meta[s]&0x3ff) << FRACBITS)
#define sprite_offset(s) ((whd_sprite_meta[s]>>21) << FRACBITS)
#define sprite_topoffset(s) (((whd_sprite_meta[s]<<11)>>21) << FRACBITS)
#define sprite_numframes(s) (whd_sprite_frame_meta[(s)+1] - whd_sprite_frame_meta[s])
#define sprite_sprdef(s) whd_sprite_frame_meta[s]
#define sprite_frame(s,n) whd_sprite_frame_meta[sprite_sprdef(s)+(n)]
#define spriteframe_rotates(f) (((f)&32768) != 0)
#define spriteframe_unrotated_pic(f) ((f)&0x7fff)
#define spriteframe_unrotated_flipped(f) 0
#define spriteframe_rotated_flipped(f, n) ((whd_sprite_frame_meta[((f)&0x7fff) + (n)] & 32768) != 0)
#define spriteframe_rotated_pic(f, n) (whd_sprite_frame_meta[((f)&0x7fff) + (n)] & 0x7fff)
#endif
extern lumpindex_t 		firstspritelump;
extern lumpindex_t 		lastspritelump;
extern lumpindex_t 		numspritelumps;

//
// Lookup tables for map data.
//
extern cardinal_t		numsprites;

extern cardinal_t		numvertexes;
extern vertex_t*	vertexes;

#if !WHD_SUPER_TINY
extern cardinal_t		numsegs;
#endif
extern seg_t*		segs;

extern cardinal_t		numsectors;
extern sector_t*	sectors;
#if LOAD_COMPRESSED || SAVE_COMPRESSED
extern whdsector_t *whd_sectors;
#endif

extern cardinal_t		numsubsectors;
extern subsector_t*	subsectors;

extern cardinal_t		numnodes;
extern node_t*		nodes;

#if !USE_INDEX_LINEBUFFER
extern line_t**		linebuffer;
#else
extern cardinal_t *	linebuffer;
#endif

#if !WHD_SUPER_TINY
#define bsp_child(n,w) (nodes[n]).children[w]
#else
static inline int bsp_child(int n, int w) {
    // w is 0 R, 1 L
    uint coded = nodes[n].coded_children;
    // code is 11ll llll lrrr rrrr  : l/r are 7 bit signed values; left leaf = 0x8000 + n - l; right leaf = 0x8000 + n - r
    //         10ll llll l--- ----  : right node is n- 1, left leaf via l as above
    //         01-- ---- -rrr rrrr  : left node is n - 1, right leaf via r is as above,
    //         00rr rrrr rrrr rrrr  : left node is n - 1, right node is n - r
    int v[2];
    if (coded & 0x8000) {
        if (w == 1) {
            return 0x8000 + n - (((int32_t)(coded << 18)) >> 25);
        } else if (coded & 0x4000) {
            return  0x8000 + n - (((int32_t)(coded << 25)) >> 25);
        } else {
            return n - 1;
        }
    } else {
        if (w == 1) {
            return n - 1;
        } else if (coded & 0x4000) {
            return 0x8000 + n - (((int32_t)(coded << 25)) >> 25);
        } else {
            return n - (coded & 0x3fffu);
        }
    }
}
#endif
extern cardinal_t		numlines;
#if WHD_SUPER_TINY
extern cardinal_t		numlines5;
// approximately lnum / 5
#define line_bitmap_index(lnum) (__fast_mul(lnum, 0x34)>>8)
#else
#endif
extern line_t*		lines;

#if !USE_WHD || !WHD_SUPER_TINY
extern cardinal_t		numsides;
extern side_t*		sides;
#else
extern const uint8_t *sides_z;
#endif


//
// POV data.
//
extern fixed_t		viewx;
extern fixed_t		viewy;
extern fixed_t		viewz;

extern angle_t		viewangle;
extern player_t*	viewplayer;

// ?
#if !FIXED_SCREENWIDTH
extern isb_int16_t	viewangletox[FINEANGLES/2];
extern angle_t		xtoviewangle[SCREENWIDTH+1];
#define x_to_viewangle(x) xtoviewangle[x]
#else
extern const int16_t		viewangletox[FINEANGLES/2];
extern const uint16_t		xtoviewangle_[SCREENWIDTH+1];
#define x_to_viewangle(x) (xtoviewangle_[x] << 16)
#endif
extern angle_t		clipangle;
//extern fixed_t		finetangent(FINEANGLES/2);

extern fixed_t		rw_distance;
extern angle_t		rw_normalangle;

// angle to line origin
extern int		rw_angle1;

// Segs count?
//extern int		sscount;

#if !NO_VISPLANES
// todo graham i don't think we need this many any more? (more recently i don't know why i thought that)
#define MAXVISPLANES    128
extern visplane_t visplanes[MAXVISPLANES];
extern visplane_t* lastvisplane;
extern visplane_t*	floorplane;
extern visplane_t*	ceilingplane;
#endif

#if MU_STATS
extern uint32_t stats_dc_iscale_min, stats_dc_iscale_max;
#endif

#if USE_RAW_MAPVERTEX
#define vertex_x(v) (((v)->x) << FRACBITS)
#define vertex_y(v) (((v)->y) << FRACBITS)
#define vertex_x_raw(v) ((v)->x)
#define vertex_y_raw(v) ((v)->y)
#define vertex_raw_to_fixed(p) (((fixed_t)(p)) << FRACBITS)
#else
#define vertex_x(v) ((v)->x)
#define vertex_y(v) ((v)->y)
#define vertex_x_raw(v) ((v)->x)
#define vertex_y_raw(v) ((v)->y)
#define vertex_raw_to_fixed(v) (v)
#endif

#if USE_WHD
#define CST_TOP 0
#define CST_MID 1
#define CST_BOTTOM 2
extern int check_switch_texture(side_t * side, int where, int texture);
#if SAVE_COMPRESSED
boolean is_switched_texture(const side_t *side, int where);
#endif
#if WHD_SUPER_TINY
#define sidenum_to_side(sidenum) (sides_z + (sidenum))
static inline uint side_sectornum(side_t *side) {
    return side[1];
}
#define side_sector(side) (sectors + side_sectornum(side))

static inline int side_toptexture(side_t *side) {
    if ((side[0] & 0xf) < 5) return 0;
    return side[2] <= LAST_SWITCH_TEXTURE ? check_switch_texture(side, CST_TOP, side[2]) : side[2];
}

static inline int side_midtexture(side_t *side) {
    static uint8_t posM[26] = { 0, 0, 2, 2, 2, 0, 0, 0, 2, 2, 2, 2, 3, 3, 3, 3};
    uint enc = side[0] & 0xf;
    if (!posM[enc]) return 0;
    return side[posM[enc]] <= LAST_SWITCH_TEXTURE ? check_switch_texture(side, CST_MID, side[posM[enc]]) : side[posM[enc]];
}

static inline int side_bottomtexture(side_t *side) {
    static uint8_t posB[26] = { 0, 2, 0, 2, 3, 0, 2, 3, 0, 2, 3, 4, 0, 2, 3, 4};
    uint enc = side[0] & 0xf;
    if (!posB[enc]) return 0;
    return side[posB[enc]] <= LAST_SWITCH_TEXTURE ? check_switch_texture(side, CST_BOTTOM, side[posB[enc]]) : side[posB[enc]];
}

static inline int side_textureoffset16(side_t *side) {
    int16_t toff = 0;
    if (side[0] & 48) {
        static uint8_t offset_offset[16] = {2, 3, 3, 3, 4, 3, 3, 4, 3, 3, 4, 4, 4, 4, 4, 5};
        uint enc = side[0] & 0xf;
        if (side[0] & 32) {
            toff = (side[offset_offset[enc]] << 8) + side[1 + offset_offset[enc]];
        } else {
            toff = side[offset_offset[enc]] << 1;
        }
    }
    return toff;
}
#define side_textureoffset(side) (side_textureoffset16(side) << FRACBITS)
static inline int side_rowoffset16(side_t *side) {
    // todo is this shared with the other?
    static uint8_t offset_offset[16] = {2, 3, 3, 3, 4, 3, 3, 4, 3, 3, 4, 4, 4, 4, 4, 5};
    int16_t rowoff = 0;
    if (side[0] & 64) {
        uint enc = side[0] & 0xf;
        uint pos = offset_offset[enc] + ((sides_z[0] >> 4)&3);
        rowoff = side[pos];
        if (rowoff & 128) {
            rowoff = ((rowoff << 8) | sides_z[pos + 1])<<1;
            rowoff /= 2;
        } else {
            rowoff <<= 1;
        }
    }
    return rowoff;
}
#define side_rowoffset(side) (side_rowoffset16(side) << FRACBITS)
#else
#define sidenum_to_side(sidenum) (&sides[sidenum])
#define side_sector(side) (&sectors[(side)->sector])
#define side_sectornum(side) (side)->sector
#define side_toptexture(side) ((side)->toptexture < LAST_SWITCH_TEXTURE ? check_switch_texture(side, CST_TOP, (side)->toptexture) : (side)->toptexture)
#define side_midtexture(side) ((side)->midtexture < LAST_SWITCH_TEXTURE ? check_switch_texture(side, CST_MID, (side)->midtexture) : (side)->midtexture)
#define side_bottomtexture(side) ((side)->bottomtexture < LAST_SWITCH_TEXTURE ? check_switch_texture(side, CST_BOTTOM, (side)->bottomtexture) : (side)->bottomtexture)
#define side_textureoffset(side) ((side)->textureoffset << FRACBITS)
#define side_textureoffset16(side) (side)->textureoffset
#define side_rowoffset(side) ((side)->rowoffset << FRACBITS)
#endif

// there is nothing in the game that changes this
#define side_setrowoffset16(side, o) assert(side_rowoffset(side) == (o))
extern uint8_t num_switched_sides;
void side_settoptexture(const side_t *side, lumpindex_t t);
void side_setmidtexture(const side_t *side, lumpindex_t t);
void side_setbottomtexture(const side_t *side, lumpindex_t t);
void side_settextureoffset16(const side_t *side, int offset);
#else
#define sidenum_to_side(sidenum) (&sides[sidenum])
#define side_sector(side) (side)->sector
#define side_sectornum(side) ((side)->sector - sectors)
#define side_toptexture(side) (side)->toptexture
#define side_midtexture(side) (side)->midtexture
#define side_bottomtexture(side) (side)->bottomtexture
#define side_textureoffset(side) (side)->textureoffset
#define side_textureoffset16(side) ((side)->textureoffset >> FRACBITS)
#define side_rowoffset(side) (side)->rowoffset
#define side_settoptexture(side, t) (side)->toptexture = t
#define side_setmidtexture(side, t) (side)->midtexture = t
#define side_setbottomtexture(side, t) (side)->bottomtexture = t
#define side_settextureoffset16(side, o) (side)->textureoffset = (o) << FRACBITS
#define side_setrowoffset16(side, o) (side)->rowoffset = (o) << FRACBITS
#endif

#if USE_RAW_MAPLINEDEF
#if !WHD_SUPER_TINY
#define line_sidenum(l, s) ((l)->sidenum[s])
#define line_flags(l) ((l)->flags)
#define line_v1(l) (&vertexes[(l)->v1])
#define line_v2(l) (&vertexes[(l)->v2])
#define line_tag(l) ((l)->tag)
// todo special case to check for NULL
// todo bits for front/back sector mismatched heights (along with twosided)
static inline sector_t *line_frontsector(const line_t *l) {
    int s = line_sidenum(l, 0);
    return s == -1 ? 0 : side_sector(sidenum_to_side(s));
}
static inline sector_t *line_backsector(const line_t *l) {
    int s = line_sidenum(l, 1);
    return s == -1 ? 0 : side_sector(sidenum_to_side(s));
}
#define line_onesided(l) (line_sidenum(l,1)==-1)
#define line_next_step(l) 1
#else

#define line_onesided(l) ((l[1] & (ML_SIDE_MASK >> 8)) == 0)
#define line_predict_side(l) ((l[1] & (ML_NO_PREDICT_SIDE >> 8)) == 0)
#define line_predict_v1(l) ((l[1] & (ML_NO_PREDICT_V1 >> 8)) == 0)
#define line_predict_v2(l) ((l[1] & (ML_NO_PREDICT_V2 >> 8)) == 0)

static inline short line_flags(const line_t *l) {
    return l[0];// + (l[1] << 8); // todo mask off top 7?
}

extern uint16_t whd_sidemul;
static inline int line_sidenum(const line_t *l, int side) {
    if (side && line_onesided(l)) return -1;
    int s;
    if (line_predict_side(l)) {
        s = ((l - lines) * whd_sidemul) >> 16;
        s += (int8_t)l[2];
    } else {
        s = l[2] + (l[3] << 8);
    }
    if (side) {
        s += l[1] >> 5;
    }
    return s;
}

extern uint16_t whd_vmul;
static inline vertex_t *line_v1(const line_t *l) {
    int v;
    uint pos = 3 + (l[1]&1);
    if (line_predict_v1(l)) {
        v = ((l - lines) * whd_vmul) >> 16;
        v += (int8_t)l[pos];
    } else {
        v = l[pos] + (l[pos+1] << 8);
    }
    return vertexes + v;
}

static inline vertex_t *line_v2(const line_t *l) {
    const static uint8_t v2pos[4] = { 4, 5, 5, 6 };
    uint pos = v2pos[l[1]&3];
    int v;
    if (line_predict_v2(l)) {
        v = ((l - lines) * whd_vmul) >> 16;
        v += (int8_t)l[pos];
    } else {
        v = l[pos] + (l[pos+1] << 8);
    }
    return vertexes + v;
}

static inline int line_tag(const line_t *l) {
    int tag = 0;
    if ((l[1] & (ML_HAS_TAG >> 8)) != 0) {
        const static uint8_t special_pos[8] = { 5, 6, 6, 7, 6, 7, 7, 8 };
        uint pos = special_pos[l[1]&7] + ((l[1] & (ML_HAS_SPECIAL >> 8)) != 0);
        tag = l[pos];
    }
    return tag;
}

// todo special case to check for NULL
// todo bits for front/back sector mismatched heights (along with twosided)
static inline sector_t *line_frontsector(const line_t *l) {
    return side_sector(sidenum_to_side(line_sidenum(l, 0)));
}

static inline sector_t *line_backsector(const line_t *l) {
    if (line_onesided(l))
        return 0;
    else
        return side_sector(sidenum_to_side(line_sidenum(l, 1)));
}
#define line_next_step(l) (5 + popcount8((l)[1]&0x1f))

#endif
#define line_is_horiz(l) (vertex_y_raw(line_v1(l)) == vertex_y_raw(line_v2(l)))
#define line_is_vert(l) (vertex_x_raw(line_v1(l)) == vertex_x_raw(line_v2(l)))
#define line_dy_gt_0(l) (vertex_y_raw(line_v2(l)) > vertex_y_raw(line_v1(l)))
#define line_dy_lt_0(l) (vertex_y_raw(line_v2(l)) < vertex_y_raw(line_v1(l)))
#define line_dx_lt_0(l) (vertex_x_raw(line_v2(l)) < vertex_x_raw(line_v1(l)))
#define line_dx_gt_0(l) (vertex_x_raw(line_v2(l)) > vertex_x_raw(line_v1(l)))
#define line_dy(l) (vertex_y(line_v2(l)) - vertex_y(line_v1(l)))
#define line_dx(l) (vertex_x(line_v2(l)) - vertex_x(line_v1(l)))
#define line_dy_raw(l) (vertex_y_raw(line_v2(l)) - vertex_y_raw(line_v1(l)))
#define line_dx_raw(l) (vertex_x_raw(line_v2(l)) - vertex_x_raw(line_v1(l)))

int line_special(should_be_const line_t *line);
void clear_line_special(should_be_const line_t *line);
#if SAVE_COMPRESSED
int whd_line_special(should_be_const line_t *line);
#endif
boolean line_validcount_update_check_impl(should_be_const  line_t *ld);
#define line_validcount_update_check(ld, vc) line_validcount_update_check_impl(ld)
void line_check_reset(void);
void line_set_mapped(should_be_const line_t *line);
boolean line_is_mapped(should_be_const line_t *line);

static inline int line_slopetype(const line_t *l) {
    if (line_is_horiz(l)) return ST_HORIZONTAL;
    if (line_is_vert(l)) return ST_VERTICAL;

    // todo graham this is horrible
    //return FixedDiv(line_dy(l), line_dx(l)) > 0 ? ST_POSITIVE : ST_NEGATIVE;
    // todo not quite the same thing, but probably what was meant... if we pre-calc we can do the exact thing
#if !PICO_ON_DEVICE
    // todo graham little sanity check
    int foo =  (( line_dy_raw(l) ^ line_dx_raw(l)) >= 0);
    int foo2 =  FixedDiv(line_dy(l), line_dx(l)) > 0;
    if (foo != foo2) {
        printf("WLALA %d %d\n", line_dx_raw(l), line_dy_raw(l));
    }
#endif
    return (( line_dy_raw(l) ^ line_dx_raw(l)) >= 0) ? ST_POSITIVE : ST_NEGATIVE;
}
#if !DOOM_SMALL // would already be defined
extern uint32_t *line_sector_check_bitmap; // shared by both line and sector checking - todo actually just alloc on the fly
#endif
extern uint32_t *line_mapped_bitmap;
extern uint32_t *line_special_cleared_bitmap;
#else
#define line_sidenum(l, s) ((l)->sidenum[s])
#define line_onesided(l) (line_sidenum(l,1)==-1)
#define line_flags(l) ((l)->flags)
#define line_v1(l) ((l)->v1)
#define line_v2(l) ((l)->v2)
#define line_tag(l) ((l)->tag)
#define line_special(l) ((l)->special)
#define line_frontsector(l) ((l)->frontsector)
#define line_backsector(l) ((l)->backsector)
#define line_is_horiz(l) ((l)->slopetype == ST_HORIZONTAL)
#define line_is_vert(l) ((l)->slopetype == ST_VERTICAL)
#define clear_line_special(l) hack_rowad_p(line_t, l, special) = 0
#define line_dy_gt_0(l) ((l)->dy > 0)
#define line_dy_lt_0(l) ((l)->dy < 0)
#define line_dx_lt_0(l) ((l)->dx < 0)
#define line_dx_gt_0(l) ((l)->dx > 0)
#define line_dx(l) ((l)->dx)
#define line_dy(l) ((l)->dy)
#define line_slopetype(l) ((l)->slopetype)
#define line_check_reset() ((void)0)
#define line_set_mapped(l) hack_rowad_p(line_t, l, flags) |= ML_MAPPED
#define line_is_mapped(l) (line_flags(l) & ML_MAPPED)

static inline boolean line_validcount_update_check(should_be_const line_t *ld, int validcount) {
    if (validcount == ld->validcount) {
        return true;
    }
    hack_rowad_p(line_t, ld, validcount) = validcount;
    return false;
}
#define line_next_step(l) 1
#endif

#if USE_RAW_MAPSEG
#include <assert.h>
#if !WHD_SUPER_TINY
#define seg_side(s) ((s)->side)
#define seg_linedef(s) (&lines[(s)->linedef])
#define seg_v1(s) (&vertexes[(s)->v1])
#define seg_v2(s) (&vertexes[(s)->v2])
#define seg_angle(s) (((s)->angle) << FRACBITS)
#define seg_offset(s) (((s)->offset) << FRACBITS)
#define seg_next_step(s) 1
#else
#define seg_linedef(s) (&lines[(s)[3] + ((s)[4] << 8)])
#define seg_side(s) ((s)[0] >> 7)
#define seg_v1(s) (&vertexes[((s)[1] | (((s)[0]&0x07)<<8))])
#define seg_v2(s) (&vertexes[((s)[2] | (((s)[0]&0x38)<<5))])
static inline fixed_t seg_offset(const seg_t *s) {
    if (!(s[0] & 64)) {
        return 0;
    }
    if (s[5] < 128) {
        return (s[5] * 4) << FRACBITS;
    } else {
        return (((s[5]<<23)|(s[6]<<17)))/2;
    }
}

static inline int seg_next_step(const seg_t *s) {
    int size = 5;
    if (s[0]&64) {
        size += s[5]&128 ? 2 : 1;
    }
    return size;
}
#endif
#define seg_sidedef(s) (sidenum_to_side(line_sidenum(seg_linedef(s), seg_side(s))))
static inline sector_t *seg_frontsector(const seg_t *seg) {
    return side_sector(sidenum_to_side(line_sidenum(seg_linedef(seg), seg_side(seg))));
}

static inline sector_t *seg_backsector(const seg_t *seg) {
    // todo this check might be redundent - see use in R_AddLine which could check flags instead
    if (!(line_flags(seg_linedef(seg)) & ML_TWOSIDED)) return NULL;
    return side_sector(sidenum_to_side(line_sidenum(seg_linedef(seg), seg_side(seg)^1)));
}
#else
#define seg_linedef(s) ((s)->linedef)
#define seg_sidedef(s) ((s)->sidedef)
#define seg_frontsector(s) ((s)->frontsector)
#define seg_backsector(s) ((s)->backsector)
#define seg_v1(s) ((s)->v1)
#define seg_v2(s) ((s)->v2)
#define seg_sidedef(s) ((s)->sidedef)
#define seg_angle(s) ((s)->angle)
#define seg_offset(s) ((s)->offset)
#define seg_next_step(s) 1
#endif

#if USE_WHD
#define subsector_firstline(ss) &segs[*(ss)]
#define subsector_sector(ss) side_sector(seg_sidedef(subsector_firstline(ss)))
#define subsector_linelimit(ss) &segs[(ss)[1]]
#else
#define subsector_firstline(ss) &segs[(ss)->firstline]
#define subsector_sector(ss) ((ss)->sector)
#define subsector_linelimit(ss) ((subsector_firstline(ss) + (ss)->numlines))
#endif

#if USE_INDEX_LINEBUFFER
#define sector_line(sector, n) &lines[linebuffer[(sector)->line_index + (n)]]
#else
#define sector_line(sector, n) (sector)->lines[n]
#endif
#endif
