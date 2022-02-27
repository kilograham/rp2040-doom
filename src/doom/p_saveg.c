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
//	Archiving: SaveGame I/O.
//

#define xprintf(x, ...) ((void)0)
#include <stdio.h>
#include <stdlib.h>

#include "dstrings.h"
#include "deh_main.h"
#include "i_system.h"
#include "z_zone.h"
#include "p_local.h"
#include "p_saveg.h"

// State.
#include "doomstat.h"
#include "g_game.h"
#include "m_misc.h"
#include "r_state.h"

#if !NO_FILE_ACCESS
FILE *save_stream;
#endif
#if LOAD_COMPRESSED
th_bit_input *sg_bi;
#endif
#if SAVE_COMPRESSED
th_bit_output *sg_bo;
#endif
#if !NO_USE_SAVE
int savegamelength;
boolean savegame_error;

// Get the filename of a temporary file to write the savegame to.  After
// the file has been successfully saved, it will be renamed to the 
// real file.

#if !NO_FILE_ACCESS
char *P_TempSaveGameFile(void)
{
    static char *filename = NULL;

    if (filename == NULL)
    {
        filename = M_StringJoin(savegamedir, "temp.dsg", NULL);
    }

    return filename;
}
#endif

#endif
// Get the filename of the save game file to use for the specified slot.

char *P_SaveGameFile(int slot)
{
#if !NO_FILE_ACCESS
    static char *filename = NULL;
    static size_t filename_size = 0;
    char basename[32];

    if (filename == NULL)
    {
        filename_size = strlen(savegamedir) + 32;
        filename = malloc(filename_size);
    }

    DEH_snprintf(basename, 32, SAVEGAMENAME "%d.dsg", slot);
    M_snprintf(filename, filename_size, "%s%s", savegamedir, basename);

    return filename;
#else
    static char fn[2];
    fn[0] = slot + '0';
    return fn;
#endif
}

// Endian-safe integer read/write functions

static byte saveg_read8(void)
{
#if !LOAD_COMPRESSED
    byte result = -1;

#if !NO_FILE_ACCESS
    if (fread(&result, 1, 1, save_stream) < 1)
    {
        if (!savegame_error)
        {
            stderr_print( "saveg_read8: Unexpected end of file while "
                            "reading save game\n");

            savegame_error = true;
        }
    }
#else
    panic_unsupported();
#endif

    return result;
#else
    return th_read_bits(sg_bi, 8);
#endif
}

#if LOAD_COMPRESSED
static uint saveg_read_bits(uint n) {
    return th_read_bits(sg_bi, n);
}

static uint saveg_read_bit() {
    return th_read_bits(sg_bi, 1);
}
#endif
#if SAVE_COMPRESSED
static void saveg_write_bits(uint bits, uint n) {
    th_write_bits(sg_bo, bits, n);
}

static void saveg_write_bit(uint bit) {
    th_write_bits(sg_bo, bit, 1);
}
#endif

static void saveg_write8(byte value)
{
#if !SAVE_COMPRESSED
#if !NO_FILE_ACCESS
    if (fwrite(&value, 1, 1, save_stream) < 1)
    {
        if (!savegame_error)
        {
            stderr_print( "saveg_write8: Error while writing save game\n");

            savegame_error = true;
        }
    }
#elif !NO_USE_SAVE
    savegame_error = true;
#endif
#else
    saveg_write_bits(value, 8);
#endif
}

static short saveg_read16(void)
{
#if !LOAD_COMPRESSED
    int result;

    result = saveg_read8();
    result |= saveg_read8() << 8;

    return result;
#else
    return th_read_bits(sg_bi, 16);
#endif
}

static void saveg_write16(short value)
{
#if !SAVE_COMPRESSED
    saveg_write8(value & 0xff);
    saveg_write8((value >> 8) & 0xff);
#else
    th_write_bits(sg_bo, (uint16_t)value, 16);
#endif
}

static int saveg_read32(void)
{
#if !LOAD_COMPRESSED
    int result;

    result = saveg_read8();
    result |= saveg_read8() << 8;
    result |= saveg_read8() << 16;
    result |= saveg_read8() << 24;

    return result;
#else
    return th_read32(sg_bi);
#endif
}

static void saveg_write32(uint32_t value)
{
#if !SAVE_COMPRESSED
    saveg_write8(value & 0xff);
    saveg_write8((value >> 8) & 0xff);
    saveg_write8((value >> 16) & 0xff);
    saveg_write8((value >> 24) & 0xff);
#else
    th_write32(sg_bo, value);
#endif
}

static boolean saveg_read_boolean()
{
#if !SAVE_COMPRESSED
    return saveg_read32();
#else
    return saveg_read_bit();
#endif
}

static void saveg_write_boolean(boolean value)
{
#if !SAVE_COMPRESSED
    saveg_write32(value);
#else
    saveg_write_bit(value);
#endif
}

#if LOAD_COMPRESSED
static int saveg_read_low_zeros() {
    uint zb = saveg_read_bits(2);
    uint rc = 0;
    for(uint i=zb;i<4;i++) {
        rc |= saveg_read8() << (8 * i);
    }
    return (int)rc;
}

typedef int (*saveg_reader)();

static uint saveg_read_maybe(uint32_t static_val, saveg_reader reader, const char *desc) {
    if (saveg_read_bit()) {
        uint rc = reader();
        xprintf("  %s explicit %08x\n", desc, (int)rc);
        return rc;
    } else {
        xprintf("  %s default %08x\n", desc, (int)static_val);
        return static_val;
    }
}
#endif
#if SAVE_COMPRESSED
static void saveg_write_low_zeros(uint32_t value) {
    uint zb = value ? __builtin_ctz(value) / 8 : 3;
    saveg_write_bits(zb,2);
    int i;
    for(i=0;i<zb;i++) {
        value >>= 8;
    }
    for(;i<4;i++) {
        saveg_write8(value & 0xff);
        value >>= 8;
    }
}

typedef void (*saveg_writer)(uint32_t);

static void saveg_write_maybe(uint32_t val, uint32_t static_val, saveg_writer writer, const char *desc) {
    if (val != static_val) {
        xprintf("  %s explicit %08x %08x\n", desc, (int)val, (int)static_val);
        saveg_write_bit(1);
        writer(val);
    } else {
        xprintf("  %s default %08x\n", desc, (int)static_val);
        saveg_write_bit(0);
    }
}
#endif
// Pad to 4-byte boundaries

static void saveg_read_pad(void)
{
#if !LOAD_COMPRESSED
    unsigned long pos;
    int padding;
    int i;

#if !NO_FILE_ACCESS
    pos = ftell(save_stream);
#else
    panic_unsupported();
#endif

    padding = (4 - (pos & 3)) & 3;

    for (i=0; i<padding; ++i)
    {
        saveg_read8();
    }
#endif
}

static void saveg_write_pad(void)
{
#if !SAVE_COMPRESSED
    unsigned long pos;
    int padding;
    int i;

#if !NO_FILE_ACCESS
    pos = ftell(save_stream);
#else
    panic_unsupported();
#endif

    padding = (4 - (pos & 3)) & 3;

    for (i=0; i<padding; ++i)
    {
        saveg_write8(0);
    }
#endif
}


// Pointers

static void *saveg_readp(void)
{
#if !DOOM_TINY
    return (void *) (intptr_t) saveg_read32();
#else
    return NULL;
#endif
}

static void saveg_writep(const void *p)
{
#if !DOOM_TINY
    saveg_write32((intptr_t) p);
#endif
}

// Enum values are 32-bit integers.

#if !LOAD_COMPRESSED
#define saveg_read_enum() saveg_read32()
#else
#define saveg_read_enum() saveg_read8();
#endif
#if !SAVE_COMPRESSED
#define saveg_write_enum(x) saveg_write32(x)
#else
#define saveg_write_enum(x) saveg_write8(x)
#endif
//
// Structure read/write functions
//

//
// mapthing_t
//

static void saveg_read_mapthing_t(mapthing_t *str)
{
    // short x;
    str->x = saveg_read16();

    // short y;
    str->y = saveg_read16();

    // short angle;
    str->angle = saveg_read16();

    // short type;
    str->type = saveg_read16();

    // short options;
    str->options = saveg_read16();
}

static void saveg_write_mapthing_t(const mapthing_t *str)
{
    // short x;
    saveg_write16(str->x);

    // short y;
    saveg_write16(str->y);

    // short angle;
    saveg_write16(str->angle);

    // short type;
    saveg_write16(str->type);

    // short options;
    saveg_write16(str->options);
}

//
// think_t
//
// This is just an actionf_t.
//

void saveg_read_think_t(think_t *think) {
#if !LOAD_COMPRESSED
    uint32_t p = saveg_read32();
#else
    uint32_t p = saveg_read_bit();
#endif
    if (!p) *think = ThinkF_NULL;
    else *think = ThinkF_INVALID; // should be overwritten
}

void saveg_write_think_t(think_t think) {
#if !SAVE_COMPRESSED
    if (think != ThinkF_NULL)
        saveg_write32(1);
    else
        saveg_write32(0);
#else
    saveg_write_bit(think != ThinkF_NULL);
#endif
}

//
// thinker_t
//

static void saveg_read_thinker_t(thinker_t *str)
{
#if !LOAD_COMPRESSED
    saveg_readp(); // prev pointer (doesn't exist any more)

    // struct thinker_s* next;
    /*str->sp_next = */saveg_readp();

#endif
    // think_t function;
    saveg_read_think_t(&str->function);
}

