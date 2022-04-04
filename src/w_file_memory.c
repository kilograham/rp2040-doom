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
//	WAD I/O functions.
//

#if USE_MEMORY_WAD
#include "config.h"

#include <string.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"
#include "w_wad.h"
#if PICO_BUILD
#include "pico.h"
#include "pico/binary_info.h"
#else
#define panic I_Error
#endif

#if PICO_ON_DEVICE
#ifndef TINY_WAD_ADDR
#error TINY_WAD_ADDR must be specified
#endif
#define wad_map_base ((const uint8_t *)TINY_WAD_ADDR)
// simplest thing here is to use a feature, rather than a separate feature group
#if WHD_SUPER_TINY
#define ADDR_FNAME "WHX at " __XSTRING(TINY_WAD_ADDR)
#else
#define ADDR_FNAME "WHD at " __XSTRING(TINY_WAD_ADDR)
#endif
bi_decl(bi_program_feature(ADDR_FNAME));
#endif

#if !USE_WHD
#error no longer supported
#else
#if !PICO_ON_DEVICE
#include "tiny.whd.h"
#define wad_map_base tiny_whd
#endif
const uint8_t *whd_map_base = wad_map_base;
#endif

extern const wad_file_class_t memory_wad_file;

static const wad_file_t fileo = {
        .file_class = &memory_wad_file,
        .length = 0, // seemingly unused
        .mapped = wad_map_base,
        .path = "<here>",
};

static wad_file_t *W_Memory_OpenFile(const char *path)
{
#if !USE_WHD
    if (fileo.mapped[0] != 'I' || fileo.mapped[1] != 'W' || fileo.mapped[2] != 'A' || fileo.mapped[3] != 'D')
        panic("NO WAD");
#else
#if WHD_SUPER_TINY
    if (fileo.mapped[0] != 'I' || fileo.mapped[1] != 'W' || fileo.mapped[2] != 'H' || fileo.mapped[3] != 'X') {
#if PICO_ON_DEVICE
        panic("No WXD at %p\n", TINY_WAD_ADDR);
#else
        panic("Expected WXD format");
#endif
    }
#else
    if (fileo.mapped[0] != 'I' || fileo.mapped[1] != 'W' || fileo.mapped[2] != 'H' || fileo.mapped[3] != 'D') {
#if PICO_ON_DEVICE
        panic("No WHD at %p\n", TINY_WAD_ADDR);
#else
        panic("Expected WHD format");
#endif
    }
#endif
#endif
    return &fileo;
}

static void W_Memory_CloseFile(wad_file_t *wad)
{
}

// Read data from the specified position in the file into the
// provided buffer.  Returns the number of bytes read.

size_t W_Memory_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    memcpy(buffer, wad->mapped + offset, buffer_len);
    return buffer_len;
}


const wad_file_class_t memory_wad_file =
{
    W_Memory_OpenFile,
    W_Memory_CloseFile,
    W_Memory_Read,
};
#endif