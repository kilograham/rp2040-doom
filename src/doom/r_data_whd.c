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
//	Preparation of data for rendering,
//	generation of lookups, caching, retrieval by name.
//

#if USE_WHD
#include <stdio.h>

#include "deh_main.h"
#include "i_swap.h"
#include "i_system.h"
#include "z_zone.h"


#include "w_wad.h"

#include "doomdef.h"
#include "m_misc.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"
#include "r_data.h"
#include <assert.h>

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
//



lumpindex_t 		firstflat;
lumpindex_t 		numflats;

lumpindex_t 		firstspritelump;
lumpindex_t 		lastspritelump;
lumpindex_t 		numspritelumps;

cardinal_t 		numtextures;
const whdtexture_t *whd_textures;
// todo if this is too big, we can do cachelump num directly (and save about 400 bytes)
const uint16_t *whd_vpatch_numbers;

// needed for texture pegging
fixed_t*		textureheight;

// for global animation
flatname_t	whd_flattranslation[NUM_SPECIAL_FLATS];
const uint8_t *whd_specialtoflat;
texturename_t 	whd_texturetranslation[NUM_SPECIAL_TEXTURES];

// framedrawable stuff
framedrawable_t framedrawables[MAX_FRAME_DRAWABLES];
uint8_t num_framedrawables;
static_assert(MAX_FRAME_DRAWABLES < (1u << (8 * sizeof(num_framedrawables))), "");
framedrawable_t *skytexture_fd;
uint8_t dc_translation_index;
byte translated_fds[3];

// needed for pre rendering
const int32_t *whd_sprite_meta;
const uint16_t *whd_sprite_frame_meta;

const lighttable_t	*colormaps;

//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//  it counts the number of composite columns
//  required in the texture and allocates space
//  for a column directory and any new columns.
// The directory will simply point inside other patches
//  if there is only one patch in a given column,
//  but any columns with multiple patches
//  will have new column_ts generated.
//



//
// R_DrawColumnInCache
// Clip and draw a column
//  from a patch into a cached post.
//
static void
R_DrawColumnInCache
        ( column_t*	patch,
          byte*		cache,
          int		originy,
          int		cacheheight )
{
    I_Error("no can do");
}



//
// R_GenerateComposite
// Using the texture definition,
//  the composite texture is created from the patches,
//  and each column is cached.
//
void R_GenerateComposite (int texnum)
{
}

//
// R_GenerateLookup
//
void R_GenerateLookup (int texnum)
{
}

void reset_framedrawables(void) {
//    printf("FD %d\n", num_framedrawables);
    num_framedrawables = 0;
    translated_fds[0] = translated_fds[1] = translated_fds[2] = 0xff;
    skytexture_fd = lookup_texture(skytexture);
    if (!whd_textures[skytexture].patch_count) {
        // this is a single covering opaque patch at 0, 0
        skytexture_patch = whd_textures[skytexture].patch0;
    } else {
        assert(whd_textures[skytexture].patch_count == 1);
        uint8_t *patch_table = &((uint8_t *)whd_textures)[whd_textures[skytexture].metdata_offset];
        skytexture_patch = patch_table[0] + (patch_table[1] << 8);
    }
}

framedrawable_t *lookup_texture(int real_id) {
    if (!real_id) return NULL; // E4M5 at least has 0 as texture values
    // todo hash table
    framedrawable_t *fd = framedrawables;
    if (dc_translation_index) {
        // since the only things translated are the players, we just track one fd per translation index
        if (translated_fds[dc_translation_index-1] < num_framedrawables) {
            if (framedrawables[translated_fds[dc_translation_index-1]].real_id == real_id) return &framedrawables[translated_fds[dc_translation_index-1]];
        }
        translated_fds[dc_translation_index-1] = num_framedrawables;
        fd += num_framedrawables;
    } else {
        for (int i = 0; i < num_framedrawables; i++, fd++) {
            if (fd->real_id == real_id) {
                return fd;
            }
        }
    }
    hard_assert(num_framedrawables < MAX_FRAME_DRAWABLES);
    num_framedrawables++;
    fd->real_id = (int16_t)real_id;
    return fd;
}