static void saveg_write_thinker_t(thinker_t *str)
{
#if !SAVE_COMPRESSED
    // struct thinker_s* prev;
    saveg_writep((void *)1); // doesn't exist any more

    // struct thinker_s* next;
    saveg_writep(shortptr_to_ptr(str->sp_next)); // garbage
#endif

    // think_t function;
    saveg_write_think_t(str->function);
}

//
// mobj_t
//

static mobj_t *saveg_read_mobj_t()
{
    int pl;

    mobj_t *str;
#if LOAD_COMPRESSED
    int type = saveg_read8();
    int size = mobj_flags_is_static(mobjinfo[type].flags) ? sizeof(mobj_t) : sizeof(mobjfull_t);
    str = Z_ThinkMalloc (size, PU_LEVEL, 0);
    // these are needed for decoding early
    str->type = type;
    str->spawnpoint = saveg_read16();
    str->flags = mobjinfo[str->type].flags; // we need MF_DECORATION, but only change modified bits later anyway
#else
    str = Z_ThinkMalloc (sizeof(mobjfull_t), PU_LEVEL, 0);
#endif
    xprintf("  type %d spawn %d flags %08x\n", str->type, str->spawnpoint, str->flags);
    // todo we need to merge in m_obj's

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

#if !LOAD_COMPRESSED
    // fixed_t x;
    str->xy.x = saveg_read32();

    // fixed_t y;
    str->xy.y = saveg_read32();
#else
    if (saveg_read_bit()) {
        str->xy.x = saveg_read32();
        str->xy.y = saveg_read32();
        xprintf("  xy explicit %08x, %08x\n", str->xy.x, str->xy.y);
    } else {
        str->xy.x = spawnpoint_mapthing(str->spawnpoint).x << FRACBITS;
        str->xy.y = spawnpoint_mapthing(str->spawnpoint).y << FRACBITS;
        xprintf("  xy spawn %08x, %08x\n", str->xy.x, str->xy.y);
    }

    // we need the subsector to decode some other stuff
    subsector_t  *ss = R_PointInSubsector (str->xy.x, str->xy.y);
#if !SHRINK_MOBJ
    str->subsector = ss;
#else
    str->sector_num = subsector_sector(ss) - sectors;
    xprintf("  sector %d\n", str->sector_num);
#endif
#endif

    // fixed_t z;
#if !LOAD_COMPRESSED
    str->z = saveg_read32();
#else
    if (!mobj_is_static(str)) {
        mobj_full(str)->height = saveg_read_maybe(mobj_info(str)->height,
                                                       saveg_read32, "height");
        mobj_full(str)->floorz = saveg_read_maybe(sector_floorheight(mobj_sector(str)), saveg_read_low_zeros, "floorz");
        mobj_full(str)->ceilingz = saveg_read_maybe(sector_ceilingheight(mobj_sector(str)), saveg_read_low_zeros, "ceilingz");
    }

    if (!saveg_read_bit()) {
        // 0 floor
        str->z = mobj_floorz(str);
        xprintf("  on floor z %08x\n", str->z);
    } else if (!saveg_read_bit()) {
        // 10 ceiling
        str->z = mobj_ceilingz(str) - mobj_height(str);
        xprintf("  on ceiling z %08x\n", str->z);
    } else {
        // 11 arbitrary
        str->z = saveg_read32();
        xprintf("  explicit z %08x\n", str->z);
    }
#endif

#if !LOAD_COMPRESSED
    // struct mobj_s* snext;
    /* str->sp_snext = */ saveg_readp();

    // struct mobj_s* sprev;
    /* str->sp_sprev = */ saveg_readp();
#endif

#if !LOAD_COMPRESSED
    // angle_t angle;
    fixed_t angle = saveg_read32();
#else
    if (!mobj_is_static(str)) {
        mobj_full(str)->angle = saveg_read_maybe(spawnpoint_mapthing(str->spawnpoint).angle << FRACBITS,
                                                  saveg_read_low_zeros, "angle");
    }
#endif

#if !LOAD_COMPRESSED
#if !SHRINK_MOBJ
    // spritenum_t sprite;
    str->sprite = saveg_read_enum();

    // int frame;
    str->frame = saveg_read32();
#if FF_FULLBRIGHT != 0x8000
    if (str->frame & 0x8000) str->frame = (str->frame & ~0x8000) | FF_FULLBRIGHT;
#endif
#else
    saveg_read_enum(); saveg_read32();
#endif
#endif

#if !LOAD_COMPRESSED
    // struct mobj_s* bnext;
    /* str->sp_bnext = */ saveg_readp();

    // struct mobj_s* bprev;
    /* str->sp_bprev = */ saveg_readp();

    // struct subsector_s* subsector;
    /* str->subsector = */saveg_readp();
#endif

#if !LOAD_COMPRESSED
    fixed_t floorz = saveg_read32();
    fixed_t ceilingz = saveg_read32();
    fixed_t radius = saveg_read32();
    fixed_t height = saveg_read32();
    fixed_t momx = saveg_read32();
    fixed_t momy = saveg_read32();
    fixed_t momz = saveg_read32();
#else
    if (!mobj_is_static(str)) {
        mobj_full(str)->radius = saveg_read_maybe(mobj_info(str)->radius, saveg_read32, "radius");
        mobj_full(str)->momx = saveg_read_maybe(0, saveg_read32, "momx");
        mobj_full(str)->momy = saveg_read_maybe(0, saveg_read32, "momy");
        mobj_full(str)->momz = saveg_read_maybe(0, saveg_read32, "momz");
    }
#endif

#if !LOAD_COMPRESSED
    // int validcount;
    /* str->validcount = */saveg_read32();
#endif

    // mobjtype_t type;
#if !LOAD_COMPRESSED
    str->type = saveg_read_enum();
#endif

#if !LOAD_COMPRESSED
    // note static check depends on type so we need to know that first
    if (!mobj_is_static(str)) {
        mobj_full(str)->angle = angle;
        mobj_full(str)->floorz = floorz;
        mobj_full(str)->ceilingz = ceilingz;
        mobj_full(str)->radius = radius;
        mobj_full(str)->height = height;
        mobj_full(str)->momx = momx;
        mobj_full(str)->momy = momy;
        mobj_full(str)->momz = momz;
    }
#endif

    // mobjinfo_t* info;
#if !LOAD_COMPRESSED
#if !SHRINK_MOBJ
    str->info = saveg_readp();
#else
    saveg_readp();
#endif
#endif

#if !LOAD_COMPRESSED
    // int tics;
    str->tics = saveg_read32();
#else
    str->tics = saveg_read_maybe(-1, (saveg_reader)saveg_read8, "tics");
#endif

#if !LOAD_COMPRESSED
    // state_t* state;
#if !SHRINK_MOBJ
    str->state = &states[saveg_read32()];
#else
    str->state_num = saveg_read32();
#endif
#else
    // todo fewer bits?
    str->state_num = saveg_read_maybe(mobj_info(str)->spawnstate, (saveg_reader)saveg_read16, "state");
#endif

    // int flags;
#if !LOAD_COMPRESSED
    str->flags = saveg_read32();
#else
    // note we already set these to mobj_info()->flags above hence str->flags as default
    str->flags = saveg_read_maybe(str->flags, saveg_read32, "flags");
#endif
    if (mobjinfo[str->type].flags & MF_DECORATION) {
        str->flags |= MF_DECORATION;
    }

#if !LOAD_COMPRESSED
    int health = saveg_read32();
    int movedir = saveg_read32();
    int movecount = saveg_read32();
    // struct mobj_s* target;
    /*str->target = */saveg_readp();

    int reactiontime = saveg_read32();
    int threshold = saveg_read32();

    if (!mobj_is_static(str)) {
        mobj_full(str)->health = health;
        mobj_full(str)->movedir = movedir;
        mobj_full(str)->movecount = movecount;
        mobj_reactiontime(str) = reactiontime;
        mobj_full(str)->threshold = threshold;
    }
#else
    if (!mobj_is_static(str)) {
        mobj_full(str)->health = saveg_read_maybe(mobj_info(str)->spawnhealth, (saveg_reader)saveg_read16, "health");
        mobj_full(str)->movedir = saveg_read_bits(3);
        // todo just curioous want this is
        mobj_full(str)->movecount = saveg_read_maybe(0, saveg_read32, "move");
        mobj_full(str)->reactiontime = saveg_read_maybe( mobj_info(str)->reactiontime, (saveg_reader)saveg_read8,
                          "reaction");
        mobj_full(str)->threshold = saveg_read_maybe(0, (saveg_reader)saveg_read8, "threshold");
    }
#endif

    // struct player_s* player;
#if !LOAD_COMPRESSED
    pl = saveg_read32();
    int lastlook = saveg_read32();
#else
    int lastlook;
    if (!mobj_is_static(str)) {
        pl = saveg_read_bit() ? saveg_read_bits(2) + 1 : 0;
        xprintf("PL %d\n", pl);
        lastlook = saveg_read_bits(2);
        xprintf("LL %d\n", lastlook);
    }
#endif

    if (!mobj_is_static(str)) {
        if (pl > 0) {
            mobj_full(str)->sp_player = player_to_shortptr(&players[pl - 1]);
            mobj_player(str)->mo = str;
        } else {
            mobj_full(str)->sp_player = 0;
        }

        // int lastlook;
        mobj_full(str)->lastlook = lastlook;
    }

#if !LOAD_COMPRESSED
    // mapthing_t spawnpoint;
    mapthing_t mt;
    saveg_read_mapthing_t(&mt);
#if !SHRINK_MOBJ
    mobj_spawnpoint(str) = mt;
#else
    str->spawnpoint = 0;
    for(int i=0;i<nummapthings;i++) {
        if (!memcmp(&mapthings[i], &mt, sizeof(mt))) {
            str->spawnpoint = i;
            break;
        }
    }
#endif
#else
#endif

#if !LOAD_COMPRESSED
    // struct mobj_s* tracer;
    /*str->tracer = */saveg_readp();
#endif
    return str;
}

