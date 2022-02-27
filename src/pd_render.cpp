/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// This is a rationalization of pd_render into something which does the sort on insert, but without all the memory
// chicanery we'll need from the final version

#include "picodoom.h"
#include "pico/sem.h"
#include "hardware/gpio.h"
#include "pico/divider.h"
#include "image_decoder.h"
#include <set>
extern "C" {
#include "doom/d_main.h"
#include "doom/r_data.h"
#include "doom/r_sky.h"
#include "doom/r_things.h"
#include "doom/m_menu.h"
#include "doom/doomstat.h"
#include "doom/am_map.h"
#include "doom/m_menu.h"
#include "doom/st_stuff.h"
#include "doom/wi_stuff.h"
#include "doom/hu_stuff.h"
#include "doom/f_wipe.h"
#include "doom/f_finale.h"
#include "v_video.h"
#include "i_video.h"
}
// todo compare with and without
#define USE_XIPCPY 0
#if PICO_ON_DEVICE
#define USE_CORE1_FOR_FLATS 1
#endif
#define USE_CORE1_FOR_REGULAR 1
#ifdef PICO_SPINLOCK_ID_OS2
#define RENDER_SPIN_LOCK PICO_SPINLOCK_ID_OS2
#else
#define RENDER_SPIN_LOCK 15
#endif
//#define DEBUG_DECODER 1
//#define DEBUG_DECODER_BUFFERS 1
//#define DEBUG_COMPOSITE 1
// we wake up core1 during rendering whole rendering part of game loop.. during
// this time it can do sound updates (since the main core is clearly not)
semaphore_t core1_wake, core0_done, core1_done;

#if USE_CORE1_FOR_FLATS
semaphore_t core1_do_flats;
int16_t core1_fr_list;
#endif
#if USE_CORE1_FOR_REGULAR
semaphore_t core1_do_regular;
#endif
pre_wipe_state_t pre_wipe_state;
static int16_t sub_gamestate;
// todo look at using scratch RAM for local linked lists/buffers (we sort of have this with stack)

CU_REGISTER_DEBUG_PINS(flat_decode, patch_decode, full_render, render_thing, render_flat, start_end)
extern uint8_t restart_song_state;

//CU_SELECT_DEBUG_PINS(patch_decode)
//CU_SELECT_DEBUG_PINS(render_thing)
//CU_SELECT_DEBUG_PINS(full_render)
//CU_SELECT_DEBUG_PINS(start_end)

#if !PICO_ON_DEVICE
#include "../whd_gen/statsomizer.h"

std::set<int> textures, patches;
statsomizer tex_count("texcount"), patch_count("patchcount");
statsomizer patch_decoder_size("patch decoder size");
#endif

#if PICO_ON_DEVICE

#include "hardware/interp.h"

#endif

extern "C" {
#include "doomtype.h"
#include "doom/r_local.h"
#include "i_system.h"
#include "w_wad.h"
#include "z_zone.h"
#include "doom/r_plane.h"
void I_UpdateSound(void);
}
void draw_cast_sprite(int sprite_lump);
#pragma GCC push_options
#if PICO_ON_DEVICE
#pragma GCC optimize("O3")
#endif

wipestate_t wipestate;
static uint8_t post_wipecount;

// todo these are only needed temporarily, so stack or "tmp buffer"
static uint16_t flat_decoder_buf[WHD_FLAT_DECODER_MAX_SIZE];
static uint8_t flat_decoder_tmp[WHD_FLAT_DECODER_MAX_SIZE];
#define PATCH_DECODER_HASH_SIZE 128
static_assert(__builtin_popcount(PATCH_DECODER_HASH_SIZE)==1, "");
static int16_t patch_hash_offsets[PATCH_DECODER_HASH_SIZE];
#define PATCH_DECODER_CIRCULAR_BUFFER_SIZE (2048-256)
static uint16_t patch_decoder_circular_buf[PATCH_DECODER_CIRCULAR_BUFFER_SIZE];
static uint16_t patch_decoder_circular_buf_write_pos;
static uint16_t patch_decoder_circular_buf_write_limit;
// this is used when decoding decoders, but also as a cache for up to 4 decoder tables (each of which are 256 bytes big)
static uint8_t patch_decoder_tmp[256 * WHD_MAX_COL_UNIQUE_PATCHES];
// which patch decoder table (or 0 if none) is stored in each of the 256 byte areas in patch_decoder_tmp
static uint16_t patch_decoder_tmp_table_patch_numbers[WHD_MAX_COL_UNIQUE_PATCHES];
// in case we max out columns during regular rendering, we will be left with gaps in the screen
// so we keep a bit set for each 4 columns (no harm in clearing columns a word wide)
static uint32_t not_fully_covered_cols[(SCREENWIDTH/4 + 31)/32];
static uint8_t not_fully_covered_yl, not_fully_covered_yh;
static int16_t fd_heads[MAX_FRAME_DRAWABLES]; // frame drawable linked lists
vpatchlists_t *vpatchlists;
int16_t visplane_heads[MAXVISPLANES];
int8_t flatnum_next[MAXVISPLANES];

#if USE_XIPCPY
#define FLAT_SOURCE_Z_SIZE 2400
static uint32_t flat_sourcez[FLAT_SOURCE_Z_SIZE/4];
#endif

#define xcolormaps colormaps

#define FUZZTABLE        50
#define FUZZOFF    (SCREENWIDTH)

const int16_t fuzzoffset[FUZZTABLE] =
        {
                FUZZOFF, -FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
                FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
                FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF,
                FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF,
                FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF,
                FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF,
                FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF, -FUZZOFF, FUZZOFF
        };

#include <algorithm>

#if PICO_ON_DEVICE
//extern fixed_t FastFixedMul(fixed_t a, fixed_t b);
#define FastFixedMul FixedMulInline
#else
#define FastFixedMul FixedMul
#endif

#define USE_INTERP PICO_ON_DEVICE
#if USE_INTERP
#define span_interp interp1_hw
#endif

#if !PICO_ON_DEVICE
#define DUMP_SORTING 1
#endif
// seems to work
typedef unsigned int uint;

int pd_frame;
int pd_flag;
fixed_t pd_scale;

extern uint8_t __aligned(4) frame_buffer[2][SCREENWIDTH * MAIN_VIEWHEIGHT];
static uint8_t __aligned(4) visplane_bit[(SCREENWIDTH / 8) * MAIN_VIEWHEIGHT]; // this is also used for patch decoding in core1 (since flats are done by then)
static int8_t flatnum_first[256];

static uint8_t *render_frame_buffer;
static uint8_t render_frame_index;
static uint8_t render_overlay_index;

static_assert(NO_USE_DC_COLORMAP, "");
static_assert(USE_ROWAD, ""); // don't want things moving!

#define SHOW_COLUMN_STATS 0
#define SHOW_COSTLY_DATA_STATS 0

// we always want to collapse range of iscale and dda
#define SHIFT 7
#define DOWN_SHIFT(x) ((x) >> SHIFT) // only used for texturemid
#define MINI (-512*65536)
#define MAXI (512*65536-1 - (1 << SHIFT))
#define TEXTUREMID_PLANE (DOWN_SHIFT(MAXI)+1)
static_assert(TEXTUREMID_PLANE == 0x3ffff, "");

#define UP_SHIFT(x) ((x) << SHIFT) // only used for texturemid
#define CLAMP(x) (((x)<<13)>>13)

#define DDA_MIN 0
#define DDA_SHIFT 0
#define DDA_DOWN_SHIFT(x) ((x)>>DDA_SHIFT)
// note max is slightly over this, but the texture is likely unintelligable at this point anyway
#define DDA_MAX 0xfffff
#define DDA_CLAMP(x) (((uint)((x)<<12))>>12)
#define DDA_UP_SHIFT(x) ((x) << DDA_SHIFT)

bool next_frame_pause;

int pd_dump_frame = -1;

// ===========================================================
// LETS LOOK AT COLUMNS AGAIN
// SORT COL
// 19: texture_mid - need markers for fuzzy, and (floor/ceiling)
// 5:  cmap_index
// 8:  len

// 22: (6:16) pd_scale (not sure we need this many fractional bits)
// 10: graphic (maybe 1 bit of y)

// 8: y
// 8: x
// 16: next

// ENCODED NON SPAN:
// <19,texture_mid:10.9> <5,cmap_index> <8, len>
// <20,iscale:4.16> <12, col_index>
// ENCODED SPAN:
// <19,0x4000> <5,cmap_index> <8, len>
// <24,0x000000> <8, visplane_idx>

struct pd_column {
#if PD_COLUMN_DEBUG
    pd_column_type type;
#endif
    int16_t next;
    uint8_t yl;
    uint8_t yh;

    uint32_t scale: 24; // ? 22 + yh = 29
    uint8_t colormap_index: 5;

    uint8_t col_hi: 3;
    uint32_t col_lo: 5;

    int32_t texturemid: 19; // 0x7ffff for flat

    union {
        uint8_t plane;
        uint8_t fd_num;
        uint8_t x;
    };
};

#define TO_COL_HI(c) ((c)>>5)
#define TO_COL_LO(c) ((c)&0x1f)
#define FROM_COL_HI_LO(h,l) (((h)<<5)|(l))

struct __packed __aligned(2) flat_run {
    int16_t next;
    uint32_t x_start: 12;
    uint32_t x_end: 12;
    uint32_t y: 8;
};

static_assert(sizeof(flat_run) == 6, "");
static_assert(sizeof(pd_column) == 12, "");
static_assert(sizeof(pd_column) == sizeof(flat_run) * 2, ""); // we pack two into the space of the other

static __aligned(4) int16_t column_heads[SCREENWIDTH * 2];
#define fuzzy_column_heads (&column_heads[SCREENWIDTH])

static void SafeUpdateSound() {
    boolean save = false;
    if (get_core_num()) {
        save = interp_in_use;
        // setting this causes the scanline code to save/restore the interp settings
        interp_in_use = true;
    }
    I_UpdateSound();
    if (get_core_num()) {
        interp_in_use = save;
    }
}

static bool column_is_psprite(const pd_column &c) {
    return c.scale == 0;
}

static bool column_is_nil(const pd_column &c) {
    return c.yl > c.yh || c.yh == 255;
}

const char *type_name(pd_column column) {
#if PD_COLUMN_DEBUG
    switch (column.type) {
        case PDCOL_MASKED:
            if (column_is_fuzzy(column)) return "fuzz";
            if (column_is_psprite(column)) return "player";
            return "masked";
        case PDCOL_BOTTOM:
            return "bottom";
        case PDCOL_CEILING:
            return "ceiling";
        case PDCOL_FLOOR:
            return "floor";
        case PDCOL_MID:
            return "mid";
        case PDCOL_SKY:
            return "sky";
        case PDCOL_TOP:
            return "top";
        default:
            return "<unknown>";
    }
#else
    if (column_is_psprite(column)) {
        return "player";
    }
    return "";
#endif
}

#define RENDER_COL_MAX 3600
static uint8_t __aligned(4) list_buffer[RENDER_COL_MAX * sizeof(pd_column) + 64*64]; // extra 64*64 is for one flat
static uint8_t *last_list_buffer_limit = list_buffer + sizeof(list_buffer);
//static_assert(text_font_cpy > list_buffer, "");
#define MAX_CACHED_FLATS (sizeof(list_buffer) / 4096)
static uint8_t cached_flat_picnum[MAX_CACHED_FLATS];
static uint8_t cached_flat_slots;
static uint8_t *cached_flat0;
static int16_t render_col_count;
#define render_cols ((pd_column *)list_buffer)
#define flat_runs ((flat_run *)list_buffer)
static int16_t render_col_free;

static int16_t alloc_pd_column(int x) {
    if (render_col_free < 0) {
        if (render_col_count == RENDER_COL_MAX) {
            assert(x>=0 && x<SCREENWIDTH);
            not_fully_covered_cols[x/(4*32)] |= 1u << ((x/4)&31);
            return -1;
        }
        render_col_free = render_col_count++;
        render_cols[render_col_free].next = -1;
    }
    int16_t rc = render_col_free;
    render_col_free = render_cols[rc].next;
    render_cols[rc].next = -1;
    return rc;
}

static void free_pd_column(int16_t rc_index) {
    assert(rc_index >= 0);
    render_cols[rc_index].next = render_col_free;
    render_col_free = rc_index;
}

#if DUMP_SORTING

const char *column_desc(int index) {
    static char buf[128];
    pd_column &pd = render_cols[index];
    sprintf(buf, "%d %s %d->%d at %08x", index, type_name(pd), pd.yl, pd.yh, pd.scale);
    return buf;
}

static void dump_column_list(int x, const char *prefix, int i) {
    printf("%d: %s:\n", x, prefix);
    int last = -1;
    bool bad = false;
    for (; i != -1; i = render_cols[i].next) {
        printf("    %s\n", column_desc(i));
        if (render_cols[i].yl <= last) {
            bad = true;
        }
        last = render_cols[i].yh;
    }
    if (bad && x < SCREENWIDTH) {
        printf("oops\n");
    }
}

static void dump_column(int x, const char *prefix) {
    dump_column_list(x, prefix, column_heads[x]);
}

#endif

