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
//	Here is a core component: drawing the floors and ceilings,
//	 while maintaining a per column clipping list only.
//	Moreover, the sky areas have to be determined.
//


#include <stdio.h>
#include <stdlib.h>

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"

#if NO_DRAW_SPANS
int no_draw_spans=1;
#else
int no_draw_spans;
#endif

#if PICO_DOOM
#include "picodoom.h"
#endif


planefunction_t floorfunc;
planefunction_t ceilingfunc;

//
// opening
//

#if !NO_VISPLANES
// Here comes the obnoxious "visplane".
visplane_t visplanes[MAXVISPLANES];
visplane_t *lastvisplane;
visplane_t *floorplane;
visplane_t *ceilingplane;
#endif

#if !NO_DRAWSEGS
// ?
#define MAXOPENINGS    SCREENWIDTH*64
short openings[MAXOPENINGS];
short *lastopening;
#endif


//
// Clip values are the solid pixel bounding the range.
//  floorclip starts out SCREENHEIGHT
//  ceilingclip starts out -1
//
floor_ceiling_clip_t floorclip[SCREENWIDTH];
floor_ceiling_clip_t ceilingclip[SCREENWIDTH];

//
// spanstart holds the start of a plane span
// initialized to 0 at start
//
int spanstart[SCREENHEIGHT];
int spanstop[SCREENHEIGHT];

//
// texture mapping
//
#if !USE_LIGHTMAP_INDEXES
const lighttable_t**		planezlight;
#else
int8_t *planezlight;
#endif
fixed_t planeheight;

