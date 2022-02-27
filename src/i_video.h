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
//	System specific interface stuff.
//


#ifndef __I_VIDEO__
#define __I_VIDEO__

#include "doomtype.h"

// Screen width and height.

#define SCREENWIDTH  320
#define SCREENHEIGHT 200
#if DOOM_TINY
#define MAIN_VIEWHEIGHT (SCREENHEIGHT - 32 /* ST_HEIGHT */)
#endif

// Screen height used when aspect_ratio_correct=true.

#define SCREENHEIGHT_4_3 240

typedef boolean (*grabmouse_callback_t)(void);

// Called by D_DoomMain,
// determines the hardware configuration
// and sets up the video mode
void I_InitGraphics (void);

void I_GraphicsCheckCommandLine(void);

void I_ShutdownGraphics(void);

// Takes full 8 bit values.
#if !USE_WHD
void I_SetPalette (should_be_const byte* palette);
#else
void I_SetPaletteNum(int num);
#endif
int I_GetPaletteIndex(int r, int g, int b);

void I_UpdateNoBlit (void);
void I_FinishUpdate (void);

void I_ReadScreen (pixel_t* scr);

void I_BeginRead (void);

void I_SetWindowTitle(const char *title);

void I_CheckIsScreensaver(void);
void I_SetGrabMouseCallback(grabmouse_callback_t func);

void I_DisplayFPSDots(boolean dots_on);
void I_BindVideoVariables(void);

void I_InitWindowTitle(void);
void I_InitWindowIcon(void);

// Called before processing any tics in a frame (just after displaying a frame).
// Time consuming syncronous operations are performed here (joystick reading).

void I_StartFrame (void);

// Called before processing each tic in a frame.
// Quick syncronous operations are performed here.

void I_StartTic (void);

// Enable the loading disk image displayed when reading from disk.

void I_EnableLoadingDisk(int xoffs, int yoffs);

extern should_be_const constcharstar video_driver;
extern boolean screenvisible;

#if !USE_VANILLA_KEYBOARD_MAPPING_ONLY
extern int vanilla_keyboard_mapping;
#else
#define vanilla_keyboard_mapping true
#endif
extern boolean screensaver_mode;
extern isb_int8_t usegamma;
extern pixel_t *I_VideoBuffer;

extern int screen_width;
extern int screen_height;
extern int fullscreen;
extern int aspect_ratio_correct;
extern int integer_scaling;
extern int vga_porch_flash;
extern int force_software_renderer;

extern should_be_const constcharstar window_position;
void I_GetWindowPosition(int *x, int *y, int w, int h);

// Joystic/gamepad hysteresis
extern unsigned int joywait;

#if DOOM_TINY
enum {
    VIDEO_TYPE_NONE,   // note order is important as we compare (also there is an array of handlers)
    VIDEO_TYPE_TEXT,
    VIDEO_TYPE_SAVING,
    VIDEO_TYPE_DOUBLE,
    VIDEO_TYPE_SINGLE,
    VIDEO_TYPE_WIPE,
};
#define FIRST_VIDEO_TYPE_WITH_OVERLAYS VIDEO_TYPE_DOUBLE

extern uint8_t next_video_type;
extern uint8_t next_frame_index; // next frame_index to be picked up by the diplsau
extern uint8_t next_overlay_index;
#if !DEMO1_ONLY
extern uint8_t *next_video_scroll;
#endif
extern int16_t *wipe_yoffsets_raw; // work area for y offsets
extern uint8_t *wipe_yoffsets; // position of start of y in each column (clipped 0->200)
extern uint32_t *wipe_linelookup; // offset of front image from start of screenbuffer (actually address of in PICO_ON_DEVICE)
#endif
#endif