static void saveg_write_mobj_t(mobj_t *str)
{
#if SAVE_COMPRESSED
    // needed to decode other things
    saveg_write8(str->type);
    saveg_write16(str->spawnpoint);
#endif

    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

#if !SAVE_COMPRESSED
    // fixed_t x;
    saveg_write32(str->xy.x);

    // fixed_t y;
    saveg_write32(str->xy.y);
#else
    if (str->xy.x == (spawnpoint_mapthing(str->spawnpoint).x << FRACBITS) &&
        str->xy.y == (spawnpoint_mapthing(str->spawnpoint).y << FRACBITS)) {
        saveg_write_bit(0);
        xprintf("Unmoved %d %08x,%08x\n", str->spawnpoint, str->xy.x, str->xy.y);
    } else {
        saveg_write_bit(1);
        xprintf("Moved %d %08x,%08x %08x,%08x\n", str->spawnpoint, str->xy.x, str->xy.y, spawnpoint_mapthing(str->spawnpoint).x << FRACBITS, spawnpoint_mapthing(str->spawnpoint).y << FRACBITS);
        // fixed_t x;
        saveg_write32(str->xy.x);

        // fixed_t y;
        saveg_write32(str->xy.y);
    }
#endif

    xprintf("  sector %d\n", str->sector_num);

#if !SAVE_COMPRESSED
    saveg_write32(str->z);
#else
    // height is needed for decode of on ceiling
    if (!mobj_is_static(str)) {
        saveg_write_maybe(mobj_height(str), mobj_info(str)->height, saveg_write32, "height");
        saveg_write_maybe(mobj_floorz(str), sector_floorheight(mobj_sector(str)), saveg_write_low_zeros, "floorz");
        saveg_write_maybe(mobj_ceilingz(str), sector_ceilingheight(mobj_sector(str)), saveg_write_low_zeros, "ceilingz");
    }

    // fixed_t z;
    if (str->z == mobj_floorz(str)) {
        saveg_write_bit(0);
        xprintf("  on the floor %08x\n", str->z);
    } else if (str->z == mobj_ceilingz(str) - mobj_height(str)) {
        saveg_write_bits(1, 2);
        xprintf("  on the ceiling %08x\n", str->z);
    } else {
        saveg_write_bits(3, 2);
        saveg_write32(str->z);
    }
#endif

    // struct mobj_s* snext;
#if !SAVE_COMPRESSED
    saveg_writep(shortptr_to_ptr(str->sp_snext)); // note these are garbage anyway

    // struct mobj_s* sprev;
    saveg_writep((void*)1);; // garbage anyway, so maybe collapse
#endif

    // angle_t angle;
#if !SAVE_COMPRESSED
    saveg_write32(mobj_is_static(str) ? 0 : (int)mobj_full(str)->angle);
#else
    if (!mobj_is_static(str)) {
        saveg_write_maybe(mobj_full(str)->angle, spawnpoint_mapthing(str->spawnpoint).angle << FRACBITS,
                          saveg_write_low_zeros, "angle");
    }
#endif

#if !SAVE_COMPRESSED // these are implicit in the current state (saved below)
    // spritenum_t sprite;
    saveg_write_enum(mobj_sprite(str));
    // int frame;
    int frame = mobj_frame(str);
#if FF_FULLBRIGHT != 0x8000
    if (frame & FF_FULLBRIGHT) frame = (frame & ~FF_FULLBRIGHT) | 0x8000;
#endif
    saveg_write32(frame);
#endif

#if !SAVE_COMPRESSED
    // struct mobj_s* bnext;
    saveg_writep(shortptr_to_ptr(str->sp_bnext)); // garbage anyway, so maybe collapse

    // struct mobj_s* bprev;
    saveg_writep((void*)1); // garbage anyway, so maybe collapse

    // struct subsector_s* subsector;
#if !SHRINK_MOBJ
    saveg_writep(str->subsector);
#else
    saveg_writep(NULL); // should do this anyway, but still
#endif
#endif

#if !SAVE_COMPRESSED
    // fixed_t floorz;
    saveg_write32(mobj_floorz(str));

    // fixed_t ceilingz;
    saveg_write32(mobj_ceilingz(str));
    // fixed_t radius;
    saveg_write32(mobj_radius(str));

    // fixed_t height;
    saveg_write32(mobj_height(str));

    // fixed_t momx;
    if (mobj_is_static(str)) {
        saveg_write32(0);
        saveg_write32(0);
        saveg_write32(0);
    } else {
        saveg_write32(mobj_full(str)->momx);

        // fixed_t momy;
        saveg_write32(mobj_full(str)->momy);

        // fixed_t momz;
        saveg_write32(mobj_full(str)->momz);
    }
#else
    if (!mobj_is_static(str)) {
        saveg_write_maybe(mobj_radius(str), mobj_info(str)->radius, saveg_write32, "radius");
        saveg_write_maybe(mobj_full(str)->momx, 0, saveg_write32, "momx");
        saveg_write_maybe(mobj_full(str)->momy, 0, saveg_write32, "momy");
        saveg_write_maybe(mobj_full(str)->momz, 0, saveg_write32, "momz");
    }
#endif

    // int validcount;
#if !SAVE_COMPRESSED
    saveg_write32(0);//str->validcount);
#endif

#if !SAVE_COMPRESSED
    // mobjtype_t type;
    saveg_write_enum(str->type);
#endif

    // mobjinfo_t* info;
#if !SAVE_COMPRESSED
    saveg_writep(mobj_info(str));
#endif

    // int tics;
#if !SAVE_COMPRESSED
    saveg_write32(str->tics);
#else
    saveg_write_maybe(str->tics, -1, (saveg_writer) saveg_write8, "tics");
#endif

    // state_t* state;
#if !SAVE_COMPRESSED
    saveg_write32(mobj_state_num(str));
#else
    // todo fewer bits?
    saveg_write_maybe(mobj_state_num(str), mobj_info(str)->spawnstate, (saveg_writer) saveg_write16, "state");
#endif

#if !SAVE_COMPRESSED
    saveg_write32(str->flags & ~MF_DECORATION);
#else
    // int flags;
    saveg_write_maybe(str->flags, mobj_info(str)->flags, saveg_write32, "flags");
#endif

    if (mobj_is_static(str)) {
#if !SAVE_COMPRESSED
        // int health;
        saveg_write32(mobj_info(str)->spawnhealth);

        // int movedir;
        saveg_write32(0);

        // int movecount;
        saveg_write32(0);

        // struct mobj_s* target;
        saveg_writep(0);

        // int reactiontime;
        saveg_write32(mobj_info(str)->reactiontime);

        // int threshold;
        saveg_write32(0);

        // struct player_s* player;
        saveg_write32(0);

        // int lastlook;
        saveg_write32(0);
#endif
    } else {
        xprintf("  non static\n");
        // int health;
#if !SAVE_COMPRESSED
        saveg_write32(mobj_full(str)->health);

        // int movedir;
        saveg_write32(mobj_full(str)->movedir);

        // int movecount;
        saveg_write32(mobj_full(str)->movecount);

        // struct mobj_s* target;
        saveg_writep(mobj_target(str));

        // int reactiontime;
        saveg_write32(mobj_reactiontime(str));

        // int threshold;
        saveg_write32(mobj_full(str)->threshold);
#else
        saveg_write_maybe(mobj_full(str)->health, mobj_info(str)->spawnhealth, (saveg_writer) saveg_write16, "health");
        saveg_write_bits(mobj_full(str)->movedir, 3);
        // todo just curioous want this is
        saveg_write_maybe(mobj_full(str)->movecount, 0, saveg_write32, "move");
        saveg_write_maybe(mobj_full(str)->reactiontime, mobj_info(str)->reactiontime, (saveg_writer) saveg_write8,
                          "reaction");
        saveg_write_maybe(mobj_full(str)->threshold, 0, (saveg_writer) saveg_write8, "threshold");
#endif

        // struct player_s* player;
        if (mobj_full(str)->sp_player) {
#if !SAVE_COMPRESSED
            saveg_write32(mobj_player(str) - players + 1);
#else
            assert(!mobj_is_static(str));
            if (!mobj_is_static(str)) {
                xprintf("PL %d\n", (mobj_player(str) - players) + 1);
                saveg_write_bit(1);
                saveg_write_bits((mobj_player(str) - players), 2);
            }
#endif
        } else {
#if !SAVE_COMPRESSED
            saveg_write32(0);
#else
            if (!mobj_is_static(str)) {
                xprintf("PL (0)\n");
                saveg_write_bit(0);
            }
#endif
        }

#if !SAVE_COMPRESSED
        // int lastlook;
        saveg_write32(mobj_full(str)->lastlook);
#else
        if (!mobj_is_static(str)) {
            xprintf("LL %d\n", mobj_full(str)->lastlook);
            saveg_write_bits(mobj_full(str)->lastlook, 2);
        }
#endif
    }
#if !SAVE_COMPRESSED // implicit with spawnpoint
    // mapthing_t spawnpoint;
    saveg_write_mapthing_t(&mobj_spawnpoint(str));
#endif

#if !SAVE_COMPRESSED
    if (mobj_is_static(str)) {
        saveg_writep(0);
    } else {
        // struct mobj_s* tracer;
        saveg_writep(mobj_tracer(str));
    }
#endif
}