#if !DOOM_TINY
fixed_t yslope[SCREENHEIGHT];
#else
fixed_t yslope[MAIN_VIEWHEIGHT];
#endif
#if !FIXED_SCREENWIDTH
fixed_t _distscale[SCREENWIDTH];
#else
static_assert(SCREENWIDTH == 320, "");
#define DC(x) ((((x)>>16)==1)?(x)&0xffffu:0xffffffff)
const uint16_t _distscale[SCREENWIDTH] = {
        DC(0x00016a75), DC(0x00016912), DC(0x000167fa), DC(0x000166e4), DC(0x000165d0), DC(0x000164bf), DC(0x000163b0), DC(0x00016262), DC(0x00016158), DC(0x00016052), DC(0x00015f0b), DC(0x00015e0b), DC(0x00015d0b), DC(0x00015bce), DC(0x00015ad6), DC(0x0001599f),
        DC(0x000158ab), DC(0x0001577c), DC(0x0001568b), DC(0x00015561), DC(0x00015475), DC(0x00015353), DC(0x00015232), DC(0x0001514e), DC(0x00015034), DC(0x00014f1b), DC(0x00014e08), DC(0x00014d2d), DC(0x00014c1d), DC(0x00014b13), DC(0x00014a0a), DC(0x00014902),
        DC(0x00014800), DC(0x000146ff), DC(0x00014601), DC(0x00014506), DC(0x0001440d), DC(0x00014319), DC(0x00014226), DC(0x00014136), DC(0x00014018), DC(0x00013f2d), DC(0x00013e46), DC(0x00013d60), DC(0x00013c50), DC(0x00013b70), DC(0x00013a91), DC(0x0001398b),
        DC(0x000138b2), DC(0x000137b1), DC(0x000136de), DC(0x000135e4), DC(0x00013516), DC(0x00013420), DC(0x0001332f), DC(0x00013267), DC(0x0001317c), DC(0x00013093), DC(0x00012fae), DC(0x00012ef1), DC(0x00012e10), DC(0x00012d33), DC(0x00012c58), DC(0x00012b80),
        DC(0x00012aab), DC(0x000129d9), DC(0x0001290a), DC(0x0001283e), DC(0x00012773), DC(0x000126ac), DC(0x000125c7), DC(0x00012506), DC(0x00012446), DC(0x0001238b), DC(0x000122b2), DC(0x000121fb), DC(0x00012146), DC(0x00012077), DC(0x00011fc7), DC(0x00011efd),
        DC(0x00011e54), DC(0x00011d90), DC(0x00011ceb), DC(0x00011c2d), DC(0x00011b8d), DC(0x00011ad5), DC(0x00011a20), DC(0x0001196e), DC(0x000118d7), DC(0x0001182a), DC(0x00011780), DC(0x000116da), DC(0x00011635), DC(0x00011594), DC(0x000114f6), DC(0x0001145a),
        DC(0x000113c1), DC(0x0001132a), DC(0x00011297), DC(0x00011206), DC(0x00011177), DC(0x000110eb), DC(0x00011062), DC(0x00010fdb), DC(0x00010f45), DC(0x00010ec3), DC(0x00010e44), DC(0x00010db6), DC(0x00010d3c), DC(0x00010cc5), DC(0x00010c40), DC(0x00010bcd),
        DC(0x00010b4e), DC(0x00010ae0), DC(0x00010a66), DC(0x000109fe), DC(0x00010989), DC(0x00010926), DC(0x000108b7), DC(0x0001084b), DC(0x000107f0), DC(0x00010789), DC(0x00010725), DC(0x000106d0), DC(0x00010671), DC(0x00010616), DC(0x000105bd), DC(0x00010567),
        DC(0x0001051d), DC(0x000104cc), DC(0x0001047e), DC(0x00010433), DC(0x000103ea), DC(0x000103a4), DC(0x00010360), DC(0x0001031e), DC(0x000102e0), DC(0x000102a4), DC(0x0001026b), DC(0x00010234), DC(0x00010200), DC(0x000101cf), DC(0x000101a0), DC(0x00010174),
        DC(0x0001014a), DC(0x00010123), DC(0x000100fd), DC(0x000100db), DC(0x000100bb), DC(0x0001009e), DC(0x00010080), DC(0x00010069), DC(0x00010053), DC(0x00010040), DC(0x00010030), DC(0x00010022), DC(0x00010016), DC(0x0001000c), DC(0x00010006), DC(0x00010002),
        DC(0x00010001), DC(0x00010002), DC(0x00010005), DC(0x0001000b), DC(0x00010015), DC(0x00010020), DC(0x0001002e), DC(0x0001003e), DC(0x00010051), DC(0x00010066), DC(0x0001007d), DC(0x0001009b), DC(0x000100b8), DC(0x000100d7), DC(0x000100f9), DC(0x0001011e),
        DC(0x00010145), DC(0x0001016f), DC(0x0001019a), DC(0x000101c9), DC(0x000101fa), DC(0x0001022e), DC(0x00010264), DC(0x0001029d), DC(0x000102d9), DC(0x00010317), DC(0x00010358), DC(0x0001039a), DC(0x000103e0), DC(0x0001042a), DC(0x00010475), DC(0x000104c3),
        DC(0x00010514), DC(0x0001055c), DC(0x000105b2), DC(0x0001060a), DC(0x00010665), DC(0x000106c4), DC(0x00010719), DC(0x0001077c), DC(0x000107e2), DC(0x0001083e), DC(0x000108aa), DC(0x00010918), DC(0x0001097b), DC(0x000109f0), DC(0x00010a57), DC(0x00010ad1),
        DC(0x00010b3e), DC(0x00010bbd), DC(0x00010c2f), DC(0x00010cb4), DC(0x00010d2c), DC(0x00010da4), DC(0x00010e33), DC(0x00010eb1), DC(0x00010f31), DC(0x00010fc8), DC(0x0001104f), DC(0x000110d8), DC(0x00011164), DC(0x000111f1), DC(0x00011282), DC(0x00011315),
        DC(0x000113ac), DC(0x00011444), DC(0x000114df), DC(0x0001157e), DC(0x0001161f), DC(0x000116c2), DC(0x00011769), DC(0x00011812), DC(0x000118bf), DC(0x00011954), DC(0x00011a06), DC(0x00011aba), DC(0x00011b72), DC(0x00011c13), DC(0x00011cd0), DC(0x00011d75),
        DC(0x00011e37), DC(0x00011ee2), DC(0x00011faa), DC(0x0001205a), DC(0x00012128), DC(0x000121dc), DC(0x00012293), DC(0x0001236c), DC(0x00012427), DC(0x000124e5), DC(0x000125a6), DC(0x0001268c), DC(0x00012752), DC(0x0001281b), DC(0x000128e7), DC(0x000129b7),
        DC(0x00012a88), DC(0x00012b5c), DC(0x00012c34), DC(0x00012d0e), DC(0x00012dea), DC(0x00012ecb), DC(0x00012f87), DC(0x0001306d), DC(0x00013155), DC(0x00013241), DC(0x00013307), DC(0x000133f8), DC(0x000134eb), DC(0x000135bb), DC(0x000136b4), DC(0x00013788),
        DC(0x00013887), DC(0x00013960), DC(0x00013a65), DC(0x00013b44), DC(0x00013c23), DC(0x00013d32), DC(0x00013e17), DC(0x00013efe), DC(0x00013fe9), DC(0x00014105), DC(0x000141f5), DC(0x000142e7), DC(0x000143dd), DC(0x000144d4), DC(0x000145cf), DC(0x000146cc),
        DC(0x000147cb), DC(0x000148cf), DC(0x000149d4), DC(0x00014add), DC(0x00014be8), DC(0x00014cf7), DC(0x00014dd0), DC(0x00014ee5), DC(0x00014ffb), DC(0x00015116), DC(0x000151fa), DC(0x00015319), DC(0x0001543b), DC(0x00015527), DC(0x0001564e), DC(0x0001573f),
        DC(0x0001586e), DC(0x00015963), DC(0x00015a97), DC(0x00015b90), DC(0x00015ccc), DC(0x00015dcb), DC(0x00015ecc), DC(0x00016010), DC(0x00016115), DC(0x0001621f), DC(0x0001636d), DC(0x0001647b), DC(0x0001658c), DC(0x0001669e), DC(0x000167b3), DC(0x000168cc),
};
#endif
fixed_t basexscale;
fixed_t baseyscale;