// new_index can be a (non overlapping) linked list (in ascending y order)
static void push_down_x_guts(int x, int16_t new_index) {
    int16_t *prev_existing_ptr = &column_heads[x];
    int16_t existing_index = *prev_existing_ptr;
//    dump_column(x, "before");
//    dump_column_list(x, "Want to insert", new_index);
//    if (pd_frame == 176 && x == 174) {
//        printf("FUZZ %d\n", column_is_fuzzy(render_cols[new_index]));
//        printf("SPSP\n");
//    }
    do {
        assert(existing_index == *prev_existing_ptr);

        pd_column &new_col = render_cols[new_index];
        assert(!column_is_nil(new_col));

        // both lists (new, existing) are y sorted and non overlapping, so skip past anything existing
        // that is above the new columns
        while (existing_index >= 0 && new_col.yl > render_cols[existing_index].yh) {
            prev_existing_ptr = &render_cols[existing_index].next;
            existing_index = *prev_existing_ptr;
        }
        if (existing_index < 0) {
            // all new columns are below the existing ones; append rest as is
            assert(*prev_existing_ptr == -1);
            *prev_existing_ptr = new_index;
            break;
        }

        pd_column &existing_col = render_cols[existing_index];
        assert(!column_is_nil(existing_col));

        if (new_col.yh < existing_col.yl) {
            // new column is before the existing col, so just insert it
            *prev_existing_ptr = new_index;
            new_index = new_col.next;
            new_col.next = existing_index;
            if (new_index < 0) break; // loop condition (only checked when we insert our new column)
            prev_existing_ptr = &new_col.next;
            *prev_existing_ptr = existing_index;
            // and move on to the next new column
            continue;
        }
        // there is some overlap
        assert(existing_col.yl <= new_col.yh);
        if (new_col.scale < existing_col.scale) {
            // new column is in front of existing column
            const auto &cf = new_col;
            auto &cb = existing_col;

            if (cf.yl <= cb.yl) {
                if (cf.yh >= cb.yh) {
                    // new column fully obscures existing column, so unlink existing column as well as freeing it
                    *prev_existing_ptr = existing_col.next;
                    free_pd_column(existing_index);
                    existing_index = *prev_existing_ptr;
                } else {
                    // new column obscures the top of existing column, so clip the existing
                    cb.yl = cf.yh + 1;
                    assert(cb.yl <= cb.yh);
                    // and now we can insert the new column, and
                    // move on to the next new column if any, since the one we
                    // were dealing with cannot interact with any future existing columns
                    // since we didn't go past the one we just clipped (and the existing
                    // items don't overlap)
                    *prev_existing_ptr = new_index;
                    new_index = new_col.next;
                    new_col.next = existing_index;
                    if (new_index < 0) break; // loop condition (only checked when we insert our new column)
                    prev_existing_ptr = &new_col.next;
                    *prev_existing_ptr = existing_index;
                }
            } else {
                if (cf.yh >= cb.yh) {
                    // new column obscures the bottom of existing column cb, so clip existing column
                    cb.yh = cf.yl - 1;
                    // ... and keep going with the same new column with the next existing column
                    prev_existing_ptr = &existing_col.next;
                    existing_index = *prev_existing_ptr;
                } else {
                    // new column splits existing in two (it is in the middle). we can clip the top half of the existing column,
                    // then insert the new column (which is also in the right place) and then add a new bottom part for the existing one

                    // bottom part
                    int16_t extra_index = alloc_pd_column(x);
                    if (extra_index >= 0) {
                        render_cols[extra_index] = cb;
                        render_cols[extra_index].yl = cf.yh + 1;

                        // top part is just clipped
                        cb.yh = cf.yl - 1;

                        // so
                        //   prev->existing->xxx with N1->N2
                        // becomes
                        //   prev->existing(clipped)->N1->e2->xxx with N2
                        //
                        // and we continue with e2 and N2

                        render_cols[extra_index].next = existing_col.next;
                        existing_col.next = new_index;
                        new_index = new_col.next;
                        new_col.next = extra_index;

                        if (new_index < 0) break; // loop condition (only checked when we insert our new column)
                        prev_existing_ptr = &new_col.next;
                        existing_index = extra_index;
                    } else {
                        // no more room, so just clip the existing column to be just the top part

                        cb.yh = cf.yl - 1;

                        // we can move onto tne next new item since it finished during the existing item and we're
                        // moving onto the next item below
                        new_index = new_col.next;
                        new_col.next = extra_index;

                        if (new_index < 0) break; // loop condition (only checked when we insert our new column)

                        // ... and keep going with the same new column with the next existing column
                        prev_existing_ptr = &existing_col.next;
                        existing_index = *prev_existing_ptr;
                    }
                }
            }
        } else {
            // new column is behind existing column
            const auto &cf = existing_col;
            auto &cb = new_col;
            if (cf.yl <= cb.yl) {
                if (cf.yh >= cb.yh) {
                    // new column fully obscured by existing column, so free and move on
                    int16_t to_free_index = new_index;
                    new_index = new_col.next;
                    free_pd_column(to_free_index);
                    if (new_index < 0) break; // loop condition
                } else {
                    // existing column obscures the top of new column, so clip our new column
                    cb.yl = cf.yh + 1;
                    assert(cb.yl <= cb.yh);
                    // ... and keep going with the same new column with the next existing column
                    prev_existing_ptr = &existing_col.next;
                    existing_index = *prev_existing_ptr;
                }
            } else {
                if (cf.yh >= cb.yh) {
                    // existing column obscures the bottom of new column, so clip
                    cb.yh = cf.yl - 1;
                    // and then insert what is left (it is in the right place)
                    *prev_existing_ptr = new_index;
                    new_index = new_col.next;
                    new_col.next = existing_index;
                    if (new_index < 0) break; // loop condition (only checked when we insert our new column)
                    prev_existing_ptr = &new_col.next;
                    *prev_existing_ptr = existing_index;
                } else {
                    // existing column splits new column in two (it is in the middle)
                    // we clip the new column for the top half (and insert it as it is in the right place),
                    // and make a second new column for the bottom half.

                    // bottom part
                    int16_t extra_index = alloc_pd_column(x);
                    if (extra_index >= 0) {
                        render_cols[extra_index] = cb;
                        render_cols[extra_index].yl = cf.yh + 1;

                        // top part
                        cb.yh = cf.yl - 1;

                        // so
                        //   prev->existing->xxx with N1->N2
                        // becomes
                        //   prev->N1->existing->xxx with N1b->N2
                        //
                        // and we continue with xxx and N1b
                        *prev_existing_ptr = new_index;
                        render_cols[extra_index].next = new_col.next;
                        new_col.next = existing_index;
                        new_index = extra_index;

                        prev_existing_ptr = &existing_col.next;
                        existing_index = *prev_existing_ptr;
                    } else {
                        // pretend existing column obscures the bottom of new column, so clip
                        cb.yh = cf.yl - 1;
                        // and then insert what is left (it is in the right place)
                        *prev_existing_ptr = new_index;
                        new_index = new_col.next;
                        new_col.next = existing_index;
                        if (new_index < 0) break; // loop condition (only checked when we insert our new column)
                        prev_existing_ptr = &new_col.next;
                        *prev_existing_ptr = existing_index;
                    }
                }
            }
        }
    } while (true);
//    dump_column(x, "after");
}

// new_index can be a (non overlapping) linked list (in ascending y order)
// for fuzzy columns we're just clipping the new columns against the existing stuff
static void push_down_x_fuzzy(int x, int16_t new_index) {
    int16_t existing_index = column_heads[x];
//    dump_column(x, "before");
//    dump_column(x+SCREENWIDTH, "before fuzz");
//    if (x == 478 - SCREENWIDTH && new_index == 246) {
//        printf("VANAR\n");
//    }
//    dump_column_list(x+SCREENWIDTH, "Want to insert", new_index);
    //    if (pd_frame == 176 && x == 174) {
    //        printf("FUZZ %d\n", column_is_fuzzy(render_cols[new_index]));
    //        printf("SPSP\n");
    //    }
    do {
        pd_column &new_col = render_cols[new_index];
        assert(!column_is_nil(new_col));

        // both lists (new, existing) are y sorted and non overlapping, so skip past anything existing
        // that is above the new columns
        while (existing_index >= 0 && new_col.yl > render_cols[existing_index].yh) {
            existing_index = render_cols[existing_index].next;
        }
        if (existing_index < 0) {
            // remaining new list can be prepended
            break;
        }

        pd_column &existing_col = render_cols[existing_index];
        assert(!column_is_nil(existing_col));

        if (new_col.yh < existing_col.yl) {
            // fuzzy column is before the existing col, so just insert it
            int16_t tmp = new_index;
            new_index = new_col.next;
            new_col.next = fuzzy_column_heads[x];
            fuzzy_column_heads[x] = tmp;
            if (new_index < 0) break;
            continue;
        }
        // there is some overlap
        assert(existing_col.yl <= new_col.yh);
        if (new_col.scale < existing_col.scale) {
            // fuzzy column is in front of existing column
            const auto &cf = new_col;
            auto &cb = existing_col;

            if (cf.yl <= cb.yl) {
                if (cf.yh >= cb.yh) {
                    // fuzzy column fully obscures existing column, so just move onto next existing one which may also overlap
                    existing_index = existing_col.next;
                } else {
                    // fuzzy column obscures the top of existing column, so the fuzzy column cannot be obscured, so insert it then move on
                    int16_t tmp = new_index;
                    new_index = new_col.next;
                    new_col.next = fuzzy_column_heads[x];
                    fuzzy_column_heads[x] = tmp;
                    if (new_index < 0) break; // loop condition
                }
            } else {
                if (cf.yh >= cb.yh) {
                    // fuzzy column obscures the bottom of an existing column, so move on with the next existing column
                    existing_index = existing_col.next;
                } else {
                    // fuzzy column splits existing in two (it is in the middle), so we can insert the fuzzy column which
                    // cannot be obscured

                    // new column obscures the top of existing column, so insert the new column which is not obscured, and move onto the next existing column
                    int16_t tmp = new_index;
                    new_index = new_col.next;
                    new_col.next = fuzzy_column_heads[x];
                    fuzzy_column_heads[x] = tmp;
                    if (new_index < 0) break; // loop condition
                }
            }
        } else {
            // fuzzy column is behind existing column
            const auto &cf = existing_col;
            auto &cb = new_col;
            if (cf.yl <= cb.yl) {
                if (cf.yh >= cb.yh) {
                    // fuzzy column fully obscured by existing column, so free and move on
                    int16_t to_free_index = new_index;
                    new_index = new_col.next;
                    free_pd_column(to_free_index);
                    if (new_index < 0) break; // loop condition
                } else {
                    // existing column obscures the top of fuzzy column, so clip our fuzzy column
                    cb.yl = cf.yh + 1;
                    assert(cb.yl <= cb.yh);
                    // ... and keep going with the same fuzzy column with the next existing column
                    existing_index = existing_col.next;
                }
            } else {
                if (cf.yh >= cb.yh) {
                    // existing column obscures the bottom of fuzzy column, so clip fuzzy column
                    cb.yh = cf.yl - 1;
                    // and then insert what is left (it is not obscured)
                    int16_t tmp = new_index;
                    new_index = new_col.next;
                    new_col.next = fuzzy_column_heads[x];
                    fuzzy_column_heads[x] = tmp;
                    if (new_index < 0) break; // loop condition (only checked when we insert our new column)
                } else {
                    // existing column splits new fuzzy in two (it is in the middle)
                    // we clip the fuzzy column for the top half (and insert it as it is now not obscured),
                    // and make a second new column for the bottom half.

                    // bottom part
                    int16_t extra_index = alloc_pd_column(x);
                    if (extra_index >= 0) {
                        render_cols[extra_index] = cb;
                        render_cols[extra_index].yl = cf.yh + 1;

                        // top part
                        cb.yh = cf.yl - 1;

                        render_cols[extra_index].next = new_col.next;
                        new_col.next = fuzzy_column_heads[x];
                        fuzzy_column_heads[x] = new_index;
                        new_index = extra_index;

                        existing_index = existing_col.next;
                    } else {
                        // pretend existing column obscures the bottom of new column, so clip
                        cb.yh = cf.yl - 1;
                        // and then insert what is left (it is not obscured)
                        int16_t tmp = new_index;
                        new_index = new_col.next;
                        new_col.next = fuzzy_column_heads[x];
                        fuzzy_column_heads[x] = tmp;
                        if (new_index < 0) break; // loop condition (only checked when we insert our new column)
                    }
                }
            }
        }
    } while (true);
    if (new_index >= 0) {
        int tmp = new_index;
        while (render_cols[tmp].next >= 0) tmp = render_cols[tmp].next;
        render_cols[tmp].next = fuzzy_column_heads[x];
        fuzzy_column_heads[x] = new_index;
    }
//    dump_column(x+SCREENWIDTH, "after");
}

static void push_down_x(int x, int new_index) {
#if DUMP_SORTING
    //    if (x == 196 && render_cols[new_index].yl == 94 && render_cols[new_index].yh == 95) {
    //        printf("Claraa\n");
    //    }
    if (0 && !x) {
        dump_column(x, "Before insert");
        dump_column_list(x, "Want to insert", new_index);
        if (x >= 145 && render_cols[new_index].next != -1) {
            printf("VLARN\n");
        }
    }
#endif
#if PICO_ON_DEVICE
    //    gpio_put(22, 1);
#endif
    push_down_x_guts(x, new_index);
#if PICO_ON_DEVICE
    //    gpio_put(22, 0);
#endif

#if DUMP_SORTING
    if (0 && !x) {
        dump_column(x, "After insert");
        printf("\n");
    }
#endif
}

void pd_begin_frame() {
    DEBUG_PINS_SET(start_end, 1);
    if (gamestate == GS_LEVEL) {
//        render_frame_index ^= 1;
    }
    render_frame_buffer = nullptr;
#if 0 && !PICO_ON_DEVICE
    printf("BEGIN FRAME %d rfb %p\n", render_frame_index, render_frame_buffer);
#endif
    sem_release(&core1_wake);

    reset_framedrawables();
#if !PICO_ON_DEVICE
    textures.clear();
    patches.clear();
#endif
#if DUMP_SORTING
    if (0) printf("BEGIN FRAME\n");
#endif
    // new
    memset(column_heads, -1, sizeof(column_heads));
    memset(visplane_bit, 0, sizeof(visplane_bit)); // todo could do this with dma
    for(uint i=0;i<count_of(not_fully_covered_cols);i++) not_fully_covered_cols[i] = 0; // only 3 of these so loop
    not_fully_covered_yl = 0;
    not_fully_covered_yh = MAIN_VIEWHEIGHT - 1;
    render_col_count = 0;
    render_col_free = -1;
    pd_frame++;
    DEBUG_PINS_CLR(start_end, 1);
}

static void interp_init() {
#if USE_INTERP
    interp_config c = interp_default_config();
//        interp_config_set_add_raw(&c, true);
//        interp_config_set_shift(&c, 16);
//        interp_config_set_mask(&c, 0, 6);
//        interp_set_config(col_interp, 0, &c);

    c = interp_default_config();
    interp_config_set_add_raw(&c, true);
    interp_config_set_shift(&c, 4);
    interp_config_set_mask(&c, 6, 11);
    interp_set_config(span_interp, 0, &c);

    c = interp_default_config();
    interp_config_set_cross_input(&c, true);
    interp_config_set_shift(&c, 26);
    interp_config_set_mask(&c, 0, 5);
    interp_set_config(span_interp, 1, &c);
#endif
}

void pd_init() {
    sem_init(&core1_wake, 0, 1);
    sem_init(&core0_done, 0, 1);
    sem_init(&core1_done, 0, 1);
#if PICO_ON_DEVICE
    static_assert(sizeof(vpatchlists_t) < 0xc00, "");
    vpatchlists = (vpatchlists_t *)(USBCTRL_DPRAM_BASE + 0x400);
#else
    vpatchlists = (vpatchlists_t*)malloc(sizeof(vpatchlists_t));
#endif
    vpatchlists->framebuffer->header.max = count_of(vpatchlists->framebuffer);
    vpatchlists->overlays[0]->header.max = vpatchlists->overlays[1]->header.max = count_of(vpatchlists->overlays[0]);
#if USE_CORE1_FOR_FLATS
    sem_init(&core1_do_flats, 0, 1);
#endif
#if USE_CORE1_FOR_REGULAR
    sem_init(&core1_do_regular, 0, 1);
#endif
    memset(patch_hash_offsets, -1, sizeof(patch_hash_offsets));
    patch_decoder_circular_buf_write_limit = PATCH_DECODER_CIRCULAR_BUFFER_SIZE;
}

void pd_add_span() {
}

#define FORCE_ISCALE 1