//
// ticcmd_t
//

static void saveg_read_ticcmd_t(ticcmd_t *str)
{

    // signed char forwardmove;
    str->forwardmove = saveg_read8();

    // signed char sidemove;
    str->sidemove = saveg_read8();

    // short angleturn;
    str->angleturn = saveg_read16();

    // short consistancy;
    str->consistancy = saveg_read16();

    // byte chatchar;
    str->chatchar = saveg_read8();

    // byte buttons;
    str->buttons = saveg_read8();
}

static void saveg_write_ticcmd_t(ticcmd_t *str)
{

    // signed char forwardmove;
    saveg_write8(str->forwardmove);

    // signed char sidemove;
    saveg_write8(str->sidemove);

    // short angleturn;
    saveg_write16(str->angleturn);

    // short consistancy;
    saveg_write16(str->consistancy);

    // byte chatchar;
    saveg_write8(str->chatchar);

    // byte buttons;
    saveg_write8(str->buttons);
}

//
// pspdef_t
//

static void saveg_read_pspdef_t(pspdef_t *str)
{
    int state;

    // state_t* state;
    state = saveg_read32();

    if (state > 0)
    {
        str->state = &states[state];
    }
    else
    {
        str->state = NULL;
    }

    // int tics;
    str->tics = saveg_read32();

    // fixed_t sx;
    str->sx = saveg_read32();

    // fixed_t sy;
    str->sy = saveg_read32();
}

static void saveg_write_pspdef_t(pspdef_t *str)
{
    // state_t* state;
    if (str->state)
    {
        saveg_write32(str->state - states);
    }
    else
    {
        saveg_write32(0);
    }

    // int tics;
    saveg_write32(str->tics);

    // fixed_t sx;
    saveg_write32(str->sx);

    // fixed_t sy;
    saveg_write32(str->sy);
}

//
// player_t
//

static void saveg_read_player_t(player_t *str)
{
    int i;

    // mobj_t* mo;
    str->mo = saveg_readp();

    // playerstate_t playerstate;
    str->playerstate = saveg_read_enum();

    // ticcmd_t cmd;
    saveg_read_ticcmd_t(&str->cmd);

    // fixed_t viewz;
    str->viewz = saveg_read32();

    // fixed_t viewheight;
    str->viewheight = saveg_read32();

    // fixed_t deltaviewheight;
    str->deltaviewheight = saveg_read32();

    // fixed_t bob;
    str->bob = saveg_read32();

    // int health;
    str->health = saveg_read32();

    // int armorpoints;
    str->armorpoints = saveg_read32();

    // int armortype;
    str->armortype = saveg_read32();

    // int powers[NUMPOWERS];
    for (i=0; i<NUMPOWERS; ++i)
    {
        str->powers[i] = saveg_read32();
    }

    // boolean cards[NUMCARDS];
    for (i=0; i<NUMCARDS; ++i)
    {
        str->cards[i] = saveg_read_boolean();
    }

    // boolean backpack;
    str->backpack = saveg_read_boolean();

    // int frags[MAXPLAYERS];
    for (i=0; i<MAXPLAYERS; ++i)
    {
        str->frags[i] = saveg_read32();
    }

    // weapontype_t readyweapon;
    str->readyweapon = saveg_read_enum();

    // weapontype_t pendingweapon;
    str->pendingweapon = saveg_read_enum();

    // boolean weaponowned[NUMWEAPONS];
    for (i=0; i<NUMWEAPONS; ++i)
    {
        str->weaponowned[i] = saveg_read_boolean();
    }

    // int ammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        str->ammo[i] = saveg_read32();
    }

    // int maxammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        str->maxammo[i] = saveg_read32();
    }

    // int attackdown;
    str->attackdown = saveg_read32();

    // int usedown;
    str->usedown = saveg_read32();

    // int cheats;
    str->cheats = saveg_read32();

    // int refire;
    str->refire = saveg_read32();

    // int killcount;
    str->killcount = saveg_read32();

    // int itemcount;
    str->itemcount = saveg_read32();

    // int secretcount;
    str->secretcount = saveg_read32();

    // char* message;
    str->message = saveg_readp();

    // int damagecount;
    str->damagecount = saveg_read32();

    // int bonuscount;
    str->bonuscount = saveg_read32();

    // mobj_t* attacker;
    str->attacker = saveg_readp();

    // int extralight;
    str->extralight = saveg_read32();

    // int fixedcolormap;
    str->fixedcolormap = saveg_read32();

    // int colormap;
    str->colormap = saveg_read32();

    // pspdef_t psprites[NUMPSPRITES];
    for (i=0; i<NUMPSPRITES; ++i)
    {
        saveg_read_pspdef_t(&str->psprites[i]);
    }

    // boolean didsecret;
    str->didsecret = saveg_read_boolean();
}

static void saveg_write_player_t(player_t *str)
{
    int i;

    // mobj_t* mo;
    saveg_writep(str->mo);

    // playerstate_t playerstate;
    saveg_write_enum(str->playerstate);

    // ticcmd_t cmd;
    saveg_write_ticcmd_t(&str->cmd);

    // fixed_t viewz;
    saveg_write32(str->viewz);

    // fixed_t viewheight;
    saveg_write32(str->viewheight);

    // fixed_t deltaviewheight;
    saveg_write32(str->deltaviewheight);

    // fixed_t bob;
    saveg_write32(str->bob);

    // int health;
    saveg_write32(str->health);

    // int armorpoints;
    saveg_write32(str->armorpoints);

    // int armortype;
    saveg_write32(str->armortype);

    // int powers[NUMPOWERS];
    for (i=0; i<NUMPOWERS; ++i)
    {
        saveg_write32(str->powers[i]);
    }

    // boolean cards[NUMCARDS];
    for (i=0; i<NUMCARDS; ++i)
    {
        saveg_write_boolean(str->cards[i]);
    }

    // boolean backpack;
    saveg_write_boolean(str->backpack);

    // int frags[MAXPLAYERS];
    for (i=0; i<MAXPLAYERS; ++i)
    {
        saveg_write32(str->frags[i]);
    }

    // weapontype_t readyweapon;
    saveg_write_enum(str->readyweapon);

    // weapontype_t pendingweapon;
    saveg_write_enum(str->pendingweapon);

    // boolean weaponowned[NUMWEAPONS];
    for (i=0; i<NUMWEAPONS; ++i)
    {
        saveg_write_boolean(str->weaponowned[i]);
    }

    // int ammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        saveg_write32(str->ammo[i]);
    }

    // int maxammo[NUMAMMO];
    for (i=0; i<NUMAMMO; ++i)
    {
        saveg_write32(str->maxammo[i]);
    }

    // int attackdown;
    saveg_write32(str->attackdown);

    // int usedown;
    saveg_write32(str->usedown);

    // int cheats;
    saveg_write32(str->cheats);

    // int refire;
    saveg_write32(str->refire);

    // int killcount;
    saveg_write32(str->killcount);

    // int itemcount;
    saveg_write32(str->itemcount);

    // int secretcount;
    saveg_write32(str->secretcount);

    // char* message;
    saveg_writep(str->message);

    // int damagecount;
    saveg_write32(str->damagecount);

    // int bonuscount;
    saveg_write32(str->bonuscount);

    // mobj_t* attacker;
    saveg_writep(str->attacker);

    // int extralight;
    saveg_write32(str->extralight);

    // int fixedcolormap;
    saveg_write32(str->fixedcolormap);

    // int colormap;
    saveg_write32(str->colormap);

    // pspdef_t psprites[NUMPSPRITES];
    for (i=0; i<NUMPSPRITES; ++i)
    {
        saveg_write_pspdef_t(&str->psprites[i]);
    }

    // boolean didsecret;
    saveg_write_boolean(str->didsecret);
}


//
// ceiling_t
//

static void saveg_read_ceiling_t(ceiling_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // ceiling_e type;
    str->type = saveg_read_enum();

    // sector_t* sector;
#if !LOAD_COMPRESSED
    sector = saveg_read32();
#else
    sector = saveg_read16();
#endif
    str->sector = &sectors[sector];

    // fixed_t bottomheight;
    str->bottomheight = saveg_read32();

    // fixed_t topheight;
    str->topheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();

    // boolean crush;
    str->crush = saveg_read_boolean();

    // int direction;
    str->direction = saveg_read32();

    // int tag;
    str->tag = saveg_read32();

    // int olddirection;
    str->olddirection = saveg_read32();
}