#if !NO_VISPLANES && !NO_VISPLANE_CACHES
fixed_t cachedheight[SCREENHEIGHT];
fixed_t cacheddistance[SCREENHEIGHT];
fixed_t cachedxstep[SCREENHEIGHT];
fixed_t cachedystep[SCREENHEIGHT];
#endif

//
// R_InitPlanes
// Only at game startup.
//
void R_InitPlanes(void) {
    // Doh!
}

#if !NO_VISPLANES
//
// R_MapPlane
//
// Uses global vars:
//  planeheight
//  ds_source
//  basexscale
//  baseyscale
//  viewx
//  viewy
//
// BASIC PRIMITIVE
//
void
R_MapPlane
        (int y,
         int x1,
         int x2) {
    angle_t angle;
    fixed_t distance;
    fixed_t length;
    unsigned index;

#ifdef RANGECHECK
    if (x2 < x1
        || x1 < 0
        || x2 >= viewwidth
        || y > viewheight) {
        I_Error("R_MapPlane: %i, %i at %i", x1, x2, y);
    }
#endif

#if !NO_VISPLANE_CACHES
    if (planeheight != cachedheight[y]) {
        cachedheight[y] = planeheight;
        distance = cacheddistance[y] = FixedMul(planeheight, yslope[y]);
        ds_xstep = cachedxstep[y] = FixedMul(distance, basexscale);
        ds_ystep = cachedystep[y] = FixedMul(distance, baseyscale);
    } else {
        distance = cacheddistance[y];
        ds_xstep = cachedxstep[y];
        ds_ystep = cachedystep[y];
    }
#else
    distance = FixedMul(planeheight, yslope[y]);
    ds_xstep = FixedMul(distance, basexscale);
    ds_ystep = FixedMul(distance, baseyscale);
#endif

    length = FixedMul(distance, distscale(x1));
    angle = (viewangle + x_to_viewangle(x1)) >> ANGLETOFINESHIFT;
    ds_xfrac = viewx + FixedMul(finecosine(angle), length);
    ds_yfrac = -viewy - FixedMul(finesine(angle), length);

    if (fixedcolormap) {
#if !USE_LIGHTMAP_INDEXES
        ds_colormap = fixedcolormap;
#else
#if !NO_USE_DS_COLORMAP
        ds_colormap = colormaps + fixedcolormap * 256;
#else
        ds_colormap_index = fixedcolormap;
#endif
#endif
    } else {
        index = distance >> LIGHTZSHIFT;

        if (index >= MAXLIGHTZ)
            index = MAXLIGHTZ - 1;

#if !USE_LIGHTMAP_INDEXES
        ds_colormap = planezlight[index];
#else
#if !NO_USE_DS_COLORMAP
        ds_colormap = colormaps + planezlight[index] * 256;
#else
        ds_colormap_index = planezlight[index];
#endif
#endif
    }

    ds_y = y;
    ds_x1 = x1;
    ds_x2 = x2;

    // high or low detail
#if !PD_COLUMNS
    if (!no_draw_spans)
        spanfunc();
#endif

}
#endif