void pd_add_column(pd_column_type type) {
    // --- VALIDATION AND CLAMPING
    int count = dc_yh - dc_yl;
    if (count < 0)
        return;

    fixed_t iscale;
#if FORCE_ISCALE
    if (true || type != PDCOL_SKY) {
        iscale = hw_divider_u32_quotient_inlined(0xffffffff, pd_scale);
    } else {
        iscale = 0x7fffffff;
    }
#else
    iscale = dc_iscale;
#endif
    // todo record these
    if (iscale < DDA_MIN) {
        I_Error("dc_iscale %d\n", iscale);
    }
    if (iscale > DDA_MAX) {
        iscale = DDA_MAX;
    }
    fixed_t texturemid = dc_texturemid;
    if (texturemid < MINI) texturemid = MINI;
    if (texturemid > MAXI) texturemid = MAXI;
    // --------

    // todo it would be nice to defer filling this in in case we are entirely clipped - let's get some stats on that
    int rc_index = alloc_pd_column(dc_x);
    if (rc_index < 0) return;
    render_cols[rc_index].yl = dc_yl;
    render_cols[rc_index].yh = dc_yh;
    assert(render_cols[rc_index].yl >= 0 && render_cols[rc_index].yl < MAIN_VIEWHEIGHT && render_cols[rc_index].yh >= 0 && render_cols[rc_index].yh < MAIN_VIEWHEIGHT);
    assert(!(pd_flag & 2)); // don't think this can happen
    render_cols[rc_index].scale = pd_flag & 2 ? 0 : iscale;
    render_cols[rc_index].colormap_index = dc_colormap_index;
    render_cols[rc_index].next = -1;
    render_cols[rc_index].texturemid = DOWN_SHIFT(texturemid);
#ifndef NDEBUG
    #warning surpising off
    // todo i wonder if we can fix this with a s small displacement? gather min/max
#if 0
    if (dc_iscale > 0x7fffff || dc_iscale < -0x800000) {
        printf("pd_column fracs surprising iscale %08x\n", dc_iscale);
    }
    if (dc_texturemid > 0x1ffffff) {
        printf("pd_column fracs surprising texturemid %08x\n", dc_texturemid);
    }
#endif
#endif

    render_cols[rc_index].fd_num = dc_source.fd_num;
    render_cols[rc_index].col_hi = TO_COL_HI(dc_source.col);
    render_cols[rc_index].col_lo = TO_COL_LO(dc_source.col);
#if !PICO_ON_DEVICE
    if (dc_source.real_id < 0) {
        // patch
        patches.insert(-dc_source.real_id);
    } else {
        assert(dc_source.real_id < numtextures);
        textures.insert(dc_source.real_id);
    }
#endif
    push_down_x(dc_x, rc_index);

#if SHOW_COLUMN_STATS
    fixed_t frac;
    fixed_t fracstep;

    auto &p = unique_columns[dc_source];
    column_count++;
    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows?
    //    dest = ylookup[dc_yl] + columnofs[dc_x];

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale;
    fixed_t frac0 = frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do {
#if SHOW_COSTLY_DATA_STATS
        unique_column_source_pixels.insert(&dc_source[(frac >> FRACBITS) & 127]);
#endif
        frac += fracstep;
    } while (count--);

    if (frac < frac0) {
        assert(false);
        std::swap(frac, frac0);
    }

    if (p.first == 0 && p.second == 0) {
        p.first = frac0;
        p.second = frac;
    } else {
        p.first = std::min(p.first, frac0);
        p.second = std::max(p.second, frac);
    }
#endif
}

void pd_add_masked_columns(uint8_t *ys, int seg_count) {
    // --- VALIDATION AND CLAMPING
    fixed_t iscale;
#if FORCE_ISCALE
    iscale = hw_divider_u32_quotient_inlined(0xffffffff, pd_scale);
#else
    iscale = dc_iscale;
#endif
    // todo record these
    if (iscale < DDA_MIN) {
        I_Error("dc_iscale %d\n", iscale);
    }
    if (iscale > DDA_MAX) {
        iscale = DDA_MAX;
    }
    // --------

    // todo it would be nice to defer filling this in in case we are entirely clipped - let's get some stats on that
    int rc_index = alloc_pd_column(dc_x);
    if (rc_index < 0) return;
    render_cols[rc_index].yl = ys[0];
    render_cols[rc_index].yh = ys[1];
    assert(render_cols[rc_index].yl >= 0 && render_cols[rc_index].yl < MAIN_VIEWHEIGHT && render_cols[rc_index].yh >= 0 && render_cols[rc_index].yh < MAIN_VIEWHEIGHT);

    assert(ys[1] >= ys[0]);
    render_cols[rc_index].scale = pd_flag & 2 ? 0 : iscale;
    render_cols[rc_index].colormap_index = dc_colormap_index;
#if !FORCE_ISCALE
    if (type != PDCOL_SKY) {
        uint32_t foo = hw_divider_u32_quotient_inlined(0xffffffff, pd_scale);
        if (foo != dc_iscale && foo != dc_iscale - 1 && foo != dc_iscale + 1) {
            printf("BOO %d : %d %d\b", foo, pd_scale, dc_iscale);
        }
    } else {
        printf("FLARK\n");
    }
#endif
    //    if (dc_yl < 0 || dc_yl > SCREENHEIGHT || dc_yh < 0 || dc_yh > SCREENHEIGHT) {
    //        I_Error("pants %d %d", dc_yl, dc_yh);
    //    }
#ifndef NDEBUG
    if (dc_iscale > 0x7fffff || dc_iscale < -0x800000 || dc_texturemid > 0x1ffffff) {
        printf("pd_column fracs surprising %08x %08x\n", dc_iscale, dc_texturemid);
    }
#endif

    render_cols[rc_index].fd_num = dc_source.fd_num;
    render_cols[rc_index].col_hi = TO_COL_HI(dc_source.col);
    render_cols[rc_index].col_lo = TO_COL_LO(dc_source.col);
#if !PICO_ON_DEVICE
    if (dc_source.real_id < 0) {
        // patch
        patches.insert(-dc_source.real_id);
    } else {
        assert(dc_source.real_id < numtextures);
        textures.insert(dc_source.real_id);
    }
#endif
    fixed_t texturemid = dc_texturemid - (ys[2] << FRACBITS);
    if (texturemid < MINI) texturemid = MINI;
    if (texturemid > MAXI) texturemid = MAXI;
    render_cols[rc_index].texturemid = DOWN_SHIFT(texturemid);
    int first_index = rc_index;
    for (int i = 1; i < seg_count; i++) {
        int new_rc_index = alloc_pd_column(dc_x);
        if (new_rc_index < 0) break;
        render_cols[new_rc_index] = render_cols[rc_index];
        render_cols[rc_index].next = new_rc_index;
        rc_index = new_rc_index;
        assert(ys[i * 3 + 1] >= ys[i * 3]);
        render_cols[rc_index].yl = ys[i * 3];
        render_cols[rc_index].yh = ys[i * 3 + 1];
        assert(render_cols[rc_index].yl >= 0 && render_cols[rc_index].yl < MAIN_VIEWHEIGHT && render_cols[rc_index].yh >= 0 && render_cols[rc_index].yh < MAIN_VIEWHEIGHT);
        texturemid = dc_texturemid - (ys[i*3 + 2] << FRACBITS);
        if (texturemid < MINI) texturemid = MINI;
        if (texturemid > MAXI) texturemid = MAXI;
        render_cols[rc_index].texturemid = DOWN_SHIFT(texturemid);
    }
    render_cols[rc_index].next = -1;
    if (dc_colormap_index < 0) {
        push_down_x_fuzzy(dc_x, first_index);
    } else {
        push_down_x(dc_x, first_index);
    }

#if SHOW_COLUMN_STATS
    fixed_t frac;
    fixed_t fracstep;

    auto &p = unique_columns[dc_source];
    column_count++;
    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows?
    //    dest = ylookup[dc_yl] + columnofs[dc_x];

    // Determine scaling,
    //  which is the only mapping to be done.
    fracstep = dc_iscale;
    fixed_t frac0 = frac = dc_texturemid + (dc_yl - centery) * fracstep;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.
    do {
#if SHOW_COSTLY_DATA_STATS
        unique_column_source_pixels.insert(&dc_source[(frac >> FRACBITS) & 127]);
#endif
        frac += fracstep;
    } while (count--);

    if (frac < frac0) {
        assert(false);
        std::swap(frac, frac0);
    }

    if (p.first == 0 && p.second == 0) {
        p.first = frac0;
        p.second = frac;
    } else {
        p.first = std::min(p.first, frac0);
        p.second = std::max(p.second, frac);
    }
#endif
}

static int lightlevel(int visplane) {
    int light = (visplanes[visplane].lightlevel >> LIGHTSEGSHIFT) + extralight;
    if (light >= LIGHTLEVELS)
        light = LIGHTLEVELS - 1;
    if (light < 0)
        light = 0;
    return light;
}

void pd_add_plane_column(int x, int yl, int yh, fixed_t scale, int floor, int fd_num) {
    int rc_index = alloc_pd_column(x);
    if (rc_index < 0) return;
    int iscale = hw_divider_u32_quotient_inlined(0xffffffff, pd_scale);
    if (yh < yl) {
        return;
    }
    render_cols[rc_index].yl = yl;
    render_cols[rc_index].yh = yh;
    assert(render_cols[rc_index].yl >= 0 && render_cols[rc_index].yl < MAIN_VIEWHEIGHT && render_cols[rc_index].yh >= 0 && render_cols[rc_index].yh < MAIN_VIEWHEIGHT);
    render_cols[rc_index].scale = pd_flag & 2 ? 0 : iscale;
    render_cols[rc_index].colormap_index = dc_colormap_index;
    render_cols[rc_index].texturemid = TEXTUREMID_PLANE;
    render_cols[rc_index].fd_num = fd_num;
    render_cols[rc_index].next = -1;
    push_down_x(x, rc_index);
}

static void reclip_fuzz_columns() {
    // re-add the fuzzy columns so they are correctly clipped
    for (int x = 0; x < SCREENWIDTH; x++) {
        int16_t cur = fuzzy_column_heads[x];
        fuzzy_column_heads[x] = -1;
        while (cur >= 0) {
            int16_t next = render_cols[cur].next;
            render_cols[cur].next = -1;
            push_down_x_fuzzy(x, cur);
            cur = next;
        }
    }
}

static int16_t predraw_visplanes() {
    int16_t free_list = -1;
    for (int x = 0; x < SCREENWIDTH; x++) {
        uint8_t *screen_col = render_frame_buffer + x;
        uint8_t *visplane_bit_col = visplane_bit + x / 8;
        uint8_t visplane_bit_bit = 1u << (x & 7u);
        int16_t *last = &column_heads[x];
        int16_t i = *last;
        while (i >= 0) {
            auto &c = render_cols[i];
            uint8_t *p = screen_col + c.yl * SCREENWIDTH;
            uint8_t color = c.plane;
//            printf("%d: %d -> %d %02x %d\n", x, c.yl, c.yh, color, c.texturemid == TEXTUREMID_PLANE);
            if (c.texturemid == TEXTUREMID_PLANE) {
                uint8_t *vp = visplane_bit_col + c.yl * SCREENWIDTH / 8;
                for (int y = c.yl; y <= c.yh; y++) {
                    *p = color;
                    p += SCREENWIDTH;
                    *vp |= visplane_bit_bit;
                    vp += SCREENWIDTH / 8;
                }
                *last = c.next;
                // we replace c with two elements in the new free list of plane runs
                flat_run *fr = (flat_run *) (&c);
                i *= 2;
                fr[0].next = (int16_t) (i + 1);
                fr[1].next = free_list;
                free_list = i;
                i = *last;
            } else {
                last = &c.next;
                i = c.next;
            }
        }
    }
    return free_list;
}

static void re_sort_regular_columns_by_fd_num() {
    memset(fd_heads, -1, sizeof(fd_heads));
    for (int x = 0; x < SCREENWIDTH; x++) {
        int16_t i = column_heads[x];
        while (i >= 0) {
            auto &c = render_cols[i];
            assert(c.texturemid != TEXTUREMID_PLANE);
            int16_t fd_next = fd_heads[c.fd_num];
            // link is index with top bit set if x >= 256... note -1 would conflict with i == 0x7fff, x>=256
            // which we don't care about because i would never be that high
            fd_heads[c.fd_num] = (x >> 8) ? (i | 0x8000) : i;
            // loop over old list
            i = c.next;
            // replace fd_num with 8 low bits of x
            c.x = x;
            // and link remainder of old chain
            c.next = fd_next;
        }
    }
}

static void clip_columns(int yl, int yh) {
    memset(fd_heads, -1, sizeof(fd_heads));
    // we will need to clip "not fully covered" columns too
    not_fully_covered_yl = yl;
    not_fully_covered_yh = yh;
    for (int x = 0; x < SCREENWIDTH; x++) {
        int16_t *prev = &column_heads[x];
        while (*prev >= 0) {
            auto &c = render_cols[*prev];
            if (c.yh < yl || c.yl > yh) {
                *prev = c.next;
            } else {
                if (c.yl < yl) c.yl = yl;
                if (c.yh > yh) c.yh = yh;
                prev = &c.next;
            }
        }
    }
}

static int translate_picnum(int picnum) {
    if (whd_flattospecial[picnum] != 0xff) {
        picnum = whd_specialtoflat[whd_flattranslation[whd_flattospecial[picnum]]];
    }
    return picnum;
}

static uint8_t *decode_flat_to_slot(int cache_slot, int picnum) {
    uint8_t *flat_data = cached_flat0 - cache_slot * 4096;
    DEBUG_PINS_SET(flat_decode, 1);
    uint16_t *pos = flat_decoder_buf;
    uint pos_size = count_of(flat_decoder_buf);
    uint16_t *rp_decoder = flat_decoder_buf;
    const uint32_t *sourcez = (const uint32_t *) W_CacheLumpNum(firstflat + picnum, PU_STATIC);
    th_bit_input bi;
#if USE_XIPCPY
    xip_copy_start(flat_sourcez, sourcez, (W_LumpLength(firstflat + picnum) + 3) / 4);
#define wait_for_input(amount) while (xip_copy_unfinished() && (bi.cur + amount) > ((uint8_t*)flat_sourcez) + xip_copy_progress() * 4)
        th_bit_input_init(&bi, (const uint8_t *)flat_sourcez);
#else
#define wait_for_input(x) ((void)0)
    th_bit_input_init(&bi, (const uint8_t *) sourcez);
#endif
    wait_for_input(512); // guess
    if (th_bit(&bi)) {
        pos = th_read_simple_decoder(&bi, pos, pos_size, flat_decoder_tmp, count_of(flat_decoder_tmp));
    } else {
        pos = read_raw_pixels_decoder(&bi, pos, pos_size, flat_decoder_tmp, count_of(flat_decoder_tmp));
    }
    assert(pos < flat_decoder_buf + count_of(flat_decoder_buf));
    th_make_prefix_length_table(rp_decoder, flat_decoder_tmp);
    wait_for_input(100000);
    bool have_same = th_bit(&bi);
    if (!have_same) {
        uint8_t *p = flat_data;
        for (int y = 0; y < 4096; y++) {
            *p++ = th_decode_table_special(rp_decoder, flat_decoder_tmp, &bi);
        }
    } else {
        for (int x = 0; x < 64; x++) {
            uint8_t *p = &flat_data[x * 64];
            if (th_bit(&bi)) {
                uint xf = th_read_bits(&bi, bitcount8(x));
                assert(xf < (uint) x);
                uint32_t *a = (uint32_t *) p;
                uint32_t *b = a + 16 * (int) (xf - x);
                for (int i = 0; i < 16; i++) {
                    a[i] = b[i];
                }
            } else {
                for (int y = 0; y < 64; y++) {
                    *p++ = th_decode_table_special(rp_decoder, flat_decoder_tmp, &bi);
                }
            }

        }
    }
//                    printf("Pass %d, caching slot %d pic (%d)\n", pass, cache_slot, picnum);
    cached_flat_picnum[cache_slot] = picnum;
    DEBUG_PINS_CLR(flat_decode, 1);
    return flat_data;
}