static void saveg_write_ceiling_t(ceiling_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // ceiling_e type;
    saveg_write_enum(str->type);

#if !SAVE_COMPRESSED
    // sector_t* sector;
    saveg_write32(str->sector - sectors);
#else
    saveg_write16(str->sector - sectors);
#endif

    // fixed_t bottomheight;
    saveg_write32(str->bottomheight);

    // fixed_t topheight;
    saveg_write32(str->topheight);

    // fixed_t speed;
    saveg_write32(str->speed);

    // boolean crush;
    saveg_write_boolean(str->crush);

    // int direction;
    saveg_write32(str->direction);

    // int tag;
    saveg_write32(str->tag);

    // int olddirection;
    saveg_write32(str->olddirection);
}

//
// vldoor_t
//

static void saveg_read_vldoor_t(vldoor_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // vldoor_e type;
    str->type = saveg_read_enum();

#if !LOAD_COMPRESSED
    sector = saveg_read32();
#else
    sector = saveg_read16();
#endif
    str->sector = &sectors[sector];

    // fixed_t topheight;
    str->topheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();

    // int direction;
    str->direction = saveg_read32();

    // int topwait;
    str->topwait = saveg_read32();

    // int topcountdown;
    str->topcountdown = saveg_read32();
}

static void saveg_write_vldoor_t(vldoor_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // vldoor_e type;
    saveg_write_enum(str->type);

    // sector_t* sector;
    saveg_write32(str->sector - sectors);

    // fixed_t topheight;
    saveg_write32(str->topheight);

    // fixed_t speed;
    saveg_write32(str->speed);

    // int direction;
    saveg_write32(str->direction);

    // int topwait;
    saveg_write32(str->topwait);

    // int topcountdown;
    saveg_write32(str->topcountdown);
}

//
// floormove_t
//

static void saveg_read_floormove_t(floormove_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // floor_e type;
    str->type = saveg_read_enum();

    // boolean crush;
    str->crush = saveg_read_boolean();

    // sector_t* sector;
#if !LOAD_COMPRESSED
    sector = saveg_read32();
#else
    sector = saveg_read16();
#endif
    str->sector = &sectors[sector];

    // int direction;
    str->direction = saveg_read32();

    // int newspecial;
    str->newspecial = saveg_read32();

    // short texture;
    str->texture = saveg_read16();

    // fixed_t floordestheight;
    str->floordestheight = saveg_read32();

    // fixed_t speed;
    str->speed = saveg_read32();
}

static void saveg_write_floormove_t(floormove_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // floor_e type;
    saveg_write_enum(str->type);

    // boolean crush;
    saveg_write_boolean(str->crush);

    // sector_t* sector;
#if !SAVE_COMPRESSED
    saveg_write32(str->sector - sectors);
#else
    saveg_write16(str->sector - sectors);
#endif

    // int direction;
    saveg_write32(str->direction);

    // int newspecial;
    saveg_write32(str->newspecial);

    // short texture;
    saveg_write16(str->texture);

    // fixed_t floordestheight;
    saveg_write32(str->floordestheight);

    // fixed_t speed;
    saveg_write32(str->speed);
}

//
// plat_t
//

static void saveg_read_plat_t(plat_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
#if !LOAD_COMPRESSED
    sector = saveg_read32();
#else
    sector = saveg_read16();
#endif
    str->sector = &sectors[sector];

    // fixed_t speed;
    str->speed = saveg_read32();

    // fixed_t low;
    str->low = saveg_read32();

    // fixed_t high;
    str->high = saveg_read32();

    // int wait;
    str->wait = saveg_read32();

    // int count;
    str->count = saveg_read32();

    // plat_e status;
    str->status = saveg_read_enum();

    // plat_e oldstatus;
    str->oldstatus = saveg_read_enum();

    // boolean crush;
    str->crush = saveg_read_boolean();

    // int tag;
    str->tag = saveg_read32();

    // plattype_e type;
    str->type = saveg_read_enum();
}

static void saveg_write_plat_t(plat_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
#if !SAVE_COMPRESSED
    saveg_write32(str->sector - sectors);
#else
    saveg_write16(str->sector - sectors);
#endif

    // fixed_t speed;
    saveg_write32(str->speed);

    // fixed_t low;
    saveg_write32(str->low);

    // fixed_t high;
    saveg_write32(str->high);

    // int wait;
    saveg_write32(str->wait);

    // int count;
    saveg_write32(str->count);

    // plat_e status;
    saveg_write_enum(str->status);

    // plat_e oldstatus;
    saveg_write_enum(str->oldstatus);

    // boolean crush;
    saveg_write_boolean(str->crush);

    // int tag;
    saveg_write32(str->tag);

    // plattype_e type;
    saveg_write_enum(str->type);
}

//
// lightflash_t
//

static void saveg_read_lightflash_t(lightflash_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
#if !LOAD_COMPRESSED
    sector = saveg_read32();
#else
    sector = saveg_read16();
#endif
    str->sector = &sectors[sector];

    // int count;
    str->count = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int minlight;
    str->minlight = saveg_read32();

    // int maxtime;
    str->maxtime = saveg_read32();

    // int mintime;
    str->mintime = saveg_read32();
}

static void saveg_write_lightflash_t(lightflash_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
#if !SAVE_COMPRESSED
    saveg_write32(str->sector - sectors);
#else
    saveg_write16(str->sector - sectors);
#endif

    // int count;
    saveg_write32(str->count);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int minlight;
    saveg_write32(str->minlight);

    // int maxtime;
    saveg_write32(str->maxtime);

    // int mintime;
    saveg_write32(str->mintime);
}

//
// strobe_t
//

static void saveg_read_strobe_t(strobe_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
#if !LOAD_COMPRESSED
    sector = saveg_read32();
#else
    sector = saveg_read16();
#endif
    str->sector = &sectors[sector];

    // int count;
    str->count = saveg_read32();

    // int minlight;
    str->minlight = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int darktime;
    str->darktime = saveg_read32();

    // int brighttime;
    str->brighttime = saveg_read32();
}

static void saveg_write_strobe_t(strobe_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
#if !SAVE_COMPRESSED
    saveg_write32(str->sector - sectors);
#else
    saveg_write16(str->sector - sectors);
#endif

    // int count;
    saveg_write32(str->count);

    // int minlight;
    saveg_write32(str->minlight);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int darktime;
    saveg_write32(str->darktime);

    // int brighttime;
    saveg_write32(str->brighttime);
}

//
// glow_t
//

static void saveg_read_glow_t(glow_t *str)
{
    int sector;

    // thinker_t thinker;
    saveg_read_thinker_t(&str->thinker);

    // sector_t* sector;
#if !LOAD_COMPRESSED
    sector = saveg_read32();
#else
    sector = saveg_read16();
#endif
    str->sector = &sectors[sector];

    // int minlight;
    str->minlight = saveg_read32();

    // int maxlight;
    str->maxlight = saveg_read32();

    // int direction;
    str->direction = saveg_read32();
}

static void saveg_write_glow_t(glow_t *str)
{
    // thinker_t thinker;
    saveg_write_thinker_t(&str->thinker);

    // sector_t* sector;
#if !SAVE_COMPRESSED
    saveg_write32(str->sector - sectors);
#else
    saveg_write16(str->sector - sectors);
#endif

    // int minlight;
    saveg_write32(str->minlight);

    // int maxlight;
    saveg_write32(str->maxlight);

    // int direction;
    saveg_write32(str->direction);
}

//
// Write the header for a savegame
//

#if LOAD_COMPRESSED || SAVE_COMPRESSED
#define LOAD_SAVE_VERSION 1

static uint32_t calc_wad_hash() {
    uint32_t hash = LOAD_SAVE_VERSION;
#if USE_WHD
    hash = hash * 31u + whdheader->hash;
#endif
    return hash;
}
#endif

void P_WriteSaveGameHeader(char *description)
{
    int i;

#if true || !SAVE_COMPRESSED // leave this for now since save game menu uses it
    for (i=0; description[i] != '\0'; ++i)
        saveg_write8(description[i]);
    for (; i<SAVESTRINGSIZE; ++i)
        saveg_write8(0);
#else
    int l = strlen(description);
    if (l > SAVESTRINGSIZE) l = SAVESTRINGSIZE;
    static_assert(SAVESTRINGSIZE < 32, "");
    saveg_write_bits(l, 5);
    for(i=0;i<l;i++) {
        saveg_write_bits(description[i], 7);
    }
#endif

#if !SAVE_COMPRESSED
    char name[VERSIONSIZE];
    memset(name, 0, sizeof(name));
    M_snprintf(name, sizeof(name), "version %i", G_VanillaVersionCode());
    for (i=0; i<VERSIONSIZE; ++i)
        saveg_write8(name[i]);
#else
    saveg_write8(G_VanillaVersionCode());
    saveg_write32(calc_wad_hash());
#endif

    saveg_write8(gameskill);
    saveg_write8(gameepisode);
    saveg_write8(gamemap);

    for (i=0 ; i<MAXPLAYERS ; i++) {
#if !SAVE_COMPRESSED
        saveg_write8(playeringame[i]);
#else
        saveg_write_bit(playeringame[i]);
#endif
    }

#if !SAVE_COMPRESSED
    saveg_write8((leveltime >> 16) & 0xff);
    saveg_write8((leveltime >> 8) & 0xff);
    saveg_write8(leveltime & 0xff);
#else
    saveg_write_bits(leveltime & 0xffffffu, 24);
#endif
}