//
// R_ClearPlanes
// At begining of frame.
//
void R_ClearPlanes(void) {
    int i;
    angle_t angle;

    // opening / clipping determination
    for (i = 0; i < viewwidth; i++) {
        floorclip[i] = viewheight + FLOOR_CEILING_CLIP_OFFSET;
        ceilingclip[i] = FLOOR_CEILING_CLIP_OFFSET - 1;
    }

#if !NO_VISPLANES
    lastvisplane = visplanes;
#endif
#if PICODOOM_RENDER_BABY
    for(int i=0;i< count_of(visplanes); i++) {
        visplanes[i].used_by &= ~(1u << render_frame_index);
        if (visplanes[i].used_by) {
            lastvisplane = visplanes + i + 1;
        }
    }
#endif

#if !NO_DRAWSEGS
    lastopening = openings;
#endif

#if !NO_VISPLANES && !NO_VISPLANE_CACHES
    // texture calculation
    memset(cachedheight, 0, sizeof(cachedheight));
#endif

    // left to right mapping
    angle = (viewangle - ANG90) >> ANGLETOFINESHIFT;

    // scale will be unit scale at SCREENWIDTH/2 distance
    basexscale = FixedDiv(finecosine(angle), centerxfrac);
    baseyscale = -FixedDiv(finesine(angle), centerxfrac);
}


#if !NO_VISPLANES
//
// R_FindPlane
//
visplane_t *
R_FindPlane
        (fixed_t height,
         int picnum,
         int lightlevel) {
    visplane_t *check;

    if (picnum == skyflatnum) {
        height = 0;            // all skys map together
        lightlevel = 0;
    } else {
#if PICODOOM_RENDER_BABY
        lightlevel = (lightlevel >> LIGHTSEGSHIFT) + extralight;
        if (lightlevel >= LIGHTLEVELS)
            lightlevel = LIGHTLEVELS-1;
        if (lightlevel < 0)
            lightlevel = 0;
#endif
    }

    // todo graham speed this up
#if PICODOOM_RENDER_BABY
    visplane_t *last_unused = NULL;
#endif
    for (check = visplanes; check < lastvisplane; check++) {
#if PICODOOM_RENDER_BABY
        if (!check->used_by) {
            last_unused = check;
        }
#endif
        if (height == check->height
            && picnum == check->picnum
            && lightlevel == check->lightlevel) {
            break;
        }
    }


    if (check < lastvisplane) {
#if PICODOOM_RENDER_BABY
        check->used_by |= 1u << render_frame_index;
#endif
        return check;
    }
#if !PICODOOM_RENDER_BABY
    if (lastvisplane - visplanes == MAXVISPLANES)
        I_Error("R_FindPlane: no more visplanes");

    lastvisplane++;
#else
    if (!last_unused) {
        if (lastvisplane - visplanes == MAXVISPLANES)
            I_Error("R_FindPlane: no more visplanes");

        lastvisplane++;
    } else {
        check = last_unused;
    }
    check->used_by |= 1u << render_frame_index;
#endif

    check->height = height;
    check->picnum = picnum;
    check->lightlevel = lightlevel;
#if !NO_VISPLANE_GUTS
    check->minx = SCREENWIDTH;
    check->maxx = -1;

    memset(check->top, 0xff, sizeof(check->top));

#endif
    return check;
}

#if !NO_VISPLANE_GUTS
//
// R_CheckPlane
//
visplane_t *
R_CheckPlane
        (visplane_t *pl,
         int start,
         int stop) {
    int intrl;
    int intrh;
    int unionl;
    int unionh;
    int x;

    if (start < pl->minx) {
        intrl = pl->minx;
        unionl = start;
    } else {
        unionl = pl->minx;
        intrl = start;
    }

    if (stop > pl->maxx) {
        intrh = pl->maxx;
        unionh = stop;
    } else {
        unionh = pl->maxx;
        intrh = stop;
    }

    for (x = intrl; x <= intrh; x++)
        if (pl->top[x] != 0xff)
            break;

    if (x > intrh) {
        pl->minx = unionl;
        pl->maxx = unionh;

        // use the same one
        return pl;
    }

    // make a new visplane
    lastvisplane->height = pl->height;
    lastvisplane->picnum = pl->picnum;
    lastvisplane->lightlevel = pl->lightlevel;

    if (lastvisplane - visplanes == MAXVISPLANES)
        I_Error("R_CheckPlane: no more visplanes");

    pl = lastvisplane++;
    pl->minx = start;
    pl->maxx = stop;

    memset(pl->top, 0xff, sizeof(pl->top));

    return pl;
}
#endif