static void flush_visplanes(int8_t *flatnum_next, int numvisplanes) {
//    printf("FRAME %d %d\n", pd_frame, numvisplanes);
    angle_t angle = (viewangle + x_to_viewangle(0)) >> ANGLETOFINESHIFT;
    fixed_t viewcosangle = finecosine(angle);
    fixed_t viewsinangle = finesine(angle);
#if MERGE_DISTSCALE0_INTO_VIEWCOSSINANGLE
    const fixed_t distscale0 = distscale(0); //0x00016a75; // todo i guess this is screen size based
    viewcosangle = FixedMul(distscale0, viewcosangle);
    viewsinangle = FixedMul(distscale0, viewsinangle);
#endif
    // two passes; first pass we try to reuse flats we have decoded
    int cache_slot = 0;
    for(int pass=0;pass<2;pass++) {
        for (int i = 0; i < numvisplanes; i++) {
            int picnum = translate_picnum(visplanes[i].picnum);
            bool any = false;
            // just realized our list of visplanes includes those that may have been fully clipped away, so duh.. we need
            // to skip such things (this also includes the sky flat it turns out which shows up in visplanes)
            for(int vpcheck = flatnum_first[picnum]; vpcheck != -1; vpcheck = flatnum_next[vpcheck]) {
                if (visplane_heads[vpcheck] != -1) {
                    any = true;
                    break;
                }
            }
            if (any) {
#if PICO_ON_DEVICE
                restart_song_state |= 1; // we may not restart a song during this call because it may blow the stack
                SafeUpdateSound();
                restart_song_state &= ~1;
                interp_init();
#endif
#if 0
                source = (const uint8_t *) W_CacheLumpNum(firstflat + picnum, PU_STATIC);
#else
                uint8_t *flat_data = nullptr;
                if (!pass) {
                    for(cache_slot=0;cache_slot<cached_flat_slots;cache_slot++) {
                        if (cached_flat_picnum[cache_slot] == picnum) {
//                            printf("Pass %d, using slot %d pic (%d)\n", pass, cache_slot, picnum);
                            flat_data = cached_flat0 - cache_slot * 4096;
                        }
                    }
                    if (!flat_data) continue;
                } else {
                    cache_slot++;
                    assert(cached_flat_slots);
                    if (cache_slot >= cached_flat_slots) {
                        cache_slot = 0;
                    }
                }
                if (!flat_data) {
                    flat_data = decode_flat_to_slot(cache_slot, picnum);
                }
#endif
                DEBUG_PINS_SET(render_flat, 2);
                DEBUG_PINS_SET(render_thing, 2);
                int8_t vp = flatnum_first[picnum];
                flatnum_first[picnum] = -1;
#if USE_INTERP
                span_interp->base[2] = (uintptr_t) flat_data;//0x20020000;//(uintptr_t)W_CacheLumpNum(firstflat + pl->picnum, PU_STATIC);
#endif
                do {
                    visplane_t *pl = &visplanes[vp];

                    int last_y = -1;
                    fixed_t rel_height = abs(pl->height - viewz);
                    int startmap = ((LIGHTLEVELS - 1 - lightlevel(vp)) * 2) * NUMCOLORMAPS / LIGHTLEVELS;
//                    int last_x_end = 0;

                    const lighttable_t *colormap;
                    fixed_t xstep;
                    fixed_t ystep;
                    fixed_t xfrac;
                    fixed_t yfrac;
#if !USE_INTERP
                    uint32_t step;
                    uint32_t position;
#endif
                    for (int16_t fr = visplane_heads[vp]; fr != -1; fr = flat_runs[fr].next) {
                        int delta;
                        if (flat_runs[fr].y != last_y) {
                            // abs rel height?
                            // todo get rid of yslope?
                            fixed_t distance = FastFixedMul(rel_height, yslope[flat_runs[fr].y]);
                            xstep = FastFixedMul(distance, basexscale);
                            ystep = FastFixedMul(distance, baseyscale);
                            // mved into viewcosangle/sinangle
#if !MERGE_DISTSCALE0_INTO_VIEWCOSSINANGLE
                            const fixed_t distscale0 = 0x00016a75; // todo i guess this is screen size based
                            fixed_t length = FastFixedMul(distance, distscale0);
                            xfrac = viewx + FastFixedMul(viewcosangle, length);
                            yfrac = -viewy - FastFixedMul(viewsinangle, length);
#else
                            xfrac = viewx + FastFixedMul(viewcosangle, distance);
                            yfrac = -viewy - FastFixedMul(viewsinangle, distance);
#endif
                            int8_t colormap_index;
                            if (fixedcolormap) {
                                colormap_index = fixedcolormap;
                            } else {
                                unsigned index = distance >> LIGHTZSHIFT;
#if !NO_USE_ZLIGHT
                                if (index >= MAXLIGHTZ)
                                    index = MAXLIGHTZ - 1;
                                const int8_t *planezlight = &grs.zlight[pl->lightlevel * MAXLIGHTZ];
                                colormap_index = planezlight[index];
#else
                                // NOTE: we assume we have no IRQs on this core using the divider
                                fixed_t scale = hw_divider_s32_quotient_inlined((SCREENWIDTH / 4), (index + 1));
                                //fixed_t scale = (SCREENWIDTH / 4) / (index + 1);
                                int level = startmap - scale;

                                if (level < 0)
                                    level = 0;

                                if (level >= NUMCOLORMAPS)
                                    level = NUMCOLORMAPS - 1;
                                colormap_index = level;
#endif
                            }
                            colormap = xcolormaps + colormap_index * 256;
                            delta = flat_runs[fr].x_start;
                            last_y = flat_runs[fr].y;
                        } else {
                            // we are in reverse x order
                            delta = flat_runs[fr].x_start;// - last_x_end;
                        }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

#if USE_INTERP
                        uint32_t position = ((yfrac << 10) & 0xffff0000)
                                            | ((xfrac >> 6) & 0x0000ffff);
                        uint32_t step = ((ystep << 10) & 0xffff0000)
                                        | ((xstep >> 6) & 0x0000ffff);
                        span_interp->accum[0] = position;
                        span_interp->base[0] = step;
#else
                        position = ((yfrac << 10) & 0xffff0000)
                                | ((xfrac >> 6) & 0x0000ffff);
                        step = ((ystep << 10) & 0xffff0000)
                                | ((xstep >> 6) & 0x0000ffff);
#endif
#pragma GCC diagnostic pop
#if USE_INTERP
                        span_interp->add_raw[0] = delta * span_interp->base[0];
#else
                        position += delta * step;
#endif
                        //            printf("partial\n");
                        uint8_t *p = render_frame_buffer + flat_runs[fr].y * SCREENWIDTH + flat_runs[fr].x_start;
                        uint8_t *p_end = p + flat_runs[fr].x_end - flat_runs[fr].x_start;
                        while (p < p_end) {
#if USE_INTERP
                            const uint8_t *texel = (const uint8_t *) span_interp->pop[2];
#else
                            // Calculate current texture index in u,v.
                            uint32_t xtemp = (position >> 4) & 0x0fc0;
                            uint32_t ytemp = (position >> 26);
                            uint32_t spot = xtemp | ytemp;
                            position += step;
                            const uint8_t *texel = &flat_data[spot];
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
                            *p++ = colormap[*texel];
#pragma GCC diagnostic pop
                        }
//                        last_x_end = flat_runs[fr].x_end;
                    }
                    vp = flatnum_next[vp];
                } while (vp != -1);
                DEBUG_PINS_CLR(render_thing, 2);
                DEBUG_PINS_CLR(render_flat, 2);
            }
        }
    }
}

static void draw_visplanes(int16_t fr_list) {
    if (!lastvisplane) return;
    int numvisplanes = lastvisplane - visplanes;

    memset(visplane_heads, -1, numvisplanes * sizeof(visplane_heads[0]));
    memset(flatnum_first, -1, sizeof(flatnum_first));
    memset(flatnum_next, -1,  numvisplanes * sizeof(flatnum_next[0]));
    for (int i = 0; i < numvisplanes; i++) {
        static_assert(sizeof(visplanes[i].picnum) == 1, "");
        uint8_t picnum = translate_picnum(visplanes[i].picnum);
        flatnum_next[i] = flatnum_first[picnum];
        flatnum_first[picnum] = (int8_t) i;
    }
#if USE_INTERP
    // - visplanes -
// todo only necessary on audio core
    interp_config c = interp_default_config();
    interp_config_set_add_raw(&c, true);
    interp_config_set_shift(&c, 4);
    interp_config_set_mask(&c, 6, 11);
    interp_set_config(span_interp, 0, &c);

    c = interp_default_config();
    interp_config_set_cross_input(&c, true);
    interp_config_set_shift(&c, 26);
    interp_config_set_mask(&c, 0, 5);
    interp_set_config(span_interp, 1, &c);
#endif

    // first sort by visplane
    int16_t fr_pos = fr_list;
    for (int y = 0; y < viewheight; y++) {
        uint8_t *vp = &visplane_bit[y * (SCREENWIDTH / 8)];
        uint8_t *vp_end = vp + SCREENWIDTH / 8;
        uint bit = 0;
        uint8_t *p0 = &render_frame_buffer[y * SCREENWIDTH];
        uint8_t *p = p0;
        while (vp != vp_end) {
//            printf("y %d, x %d + %d = %02x\n", y, (int)(vp- &visplane_bit[y * (SCREENWIDTH / 8)]), bit, *vp);
            assert(bit < 8);
            assert(0 == (*vp & ((1u << (bit)) - 1)));
            if (!*vp) {
                vp++;
                p += 8 - bit;
                bit = 0;
                continue;
            }
            // new span starts within the 8 pixels
            while (!(*vp & (1u << bit))) bit++, p++; // todo ctz table?
//            printf("skipped ot y %d, x %d + %d plane=%d\n", y, (int)(vp- &visplane_bit[y * (SCREENWIDTH / 8)]), bit, *p);
            assert(bit < 8);
            assert(*p != 255);
            uint x_start = p - p0;
            uint8_t plane_num = *p;
//            if (plane_num >= numvisplanes) {
//                printf("WAHAHF x=%d y=%d %d %d *vp %02x bit %d %p\n", x_start, y, plane_num, numvisplanes, *vp, bit, p);
//            }
            assert(plane_num < numvisplanes);
            while (bit < 8 && *p == plane_num && *vp & (1u << bit)) {
                p++;
                bit++;
            }
            if (bit == 8) {
                vp++;
                bit = 0;
                if (*vp == 255 && *p == plane_num) {
                    uint fourx = plane_num * 0x1010101;
                    assert(!(((uintptr_t) p) & 0x3));
                    while (vp < vp_end && *vp == 255 && ((uint32_t *) p)[0] == fourx && ((uint32_t *) p)[1] == fourx) {
                        p += 8;
                        vp++;
                    }
                }
                if (vp < vp_end) {
                    while (bit < 8 && *p == plane_num && *vp & (1u << bit)) {
                        p++;
                        bit++;
                    }
                    assert(bit != 8); // should have been handled above by 8x case
                    *vp &= ~((1u << bit) - 1);
                }
            } else {
                *vp &= ~((1u << bit) - 1);
            }
            if (fr_pos == -1) {
                flush_visplanes(flatnum_next, numvisplanes);
                memset(visplane_heads, -1, MAXVISPLANES * 2);
                fr_pos = fr_list;
                break;
            }
            auto &fr = flat_runs[fr_pos];
            fr.x_start = x_start;
            fr.x_end = p - p0;
            fr.y = y;
            int16_t tmp = fr.next;
            fr.next = visplane_heads[plane_num];
            visplane_heads[plane_num] = fr_pos;
            fr_pos = tmp;
        }
    }
    if (fr_pos != fr_list) {
        flush_visplanes(flatnum_next, numvisplanes);
    }
}

static inline void col_render(uint8_t *dest, uint count, const uint8_t *source, fixed_t frac, fixed_t fracstep, const lighttable_t* colormap) {
#if 0 && PICO_ON_DEVICE
    col_interp->accum[0] = xs->col.frac;
    col_interp->base[0] = xs->col.fracstep;
    col_interp->base[2] = (uintptr_t)xs->col.source;

    static_assert(BAND_HEIGHT == 8, ""); // needs code fixup
    count = __fast_mul((BAND_HEIGHT - 1 - count), 14); // 12 is size of FULL_COL_PIXEL + 2 for add
    uint32_t tmp;
    __asm__ volatile (
    ".syntax unified\n"

    "add pc, %[r_count]\n"
    "nop\n" // because adding 0 to pc above will jump 4 bytes, and this way we save an extra instruction in control flow for count -= 2

    FULL_COL_PIXEL
    "add  %[r_dest], %[r_deststep]\n"
    FULL_COL_PIXEL
    "add  %[r_dest], %[r_deststep]\n"
    FULL_COL_PIXEL
    "add  %[r_dest], %[r_deststep]\n"
    FULL_COL_PIXEL
    "add  %[r_dest], %[r_deststep]\n"
    FULL_COL_PIXEL
    "add  %[r_dest], %[r_deststep]\n"
    FULL_COL_PIXEL
    "add  %[r_dest], %[r_deststep]\n"
    FULL_COL_PIXEL
    "add  %[r_dest], %[r_deststep]\n"

    : [ r_dest] "+l" (dest),
    [ r_tmp] "=&l" (tmp)
    : [ r_interp] "l" (col_interp),
    [ r_colormap] "l" (colormap),
    [ r_palette] "l" (palette),
    [ POP_FULL_OFFSET] "n" (SIO_INTERP0_POP_FULL_OFFSET - SIO_INTERP0_ACCUM0_OFFSET),
    [ r_deststep ]"r" (SCREENWIDTH * 2),
    [r_count] "r" (count)
    :
    );
    xs->col.frac = col_interp->accum[0];
#else
    do {
        *dest = colormap[source[(frac >> FRACBITS) & 127]];
        dest += SCREENWIDTH;
        frac += fracstep;
    } while (count--);
#endif
}

struct patch_hash_entry_header {
    uint16_t patch_num;
    int16_t next;
    uint16_t size:15;
    uint16_t encoding:1;
};

struct patch_decode_info {
    const patch_t *patch;
    const uint16_t *col_offsets;
    const uint16_t *decoder;
    uint data_index; // offset in patch of start of data (col_offsets relative to this)
    patch_hash_entry_header header;
    uint16_t w;
};

#define PATCH_HASH_ENTRY_HEADER_HWORDS 3
static_assert(sizeof(struct patch_hash_entry_header)==2 * PATCH_HASH_ENTRY_HEADER_HWORDS, "");
static inline int patch_hash(int patch_num) {
    // todo better hash
    return patch_num & (PATCH_DECODER_HASH_SIZE - 1);
}

// returns positive for existing slot, inverted for where to put
static inline int patch_offset_or_inverse_slot(int patch_num) {
    int slot = patch_hash(patch_num);
    int offset = patch_hash_offsets[slot];
    while (offset != -1) {
        patch_hash_entry_header *header = (patch_hash_entry_header *)(patch_decoder_circular_buf + offset);
        assert(slot == patch_hash(header->patch_num));
        if (header->patch_num == patch_num) return offset;
        offset = header->next;
    }
    return ~slot;
}