// 
// Read the header for a savegame
//

boolean P_ReadSaveGameHeader(void)
{
    int	 i; 
    byte a, b, c; 
    char vcheck[VERSIONSIZE]; 
    char read_vcheck[VERSIONSIZE];
	 
    // skip the description field 

    for (i=0; i<SAVESTRINGSIZE; ++i)
        saveg_read8();
    
#if !LOAD_COMPRESSED
    for (i=0; i<VERSIONSIZE; ++i)
        read_vcheck[i] = saveg_read8();

    memset(vcheck, 0, sizeof(vcheck));
    M_snprintf(vcheck, sizeof(vcheck), "version %i", G_VanillaVersionCode());
    if (strcmp(read_vcheck, vcheck) != 0)
	return false;				// bad version
#else
    if (saveg_read8() != G_VanillaVersionCode()) return false;
    if (saveg_read32() != calc_wad_hash()) return false;
#endif

    gameskill = saveg_read8();
    gameepisode = saveg_read8();
    gamemap = saveg_read8();

    for (i=0 ; i<MAXPLAYERS ; i++) {
#if !LOAD_COMPRESSED
        playeringame[i] = saveg_read8();
#else
        playeringame[i] = saveg_read_bit();
#endif
    }

    // get the times
#if !LOAD_COMPRESSED
    a = saveg_read8();
    b = saveg_read8();
    c = saveg_read8();
    leveltime = (a<<16) + (b<<8) + c;
#else
    leveltime = saveg_read_bits(24);
#endif

    return true;
}

//
// Read the end of file marker.  Returns true if read successfully.
// 

boolean P_ReadSaveGameEOF(void)
{
    int value;

    value = saveg_read8();

    return value == SAVEGAME_EOF;
}

//
// Write the end of file marker
//

void P_WriteSaveGameEOF(void)
{
    saveg_write8(SAVEGAME_EOF);
}

//
// P_ArchivePlayers
//
void P_ArchivePlayers (void)
{
    int		i;
		
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	
	saveg_write_pad();

        saveg_write_player_t(&players[i]);
    }
}



//
// P_UnArchivePlayers
//
void P_UnArchivePlayers (void)
{
    int		i;
	
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (!playeringame[i])
	    continue;
	
	saveg_read_pad();

        saveg_read_player_t(&players[i]);
	
	// will be set when unarc thinker
	players[i].mo = NULL;	
	players[i].message = NULL;
	players[i].attacker = NULL;
    }
}

//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*		sec;
    line_t*		li;
    should_be_const side_t*		si;

#if !SAVE_COMPRESSED
    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	saveg_write16(sector_floorheight(sec) >> FRACBITS);
	saveg_write16(sector_ceilingheight(sec) >> FRACBITS);
	saveg_write16(sec->floorpic);
	saveg_write16(sec->ceilingpic);
	saveg_write16(sec->lightlevel);
	saveg_write16(sec->special);		// needed?
	saveg_write16(sec->tag);		// needed?
    }
#else
    // do sectors
    uint8_t *po = sg_bo->cur;
    uint bi0 = sg_bo->bits;
    for (i=0, sec = sectors; i<numsectors ; i++,sec++)
    {
        xprintf("SEC %d %08x\n", i, (sg_bo->cur - po) * 8 + sg_bo->bits - bi0);
        th_bit_output mark = *sg_bo;
        saveg_write_bit(1);
        saveg_write_maybe(sector_floorheight(sec) >> FRACBITS, whd_sectors[i].floorheight, (saveg_writer)saveg_write16, "floorheight");
        saveg_write_maybe(sector_ceilingheight(sec) >> FRACBITS, whd_sectors[i].ceilingheight, (saveg_writer)saveg_write16, "ceilingheight");
        saveg_write_maybe(sec->floorpic, whd_sectors[i].floorpic, (saveg_writer)saveg_write8, "floorpic");
        saveg_write_maybe(sec->ceilingpic, whd_sectors[i].ceilingpic, (saveg_writer)saveg_write8, "ceilingpic");
        saveg_write_maybe(sec->lightlevel, whd_sectors[i].lightlevel, (saveg_writer)saveg_write16, "lightlevel");
        if (sec->special != whd_sectors[i].special) {
            saveg_write_bit(1);
            saveg_write_bits(sec->special, 5);		 // needed?
        } else {
            saveg_write_bit(0);
        }
        // tags are immutable
        // saveg_write_maybe(sec->tag, whd_sectors[i].tag, (saveg_writer)saveg_write16, "tag");
        uint used = (sg_bo->cur - mark.cur) * 8 + (sg_bo->bits - mark.bits);
        if (used == 7) {
            // there were no diffs
            *sg_bo = mark;
            saveg_write_bit(0);
        }
    }
#endif
#if !SAVE_COMPRESSED
        // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
	saveg_write16(line_flags(li));
	saveg_write16(line_special(li));
	saveg_write16(line_tag(li));
	for (j=0 ; j<2 ; j++)
	{
	    if (line_sidenum(li, j) == -1)
		continue;
	    
	    si = sidenum_to_side(line_sidenum(li, j));

	    saveg_write16(side_textureoffset(si) >> FRACBITS);
	    saveg_write16(side_rowoffset(si) >> FRACBITS);
	    saveg_write16(side_toptexture(si));
	    saveg_write16(side_bottomtexture(si));
	    saveg_write16(side_midtexture(si));
	}
    }
#else
    saveg_write16(linespecialoffset);
    for (i = 0,li=lines; i < numlines; i++) {
        // flags are immutable
        //saveg_write16(line_flags(li));
        // special can only be cleared
        xprintf("LI %d %p %08x\n", i, li, (sg_bo->cur - po) * 8 + sg_bo->bits - bi0);
        assert(!line_special(li) || line_special(li) == whd_line_special((li)));
        if (whd_line_special(li)) {
            saveg_write_bit(!line_special(li));
        }
        saveg_write_bit(line_is_mapped(li));
        // tags are immutable
//        saveg_write16(line_tag(li));
        for (j=0 ; j<2 ; j++)
        {
            if (line_sidenum(li, j) == -1)
                continue;

            si = sidenum_to_side(line_sidenum(li, j));

            // we keep use linespecialoffset (global) instead
            //saveg_write16(side_textureoffset(si) >> FRACBITS);

            // never changed it seems
//            saveg_write16(side_rowoffset(si) >> FRACBITS);

            uint tex = side_toptexture(si);
            xprintf("%d %d", tex <= LAST_SWITCH_TEXTURE, tex);
            if (tex <= LAST_SWITCH_TEXTURE) saveg_write_bit(is_switched_texture(si, CST_TOP));
            tex = side_bottomtexture(si);
            xprintf(" %d %d", tex <= LAST_SWITCH_TEXTURE, tex);
            if (tex <= LAST_SWITCH_TEXTURE) saveg_write_bit(is_switched_texture(si, CST_BOTTOM));
            tex = side_midtexture(si);
            xprintf(" %d %d\n", tex <= LAST_SWITCH_TEXTURE, tex);
            if (tex < LAST_SWITCH_TEXTURE) saveg_write_bit(is_switched_texture(si, CST_MID));
        }
        li += line_next_step(li);
    }
#endif
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*		sec;
    line_t*		li;
    side_t*		si;

#if !LOAD_COMPRESSED
    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
	sector_set_floorheight(sec, saveg_read16() << FRACBITS);
	sector_set_ceilingheight(sec, saveg_read16() << FRACBITS);
	sec->floorpic = saveg_read16();
	sec->ceilingpic = saveg_read16();
	sec->lightlevel = saveg_read16();
	sec->special = saveg_read16();		// needed?
	sec->tag = saveg_read16();		// needed?
	sec->specialdata = 0;
	sec->soundtarget = 0;
    }
#else
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++) {
        if (saveg_read_bit()) {
            sector_set_floorheight(sec, saveg_read_maybe(whd_sectors[i].floorheight, (saveg_reader)saveg_read16, "floorheight") << FRACBITS);
            sector_set_ceilingheight(sec, saveg_read_maybe(whd_sectors[i].ceilingheight, (saveg_reader)saveg_read16, "ceilingheight") << FRACBITS);
            sec->floorpic = saveg_read_maybe( whd_sectors[i].floorpic, (saveg_reader)saveg_read8, "floorpic");
            sec->ceilingpic = saveg_read_maybe(whd_sectors[i].ceilingpic, (saveg_reader)saveg_read8, "ceilingpic");
            sec->lightlevel = saveg_read_maybe(whd_sectors[i].lightlevel, (saveg_reader)saveg_read16, "lightlevel");
            if (saveg_read_bit()) {
                sec->special = saveg_read_bits(5);
            }
        }
    }
#endif

#if !LOAD_COMPRESSED
    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
#if !(USE_RAW_MAPLINEDEF && TEMP_IMMUTABLE_DISABLED)
        hack_rowad_p(line_t, li, flags) = saveg_read16();
        hack_rowad_p(line_t, li, special) = saveg_read16();
        hack_rowad_p(line_t, li, tag) = saveg_read16();
#else
        // todo we can set these flags correctly
        saveg_read16(); saveg_read16(); saveg_read16();