//
// R_MakeSpans
//
void
R_MakeSpans
        (int x,
         int t1,
         int b1,
         int t2,
         int b2) {
    while (t1 < t2 && t1 <= b1) {
        R_MapPlane(t1, spanstart[t1], x - 1);
        t1++;
    }
    while (b1 > b2 && b1 >= t1) {
        R_MapPlane(b1, spanstart[b1], x - 1);
        b1--;
    }

    while (t2 < t1 && t2 <= b2) {
        spanstart[t2] = x;
        t2++;
    }
    while (b2 > b1 && b2 >= t2) {
        spanstart[b2] = x;
        b2--;
    }
}


#if !NO_VISPLANE_GUTS
//
// R_DrawPlanes
// At the end of each frame.
//
void R_DrawPlanes(void) {
    visplane_t *pl;
    int light;
    int x;
    int stop;
    int angle;
    int lumpnum;

#ifdef RANGECHECK
#if !NO_DRAWSEGS
    if (ds_p - drawsegs > MAXDRAWSEGS)
        I_Error("R_DrawPlanes: drawsegs overflow (%" PRIiPTR ")",
                ds_p - drawsegs);
    if (lastopening - openings > MAXOPENINGS)
        I_Error("R_DrawPlanes: opening overflow (%" PRIiPTR ")",
                lastopening - openings);
#endif

    if (lastvisplane - visplanes > MAXVISPLANES)
        I_Error("R_DrawPlanes: visplane overflow (%" PRIiPTR ")",
                lastvisplane - visplanes);

#endif

    for (pl = visplanes; pl < lastvisplane; pl++) {
        if (pl->minx > pl->maxx)
            continue;


        // sky flat
        if (pl->picnum == skyflatnum) {
            dc_iscale = pspriteiscale >> detailshift;

            // Sky is allways drawn full bright,
            //  i.e. colormaps[0] is used.
            // Because of this hack, sky is not affected
            //  by INVUL inverse mapping.
#if !NO_USE_DC_COLORMAP
            dc_colormap = colormaps;
#else
            dc_colormap_index = 0;
#endif

            dc_texturemid = skytexturemid;
            for (x = pl->minx; x <= pl->maxx; x++) {
                dc_yl = pl->top[x];
                dc_yh = pl->bottom[x];

                if (dc_yl <= dc_yh) {
                    angle = (viewangle + xtoviewangle[x]) >> ANGLETOSKYSHIFT;
                    dc_x = x;
                    dc_source = R_GetColumn(skytexture, angle);
#if PD_COLUMNS
                    pd_add_column(PDCOL_SKY);
#endif
#if !NO_DRAW_SKY
                    colfunc();
#endif
                }
            }
            continue;
        }

#if !NO_DRAW_VISPLANES
        // regular flat
                lumpnum = firstflat + flattranslation[pl->picnum];
            ds_source = W_CacheLumpNum(lumpnum, PU_STATIC);

            planeheight = abs(pl->height-viewz);
            light = (pl->lightlevel >> LIGHTSEGSHIFT)+extralight;

            if (light >= LIGHTLEVELS)
                light = LIGHTLEVELS-1;

            if (light < 0)
                light = 0;

            planezlight = zlight[light];

            pl->top[pl->maxx+1] = 0xff;
            pl->top[pl->minx-1] = 0xff;

            stop = pl->maxx + 1;

            for (x=pl->minx ; x<= stop ; x++)
            {
                R_MakeSpans(x,pl->top[x-1],
                    pl->bottom[x-1],
                    pl->top[x],
                    pl->bottom[x]);
            }

                W_ReleaseLumpNum(lumpnum);
#endif

    }
}
#endif
#endif