static void get_patch_decoder(int patch_num, patch_decode_info* pdis, int pdi_pos = 0, int pdi_count = 1) {
    auto& pdi = pdis[pdi_pos];
    pdi.patch = (patch_t *) W_CacheLumpNum(patch_num, PU_CACHE);
    bool simple_path = get_core_num();
    int offset_or_inverse_slot = patch_offset_or_inverse_slot(patch_num);
    uint data_index = 3 + patch_has_extra(pdi.patch);
    if (!simple_path && offset_or_inverse_slot >= 0) {
        data_index += ((uint8_t *) pdi.patch)[data_index * 2]; // skip over decoder metadata
        patch_hash_entry_header *header = (patch_hash_entry_header *)(patch_decoder_circular_buf + offset_or_inverse_slot);
        assert(header->patch_num == patch_num);
        pdi.decoder = patch_decoder_circular_buf + offset_or_inverse_slot + PATCH_HASH_ENTRY_HEADER_HWORDS;
        pdi.header = *header;
#if DECODER_DECODER_BUFFERS
        printf("found patch=%d at offset %d\n", patch_num, offset_or_inverse_slot);
#endif
    } else {
        DEBUG_PINS_SET(patch_decode, 1);
        int space_needed = patch_decoder_size_needed(pdi.patch) + PATCH_HASH_ENTRY_HEADER_HWORDS;
#if DEBUG_DECODER_BUFFERS
        printf("Need slot of size %d\n", space_needed);
#endif
        uint16_t *pos;
        patch_hash_entry_header *header;
        if (simple_path) {
            pos = flat_decoder_buf;
            header = (patch_hash_entry_header*)pos;
        } else {
            while (patch_decoder_circular_buf_write_pos >=
                   patch_decoder_circular_buf_write_limit - space_needed) {
                if (patch_decoder_circular_buf_write_limit == PATCH_DECODER_CIRCULAR_BUFFER_SIZE) {
                    // we have wrapped
                    patch_decoder_circular_buf_write_pos = patch_decoder_circular_buf_write_limit = 0;
                } else {
                    // we need to advance
                    patch_hash_entry_header *header = (patch_hash_entry_header *) (patch_decoder_circular_buf +
                                                                                   patch_decoder_circular_buf_write_limit);
                    if (header->patch_num) {
                        // free the patch we now encounter
#if DEBUG_DECODER_BUFFERS
                        printf("Freeing slot (%d) at %08x->%08x\n", header->patch_num, patch_decoder_circular_buf_write_limit, patch_decoder_circular_buf_write_limit + header->size);
#endif
                        uint slot = patch_hash(header->patch_num);
                        int16_t *last = &patch_hash_offsets[slot];
                        while (*last != -1 && *last != patch_decoder_circular_buf_write_limit) {
                            last = &((patch_hash_entry_header *) (patch_decoder_circular_buf + *last))->next;
                        }
                        assert(*last != -1);
                        *last = ((patch_hash_entry_header *) (patch_decoder_circular_buf + *last))->next;
                        for (int p = 0; p < pdi_count; p++) {
                            if (pdis[p].header.patch_num == header->patch_num) {
                                pdis[p].decoder = nullptr;
                            }
                        }
                        patch_decoder_circular_buf_write_limit += header->size;
                    } else {
                        // we've reached the end of what was there
                        patch_decoder_circular_buf_write_limit = PATCH_DECODER_CIRCULAR_BUFFER_SIZE;
                    }
                }
            }
            pos = patch_decoder_circular_buf + patch_decoder_circular_buf_write_pos;
            uint slot = ~offset_or_inverse_slot;
#if DEBUG_DECODER_BUFFERS
            printf("Allocate slot %d (%d) at %08x limit %08x\n", slot, patch_num, patch_decoder_circular_buf_write_pos, patch_decoder_circular_buf_write_limit);
#endif
            uint slot_offset = patch_decoder_circular_buf_write_pos;
            header = (patch_hash_entry_header *)(patch_decoder_circular_buf + slot_offset);
            assert( slot >=0 && slot < PATCH_DECODER_HASH_SIZE);
            header->next = patch_hash_offsets[slot];
            patch_hash_offsets[slot] = slot_offset;
        }
        header->patch_num = patch_num;
        pos += PATCH_HASH_ENTRY_HEADER_HWORDS;
        pdi.decoder = pos;
        const uint8_t *sourcez = pdi.patch + data_index * 2 + 1;
        data_index += ((uint8_t *) pdi.patch)[data_index * 2]; // skip over decoder metadata
        th_bit_input bi;
        th_bit_input_init(&bi, (const uint8_t *) sourcez);
        header->encoding = th_read_bits(&bi, 1);
        // todo make this correct for each core
#define effective_decoder_buf_size sizeof(patch_decoder_tmp)
        static_assert(effective_decoder_buf_size <= count_of(patch_decoder_tmp), "");
        //static_assert(effective_decoder_buf_size <= count_of(flat_decoder_tmp), "");
        uint8_t *decoder_tmp = simple_path ? flat_decoder_tmp : patch_decoder_tmp;
        int decoder_tmp_use_estimate; // this is a very ugly hack, but saves us polluting the decoder stuff since none of the other users care
        switch (header->encoding) {
            case 0:
                if (th_bit(&bi)) {
                    pos = th_read_simple_decoder(&bi, pos, space_needed, decoder_tmp, effective_decoder_buf_size);
                } else {
                    pos = read_raw_pixels_decoder(&bi, pos, space_needed, decoder_tmp, effective_decoder_buf_size);
                }
                // pos-pdi.decoder_size (X) is equal to max_code_length * 2 + (count + 1) / 2 where count is elements used in decoder_tmp
                // (count + 1) /2 < X
                // count < 2X
                // use_estimate = 2*count (as each element is 2 bytes)
                // use_estimate < X
                decoder_tmp_use_estimate = pos - pdi.decoder;
                break;
            case 1:
                pos = read_raw_pixels_decoder_c3(&bi, pos, space_needed, decoder_tmp, effective_decoder_buf_size);
                // pos-pdi.decoder_size (X) is equal to max_code_length * 2 + count where count is elements used in decoder_tmp
                // count < X
                // use_estimate = 3*count (as each element is 3 bytes)
                // use_estimate < 3X
                decoder_tmp_use_estimate = (pos - pdi.decoder) * 3;
                break;
            default:
                assert(false);
        }
        assert(pos - pdi.decoder <= space_needed  - PATCH_HASH_ENTRY_HEADER_HWORDS);
        assert(pos <= patch_decoder_circular_buf + patch_decoder_circular_buf_write_limit);
        if (!simple_path) {
            // we may have trashed some of our decoder tmp tables
            patch_decoder_tmp_table_patch_numbers[0] = 0;
            if (decoder_tmp_use_estimate > 256) {
                patch_decoder_tmp_table_patch_numbers[1] = 0;
                if (decoder_tmp_use_estimate > 512) {
                    patch_decoder_tmp_table_patch_numbers[2] = 0;
                    if (decoder_tmp_use_estimate > 768) {
                        patch_decoder_tmp_table_patch_numbers[3] = 0;
                    }
                }
            }
            assert(pos <= patch_decoder_circular_buf + PATCH_DECODER_CIRCULAR_BUFFER_SIZE - 1);
            header->size = pos + PATCH_HASH_ENTRY_HEADER_HWORDS - pdi.decoder;
#if !PICO_ON_DEVICE
            patch_decoder_size.record(header->size);
#endif
            patch_decoder_circular_buf_write_pos += header->size;
            patch_decoder_circular_buf[patch_decoder_circular_buf_write_pos] = 0; // we need a zero patch number to follow
        }
        pdi.header = *header;
        DEBUG_PINS_CLR(patch_decode, 1);
    }
    pdi.col_offsets = &((uint16_t*)pdi.patch)[data_index];
    uint w = patch_width(pdi.patch);
    assert(w > 0 && w <= 320);
    pdi.data_index = (data_index + w) * 2 + 2; // + 2 because we have one extra col_data offset
    pdi.w = (uint16_t) w;
}

const uint8_t *get_patch_decoder_table(uint patch_num, const uint16_t *decoder) {
    if (get_core_num()) {
        th_make_prefix_length_table(decoder,
                                    flat_decoder_tmp); // the table is large and quick to generate, so we don't cache
        return flat_decoder_tmp;
    } else {
        for (int i = 0; i < WHD_MAX_COL_UNIQUE_PATCHES; i++) {
            if (patch_num == patch_decoder_tmp_table_patch_numbers[i]) return patch_decoder_tmp + i * 256;
        }
        th_make_prefix_length_table(decoder,
                                    patch_decoder_tmp); // the table is large and quick to generate, so we don't cache
        patch_decoder_tmp_table_patch_numbers[0] = patch_num;
        return patch_decoder_tmp;
    }
}

const uint8_t *get_patch_decoder_table(uint patch_num, const uint16_t *decoder, int pos) {
    if (patch_num == patch_decoder_tmp_table_patch_numbers[pos]) return patch_decoder_tmp + pos * 256;
#if DEBUG_DECODER
    printf("Get decoder %d table pos %d\n", patch_num, pos);
#endif
    th_make_prefix_length_table(decoder, patch_decoder_tmp + pos * 256); // the table is large and quick to generate, so we don't cache
    patch_decoder_tmp_table_patch_numbers[pos] = patch_num;
    return patch_decoder_tmp + pos * 256;
}

static void draw_patch_columns(int patch_num, int patch_head, int16_t *col_heads, uint8_t *col_height, int translated) {
    // fix up the sky scale (we had to preserve the original scale for column clipping/sorting)
    //  note: we do this as a rare edge case here, rather than checking in loops
    if (patch_num == skytexture_patch) {
        for(int j=patch_head; j != -1;) {
            auto &c = render_cols[j & 0x7fffu];
            c.scale = 0x10000;
            j = c.next;
        }
    }
    patch_decode_info pdi;
    get_patch_decoder(patch_num, &pdi);
    assert(pdi.w <= WHD_PATCH_MAX_WIDTH);
    // moved these to the caller because not enough stack on core 1, core 0 can use a fixed 256 size since it seems to have enough stack (it isn't calling audio from here)
//    int16_t col_heads[pdi.w];
//    uint8_t col_height[pdi.w];
    memset(col_heads, -1, pdi.w * 2);
    int i = patch_head;
    assert(i != -1);
    int h = patch_height(pdi.patch);
    do {
        auto &c = render_cols[i & 0x7fffu];
        uint col = FROM_COL_HI_LO(c.col_hi, c.col_lo);
        assert(col < pdi.w);
        int16_t tmp = col_heads[col];
        col_heads[col] = (int16_t)i;
        i = c.next;
        c.next = tmp;

        // we actually don't know how many pixels are in the column without looking at the run data, however
        // we can see the last pixel we've used, which means, as a bonus, we won't decode all of a column when we only
        // use the top
        fixed_t texturemid = UP_SHIFT(c.texturemid);
        fixed_t fracstep = DDA_UP_SHIFT(c.scale);
        if (!fracstep) fracstep = 0x10000;
        fixed_t start = (texturemid + (c.yl - centery) * fracstep) >> FRACBITS;
        fixed_t end = (texturemid + (c.yh - centery) * fracstep) >> FRACBITS;
        if (start < -1) {
//            printf("Awooga\n");
            end = 128; // todo gate on texture only (it should not happen otherwise)
        }
//        printf("tm %08x fracstep %08x yl %d yh %d centery %d flan %08x end %d\n", texturemid, fracstep, c.yl, c.yh, centery, (texturemid + (c.yh - centery) * fracstep), end);
        if (end < 0) end = 0;
//        assert(end >= 0 && end < 256);
        if (end >= h) end = h-1;
        if (tmp == -1 || end > col_height[col]) col_height[col] = (uint8_t)end;
    } while (i != -1);

    const uint16_t *col_offsets = pdi.col_offsets;
    const uint8_t *patch_decoder_table = get_patch_decoder_table(patch_num, pdi.decoder);
    for(int col = 0; col < pdi.w; col++) {
        i = col_heads[col];
        if (!(col & 63) && get_core_num()) {
            restart_song_state |= 1; // we may not restart a song during this call because it may blow the stack
            SafeUpdateSound();
            restart_song_state &= ~1;
        }
        if (i != -1) {
            uint16_t col_offset = col_offsets[col];
            if (0xff == (col_offset >> 8)) {
                assert((col_offset&0xff)<pdi.w);
                col_offset = col_offsets[col_offset & 0xff];
            }
            uint8_t pixels[257];
            th_bit_input bi;
            if (patch_byte_addressed(pdi.patch)) {
                th_bit_input_init(&bi, pdi.patch + pdi.data_index + col_offset); // todo read off end potential
            } else {
                th_bit_input_init_bit_offset(&bi, pdi.patch + pdi.data_index, col_offset); // todo read off end potential
            }
            if (!pdi.header.encoding) {
                for (int j = 0; j <= col_height[col]; j++) {
                    pixels[j] = th_decode_table_special(pdi.decoder, patch_decoder_table, &bi);
                }
            } else {
                for (int j = 0; j <= col_height[col]; j++) {
//                    uint16_t p = th_decode_16(rp_decoder, &bi);
                    uint16_t p = th_decode_table_special_16(pdi.decoder, patch_decoder_table, &bi);
                    if (p < 256) {
                        pixels[j] = p;
                    } else {
                        int prev = j - 1;
                        assert(prev>=0);
                        assert(1 == p >> 8);
                        p &= 0xff;
                        assert(p<7);
                        pixels[j] = pixels[prev] + p - 3;
                    }
                }
            }
#if USE_PICO_NET
            // bit of a waste of time mostly. would be cheaper to change the decoder tables, but then again
            // that is a lot of dealing with polluting caches, so unless this causes marked slowdown, go with this
            if (translated) {
                int base = 0x80 - translated * 0x20;
                for (int j = 0; j <= col_height[col]; j++) {
                    if ((pixels[j] >> 4) == 7) {
                        pixels[j] = base + (pixels[j]&0xf);
                    }
                }
            }
#endif
            if (h < 127) {
                pixels[127] = pixels[0];
                pixels[h] = pixels[h-1];
            }

            if (fixedcolormap || patch_num == skytexture_patch) {
#if NO_USE_DC_COLORMAP
                should_be_const lighttable_t *dc_colormap = colormaps + 256 * (patch_num == 1203 ? 0 : fixedcolormap);
#endif
                do {
                    const auto &c = render_cols[i & 0x7fffu];
                    uint8_t *p = render_frame_buffer + __mul_instruction(c.yl, SCREENWIDTH) + c.x +
                                 ((i & 0x8000u) >> 7u);
                    assert (c.texturemid != TEXTUREMID_PLANE);
                    fixed_t fracstep = DDA_UP_SHIFT(c.scale);
                    if (!fracstep) fracstep = 0x10000;
                    fixed_t frac = UP_SHIFT(c.texturemid) + (c.yl - centery) * fracstep;
                    col_render(p, c.yh - c.yl, pixels, frac, fracstep, dc_colormap);
                    i = c.next;
                } while (i != -1);
            } else {
                do {
                    const auto &c = render_cols[i & 0x7fffu];
                    uint8_t *p = render_frame_buffer + __mul_instruction(c.yl, SCREENWIDTH) + c.x +
                                 ((i & 0x8000u) >> 7u);
#if NO_USE_DC_COLORMAP
                    should_be_const lighttable_t *dc_colormap = colormaps + 256 * c.colormap_index;
#endif
                    assert (c.texturemid != TEXTUREMID_PLANE);
                    fixed_t fracstep = DDA_UP_SHIFT(c.scale);
                    if (!fracstep) fracstep = 0x10000;
                    fixed_t frac = UP_SHIFT(c.texturemid) + (c.yl - centery) * fracstep;
//                    if (get_core_num()) {
//                        for(int yy=c.yl;yy<=c.yh;yy++) {
//                            p[0] = 0xfc;
//                            p += SCREENWIDTH;
//                        }
//                    } else {
//                        for(int yy=c.yl;yy<=c.yh;yy++) {
//                            p[0] = 0x5;
//                            p += SCREENWIDTH;
//                        }
//                    }
                    col_render(p, c.yh - c.yl, pixels, frac, fracstep, dc_colormap);
                    i = c.next;
                } while (i != -1);
            }
        }
    }
}