#endif
	for (j=0 ; j<2 ; j++)
	{
	    if (line_sidenum(li, j) == -1)
		continue;
	    si = (side_t *)sidenum_to_side(line_sidenum(li, j));
	    side_settextureoffset16(si, saveg_read16());
	    side_setrowoffset16(si, saveg_read16());
	    side_settoptexture(si, saveg_read16());
	    side_setbottomtexture(si, saveg_read16());
	    side_setmidtexture(si, saveg_read16());
	}
    }
#else
    linespecialoffset = saveg_read16();
    // do lines
    for (i=0, li = lines ; i<numlines ; i++)
    {
        xprintf("LI %d %p %08x\n", i, li, (sg_bi->cur - po) * 8 + bi0- sg_bi->bits);
        if (whd_line_special(li)) {
            if (saveg_read_bit()) clear_line_special(li);
        }
        if (saveg_read_bit()) line_set_mapped(li);
        // no tags as these are immutable
        for (j=0 ; j<2 ; j++)
        {
            if (line_sidenum(li, j) == -1)
                continue;
            si = (side_t *)sidenum_to_side(line_sidenum(li, j));
//            side_settextureoffset16(si, saveg_read16()); // uses linespecial
//            side_setrowoffset16(si, saveg_read16()); // never used
            uint tex = side_toptexture(si);
            xprintf("%d %d", tex <= LAST_SWITCH_TEXTURE, tex);
            if (tex <= LAST_SWITCH_TEXTURE && saveg_read_bit())
                side_settoptexture(si, tex ^ 1);
            tex = side_bottomtexture(si);
            xprintf(" %d %d", tex <= LAST_SWITCH_TEXTURE, tex);
            if (tex <= LAST_SWITCH_TEXTURE && saveg_read_bit())
                side_setbottomtexture(si, tex ^ 1);
            tex = side_midtexture(si);
            xprintf(" %d %d\n", tex <= LAST_SWITCH_TEXTURE, tex);
            if (tex <= LAST_SWITCH_TEXTURE && saveg_read_bit())
                side_setmidtexture(si, tex ^ 1);
        }
        li += line_next_step(li);
    }
#endif
}





//
// Thinkers
//
typedef enum
{
    tc_end,
    tc_mobj

} thinkerclass_t;


//
// P_ArchiveThinkers
//
void P_ArchiveThinkers (void)
{
    thinker_t*		th;

    // save off the current thinkers
    for (th = thinker_next(&thinkercap) ; th != &thinkercap ; th=thinker_next(th))
    {
	if (th->function == ThinkF_P_MobjThinker)
	{
#if !SAVE_COMPRESSED
            saveg_write8(tc_mobj);
	    saveg_write_pad();
#else
        saveg_write_bit(0);
#endif
        saveg_write_mobj_t((mobj_t *) th);

	    continue;
	}
		
	// I_Error ("P_ArchiveThinkers: Unknown thinker function");
    }

    // add a terminating marker
#if !SAVE_COMPRESSED
    saveg_write8(tc_end);
#else
    saveg_write_bit(1);
#endif
}



//
// P_UnArchiveThinkers
//
void P_UnArchiveThinkers (void)
{
    byte		tclass;
    thinker_t*		currentthinker;
    thinker_t*		next;
    mobj_t*		mobj;
    
    // remove all the current thinkers
    currentthinker = thinker_next(&thinkercap);
    while (currentthinker != &thinkercap)
    {
	next = thinker_next(currentthinker);

	if (currentthinker->function == ThinkF_P_MobjThinker) {
        P_RemoveMobj((mobj_t *) currentthinker);
    }
    Z_ThinkFree (currentthinker);

	currentthinker = next;
    }
    P_InitThinkers ();
    
    // read in saved thinkers
    while (1)
    {
#if !LOAD_COMPRESSED
	tclass = saveg_read8();
	switch (tclass)
	{
	  case tc_end:
	    return; 	// end of list
			
	  case tc_mobj:
          break;

	  default:
	    I_Error ("Unknown tclass %i in savegame",tclass);
	}
#else
        if (saveg_read_bit()) break;
#endif
        xprintf("MOBJ\n");
	    saveg_read_pad();

        mobj = saveg_read_mobj_t();

//#if !LOAD_COMPRESSED // should be zero anyway
        if (!mobj_is_static(mobj)) {
            mobj_full(mobj)->sp_target = 0;
            mobj_full(mobj)->sp_tracer = 0;
        }
//#endif
	    P_SetThingPosition (mobj);
#if !SHRINK_MOBJ
	    mobj->info = &mobjinfo[mobj->type];
#endif
        if (!mobj_is_static(mobj)) {
            mobj_full(mobj)->floorz = sector_floorheight(mobj_sector(mobj));
            mobj_full(mobj)->ceilingz = sector_ceilingheight(mobj_sector(mobj));
        }
	    mobj->thinker.function = ThinkF_P_MobjThinker;
	    P_AddThinker (&mobj->thinker);
    }

}


//
// P_ArchiveSpecials
//
enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_endspecials

} specials_e;

static void saveg_write_special_code(uint8_t e) {
#if !SAVE_COMPRESSED
    saveg_write8(e);
#else
    static_assert(tc_endspecials < 8, "");
    saveg_write_bits(e, 3);
#endif
}

static int saveg_read_special_code() {
#if !SAVE_COMPRESSED
    return saveg_read8();
#else
    static_assert(tc_endspecials < 8, "");
    return saveg_read_bits(3);
#endif
}

//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
//
void P_ArchiveSpecials (void)
{
    thinker_t*		th;
    int			i;

    // save off the current thinkers
    for (th = thinker_next(&thinkercap) ; th != &thinkercap ; th=thinker_next(th))
    {
	if (th->function == ThinkF_NULL)
	{
	    for (i = 0; i < MAXCEILINGS;i++)
		if (activeceilings[i] == ptr_to_shortptr((ceiling_t *)th))
		    break;
	    
	    if (i<MAXCEILINGS)
	    {
                saveg_write_special_code(tc_ceiling);
		saveg_write_pad();
                saveg_write_ceiling_t((ceiling_t *) th);
	    }
	    continue;
	}
			
	if (th->function == ThinkF_T_MoveCeiling)
	{
        saveg_write_special_code(tc_ceiling);
	    saveg_write_pad();
            saveg_write_ceiling_t((ceiling_t *) th);
	    continue;
	}

    if (th->function == ThinkF_T_VerticalDoor)
	{
        saveg_write_special_code(tc_door);
	    saveg_write_pad();
            saveg_write_vldoor_t((vldoor_t *) th);
	    continue;
	}

    if (th->function == ThinkF_T_MoveFloor)
	{
        saveg_write_special_code(tc_floor);
	    saveg_write_pad();
            saveg_write_floormove_t((floormove_t *) th);
	    continue;
	}

    if (th->function == ThinkF_T_PlatRaise)
	{
        saveg_write_special_code(tc_plat);
	    saveg_write_pad();
            saveg_write_plat_t((plat_t *) th);
	    continue;
	}

    if (th->function == ThinkF_T_LightFlash)
	{
        saveg_write_special_code(tc_flash);
	    saveg_write_pad();
            saveg_write_lightflash_t((lightflash_t *) th);
	    continue;
	}

    if (th->function == ThinkF_T_StrobeFlash)
	{
        saveg_write_special_code(tc_strobe);
	    saveg_write_pad();
            saveg_write_strobe_t((strobe_t *) th);
	    continue;
	}

    if (th->function == ThinkF_T_Glow)
	{
        saveg_write_special_code(tc_glow);
	    saveg_write_pad();
            saveg_write_glow_t((glow_t *) th);
	    continue;
	}
    }
	
    // add a terminating marker
    saveg_write_special_code(tc_endspecials);

}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
{
    byte		tclass;
    ceiling_t*		ceiling;
    vldoor_t*		door;
    floormove_t*	floor;
    plat_t*		plat;
    lightflash_t*	flash;
    strobe_t*		strobe;
    glow_t*		glow;
	
	
    // read in saved thinkers
    while (1)
    {
	tclass = saveg_read_special_code();

	switch (tclass)
	{
	  case tc_endspecials:
	    return;	// end of list
			
	  case tc_ceiling:
	    saveg_read_pad();
	    ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, 0);
        saveg_read_ceiling_t(ceiling);
	    ceiling->sector->specialdata = ptr_to_shortptr(ceiling);

	    if (ceiling->thinker.function != ThinkF_NULL)
		ceiling->thinker.function = ThinkF_T_MoveCeiling;

	    P_AddThinker (&ceiling->thinker);
	    P_AddActiveCeiling(ceiling);
	    break;
				
	  case tc_door:
	    saveg_read_pad();
	    door = Z_Malloc (sizeof(*door), PU_LEVEL, 0);
            saveg_read_vldoor_t(door);
	    door->sector->specialdata = ptr_to_shortptr(door);
	    door->thinker.function = ThinkF_T_VerticalDoor;
	    P_AddThinker (&door->thinker);
	    break;
				
	  case tc_floor:
	    saveg_read_pad();
	    floor = Z_Malloc (sizeof(*floor), PU_LEVEL, 0);
            saveg_read_floormove_t(floor);
	    floor->sector->specialdata = ptr_to_shortptr(floor);
	    floor->thinker.function = ThinkF_T_MoveFloor;
	    P_AddThinker (&floor->thinker);
	    break;
				
	  case tc_plat:
	    saveg_read_pad();
	    plat = Z_Malloc (sizeof(*plat), PU_LEVEL, 0);
            saveg_read_plat_t(plat);
	    plat->sector->specialdata = ptr_to_shortptr(plat);

	    if (plat->thinker.function != ThinkF_NULL)
		plat->thinker.function = ThinkF_T_PlatRaise;

	    P_AddThinker (&plat->thinker);
	    P_AddActivePlat(plat);
	    break;
				
	  case tc_flash:
	    saveg_read_pad();
	    flash = Z_Malloc (sizeof(*flash), PU_LEVEL, 0);
            saveg_read_lightflash_t(flash);
	    flash->thinker.function = ThinkF_T_LightFlash;
	    P_AddThinker (&flash->thinker);
	    break;
				
	  case tc_strobe:
	    saveg_read_pad();
	    strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, 0);
            saveg_read_strobe_t(strobe);
	    strobe->thinker.function = ThinkF_T_StrobeFlash;
	    P_AddThinker (&strobe->thinker);
	    break;
				
	  case tc_glow:
	    saveg_read_pad();
	    glow = Z_Malloc (sizeof(*glow), PU_LEVEL, 0);
            saveg_read_glow_t(glow);
	    glow->thinker.function = ThinkF_T_Glow;
	    P_AddThinker (&glow->thinker);
	    break;
				
	  default:
	    I_Error ("P_UnarchiveSpecials:Unknown tclass %i "
		     "in savegame",tclass);
	}
	
    }

}

