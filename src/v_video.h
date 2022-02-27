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
//	Gamma correction LUT.
//	Functions to draw patches (by post) directly to screen.
//	Functions to blit a block to the screen.
//


#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "doomtype.h"

// Needed because we are refering to patches.
#include "v_patch.h"

//
// VIDEO
//

#define CENTERY			(SCREENHEIGHT/2)


extern int dirtybox[4];

#if !DOOM_ONLY
should_be_const extern byte *tinttable;
// haleyjd 08/28/10: implemented for Strife support
// haleyjd 08/28/10: Patch clipping callback, implemented to support Choco
// Strife.
typedef boolean (*vpatchclipfunc_t)(const patch_t *, int, int);
void V_SetPatchClipCallback(vpatchclipfunc_t func);
#endif

// Allocates buffer screens, call before R_Init.
void V_Init (void);

// Draw a block from the specified source screen to the screen.

void V_CopyRect(int srcx, int srcy, pixel_t *source,
                int width, int height,
                int destx, int desty);

#if USE_WHD
#define VPATCHLIST_COUNT_FRAMEBUFFER 120
#define VPATCHLIST_COUNT_OVERLAY (VPATCHLIST_COUNT_FRAMEBUFFER + 32)
static_assert(VPATCHLIST_COUNT_OVERLAY < 256, "");

typedef struct {
    // todo this could be shared with one of the overlays perhaps
    vpatchlist_t framebuffer[VPATCHLIST_COUNT_FRAMEBUFFER];
    // we need one for the active display, one to build in the background
    vpatchlist_t overlays[2][VPATCHLIST_COUNT_OVERLAY];
    uint16_t vpatch_doff[VPATCHLIST_COUNT_OVERLAY]; // where we are in the data
    // heads for each row of ordered (display order) patchlist indexes that start on that row
    uint8_t vpatch_starters[200]; // screenheight
    uint8_t vpatch_next[VPATCHLIST_COUNT_OVERLAY];
} vpatchlists_t;
static_assert(sizeof(vpatchlists_t) < 0xc00, "");
extern vpatchlists_t *vpatchlists;

typedef enum {
    PRE_WIPE_NONE=0,
    PRE_WIPE_EXTRA_FRAME_NEEDED=1,
    PRE_WIPE_EXTRA_FRAME_DONE=2
} pre_wipe_state_t;
extern pre_wipe_state_t pre_wipe_state;
#define TEXT_SCANLINE_BUFFER_WORDS (SCREENWIDTH + 4)
#define TEXT_SCANLINE_BUFFER_TOTAL_WORDS (PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT * TEXT_SCANLINE_BUFFER_WORDS)
void V_BeginPatchList(vpatchlist_t *patchlist);
void V_EndPatchList(void);
void V_DrawPatchList(const vpatchlist_t *patchlist);
extern uint8_t vpatch_clip_top, vpatch_clip_bottom;
#endif
void V_DrawPatch(int x, int y, vpatch_handle_large_t patch);
void V_DrawPatchN(int x, int y, vpatch_handle_large_t patch, int repeat);
void V_DrawPatchFlipped(int x, int y, vpatch_handle_large_t patch);
#if !DOOM_ONLY
void V_DrawTLPatch(int x, int y, patch_t *patch);
void V_DrawAltTLPatch(int x, int y, patch_t * patch);
void V_DrawShadowedPatch(int x, int y, patch_t *patch);
void V_DrawXlaPatch(int x, int y, patch_t * patch);     // villsa [STRIFE]
#endif
void V_DrawPatchDirect(int x, int y, vpatch_handle_large_t patch);
void V_DrawPatchDirectN(int x, int y, vpatch_handle_large_t patch, int repeat);

// Draw a linear block of pixels into the view buffer.

void V_DrawBlock(int x, int y, int width, int height, pixel_t *src);

#if !USE_WHD
void V_MarkRect(int x, int y, int width, int height);
#else
#define V_MarkRect(x, y, width, height) ((void)0)
#endif

void V_DrawFilledBox(int x, int y, int w, int h, int c);
void V_DrawHorizLine(int x, int y, int w, int c);
void V_DrawVertLine(int x, int y, int h, int c);
void V_DrawBox(int x, int y, int w, int h, int c);

// Draw a raw screen lump

void V_DrawRawScreen(pixel_t *raw);

// Temporarily switch to using a different buffer to draw graphics, etc.

void V_UseBuffer(pixel_t *buffer);

// Return to using the normal screen buffer to draw graphics.

void V_RestoreBuffer(void);

// Save a screenshot of the current screen to a file, named in the 
// format described in the string passed to the function, eg.
// "DOOM%02i.pcx"

#if !NO_SCREENSHOT
void V_ScreenShot(const char *format);
#endif

// Load the lookup table for translucency calculations from the TINTTAB
// lump.

void V_LoadTintTable(void);

// villsa [STRIFE]
// Load the lookup table for translucency calculations from the XLATAB
// lump.

void V_LoadXlaTable(void);

void V_DrawMouseSpeedBox(int speed);

#endif