static void draw_composite_columns(int texture_num, int tex_head) {
    uint w = texture_width(texture_num);
    int16_t col_heads[w];
    memset(col_heads, -1, sizeof(col_heads));
    int i = tex_head;
    assert(i != -1);
    // todo not sure this is beneficial
    int min = 128;
    int max = 0;
//    printf("DCC tex=%d\n", texture_num);
    do {
        auto &c = render_cols[i & 0x7fffu];
        uint col = FROM_COL_HI_LO(c.col_hi, c.col_lo);
        assert(col < w);
        int16_t tmp = col_heads[col];
        col_heads[col] = (int16_t)i;
        i = c.next;
        c.next = tmp;

        // we actaully don't know how many pixels are in the column without looking at the run data, however
        // we can see the last pixel we've used, which means, as a bonus, we won't decode all of a column when we only
        // use the top
        fixed_t texturemid = UP_SHIFT(c.texturemid);
        fixed_t fracstep = DDA_UP_SHIFT(c.scale);
        if (!fracstep) fracstep = 0x10000;
        fixed_t start = (texturemid + (c.yl - centery) * fracstep) >> FRACBITS;
        fixed_t end = (texturemid + (c.yh - centery) * fracstep) >> FRACBITS;
        if (start < -1 || end > 128) {
            min = 0;
            max = 128;
        } else {
            min = std::min(min, start);
            max = std::max(max, end);
        }
    } while (i != -1);

#if DEBUG_COMPOSITE
    printf("Texture %d h=%d, %d->%d\n", texture_num, texture_height(texture_num)>>FRACBITS, min, max);
#endif
    int pc = whd_textures[texture_num].patch_count;
    assert(pc);
    uint8_t *patch_table = &((uint8_t *)whd_textures)[whd_textures[texture_num].metdata_offset];
    uint8_t *metadata = patch_table + pc * 2;
    // skip over the non composite column metadata (always pretty small)
    uint xx=0;
    while (xx < w) {
        uint b = *metadata++;
        xx += (b&0x7f)+1;
        if (b&0x80) metadata+=2;
    }
    patch_decode_info pdis[WHD_MAX_COL_UNIQUE_PATCHES];
    for(uint p=0;p<count_of(pdis);p++) {
        pdis[p].header.patch_num = 0;
    }
    for(uint base = 0; base < w;) {
        // we have
        // 0 - number of cols - 1
        // 1 - array of patches {
        //      [0] local patch_num
        //      [1] (128 for last) | (height - 1)
        //      [2] originx
        //      [3] originy
        // }
        uint limit = base + *metadata++ + 1;
        assert(limit <= w);
        uint col;
        // if the first localpatch is 0xff, this is a run of single patches (which
        // don't come to this function)
        if (metadata[0] != 0xff) {
#if DEBUG_COMPOSITE
            printf("composites, limit %d\n", limit);
#endif
            // quick check for no columns in this range
            for(col = base; col < limit; col++) {
                if (col_heads[col] != -1) break;
            }
    #if DEBUG_DECODER
            printf("Texture %d base cols %d->%d skip %d\n", texture_num, base, limit, col == limit);
    #endif
            if (col != limit) {
                struct {
                    uint8_t y;
                    uint8_t count;
                    uint8_t pdi_index; // 0xff for memcpy
                    uint8_t col;
                    uint8_t src_offset; // for memcpy this is the source
                } runs[WHD_MAX_COL_SEGS];
                int run_count = 0;
                int y = 0;
                uint used_this_time = 0;
                do {
                    int local_patch = metadata[0];
                    int m1 = metadata[1];
                    if (local_patch & WHD_COL_SEG_EXPLICIT_Y) {
                        y = metadata[2];
                        metadata++;
                    }
                    int length = 1 + (m1 & 0x7f);
                    if (local_patch & WHD_COL_SEG_MEMCPY) {
                        // note y < max, because we only copy from above, and indeed having y > max in non local_patch & 0x80 causes us to not know how many pixels to draw
                        if (y <= max && (y + length > min || (local_patch & WHD_COL_SEG_MEMCPY_SOURCE))) { // 0x20 means used for memcpy
                            assert(run_count < WHD_MAX_COL_SEGS);
                            runs[run_count].pdi_index = local_patch & WHD_COL_SEG_MEMCPY_IS_BACKWARDS ? 0xff : 0xfe;
                            // todo we could clip more, but barely seems worth it
                            runs[run_count].y = y;
                            runs[run_count].src_offset = metadata[2];
                            runs[run_count].count = std::min(1 + max - y, length);
#if DEBUG_COMPOSITE
                            printf("  memcpy %d <- %d + %d\n", y, runs[run_count].src_offset, length);
#endif
                            run_count++;
                        }
                        metadata += 3;
                    } else {
                        // note y < max, because we only copy from above, and indeed having y > max in non local_patch & 0x80 causes us to not know how many pixels to draw
                        if (y <= max && (y + length > min || (local_patch & 0x20))) { // 0x20 means used for memcpy
                            // todo bitfields here
                            // 0qzy xxxx include y start    : <length> <xoff> (if y <y>) (if z <yoff>)
                            //    q means needed for memcpy
                            // 1yyy yyyy memcpy             : <length> (y is start y)
                            // 1111 1111 single columns (ignore) _ eqqivalent to memcpy from 0x7f which would be silly
                            // todo local_patch_mapping? probably unnecessary as we don't often have the same patch in a column
                            local_patch &= 0xf;
                            int patch_num = patch_table[local_patch * 2] | (patch_table[local_patch * 2 + 1] << 8);
    #if DEBUG_DECODER
                            printf("  looking for local patch %d (%d) offset %d,%d\n", local_patch, patch_num, (int8_t)metadata[2], (int8_t)metadata[3]);
    #endif
                            uint p;
                            for (p = 0; p < count_of(pdis); p++) {
                                if (pdis[p].header.patch_num == patch_num) {
                                    break;
                                }
                            }
                            if (p == count_of(pdis)) {
                                assert(used_this_time < 15); // simply for this crappy table
                                static const int lookup[15] = {
                                        3, 3, 3, 3, 3, 3, 3, 3,
                                        2, 2, 2, 2, 1, 1, 0
                                };
                                p = lookup[used_this_time];
    #if DEBUG_DECODER
                                printf("    not found, inserting at index %d\n", p);
    #endif
                                get_patch_decoder(patch_num, pdis, p, count_of(pdis));
                            } else {
    #if DEBUG_DECODER
                                printf("    found at index %d\n", p);
    #endif
                            }
                            used_this_time |= (1u << p);
                            assert(run_count <= WHD_MAX_COL_SEGS);
                            runs[run_count].pdi_index = p;
                            // todo we could clip more, but barely seems worth it
                            runs[run_count].y = y;
                            runs[run_count].col = metadata[2] - base;
                            runs[run_count].src_offset = metadata[3];
                            runs[run_count].count = std::min(1 + max - y, length);
#if DEBUG_COMPOSITE
                            printf("  patch %d, %d + %d\n", patch_num, y, length);
#endif
                            run_count++;
                        }
                        metadata += 4;
                    }
                    y += length;
                    if (m1 >= 128) break;
                } while (true);
                const uint8_t *decoder_tables[count_of(pdis)];
                for (uint p = 0; p < count_of(pdis); p++) {
                    if (used_this_time & (1u << p)) {
                        // unfortunate state... one of our (cached) decoders got deleted during the loop allocating new ones
                        // note we do this test here rather than in a separate loop above as it is rare, and it saves us another loop in the common case
                        if (!pdis[p].decoder) {
                            get_patch_decoder(pdis[p].header.patch_num, pdis, p, count_of(pdis));
                            p = -1; continue; // start loop over (we may have freed something from the previous iteration
                            // note we assume that we can always actually fit the decoders for all count_of(pdis) (4) patches so this loop will terminate
                        }
                        decoder_tables[p] = get_patch_decoder_table(pdis[p].header.patch_num, pdis[p].decoder, p);
                    }
                }
    #if 1
                for (col = base; col < limit; col++) {
                    i = col_heads[col];
                    if (i != -1) {
    #if 1
                        uint8_t pixels[129];
                        for(int r=0;r<run_count;r++) {
                            const auto &run = runs[r];
                            assert(run.y + run.count <= 128);
                            if (run.pdi_index >= 0xfe) {
                                if (run.pdi_index == 0xfe) {
                                    for (int yy = 0; yy < run.count; yy++) {
                                        pixels[run.y + yy] = pixels[run.src_offset + yy];
                                    }
//                                    memset(pixels+run.y, 0xfc, run.count);
                                } else {
                                    for (int yy = run.count-1; yy >= 0; yy--) {
                                        pixels[run.y + yy] = pixels[run.src_offset + yy];
                                    }
//                                    memset(pixels+run.y, 0xfe, run.count);
                                }
                            } else {
                                const auto &pdi = pdis[run.pdi_index];
                                const uint16_t *col_offsets = pdi.col_offsets;
                                const uint8_t *patch_decoder_table = decoder_tables[run.pdi_index];
                                uint8_t pcol = col + run.col;
                                assert(pcol < patch_width(pdi.patch));
                                uint16_t col_offset = col_offsets[pcol];
                                if (0xff == (col_offset >> 8)) {
                                    assert((col_offset & 0xff) < w);
                                    col_offset = col_offsets[col_offset & 0xff];
                                }
                                th_bit_input bi;
                                if (patch_byte_addressed(pdi.patch)) {
                                    th_bit_input_init(&bi, pdi.patch + pdi.data_index +
                                                           col_offset); // todo read off end potential
                                } else {
                                    th_bit_input_init_bit_offset(&bi, pdi.patch + pdi.data_index,
                                                                 col_offset); // todo read off end potential
                                }
                                uint8_t *lp = pixels + run.y;
                                if (!pdi.header.encoding) {
                                    for (int j = 0; j < run.src_offset; j++) {
                                        th_decode_table_special(pdi.decoder, patch_decoder_table, &bi);
                                    }
                                    for (int j = 0; j < run.count; j++) {
                                        lp[j] = th_decode_table_special(pdi.decoder, patch_decoder_table, &bi);
                                    }
                                } else {
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
                                    uint8_t prev_pixel;
    #pragma GCC diagnostic pop
                                    for (int j = 0; j < run.src_offset; j++) {
                                        //                    uint16_t p = th_decode_16(rp_decoder, &bi);
                                        uint16_t p = th_decode_table_special_16(pdi.decoder, patch_decoder_table, &bi);
                                        if (p < 256) {
                                            prev_pixel = p;
                                        } else {
                                            p &= 0xff;
                                            assert(p < 7);
                                            prev_pixel = prev_pixel + p - 3;
                                        }
                                    }
                                    for (int j = 0; j < run.count; j++) {
                                        //                    uint16_t p = th_decode_16(rp_decoder, &bi);
                                        uint16_t p = th_decode_table_special_16(pdi.decoder, patch_decoder_table, &bi);
                                        if (p < 256) {
                                            lp[j] = p;
                                        } else {
                                            p &= 0xff;
                                            assert(p < 7);
                                            lp[j] = prev_pixel + p - 3;
                                        }
                                        prev_pixel = lp[j];
                                    }
                                }
                            }
                        }
                        uint hh = texture_height(texture_num) >> FRACBITS;
                        if (hh != 128) {
                            pixels[127] = pixels[0];
                            pixels[hh] = pixels[hh - 1];
                        }
                        if (fixedcolormap) {
    #if NO_USE_DC_COLORMAP
                            should_be_const lighttable_t *dc_colormap = colormaps + 256 * fixedcolormap;
    #endif
                            do {
                                const auto &c = render_cols[i & 0x7fffu];
                                uint8_t *p = render_frame_buffer + __mul_instruction(c.yl, SCREENWIDTH) + c.x +
                                             ((i & 0x8000u) >> 7u);
                                assert (c.texturemid != TEXTUREMID_PLANE);
                                fixed_t fracstep = DDA_UP_SHIFT(c.scale);
                                if (!fracstep) fracstep = 0x10000;
                                fixed_t frac = UP_SHIFT(c.texturemid) + (c.yl - centery) * fracstep;
                                col_render(p, c.yh - c.yl, pixels, frac, fracstep, dc_colormap);
                                i = c.next;
                            } while (i != -1);
                        } else {
                            do {
                                const auto &c = render_cols[i & 0x7fffu];
                                uint8_t *p = render_frame_buffer + __mul_instruction(c.yl, SCREENWIDTH) + c.x +
                                             ((i & 0x8000u) >> 7u);
    #if NO_USE_DC_COLORMAP
                                should_be_const lighttable_t *dc_colormap = colormaps + 256 * c.colormap_index;
    #endif
                                assert (c.texturemid != TEXTUREMID_PLANE);
                                fixed_t fracstep = DDA_UP_SHIFT(c.scale);
                                if (!fracstep) fracstep = 0x10000;
                                fixed_t frac = UP_SHIFT(c.texturemid) + (c.yl - centery) * fracstep;
                                col_render(p, c.yh - c.yl, pixels, frac, fracstep, dc_colormap);
                                i = c.next;
                            } while (i != -1);
                        }
    #else
                        uint8_t color = texture_num & 0xff;
                    do {
                        const auto &c = render_cols[i & 0x7fffu];
                        uint8_t *p =
                                render_frame_buffer + __mul_instruction(c.yl, SCREENWIDTH) + c.x + ((i & 0x8000u) >> 7u);
                        assert (c.texturemid != TEXTUREMID_PLANE);
    #if NO_USE_DC_COLORMAP
                        should_be_const lighttable_t *dc_colormap = colormaps + 256 * c.colormap_index;
    #endif
                        uint8_t lcolor = dc_colormap[color];
    //                    fixed_t texturemid = UP_SHIFT(c.texturemid);
    //                    fixed_t fracstep = DDA_UP_SHIFT(c.scale);
    //                    fixed_t start = (texturemid + (c.yl - centery) * fracstep) >> FRACBITS;
    //                    fixed_t end = (texturemid + (c.yh - centery) * fracstep) >> FRACBITS;
    //                    if (end > 128) lcolor = 0xfc;
                        for (int y = 0; y <= c.yh - c.yl; y++) {
                            *p = lcolor;
                            p += SCREENWIDTH;
                        }
                        i = c.next;
                    } while (i != -1);

    #endif

                    }
                }
    #endif
            } else {
#if DEBUG_COMPOSITE
                printf("  no cols though\n");
#endif
                int last;
                do {
                    last = metadata[1] & 0x80;
                    bool has_y = metadata[0] & WHD_COL_SEG_EXPLICIT_Y;
                    if (metadata[0] & 0x80) {
                        metadata += 3 + has_y;
                    } else {
                        metadata += 4 + has_y;
                    }
                } while (!last);
            }
        } else {
#if DEBUG_COMPOSITE
            printf("single patch, limit %d\n", limit);
#endif
            metadata += 2;
        }
        base = limit;
    }
}

// noinline as it uses alloca
static void __noinline draw_regular_columns(int core) {
    if (!core) {
        // on core 0 draw the textures first
        for(int fd_num=0; fd_num < num_framedrawables; fd_num++) {
            int i = fd_heads[fd_num];
            if (i != -1 && framedrawables[fd_num].real_id > 0) {
                DEBUG_PINS_SET(render_thing, 1<<core);
                draw_composite_columns(framedrawables[fd_num].real_id, i);
                DEBUG_PINS_CLR(render_thing, 1<<core);
            }
        }
    }
    spin_lock_t *lock = spin_lock_instance(RENDER_SPIN_LOCK);
    uint8_t *buffer;
    if (core) {
        static_assert(sizeof(visplane_bit) >= WHD_PATCH_MAX_WIDTH, "");
        // visplane_bit is no longer used on core 1 as we've already drawn
        buffer = visplane_bit;
    } else {
        // on core 0 we can use the stack
        buffer = (uint8_t *)__builtin_alloca(WHD_PATCH_MAX_WIDTH * 3);
    }
    for(int fd_num=0; fd_num < num_framedrawables; fd_num++) {
        int i = fd_heads[fd_num];
        if (i != -1) {
            uint32_t save = spin_lock_blocking(lock);
            int id = framedrawables[fd_num].real_id;
            if (id < 0) {
                framedrawables[fd_num].real_id = 0; // mark as done
            }
            spin_unlock(lock, save);
            if (id < 0) {
                DEBUG_PINS_SET(render_thing, 1<<core);
                int translated = 0;
                if (fd_num == translated_fds[0]) {
                    translated = 1;
                } else if (fd_num == translated_fds[1]) {
                    translated = 2;
                } else if (fd_num == translated_fds[2]) {
                    translated = 3;
                }
                draw_patch_columns(-id, i, (int16_t*)buffer, buffer + WHD_PATCH_MAX_WIDTH * 2, translated);
                DEBUG_PINS_CLR(render_thing, 1<<core);
            }
        }
    }
}

int8_t fuzzpos;