framedrawable_t *lookup_patch(int patchnum) {
    return lookup_texture(-(patchnum+firstspritelump));
}

framedrawable_t *lookup_masked_texture(int texturenum) {
    if (!whd_textures[texturenum].patch_count) {
        // this is currently a single patch at 0,0
        return lookup_texture(-whd_textures[texturenum].patch0);
    } else {
        // turns out this does happen e.g. in E4M3 in ultimate doom
        return lookup_texture(texturenum);
    }
}
//
// R_InitTextures
// Initializes the texture list
//  with the textures from the world map.
//
void R_InitTextures (void)
{
    lumpindex_t lump  = W_CheckNumForName("TEXTURE1");
    const uint8_t *data = W_CacheLumpNum(lump, PU_STATIC);
    numtextures = data[0] | (data[1] << 8);
    whd_textures = (const whdtexture_t *)(data + 2);
    assert(numtextures >= NUM_SPECIAL_TEXTURES);

    for (int i=0 ; i< count_of(whd_texturetranslation) ; i++)
        whd_texturetranslation[i] = i;
}

//
// R_InitFlats
//
void R_InitFlats (void)
{
    int		i;

    firstflat = W_GetNumForName (DEH_String("F_START")) + 1;
    lumpindex_t lastflat = W_GetNumForName (DEH_String("F_END")) - 1;
    numflats = lastflat - firstflat + 1;
#if USE_FLAT_MAX_256
    assert(numflats <= 256);
#endif
    assert(W_LumpLength(firstflat-1) == NUM_SPECIAL_FLATS + numflats );
    whd_specialtoflat = W_CacheLumpNum(firstflat - 1, PU_STATIC);

    for (i=0 ; i<NUM_SPECIAL_FLATS ; i++)
        whd_flattranslation[i] = i;
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//  so the sprite does not need to be cached completely
//  just for having the header info ready during rendering.
//
void R_InitSpriteLumps (void)
{
    firstspritelump = W_GetNumForName (DEH_String("S_START")) + 1;
    lastspritelump = W_GetNumForName (DEH_String("S_END")) - 1;

    numspritelumps = lastspritelump - firstspritelump + 1;
    assert(4 * numspritelumps == W_LumpLength(firstspritelump-1));
    whd_sprite_meta = W_CacheLumpNum(firstspritelump-1, PU_STATIC);
    whd_sprite_frame_meta = W_CacheLumpNum(lastspritelump+1, PU_STATIC);

    // todo if this is too big we can do away with it and access the patch directly
    whd_vpatch_numbers = (const uint16_t *) W_CacheLumpName("P_START", PU_STATIC);
    if (W_LumpLength(W_GetNumForName("P_START")) != NUM_VPATCHES * 2) {
        I_Error("whd is out of sync with VPATCH enum"); // make sure whd_gen is up to date with code, and whd is regenerated
    }
}

//
// R_InitColormaps
//
void R_InitColormaps (void)
{
    int	lump;

    // Load in the light tables,
    //  256 byte align tables.
    lump = W_GetNumForName(DEH_String("COLORMAP"));
    colormaps = W_CacheLumpNum(lump, PU_STATIC);
#if PRINT_COLORMAPS
    int size = W_LumpLength(lump);
    assert(!(size & 0xff));
    printf("\nlighttable_t colormaps[%d * 256] = {\n", size / 256);
    for(int i=0;i<size;i+=32) {
        if (!(i & 0xff)) printf("    // %d \n", i/256);
        printf("    ");
        for(int j=i;j<MIN(size,i+32);j++) {
            printf("0x%02x, ", colormaps[j]);
        }
        printf("\n");
    }
    printf("};\n");
#endif
}



//
// R_InitData
// Locates all the lumps
//  that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
#
    R_InitTextures ();
    printf (".");
    R_InitFlats ();
    printf (".");
    R_InitSpriteLumps ();
    printf (".");
    R_InitColormaps ();
}



