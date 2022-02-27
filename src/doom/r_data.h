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
//  Refresh module, data I/O, caching, retrieval of graphics
//  by name.
//


#ifndef __R_DATA__
#define __R_DATA__

#include "r_defs.h"
#include "r_state.h"

#if !USE_WHD
typedef const byte *texturecolumn_t;
typedef const column_t *maskedcolumn_t;

texturecolumn_t R_GetColumn
        ( int		tex,
          int		col );
maskedcolumn_t R_GetMaskedColumn
        ( int		tex,
          int		col );
maskedcolumn_t R_GetPatchColumn(const patch_t *patch, int col);
#define lookup_texture(t) (t)
#define lookup_masked_texture(t) (t)
#else

// drawable is local
// make this non packed ideally divisible by power of 2
typedef struct {
    int16_t real_id;
} framedrawable_t;
#define MAX_FRAME_DRAWABLES 128
extern framedrawable_t framedrawables[MAX_FRAME_DRAWABLES];
extern uint8_t translated_fds[3]; // fds which are for trasnlated other players
extern uint8_t num_framedrawables;
framedrawable_t *lookup_texture(int texture_id);
framedrawable_t *lookup_masked_texture(int texture_id);
framedrawable_t *lookup_patch(int patch_id);
void reset_framedrawables(void);

typedef struct {
    uint8_t fd_num;
    uint8_t col;
    int16_t real_id;
} drawcolumn_t;

static_assert(sizeof(drawcolumn_t) == 4, "");

typedef drawcolumn_t texturecolumn_t;
typedef drawcolumn_t maskedcolumn_t;

static inline drawcolumn_t make_drawcolumn(framedrawable_t *fd, int col) {
    assert(col >=0  && col < 256);
    assert(fd);
    int fd_num = fd - framedrawables;
    assert(fd_num >=0 && fd_num < MAX_FRAME_DRAWABLES);
    drawcolumn_t rc = {
            .fd_num = (uint8_t)fd_num,
            .col = (uint8_t)col,
            .real_id = fd->real_id,
    };
    return rc;
}

static inline texturecolumn_t R_GetColumn(framedrawable_t *fd, int col) {
    assert(fd->real_id >= 0);
    // had seen this in the past with bug related to certain maps (hopefully now fixed everywhere)
    // todo ^ happens in tnt for now due to anim issue
    assert( whd_textures[fd->real_id].width != 0); // texture is missing
    col &= (whd_textures[fd->real_id].width - 1);
    return make_drawcolumn(fd, col);
}

static inline maskedcolumn_t R_GetMaskedColumn(framedrawable_t *fd, uint8_t col) {
    //assert(fd->real_id <= 0); // this should be a patch also
    if (fd->real_id >= 0) {
        // we do see regular textures as masked columns.. we'll just draw as it
        return R_GetColumn(fd, col);
    }
    return make_drawcolumn(fd, col);
}

static inline maskedcolumn_t R_GetPatchColumn(framedrawable_t *fd, uint8_t col) {
    assert(fd->real_id <= 0);
    return make_drawcolumn(fd, col);
}
#endif


// I/O, setting up the stuff.
void R_InitData (void);
void R_PrecacheLevel (void);




// Called by P_Ticker for switches and animations,
// returns the texture number for the texture name.
#if !USE_WHD
int R_TextureNumForName(texturename_t name);
int R_CheckTextureNumForName(texturename_t name);
int R_FlatNumForName(flatname_t name);
#define resolve_vpatch_handle(h) (h)
#else
#define R_TextureNumForName(x) (x)
#define R_CheckTextureNumForName(x) (x)
#define R_FlatNumForName(x) (x)
extern const uint16_t *whd_vpatch_numbers;
//#define check_vpatch_handle(handle) ({assert((handle)<NUM_VPATCHES); whd_vpatch_numbers[handle]; })
#define check_vpatch_handle(handle) whd_vpatch_numbers[handle]
#define resolve_vpatch_handle(handle) ((const patch_t *)W_CacheLumpNum(check_vpatch_handle(handle), PU_STATIC))
#endif

#endif