static void draw_fuzz_columns() {
    for (int x = 0; x < SCREENWIDTH; x++) {
        const lighttable_t *darken_map = xcolormaps + 256 * 6;
        int16_t i = fuzzy_column_heads[x];
        while (i >= 0) {
            const auto &c = render_cols[i];
            uint8_t *screen_col = render_frame_buffer + x;
            int yl = c.yl;
            int yh = c.yh;
            assert(yl <= MAIN_VIEWHEIGHT);
            assert(yh <= MAIN_VIEWHEIGHT);
            if (yl == 0) yl = 1;
            if (yh >= MAIN_VIEWHEIGHT - 1) yh = MAIN_VIEWHEIGHT - 2;

            if (yl <= yh) {
                uint8_t *p = screen_col + yl * SCREENWIDTH;
                for (int y = 0; y <= yh - yl; y++) {
                    *p = darken_map[p[fuzzoffset[fuzzpos]]];

                    // Clamp table lookup index.
                    if (++fuzzpos == FUZZTABLE)
                        fuzzpos = 0;
                    p += SCREENWIDTH;
                }
            }
            i = c.next;
        }
    }
}

static void draw_splash(int patch_num, int top, int bottom, uint8_t *dest, int single_col = -1) {
    patch_decode_info pdi;
    get_patch_decoder(patch_num, &pdi);
    int w = patch_width(pdi.patch);
    int h = patch_height(pdi.patch);
    assert(w == 320 && h == 200);
    const uint16_t *col_offsets = pdi.col_offsets;
    const uint8_t *patch_decoder_table = get_patch_decoder_table(patch_num, pdi.decoder);
    uint32_t delta = (bottom - top) * SCREENWIDTH - 1;
    int col;
    int stride;
    if (single_col < 0) {
        col = 0;
        stride = SCREENWIDTH;
    } else {
        // hack to draw only one column
        col = single_col;
        w = col + 1;
        stride = 1;
    }
    for (; col < w; col++) {
        uint16_t col_offset = col_offsets[col];
        if (0xff == (col_offset >> 8)) {
            assert((col_offset & 0xff) < pdi.w);
            col_offset = col_offsets[col_offset & 0xff];
        }
        th_bit_input bi;
        if (patch_byte_addressed(pdi.patch)) {
            th_bit_input_init(&bi, pdi.patch + pdi.data_index + col_offset); // todo read off end potential
        } else {
            th_bit_input_init_bit_offset(&bi, pdi.patch + pdi.data_index, col_offset); // todo read off end potential
        }
        if (!pdi.header.encoding) {
            assert(false); // we don't have this
//            for (int j = 0; j <= h; j++) {
//                pixels[j] = th_decode_table_special(pdi.decoder, patch_decoder_table, &bi);
//            }
        } else {
            int y;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
            uint8_t prev_pixel;
#pragma GCC diagnostic pop
            for (y = 0; y < top; y++) {
                uint16_t p = th_decode_table_special_16(pdi.decoder, patch_decoder_table, &bi);
                if (p < 256) {
                    prev_pixel = p;
                } else {
                    p &= 0xff;
                    assert(p < 7);
                    prev_pixel = prev_pixel + p - 3;
                }
            }
            for (; y < bottom; y++) {
                uint16_t p = th_decode_table_special_16(pdi.decoder, patch_decoder_table, &bi);
                if (p < 256) {
                    *dest = p;
                } else {
                    p &= 0xff;
                    assert(p < 7);
                    *dest = prev_pixel + p - 3;
                }
                prev_pixel = *dest;
                dest += stride;
            }
            dest -= delta;
        }
    }
}

extern "C" int M_Random();

void maybe_draw_single_screen(int patch_num) {
    if (sub_gamestate == 0) {
        next_video_type = VIDEO_TYPE_SINGLE;
        draw_splash(patch_num, 0, MAIN_VIEWHEIGHT, frame_buffer[render_frame_index]);
        sub_gamestate = 1;
    } else if (sub_gamestate == 1) {
        draw_splash(patch_num, MAIN_VIEWHEIGHT, SCREENHEIGHT,
                    frame_buffer[render_frame_index^1] + (MAIN_VIEWHEIGHT - 32) * SCREENWIDTH);
        sub_gamestate = 2;
    }
}

void draw_stbar_on_framebuffer(int frame, boolean refresh) {
    V_BeginPatchList(vpatchlists->framebuffer);
    // we call ST_drawwidgets directly as we don't want to mess with palette stuff (we call this during startup when not initialized)
//    ST_Drawer(false, refresh);
    ST_drawWidgets(refresh);
    // draw the status bar onto the bottom (now non visible part of the top buffer)
    I_VideoBuffer = frame_buffer[frame] - 32 * SCREENWIDTH;
    V_RestoreBuffer();
    V_DrawPatchList(vpatchlists->framebuffer);
    I_VideoBuffer = render_frame_buffer;
}

static void draw_framebuffer_patches_fullscreen() {
    V_RestoreBuffer();
    vpatch_clip_bottom = MAIN_VIEWHEIGHT;
    V_DrawPatchList(vpatchlists->framebuffer);
    I_VideoBuffer = frame_buffer[render_frame_index^1] - 32 * SCREENWIDTH;
    V_RestoreBuffer();
    vpatch_clip_top = SCREENHEIGHT-32;
    vpatch_clip_bottom = SCREENHEIGHT;
    V_DrawPatchList(vpatchlists->framebuffer);
    vpatch_clip_top = 0;
    I_VideoBuffer = frame_buffer[render_frame_index];
}

void draw_fullscreen_background(int top, int bottom) {
    assert(top < bottom);
    assert((top < MAIN_VIEWHEIGHT && bottom <= MAIN_VIEWHEIGHT) ||
           (top >= MAIN_VIEWHEIGHT && bottom > MAIN_VIEWHEIGHT));
    int patch_num = 0;
    byte *top_pixel = top < MAIN_VIEWHEIGHT ? render_frame_buffer + top * SCREENWIDTH :
                        frame_buffer[render_frame_index^1] + (top - 32) * SCREENWIDTH;
    switch (gamestate) {
        case GS_INTERMISSION:
            patch_num = wi_background_patch_num;
            break;
        case GS_DEMOSCREEN:
            patch_num = W_GetNumForName(pagename);
            break;
        case GS_FINALE:
            if (finalestage == F_STAGE_TEXT) {
                int picnum = W_GetNumForName(finaleflat);
                if (picnum) {
                    int cache_slot;
                    uint8_t *flat_data = 0;
                    for (cache_slot = 0; cache_slot < cached_flat_slots; cache_slot++) {
                        if (cached_flat_picnum[cache_slot] == picnum) {
                            flat_data = cached_flat0 - cache_slot * 4096;
                        }
                    }
                    if (!flat_data) {
                        // just use slot 0
                        assert(cached_flat_slots);
                        cache_slot = 0;
                        flat_data = decode_flat_to_slot(cache_slot, picnum - firstflat); // note this uses core1's data area, but it is not drawing flats at the moment
                    }
                    // todo is this rotated 90 degress
                    for (int y = top; y < bottom; y++) {
                        uint8_t *src = flat_data + (y & 63);
                        for(int i=0;i<SCREENWIDTH;i++) {
                            *top_pixel++ = src[(i&63)*64]; // patch is x/y flipped by decode for reasons i now forget :-)
                        }
                    }
                }
            } else if (finalestage == F_STAGE_ARTSCREEN) {
                const char *ln = F_ArtScreenLumpName();
                if (!ln) {
#if !DEMO1_ONLY
                    patch_num = W_GetNumForName("PFUB1");
#endif
                } else {
                    patch_num = W_GetNumForName(ln);
                }
#if !NO_USE_FINALE_CAST
            } else if (finalestage == F_STAGE_CAST) {
                patch_num = W_GetNumForName("BOSSBACK");
#endif
            }
        default:
            break;
    }
    if (patch_num) {
        draw_splash(patch_num, top, bottom, top_pixel);
    }
    if (gamestate == GS_INTERMISSION) {
        // need to draw the initial text
        V_BeginPatchList(vpatchlists->framebuffer);
        WI_Drawer();
        if (top >= MAIN_VIEWHEIGHT) {
            I_VideoBuffer = frame_buffer[render_frame_index ^ 1] - 32 * SCREENWIDTH;
        }
        V_RestoreBuffer();
        vpatch_clip_top = top;
        vpatch_clip_bottom = bottom;
        V_DrawPatchList(vpatchlists->framebuffer);
        vpatch_clip_top = 0;
        vpatch_clip_bottom = SCREENHEIGHT;
        I_VideoBuffer = render_frame_buffer;
    }
}