#if PICO_ON_DEVICE
#include "w_wad.h"
#include "picoflash.h"
#include "hardware/sync.h"
#include "hardware/address_mapped.h"
#include "hardware/timer.h"
#include "picodoom.h"

const uint8_t *get_end_of_flash(void) {
    static const uint8_t *end_of_flash;
    if (!end_of_flash) {
        // look for end of flash by repeated data
        for(end_of_flash = (const uint8_t *)XIP_BASE + 2 * 1024*1024; end_of_flash < (const uint8_t *)XIP_BASE + 16 * 1024 * 1024; end_of_flash += 2 * 1024 * 1024) {
            if (!memcmp(end_of_flash, (const uint8_t *)XIP_BASE, 512)) {
                break;
            }
        }
//        printf("FLASH SPACE %p -> %p\n", whd_map_base + whdheader->size, end_of_flash);
    }
    return end_of_flash;
}

void P_SaveGameGetExistingFlashSlotAddresses(flash_slot_info_t *slots, int count) {
    const uint8_t *index = get_end_of_flash() - 4;
    int i;
    const uint8_t *limit = whd_map_base + whdheader->size;
    for(i=0;i<count;i++) {
        bool ok = false;
        if (index[0] == 0x53 && index[1] >= i && index[1] < count) {
            int size = index[2] + (index[3] << 8);
            const uint8_t *start = index - size - 4;
            if (start >= limit &&
                start[0] == 0xb7 &&
                start[1] == index[1] &&
                start[2] == (uint8_t)(size ^ 0x55) &&
                start[3] == (uint8_t)((size>>8) ^ 0xaa)) {
                ok = true;
                for(;i<index[1];i++) {
                    slots[i].data = 0;
                    slots[i].size = 0;
                }
                slots[i].data = start + 4;
                slots[i].size = size;
                index = start - 4;
//                printf("SLOT %d %s %p->%p (+%04x)\n", i, slots[i].data, slots[i].data-4, slots[i].data-4 + slots[i].size+8, slots[i].size+8);
            }
        }
        if (!ok) break;
    }
    for(;i<count;i++) {
        slots[i].data = 0;
        slots[i].size = 0;
    }
}

typedef struct {
    const uint8_t *dest;
    const uint8_t *src;
    int size;
} flash_write_element;

static void __no_inline_not_in_flash_func(write_flash_elements)(const flash_write_element *elements, int num, const uint8_t *low_dest, const uint8_t *high_dest, uint8_t *buffer4k, bool forwards) {
    static_assert(FLASH_SECTOR_SIZE == 4096, "");
    const uint8_t *first_sector = (const uint8_t *)(((uintptr_t)low_dest)&~(FLASH_SECTOR_SIZE-1));
    const uint8_t *last_sector = (const uint8_t *)(((uintptr_t)high_dest-1)&~(FLASH_SECTOR_SIZE-1));
//    for(int i=0;i<num;i++) {
//        if (elements[i].dest < low_dest || elements[i].dest+elements[i].size > high_dest) {
//            panic("bad ranges");
//        }
//    }

//    uint32_t save = spin_lock_blocking(spin_lock_instance(PICO_SPINLOCK_ID_HARDWARE_CLAIM));
    for(const uint8_t *sector = forwards ? first_sector : last_sector;
            forwards ? sector <= last_sector : sector >= first_sector;
            sector += forwards ? FLASH_SECTOR_SIZE : -FLASH_SECTOR_SIZE) {
        memcpy(buffer4k, sector, FLASH_SECTOR_SIZE);
//        printf("SAVE/ERASE %08x + %04x\n", sector, FLASH_SECTOR_SIZE);
        for(int i=0;i<num;i++) {
            const uint8_t *to = sector;
            int from_offset = sector - elements[i].dest;
            int size = FLASH_SECTOR_SIZE;
            if (from_offset < 0) {
                size += from_offset;
                to -= from_offset;
                from_offset = 0;
            }
            if (from_offset + size > elements[i].size) {
                size = elements[i].size - from_offset;
            }
            if (size > 0) {
//                printf("  WRITE %p (%p+%04x) + %04x at %p (%p+%04x)\n", elements[i].src + from_offset, elements[i].src, from_offset, size, to, sector, to-sector);
//                hard_assert( to >= sector && to + size - sector <= FLASH_SECTOR_SIZE);
                memmove(buffer4k + (to - sector), elements[i].src + from_offset, size);
            }
        }
        uint32_t save = save_and_disable_interrupts();
        picoflash_sector_program((uintptr_t)sector - XIP_BASE, buffer4k);
        restore_interrupts(save);
    }
//    spin_unlock(spin_lock_instance(PICO_SPINLOCK_ID_HARDWARE_CLAIM), save);
}

boolean __noinline P_SaveGameWriteFlashSlot(int slot, const uint8_t *buffer, uint size, uint8_t *buffer4k) {
#define MAX_SLOTS 8
#define SLOT_OVERHEAD 8
    flash_slot_info_t slots[MAX_SLOTS];
    P_SaveGameGetExistingFlashSlotAddresses(slots, count_of(slots));
    int used = 0;
    const uint8_t *prev_slot_bottom = get_end_of_flash();
    int last_slot = 0;
    for(int i=0;i< count_of(slots);i++) {
        if (slots[i].data) {
            if (i < slot) {
                prev_slot_bottom = slots[i].data - 4;
            }
            if (i != slot) {
                used += SLOT_OVERHEAD + slots[i].size;
            }
            last_slot = i;
        }
    }
    const uint8_t *limit = whd_map_base + whdheader->size;
    int freespace = (get_end_of_flash() - limit) - used;
//    printf("SPACE %d, required %d\n", freespace, size + SLOT_OVERHEAD);
    if (freespace < size + SLOT_OVERHEAD) {
        return false;
    }
    pd_start_save_pause();
//    printf("Need to add %p->%p (+%04x)\n", prev_slot_bottom - 4 - size, prev_slot_bottom, size + 8);
    if (last_slot > slot) {
        assert(slots[last_slot.data]);
        const uint8_t *from_top = slots[slot].data ? slots[slot].data - 4 : prev_slot_bottom;
        const uint8_t *from_bottom = slots[last_slot].data - 4;
        const uint8_t *to_top = prev_slot_bottom;
        if (buffer) to_top -= size + SLOT_OVERHEAD;
        assert(from_top - from_bottom > 0);
        flash_write_element element = {
                .size = from_top - from_bottom,
                .dest = to_top - (from_top - from_bottom),
                .src = from_bottom
        };
//        printf("Need to move %p->%p (+%04x) to %p->%p\n", from_bottom, from_top, element.size, to_top - element.size, to_top);
        write_flash_elements(&element, 1, to_top - element.size, to_top, buffer4k, to_top < from_top);
    }
    if (buffer) {
        uint8_t high_marker[] = {
                0x53, (uint8_t) slot, size & 0xff, (size >> 8) & 0xff
        };
        uint8_t low_marker[] = {
                0xb7, (uint8_t) slot, 0x55 ^ (size & 0xff), 0xaa ^ ((size >> 8) & 0xff)
        };

        flash_write_element elements[3] = {
                {
                        .dest = prev_slot_bottom - 4,
                        .src = high_marker,
                        .size = 4,
                },
                {
                        .dest = prev_slot_bottom - 4 - size,
                        .src = buffer,
                        .size = (int) size,
                },
                {
                        .dest = prev_slot_bottom - 8 - size,
                        .src = low_marker,
                        .size = 4,
                }
        };
        write_flash_elements(elements, count_of(elements), prev_slot_bottom - 8 - size, prev_slot_bottom, buffer4k,
                             true);
    } else if (slot == last_slot && slots[slot].data) {
//        printf("Nuking slot %d\n", slot);
        uint8_t dummy[4] = {0};
        flash_write_element element = {
                .size = 4,
                .dest = slots[slot].data + slots[slot].size,
                .src = dummy
        };
        write_flash_elements(&element, 1, element.dest, element.dest+4, buffer4k, true);
    }
    pd_end_save_pause();
    return true;
}

#endif