//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
int		flatmemory;
int		texturememory;
int		spritememory;

void R_PrecacheLevel (void)
{
#if 0
    char*		flatpresent;
    char*		spritepresent;

    int			i;
    int			j;
    int			k;
    int			lump;

//    texture_t*		texture;
    thinker_t*		th;

    if (demoplayback)
        return;

    // Precache flats.
    flatpresent = Z_Malloc(numflats, PU_STATIC, 0);
    memset (flatpresent,0,numflats);

    for (i=0 ; i<numsectors ; i++)
    {
        flatpresent[sectors[i].floorpic] = 1;
        flatpresent[sectors[i].ceilingpic] = 1;
    }

    flatmemory = 0;
    int flat_count = 0;

    for (i=0 ; i<numflats ; i++)
    {
        if (flatpresent[i])
        {
            flat_count++;
            lump = firstflat + i;
            flatmemory += W_LumpLength(lump);
            W_CacheLumpNum(lump, PU_CACHE);
        }
    }

    Z_Free(flatpresent);

//    // Precache textures.
//    texturepresent = Z_Malloc(numtextures, PU_STATIC, 0);
//    memset (texturepresent,0, numtextures);
//
//    for (i=0 ; i<numsides ; i++)
//    {
//        texturepresent[side_toptexture(&sides[i])] = 1;
//        texturepresent[side_midtexture(&sides[i])] = 1;
//        texturepresent[side_bottomtexture(&sides[i])] = 1;
//    }

    // Sky texture is always present.
    // Note that F_SKY1 is the name used to
    //  indicate a sky floor/ceiling as a flat,
    //  while the sky texture is stored like
    //  a wall texture, with an episode dependend
    //  name.
//    texturepresent[skytexture] = 1;
//
//    texturememory = 0;
//    int texture_count =0;
//    for (i=0 ; i<numtextures ; i++)
//    {
//        if (!texturepresent[i])
//            continue;
//
//        texture_count++;
//        texture = textures[i];
//
//        for (j=0 ; j<texture->patchcount ; j++)
//        {
//            lump = texture->patches[j].patch;
//            texturememory += lump_info(lump)->size;
//            W_CacheLumpNum(lump , PU_CACHE);
//        }
//    }
//
//    Z_Free(texturepresent);
//
    // Precache sprites.
//    spritepresent = Z_Malloc(numsprites, PU_STATIC, 0);
//    memset (spritepresent,0, numsprites);
//
//    for (th = thinker_next(&thinkercap) ; th != &thinkercap ; th=thinker_next(th))
//    {
//        if (th->function == ThinkF_P_MobjThinker)
//            spritepresent[mobj_sprite((mobj_t *)th)] = 1;
//    }
//
//    spritememory = 0;
//    int sprite_count = 0;
//    for (i=0 ; i<numsprites ; i++)
//    {
//        if (!spritepresent[i])
//            continue;
//        sprite_count++;
//
//        for (j=0 ; j<sprites[i].numframes ; j++)
//        {
//            sf = &sprites[i].spriteframes[j];
//            for (k=0 ; k<8 ; k++)
//            {
//                lump = firstspritelump + sf->lump[k];
//                spritememory += lump_info(lump)->size;
//                W_CacheLumpNum(lump , PU_CACHE);
//            }
//        }
//    }

//    printf("PRECACHE Summary:\n");
//    printf("  Flats %d/%d = %d bytes\n", flat_count, numflats, flatmemory);
//    printf("  Textures %d/%d = %d bytes\n", texture_count, numtextures, texturememory);
//    printf("  Sprites %d/%d = %d bytes\n", sprite_count, numsprites, spritememory);
//    Z_Free(spritepresent);
#endif
}

#endif