static void uh_oh_discard_columns(int render_col_limit) {
//    memset(render_frame_buffer, 0xfc, SCREENWIDTH * MAIN_VIEWHEIGHT); // not quite right in clip
    for (int x = 0; x < SCREENWIDTH * 2; x++) { // >= SCREENWIDTH these are fuzzy columns
        int16_t *last = &column_heads[x];
        int16_t i = *last;
        while (i >= 0) {
            auto &c = render_cols[i];
            if (i >= render_col_limit) {
                // we have to throw it out, but we will draw something - black is as good as anything i guess
                if (x < SCREENWIDTH) {
                    uint8_t *dest = render_frame_buffer + x + c.yl * SCREENWIDTH;
                    for(int y = c.yh -c.yl; y >= 0; y--, dest += SCREENWIDTH) {
                        *dest = 0;
                    }
                }
                *last = c.next;
                i = *last;
            } else {
                last = &c.next;
                i = c.next;
            }
        }
    }
}
void pd_end_frame(int wipe_start) {
    DEBUG_PINS_SET(start_end, 2);
#if !PICO_ON_DEVICE
//    tex_count.record_print(textures.size());
//    patch_count.record_print(patches.size());
//    patch_decoder_size.print_summary();
//    patch_decoder_size.reset();
#endif
    // these were only clipped as they were inserted (so may be more obscured)
    reclip_fuzz_columns();
#if PICO_ON_DEVICE
//    gpio_put(22, 1);
    while (!sem_available(&display_frame_freed)) {
        I_UpdateSound();
    }
//    gpio_put(22, 0);
#endif
    sem_acquire_blocking(&display_frame_freed);
    bool showing_help = inhelpscreens;
    static boolean was_in_help;
    if (gamestate == GS_LEVEL) {
        if (!wipestate && (!showing_help || !was_in_help)) render_frame_index ^= 1;
    } else {
        // we expect all the rendering code to be a no-op
        assert(render_col_count == 0);
    }
    render_frame_buffer = frame_buffer[render_frame_index];
#if 0 && !PICO_ON_DEVICE
    printf("END FRAME %d %p ws %d cols %d\n", render_frame_index, render_frame_buffer, wipe_start, render_col_count);
#endif

    DEBUG_PINS_SET(full_render, 1);

    uint8_t *list_buffer_limit = list_buffer + count_of(list_buffer);
    if (!inhelpscreens) {
        if (was_in_help) {
//            // todo graham, wtf this was a complete guess - why should this be necessary, and if so why
//            //  not for splash screens
//            memset(patch_decoder_tmp_table_patch_numbers, 0, sizeof(patch_decoder_tmp_table_patch_numbers));
            next_video_type = gamestate == GS_LEVEL ? VIDEO_TYPE_DOUBLE : VIDEO_TYPE_SINGLE;
            wipestate = WIPESTATE_NONE;
            post_wipecount = 0; // this causes us to redraw intermission screens
            sub_gamestate = 0; // and splash screens
        }
        switch (wipestate) {
            case WIPESTATE_NONE: {
                if (wipe_start) {
                    if (gamestate != GS_LEVEL) {
                        render_frame_index ^= 1;
                        render_frame_buffer = frame_buffer[render_frame_index];
                        I_VideoBuffer = render_frame_buffer;
                        draw_fullscreen_background(0, MAIN_VIEWHEIGHT - 32);
                    }
                    if (next_video_type == VIDEO_TYPE_DOUBLE) {
                        // coming from level already, so draw statusbar
                        draw_stbar_on_framebuffer(render_frame_index, false); // argh it is the wrong status bar
                    }
                    clip_columns(0, MAIN_VIEWHEIGHT - 32 -
                                    1); // note this is a noop in non GS_LEVEL so don't bother to add if
                    next_video_type = VIDEO_TYPE_WIPE;
                    // steal space for our wipe data structures
                    memset(cached_flat_picnum, -1, sizeof(cached_flat_picnum));
                    wipe_yoffsets_raw = (int16_t *) (list_buffer_limit - 4096);
                    wipe_yoffsets = list_buffer_limit - 4096 + SCREENWIDTH * 2;

                    memset(wipe_yoffsets, 0, SCREENWIDTH);
                    wipe_yoffsets_raw[0] = -6;//-(M_Random() % 12);
                    for (int i = 1; i < SCREENWIDTH; i++) {
                        int r = (M_Random() % 3) - 1;
                        wipe_yoffsets_raw[i] = wipe_yoffsets_raw[i - 1] + r;
                        if (wipe_yoffsets_raw[i] > 0) wipe_yoffsets_raw[i] = 0;
                        else if (wipe_yoffsets_raw[i] == -12) wipe_yoffsets_raw[i] = -11;
                    }
                    wipe_linelookup = (uint32_t *) (wipe_yoffsets + SCREENWIDTH);
                    uint screen_front = render_frame_index ^ 1; // what was currently displayed
                    uint32_t base;
#if PICO_ON_DEVICE
                    base = (uintptr_t) &frame_buffer[0][0];
#else
                    base = 0;
#endif
                    for (int i = 0; i < SCREENHEIGHT; i++) {
                        if (i < MAIN_VIEWHEIGHT)
                            wipe_linelookup[i] = base + screen_front * SCREENWIDTH * MAIN_VIEWHEIGHT + i * SCREENWIDTH;
                        else
                            wipe_linelookup[i] =
                                    base + (screen_front ^ 1) * SCREENWIDTH * MAIN_VIEWHEIGHT + (i - 32) * SCREENWIDTH;
                    }
                    wipestate = WIPESTATE_SKIP1;
                    wipe_min = 0;
                }
                break;
            }
            case WIPESTATE_SKIP1: {
                if (wipe_min > 32) wipestate = WIPESTATE_REDRAW1;
                break;
            }
            case WIPESTATE_REDRAW1: {
                // we need to render the bottom of the screen
                clip_columns(MAIN_VIEWHEIGHT - 32,
                             MAIN_VIEWHEIGHT - 1); // note this is a noop in non GS_LEVEL so don't bother to add if
                if (gamestate != GS_LEVEL) {
                    draw_fullscreen_background(MAIN_VIEWHEIGHT - 32, MAIN_VIEWHEIGHT);
                }
                wipestate = WIPESTATE_SKIP2;
                break;
            }
            case WIPESTATE_SKIP2: {
                if (wipe_min > 64) wipestate = WIPESTATE_REDRAW2;
                break;
            }
            case WIPESTATE_REDRAW2: {
                if (gamestate == GS_LEVEL) {
                    draw_stbar_on_framebuffer(render_frame_index ^ 1, true);
                } else {
                    draw_fullscreen_background(MAIN_VIEWHEIGHT, SCREENHEIGHT);
                }
                wipestate = WIPESTATE_SKIP3;
                break;
            }
            case WIPESTATE_SKIP3: {
                // todo check we are on the right frame before exiting
                if (wipe_min >= 200) {
                    next_video_type = gamestate == GS_LEVEL ? VIDEO_TYPE_DOUBLE : VIDEO_TYPE_SINGLE;
                    wipestate = WIPESTATE_NONE;
                    post_wipecount = 0;
                }
                break;
            }
        }
    }
    I_VideoBuffer = render_frame_buffer;
    if (wipestate) list_buffer_limit -= 4096;
    // we need to use the lower limit of this frame and the last since the final wipe frame may still be using the data
    uint8_t *this_time_limit = std::min(list_buffer_limit, last_list_buffer_limit);
    if (cached_flat0 != this_time_limit - 4096) {
        // this should only happen coming in and out of wipe, so we trash all our flash slots
        cached_flat0 = this_time_limit - 4096;
        memset(cached_flat_picnum, -1, sizeof(cached_flat_picnum));
    }
//    printf("CF0 %p ll %p ttl %p overall %p\n", cached_flat0, list_buffer_limit, this_time_limit, list_buffer + sizeof(list_buffer));
    last_list_buffer_limit = list_buffer_limit;

    int new_cache_flat_slots = 1 + ((int)(cached_flat0 - list_buffer - render_col_count * sizeof(pd_column))) / 4096;
    if (new_cache_flat_slots < 1) {
        // flat 0 - list_buffer - render_col_count * 12 == 4096
        int render_col_limit = (cached_flat0 - list_buffer ) / sizeof(pd_column);
//        printf("THIS IS A PROBLEM LIMIT TO %d cols\n", render_col_limit);
        new_cache_flat_slots = 1;
        uh_oh_discard_columns(render_col_limit);
    } else if (render_col_count == RENDER_COL_MAX) {
        static int foo;
//        printf("OOPS MAXXED OUT %d\n", foo++);
    }
    for(int i=cached_flat_slots; i<new_cache_flat_slots; i++) {
        cached_flat_picnum[i] = 0xff;
    }
    cached_flat_slots = new_cache_flat_slots;

    if (showing_help) {
        // bit hacky, but does the job (we don't want to draw anything at all when fully covered
        memset(column_heads, -1, sizeof(column_heads));
    } else {
        for (uint i = 0; i < count_of(not_fully_covered_cols); i++) {
            if (not_fully_covered_cols[i]) {
                for (int j = 0; j < 32; j++) {
                    if (not_fully_covered_cols[i] & (1u << j)) {
                        uint32_t *dest = (uint32_t *) (render_frame_buffer + i * 4 * 32 + j * 4 +
                                                       not_fully_covered_yl * SCREENHEIGHT);
                        for (int y = not_fully_covered_yl; y <= not_fully_covered_yh; y++, dest += SCREENWIDTH / 4) {
                            *dest = 0;
                        }
                    }
                }
            }
        }
    }
    // render the visplane identifiers, freeing up the visplane columns (which we will use below)
    int16_t fr_list = predraw_visplanes();

    // ... now we can be parallel
#if !USE_CORE1_FOR_FLATS
    draw_visplanes(fr_list);
#else
    core1_fr_list = fr_list;
    sem_release(&core1_do_flats);
#endif
    re_sort_regular_columns_by_fd_num();
#if USE_CORE1_FOR_REGULAR
    sem_release(&core1_do_regular);
#endif
    draw_regular_columns(0);
#if !DEMO1_ONLY
    if (gamestate == GS_FINALE && finalestage == F_STAGE_CAST && !wipestate) {
        // note we do this before core0_done so core1 is still playing music
        int sprite_lump = F_CastSprite();
        draw_cast_sprite(sprite_lump);
    }
#endif
    sem_release(&core0_done);
    sem_acquire_blocking(&core1_done);
    draw_fuzz_columns();
    DEBUG_PINS_CLR(full_render, 1);
    NetUpdate();

    if (gamestate == GS_FINALE) {
        V_BeginPatchList(vpatchlists->framebuffer);
        F_Drawer();
        draw_framebuffer_patches_fullscreen();
    }
    V_BeginPatchList(vpatchlists->overlays[render_overlay_index]);
    if (!showing_help) {
        was_in_help = false;
        switch (gamestate) {
            case GS_LEVEL:
//                if (!gametic)
//                    break;
                if (!wipestate) {
                    if (automapactive)
                        AM_Drawer();
                    // goes into overlay set above
                    ST_Drawer(false, !pre_wipe_state);
                    sub_gamestate = 0;
                    next_video_type = VIDEO_TYPE_DOUBLE;
                }
                break;

            case GS_INTERMISSION: {
                static int16_t *wipe_yoffsets_raw;
                if (!wipestate && post_wipecount < 2) {
                    // todo we don't need to check wi_background_patch_num
                    if (post_wipecount == 1 && wi_background_patch_num) {
                        // at this point the text should be drawn by overlay, so we must erease it from the background
                        draw_splash(wi_background_patch_num, 0, MAIN_VIEWHEIGHT, frame_buffer[render_frame_index]);
                        draw_splash(wi_background_patch_num, MAIN_VIEWHEIGHT, SCREENHEIGHT,
                                    frame_buffer[render_frame_index ^ 1] + (MAIN_VIEWHEIGHT - 32) * SCREENWIDTH);
                    }
                    post_wipecount++;
                }
                // todo we should draw static stuff except the numbers on the background above, which also would be we might need to refresh the first time here
                if (pre_wipe_state) {
                    // to framebuffer (otherwise it goes to the overlay)
                    V_BeginPatchList(vpatchlists->framebuffer);
                }
                if (!wipestate) WI_Drawer();
                if (pre_wipe_state) {
                    draw_framebuffer_patches_fullscreen();
                }
                break;
            }
            case GS_FINALE: {
#if !DEMO1_ONLY
                if (finalestage==F_STAGE_ARTSCREEN && !F_ArtScreenLumpName() && !wipestate) {
                    static uint16_t last_scroll;
                    int scroll = SCREENWIDTH - F_BunnyScrollPos();
                    if (last_scroll > scroll) last_scroll = 0;
                    if (scroll > last_scroll) {
                        // i think using the list buffer here as scratch space is fine ... you'll get a garbage column if you start a new game half way thru the scroll!! i don't think i care!
                        next_video_scroll = list_buffer + SCREENHEIGHT * render_frame_index;
                        int patch_num = W_GetNumForName("PFUB2");
                        if (scroll != SCREENWIDTH) {
                            draw_splash(patch_num, 0, SCREENHEIGHT, next_video_scroll, SCREENWIDTH - 1 - scroll);
                        }
                        last_scroll++;
                    } else {
                        next_video_scroll = nullptr;
                    }
                    F_BunnyDrawPatches();
                } else {
                    next_video_scroll = nullptr;
                }
#endif
                break;
            }
            case GS_DEMOSCREEN: {
                //assert(num_framedrawables == 0);
                if (!wipestate) {
                    static bool warmup_done;
                    if (!warmup_done) {
                        // hack alert: we draw the status bar (to be immediately overdrawn) as a first thing so that we don't have a cold cache
                        // when we draw it in the middle of the first wipe where it causes a cache fight with the video_newhope scanline stuff
                        draw_stbar_on_framebuffer(render_frame_index, false);
                        warmup_done = true;
                    }
                    int pnum = W_GetNumForName(pagename);
                    assert(pnum);
                    maybe_draw_single_screen(pnum);
                }
                break;
            }
        }
    } else {
        static const char *last_name;
        if (!was_in_help) {
            next_video_type = VIDEO_TYPE_SINGLE;
            last_name = NULL;
            was_in_help = true;
        }
        if (gamestate == GS_LEVEL) {
            ST_doPaletteStuff();
        }
        const char *name = nullptr;
        switch (inhelpscreens & 127) {
            case 1:
                name = "HELP1";
                break;
            case 2:
                name = "HELP2";
                break;
            case 3:
                name = "HELP";
                break;
        }
        if (name) {
            if (last_name != name) {
                sub_gamestate = 0; // redraw
                last_name = name;
            }
            int pnum = W_GetNumForName(name);
            maybe_draw_single_screen(pnum);
        }
    }
//    ST_FpsDrawer(render_col_count);
//    ST_FpsDrawer(cached_flat_slots);
    ST_FpsDrawer(-1);

    // todo this might not be right
    // advance demo is set on the last frame of a demo, pre_wipe_state is set for last frame of gameplay in other state changes (by g_game)
    // inhelpscreens has skull which is in an iconvenient place
    bool render_menu_etc_to_fb = !advancedemo && !pre_wipe_state && next_video_type == VIDEO_TYPE_DOUBLE && !inhelpscreens;
    if (render_menu_etc_to_fb) {
        // render menu/hu to framebuffer (otherwise it goes to the overlay)
        V_BeginPatchList(vpatchlists->framebuffer);
    }

    if (!pre_wipe_state && !wipestate && gamestate == GS_LEVEL && gametic && !inhelpscreens) {
        HU_Drawer();
    }
#if !DEMO1_ONLY
    if (gamestate == GS_FINALE && finalestage == F_STAGE_CAST && !wipestate) {
        F_CastDrawer(); // just draw the text
    }
#endif
    M_Drawer();

    if (render_menu_etc_to_fb) {
        // render menu/hu to framebuffer
        V_RestoreBuffer();
        V_DrawPatchList(vpatchlists->framebuffer);
    }
    if (pre_wipe_state == PRE_WIPE_EXTRA_FRAME_NEEDED) {
        pre_wipe_state = PRE_WIPE_EXTRA_FRAME_DONE;
    }

    next_frame_index = render_frame_index;
    next_overlay_index = render_overlay_index;
    render_overlay_index ^= 1;
#if !DEMO1_ONLY
    if (next_video_type == VIDEO_TYPE_SINGLE && gamestate != GS_FINALE) {
        next_video_scroll = nullptr;
    }
#endif
#if 0 && !PICO_ON_DEVICE
    printf("GS %d vt %d fi %d\n", gamestate, next_video_type, next_frame_index);
#endif
    sem_release(&render_frame_ready);
    DEBUG_PINS_CLR(start_end, 2);
}

void pd_core1_loop() {
#if PICO_ON_DEVICE
    sem_acquire_blocking(&core1_wake);
#if USE_CORE1_FOR_FLATS
    while (!sem_acquire_timeout_ms(&core1_do_flats, 1)) {
        SafeUpdateSound();
    }
    interp_in_use = true;
    draw_visplanes(core1_fr_list);
    interp_in_use = false;
#if USE_CORE1_FOR_REGULAR
    while (!sem_acquire_timeout_ms(&core1_do_regular, 1)) {
        SafeUpdateSound();
    }
    draw_regular_columns(1);
#endif
#endif
    while (!sem_acquire_timeout_ms(&core0_done, 1)) {
        SafeUpdateSound();
    }
#endif
    sem_release(&core1_done);
}

#if PICO_ON_DEVICE
extern "C" {
#include "i_picosound.h"
}
static uint8_t old_video_type;
void pd_start_save_pause(void) {
    I_PicoSoundFade(false);
    while (!sem_available(&display_frame_freed) || I_PicoSoundFading()) {
        I_UpdateSound();
    }
    sem_acquire_blocking(&display_frame_freed);
    old_video_type = next_video_type;
    // this should be the case
    if (old_video_type == VIDEO_TYPE_DOUBLE) {
        draw_stbar_on_framebuffer(render_frame_index ^ 1, false);
    }
    next_video_type = VIDEO_TYPE_SAVING;
    sem_release(&render_frame_ready);
    // need to be sure we've picked up the change
    while (!sem_available(&display_frame_freed)) {
        I_UpdateSound();
    }
    sem_acquire_blocking(&display_frame_freed);
}

void pd_end_save_pause(void) {
    next_video_type = old_video_type;
    sem_release(&render_frame_ready);
    I_PicoSoundFade(true);
    while (I_PicoSoundFading()) {
        I_UpdateSound();
    }
}

#endif

#pragma GCC pop_options

void th_bit_overrun(th_bit_input *bi) {
    panic("BIT OVERRUN");
}

uint8_t *pd_get_work_area(uint32_t *size) {
    *size = last_list_buffer_limit - list_buffer;
    return list_buffer;
}

#if !DEMO1_ONLY
void draw_cast_sprite(int sprite_lump) {
    // drawing the sprites here is somewhat painful... f_finale uses V_DrawPatch but in Pico Doom we
    // vpatches are different from patches. the simplest thing for us to do (to avoid duplicating
    // decompressing code) is to pass the sprite similarly to the regular game, giving us some columns
    // to draw via draw_patch_columns
    boolean flip;
    if (sprite_lump < 0) {
        sprite_lump = -1 - sprite_lump;
        flip = true;
    } else {
        flip = false;
    }
    vissprite_t avis;
    vissprite_t *vis = &avis;
    vis->mobjflags = 0;
    // note that f_finale uses patch offsets (which we do not store), however the sprite offsets seem to be the same (or not noticeably different)
    // save for a different y offset

    // fortunately and entirely coincidentally the maximum height we need is EXACTLY 168-32 (which is how much offscreen buffer we have left)
    // 145 puts the bottom of the graphics at the bottom of that
    int ypos = (145 << FRACBITS) - sprite_topoffset(sprite_lump);
    vis->texturemid = ((SCREENHEIGHT / 2) << FRACBITS) + FRACUNIT / 2 - ypos;
    vis->x1 = SCREENWIDTH / 2 - (sprite_offset(sprite_lump) >> FRACBITS);
    vis->x2 = vis->x1 + ((sprite_width(sprite_lump) - 1) >> FRACBITS);
    vis->scale = pspritescale << detailshift;

    if (flip) {
        vis->xiscale = -pspriteiscale;
        vis->startfrac = sprite_width(sprite_lump) - 1;
    } else {
        vis->xiscale = pspriteiscale;
        vis->startfrac = 0;
    }

//    if (vis->x1 > x1)
//        vis->startfrac += vis->xiscale * (vis->x1 - x1);

    vis->patch = sprite_lump;
    vis->colormap = 0;

    pd_flag |= 2;
    R_DrawVisSprite(vis, 0, 0); // vis->x1, vis->x2); the params are ignored
    pd_flag &= ~2;

    // sort into correct lists
    uint8_t buffer[WHD_PATCH_MAX_WIDTH * 3];
    int16_t head = -1;
    const int height = MAIN_VIEWHEIGHT - 32;
    for (int x = 0; x < SCREENWIDTH; x++) {
        int16_t i = column_heads[x];
        while (i >= 0) {
            auto &c = render_cols[i];
            int16_t fd_next = head;
            // link is index with top bit set if x >= 256... note -1 would conflict with i == 0x7fff, x>=256
            // which we don't care about because i would never be that high
            head = (x >> 8) ? (i | 0x8000) : i;
            // loop over old list
            i = c.next;
            // replace fd_num with 8 low bits of x
            c.x = x;
            if (c.yh>167-32) c.yh = 167-32; // may as well clip
            // and link remainder of old chain
            c.next = fd_next;
        }
    }
    // to save having a new overlay mode, we'll render to an off screen buffer and attempt to copy this avoiding the scanline (or not - perhaps we don't really care)
    render_frame_buffer = frame_buffer[render_frame_index ^ 1];
    const int top = 41; // window top -> top + height is the window we care about
    // draw the bit of the background we need
    draw_splash(W_GetNumForName("BOSSBACK"), top, top + height, render_frame_buffer);
    // draw the stuff over the top
    draw_patch_columns(firstspritelump+sprite_lump, head, (int16_t *) buffer, buffer + WHD_PATCH_MAX_WIDTH * 2, 0);

    // now blit to the screen (could have waited for a vsync here but i don't see any flickering)
    static_assert(top + height > MAIN_VIEWHEIGHT, ""); // just to check we need to split this copy into two bits
    memcpy(frame_buffer[render_frame_index] + top * SCREENWIDTH, render_frame_buffer, (MAIN_VIEWHEIGHT - top) * SCREENWIDTH);
    // bottom bit goes on the last 32 pixels of the other buffer
    memcpy(render_frame_buffer + (MAIN_VIEWHEIGHT-32) * SCREENWIDTH, render_frame_buffer + (MAIN_VIEWHEIGHT - top) * SCREENWIDTH, (top + height - MAIN_VIEWHEIGHT) * SCREENWIDTH);
}
#endif