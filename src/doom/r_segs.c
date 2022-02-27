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
//	All the clipping: columns, horizontal spans, sky columns.
//





#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#include "i_system.h"

#include "doomdef.h"
#include "doomstat.h"

#include "r_local.h"
#include "r_sky.h"
#if PICO_DOOM
#include "picodoom.h"
#endif
#if USE_WHD
#include "p_spec.h"
#endif

#if PD_COLUMNS && USE_WHD
void pd_add_column2(pd_column_type type);
#else
#define pd_add_column2(x) pd_add_column(x)
#endif

// OPTIMIZE: closed two sided lines as single sided

// True if any of the segs textures might be visible.
boolean segtextured;

// False if the back side is the same plane.
boolean markfloor;
boolean markceiling;

#if !USE_WHD
isb_int16_t toptexture;
isb_int16_t bottomtexture;
isb_int16_t midtexture;
isb_int16_t maskedtexture;
isb_int16_t maskedtexture_tex;
#else
framedrawable_t *toptexture;
framedrawable_t *bottomtexture;
framedrawable_t *midtexture;
framedrawable_t *maskedtexture;
isb_int16_t maskedtexture_tex;
#endif

angle_t rw_normalangle;
// angle to line origin
int rw_angle1;

//
// regular wall
//
int rw_x;
int rw_stopx;
angle_t rw_centerangle;
fixed_t rw_offset;
fixed_t rw_distance;
fixed_t rw_scale;
fixed_t rw_scalestep;
fixed_t rw_midtexturemid;
fixed_t rw_toptexturemid;
fixed_t rw_bottomtexturemid;
fixed_t rw_maskedtexturemid;

int worldtop;
int worldbottom;
int worldhigh;
int worldlow;

fixed_t pixhigh;
fixed_t pixlow;
fixed_t pixhighstep;
fixed_t pixlowstep;

fixed_t topfrac;
fixed_t topstep;

fixed_t bottomfrac;
fixed_t bottomstep;


#if !USE_LIGHTMAP_INDEXES
const lighttable_t**	walllights;
#else
int8_t *walllights;
#endif

#if !NO_DRAWSEGS
short *maskedtexturecol;
//
// R_RenderMaskedSegRange
//
void
R_RenderMaskedSegRange
        (drawseg_t *ds,
         int x1,
         int x2) {
    unsigned index;
    column_t *col;
    int lightnum;
    int texnum;

    // Calculate light table.
    // Use different light tables
    //   for horizontal / vertical / diagonal. Diagonal?
    // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
    curline = ds->curline;
    frontsector = curline->frontsector;
    backsector = curline->backsector;
    texnum = texturetranslation[curline->sidedef->midtexture];

    lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT) + extralight;

    if (vertex_y(seg_v1(curline)) == vertex_y(seg_v2(curline)))
        lightnum--;
    else if (vertex_x(seg_v1(curline)) == vertex_x(seg_v2(curline)))
        lightnum++;

    if (lightnum < 0)
        walllights = scalelight[0];
    else if (lightnum >= LIGHTLEVELS)
        walllights = scalelight[LIGHTLEVELS - 1];
    else
        walllights = scalelight[lightnum];

    maskedtexturecol = ds->maskedtexturecol;

    rw_scalestep = ds->scalestep;
    spryscale = ds->scale1 + (x1 - ds->x1) * rw_scalestep;
    mfloorclip = ds->sprbottomclip;
    mceilingclip = ds->sprtopclip;

    // find positioning
    if (curline->linedef->flags & ML_DONTPEGBOTTOM) {
        dc_texturemid = frontsector->rawfloorheight > backsector->rawfloorheight
                        ? sector_floorheight(frontsector) : sector_floorheight(backsector);
        dc_texturemid = dc_texturemid + textureheight[texnum] - viewz;
    } else {
        dc_texturemid = frontsector->rawceilingheight < backsector->rawceilingheight
                        ? sector_ceilingheight(frontsector) : sector_ceilingheight(backsector);
        dc_texturemid = dc_texturemid - viewz;
    }
    dc_texturemid += curline->sidedef->rowoffset;

#if !USE_LIGHTMAP_INDEXES
    if (fixedcolormap)
        dc_colormap = fixedcolormap;
#else
    if (fixedcolormap) {
#if !NO_USE_DC_COLORMAP
        dc_colormap = colormaps + fixedcolormap * 256;
#else
        dc_colormap_index = fixedcolormap;
#endif
    }
#endif

    // draw the columns
    for (dc_x = x1; dc_x <= x2; dc_x++) {
        // calculate lighting
        if (maskedtexturecol[dc_x] != SHRT_MAX) {
            if (!fixedcolormap) {
                index = spryscale >> LIGHTSCALESHIFT;

                if (index >= MAXLIGHTSCALE)
                    index = MAXLIGHTSCALE - 1;

#if !USE_LIGHTMAP_INDEXES
                dc_colormap = walllights[index];
#else
#if !NO_USE_DS_COLORMAP
                dc_colormap = colormaps + walllights[index] * 256;
#else
                dc_colormap_index = walllights[index];
#endif
#endif
            }

            sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
            dc_iscale = 0xffffffffu / (unsigned) spryscale;

#if PD_SCALE_SORT
            pd_scale = spryscale;
#endif
            // draw the texture
            col = (column_t *) (
                    (byte *) R_GetColumn(texnum, maskedtexturecol[dc_x]) - 3);

//            printf("Draw masked column %d %08x %08x\n", dc_x, sprtopscreen, spryscale);
            R_DrawMaskedColumn(col);
            maskedtexturecol[dc_x] = SHRT_MAX;
        }
        spryscale += rw_scalestep;
    }

}
#endif



//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked
//  texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling
//  textures.
// CALLED: CORE LOOPING ROUTINE.
//
#define HEIGHTBITS        12
#define HEIGHTUNIT        (1<<HEIGHTBITS)
#define WORLD_TO_HEIGHT_SHIFT (FRACBITS - HEIGHTBITS)

#if NO_DRAW_MID
#define no_draw_mid 1
#else
int no_draw_mid;
#endif
#if NO_DRAW_TOP
#define no_draw_top 1
#else
int no_draw_top;
#endif
#if NO_DRAW_BOTTOM
#define no_draw_bottom 1
#else
int no_draw_bottom;
#endif

#define EPSILON 1
void R_RenderSegLoop(void) {
    angle_t angle;
    unsigned index;
    int yl;
    int yh;
    int mid;
    fixed_t texturecolumn;
    int top;
    int bottom;

    for (; rw_x < rw_stopx; rw_x++) {
#if PD_SCALE_SORT
        pd_scale = rw_scale;
#endif

        // mark floor / ceiling areas
        yl = (topfrac + HEIGHTUNIT - 1) >> HEIGHTBITS;

        // no space above wall?
        if (yl < ceilingclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET + 1)
            yl = ceilingclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET + 1;

        if (markceiling) {
            top = ceilingclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET + 1;
            bottom = yl - 1;

            if (bottom >= floorclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET)
                bottom = floorclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET - 1;

            if (top <= bottom) {
#if !NO_VISPLANE_GUTS
                ceilingplane->top[rw_x] = top;
                ceilingplane->bottom[rw_x] = bottom;
#endif
#if PICO_DOOM
                if (frontsector->ceilingpic != skyflatnum) {
                    pd_add_plane_column(rw_x, top, bottom, rw_scale + EPSILON, false, ceilingplane - visplanes);
                } else {
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

                    dc_x = rw_x;
                    dc_yl = top;
                    dc_yh = bottom;

                    if (dc_yl <= dc_yh) {
                        angle = (viewangle + x_to_viewangle(rw_x)) >> ANGLETOSKYSHIFT;
#if USE_WHD
                        dc_source = R_GetColumn(skytexture_fd, angle);
#else
                        dc_source = R_GetColumn(skytexture, angle);
#endif
#if PD_COLUMNS
                        pd_add_column2(PDCOL_SKY); // to just use sky texture
#endif
#if !NO_DRAW_SKY
                        colfunc();
#endif
                    }
                }
#endif
            }
        }

        yh = bottomfrac >> HEIGHTBITS;

        if (yh >= floorclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET)
            yh = floorclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET - 1;

        if (markfloor) {
            top = yh + 1;
            bottom = floorclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET - 1;
            if (top <= ceilingclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET)
                top = ceilingclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET + 1;
            if (top <= bottom) {
#if !NO_VISPLANE_GUTS
                floorplane->top[rw_x] = top;
                floorplane->bottom[rw_x] = bottom;
#endif
#if PICO_DOOM
                pd_add_plane_column(rw_x, top, bottom, rw_scale + EPSILON, true, floorplane - visplanes);
#endif
            }
        }

        // texturecolumn and lighting are independent of wall tiers
        if (segtextured) {
            // calculate texture offset
            angle = (rw_centerangle + x_to_viewangle(rw_x)) >> ANGLETOFINESHIFT;
            texturecolumn = rw_offset - FixedMul(finetangent(angle), rw_distance);
            texturecolumn >>= FRACBITS;
            // calculate lighting
            index = rw_scale >> LIGHTSCALESHIFT;

            if (index >= MAXLIGHTSCALE)
                index = MAXLIGHTSCALE - 1;

#if !USE_LIGHTMAP_INDEXES
            dc_colormap = walllights[index];
#else
#if !NO_USE_DC_COLORMAP
            dc_colormap = colormaps + walllights[index] * 256;
#else
            dc_colormap_index = walllights[index];
#endif
#endif
            dc_x = rw_x;
            dc_iscale = 0xffffffffu / (unsigned) rw_scale;
#if MU_STATS
            stats_dc_iscale_min = MIN(dc_iscale, stats_dc_iscale_min);
            stats_dc_iscale_max = MAX(dc_iscale, stats_dc_iscale_max);
#endif
        } else {
            // purely to shut up the compiler

            texturecolumn = 0;
        }

        // draw the wall tiers
        if (midtexture) {
            // single sided line
            dc_yl = yl;
            dc_yh = yh;
            dc_texturemid = rw_midtexturemid;
            dc_source = R_GetColumn(midtexture, texturecolumn);
#if PD_COLUMNS
#if PD_SCALE_SORT
            pd_add_column2(PDCOL_MID);
#else
            if (!maskedtexture) pd_add_column(PDCOL_MID);
#endif
#endif
            if (!no_draw_mid) {
//                if (!maskedtexture) {
//                    I_Error("Duh\n");
//                }
                // todo graham; added !maskedtexture as these must be drawn again later
                if (!maskedtexture) colfunc();
            }
            ceilingclip[rw_x] = viewheight + FLOOR_CEILING_CLIP_OFFSET;
            floorclip[rw_x] = FLOOR_CEILING_CLIP_OFFSET - 1;
        } else {
            // two sided line
            if (toptexture) {
                // top wall
                mid = pixhigh >> HEIGHTBITS;
                pixhigh += pixhighstep;

                if (mid >= floorclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET)
                    mid = floorclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET - 1;

                if (mid >= yl) {
                    dc_yl = yl;
                    dc_yh = mid;
                    dc_texturemid = rw_toptexturemid;
                    dc_source = R_GetColumn(toptexture, texturecolumn);
#if PD_COLUMNS
                    pd_add_column2(PDCOL_TOP);
#endif
                    if (!no_draw_top)
                        colfunc();
                    ceilingclip[rw_x] = mid + FLOOR_CEILING_CLIP_OFFSET;
                } else
                    ceilingclip[rw_x] = yl + FLOOR_CEILING_CLIP_OFFSET - 1;
            } else {
                // no top wall
                if (markceiling)
                    ceilingclip[rw_x] = yl + FLOOR_CEILING_CLIP_OFFSET - 1;
            }

            if (bottomtexture) {
                // bottom wall
                mid = (pixlow + HEIGHTUNIT - 1) >> HEIGHTBITS;
                pixlow += pixlowstep;

                // no space above wall?
                if (mid <= ceilingclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET)
                    mid = ceilingclip[rw_x] - FLOOR_CEILING_CLIP_OFFSET + 1;

                if (mid <= yh) {
                    dc_yl = mid;
                    dc_yh = yh;
                    dc_texturemid = rw_bottomtexturemid;
                    dc_source = R_GetColumn(bottomtexture,
                                            texturecolumn);
#if PD_COLUMNS
                    pd_add_column2(PDCOL_BOTTOM);
#endif
                    if (!no_draw_bottom)
                        colfunc();
                    assert(mid >= 0 && mid <= viewheight);
                    floorclip[rw_x] = mid + FLOOR_CEILING_CLIP_OFFSET;
                } else {
                    if (yh + 1 >= 0) { // not sure why this isn't the case some times
                        assert(yh + 1 >= 0 && yh + 1 <= viewheight);
                        floorclip[rw_x] = yh + FLOOR_CEILING_CLIP_OFFSET + 1;
                    }
                }
            } else {
                // no bottom wall
                if (markfloor) {
                    assert(yh + 1 >= 0 && yh + 1 <= viewheight);
                    floorclip[rw_x] = yh + FLOOR_CEILING_CLIP_OFFSET + 1;
                }
            }

            if (maskedtexture) {
                // save texturecol
                //  for backdrawing of masked mid texture
#if !NO_DRAWSEGS
                maskedtexturecol[rw_x] = texturecolumn;
#else
                // graham: added drawing here
                // draw the texture
#if USE_WHD
                maskedcolumn_t col = R_GetMaskedColumn(maskedtexture, texturecolumn & (whd_textures[maskedtexture_tex].width -1));
#else
                maskedcolumn_t col = R_GetMaskedColumn(maskedtexture, texturecolumn);
#endif
                dc_texturemid = rw_maskedtexturemid;

#if !NO_MASKED_FLOOR_CLIP
                mfloorclip = floorclip;
                mceilingclip = ceilingclip;
#endif
                spryscale = rw_scale;
                sprtopscreen = centeryfrac - FixedMul(dc_texturemid, spryscale);
#if !USE_WHD
                R_DrawMaskedColumn(col);
#else
                if (col.real_id < 0) {
                    R_DrawMaskedColumn(col);
                } else {
                    int sprbottomscreen = sprtopscreen + spryscale * texture_height(col.real_id);
                    dc_yl = (sprtopscreen + FRACUNIT - 1) >> FRACBITS;
                    dc_yh = (sprbottomscreen - 1) >> FRACBITS;

                    if (dc_yh >= mfloorclip[dc_x] - FLOOR_CEILING_CLIP_OFFSET)
                        dc_yh = mfloorclip[dc_x] - FLOOR_CEILING_CLIP_OFFSET - 1;
                    if (dc_yl <= mceilingclip[dc_x] - FLOOR_CEILING_CLIP_OFFSET)
                        dc_yl = mceilingclip[dc_x] - FLOOR_CEILING_CLIP_OFFSET + 1;
                    dc_source = col;
                    pd_add_column2(PDCOL_MID);
                }
#endif
#endif
            }
        }

        rw_scale += rw_scalestep;
        topfrac += topstep;
        bottomfrac += bottomstep;
    }
}


//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
void
R_StoreWallRange
        (int start,
         int stop) {
    fixed_t hyp;
    fixed_t sineval;
    angle_t distangle, offsetangle;
    fixed_t vtop;
    int lightnum;

#if DOOM_TINY
    dc_translation_index = 0; // no palette translation
#endif

#if !NO_DRAWSEGS
    // don't overflow and crash
    if (ds_p == &drawsegs[MAXDRAWSEGS])
        return;
#endif

#ifdef RANGECHECK
    if (start >= viewwidth || start > stop)
        I_Error("Bad R_RenderWallRange: %i to %i", start, stop);
#endif

    sidedef = seg_sidedef(curline);
    linedef = seg_linedef(curline);

    // mark the segment as visible for auto map
    line_set_mapped(linedef);

    // calculate rw_distance for scale calculation
#if WHD_SUPER_TINY
    // don't bother to store angle in seg
    int dx = vertex_x(seg_v2(curline)) - vertex_x(seg_v1(curline));
    int dy = vertex_y(seg_v2(curline)) - vertex_y(seg_v1(curline));
    rw_normalangle = R_PointToAngleDX(dx, dy) + ANG90;
#else
    rw_normalangle = seg_angle(curline) + ANG90;
#endif
    offsetangle = abs((int) rw_normalangle - (int) rw_angle1);

    if (offsetangle > ANG90)
        offsetangle = ANG90;

    distangle = ANG90 - offsetangle;
    hyp = R_PointToDist(vertex_x(seg_v1(curline)), vertex_y(seg_v1(curline)));
    sineval = finesine(distangle >> ANGLETOFINESHIFT);
    rw_distance = FixedMul(hyp, sineval);


    rw_x = start;
#if !NO_DRAWSEGS
    ds_p->x1 = rw_x;
    ds_p->x2 = stop;
    ds_p->curline = curline;
#endif
    rw_stopx = stop + 1;


    // calculate scale at both ends and step
    rw_scale =
            R_ScaleFromGlobalAngle(viewangle + x_to_viewangle(start));
#if !NO_DRAWSEGS
    ds_p->scale1 = rw_scale;
#endif

    if (stop > start) {
        fixed_t scale2 = R_ScaleFromGlobalAngle(viewangle + x_to_viewangle(stop));
        rw_scalestep = (scale2 - rw_scale) / (stop - start);
#if !NO_DRAWSEGS
        ds_p->scale2 = scale2;
        ds_p->scalestep = rw_scalestep;
#endif
    } else {
        // UNUSED: try to fix the stretched line bug
#if 0
        if (rw_distance < FRACUNIT/2)
        {
            fixed_t		trx,try;
            fixed_t		gxt,gyt;

            trx = seg_v1(curline)->x - viewx;
            try = seg_v1(curline)->y - viewy;

            gxt = FixedMul(trx,viewcos);
            gyt = -FixedMul(try,viewsin);
            ds_p->scale1 = FixedDiv(projection, gxt-gyt)<<detailshift;
        }
#endif
#if !NO_DRAWSEGS
        ds_p->scale2 = ds_p->scale1;
#endif
    }

    // calculate texture boundaries
    //  and decide if floor / ceiling marks are needed
    worldtop = sector_ceilingheight(frontsector) - viewz;
    worldbottom = sector_floorheight(frontsector) - viewz;

    midtexture = toptexture = bottomtexture = maskedtexture = 0;
#if !NO_DRAWSEGS
    ds_p->maskedtexturecol = NULL;
#endif

    if (!backsector) {
        // single sided line
        midtexture = lookup_texture(texture_translation(side_midtexture(sidedef)));
        // a single sided line is terminal, so it must mark ends
        markfloor = markceiling = true;
        if (line_flags(linedef) & ML_DONTPEGBOTTOM) {
            vtop = sector_floorheight(frontsector) +
                   texture_height(side_midtexture(sidedef));
            // bottom of texture at bottom
            rw_midtexturemid = vtop - viewz;
        } else {
            // top of texture at top
            rw_midtexturemid = worldtop;
        }
        rw_midtexturemid += side_rowoffset(sidedef);

#if !NO_DRAWSEGS
        ds_p->silhouette = SIL_BOTH;
        ds_p->sprtopclip = maxfloorceilingcliparray;
        ds_p->sprbottomclip =  minfloorceilingcliparray;
        ds_p->bsilheight = INT_MAX;
        ds_p->tsilheight = INT_MIN;
#endif
    } else {
#if !NO_DRAWSEGS
        // two sided line
        ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
        ds_p->silhouette = 0;

        if (frontsector->rawfloorheight > backsector->rawfloorheight) {
            ds_p->silhouette = SIL_BOTTOM;
            ds_p->bsilheight = sector_floorheight(frontsector);
        } else if (sector_floorheight(backsector) > viewz) {
            ds_p->silhouette = SIL_BOTTOM;
            ds_p->bsilheight = INT_MAX;
            // ds_p->sprbottomclip = negonearray;
        }

        if (frontsector->rawceilingheight < backsector->rawceilingheight) {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = sector_ceilingheight(frontsector);
        } else if (sector_ceilingheight(backsector) < viewz) {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = INT_MIN;
            // ds_p->sprtopclip = screenheightarray;
        }

        if (backsector->rawceilingheight <= frontsector->rawfloorheight) {
            ds_p->sprbottomclip = minfloorceilingcliparray;
            ds_p->bsilheight = INT_MAX;
            ds_p->silhouette |= SIL_BOTTOM;
        }

        if (backsector->rawfloorheight >= frontsector->rawceilingheight) {
            ds_p->sprtopclip = maxfloorceilingcliparray;
            ds_p->tsilheight = INT_MIN;
            ds_p->silhouette |= SIL_TOP;
        }
#endif
        worldhigh = sector_ceilingheight(backsector) - viewz;
        worldlow = sector_floorheight(backsector) - viewz;

        // hack to allow height changes in outdoor areas
        if (frontsector->ceilingpic == skyflatnum
            && backsector->ceilingpic == skyflatnum) {
            worldtop = worldhigh;
        }


        if (worldlow != worldbottom
            || backsector->floorpic != frontsector->floorpic
            || backsector->lightlevel != frontsector->lightlevel) {
            markfloor = true;
        } else {
            // same plane on both sides
            markfloor = false;
        }


        if (worldhigh != worldtop
            || backsector->ceilingpic != frontsector->ceilingpic
            || backsector->lightlevel != frontsector->lightlevel) {
            markceiling = true;
        } else {
            // same plane on both sides
            markceiling = false;
        }

        if (backsector->rawceilingheight <= frontsector->rawfloorheight
            || backsector->rawfloorheight >= frontsector->rawceilingheight) {
            // closed door
            markceiling = markfloor = true;
        }


        if (worldhigh < worldtop) {
            // top texture
            toptexture = lookup_texture(texture_translation(side_toptexture(sidedef)));
            if (line_flags(linedef) & ML_DONTPEGTOP) {
                // top of texture at top
                rw_toptexturemid = worldtop;
            } else {
                vtop =
                        sector_ceilingheight(backsector)
                        + texture_height(side_toptexture(sidedef));

                // bottom of texture
                rw_toptexturemid = vtop - viewz;
            }
        }
        if (worldlow > worldbottom) {
            // bottom texture
            bottomtexture = lookup_texture(texture_translation(side_bottomtexture(sidedef)));

            if (line_flags(linedef) & ML_DONTPEGBOTTOM) {
                // bottom of texture at bottom
                // top of texture at top
                rw_bottomtexturemid = worldtop;
            } else    // top of texture at top
                rw_bottomtexturemid = worldlow;
        }
        rw_toptexturemid += side_rowoffset(sidedef);
        rw_bottomtexturemid += side_rowoffset(sidedef);

        // allocate space for masked texture tables
        if (side_midtexture(sidedef)) {
            // masked midtexture
            maskedtexture_tex = texture_translation(side_midtexture(sidedef));
            maskedtexture = lookup_masked_texture(maskedtexture_tex);
#if NO_DRAWSEGS
            // find positioning
            if (line_flags(seg_linedef(curline)) & ML_DONTPEGBOTTOM) {
                rw_maskedtexturemid = frontsector->rawfloorheight > backsector->rawfloorheight
                                ? sector_floorheight(frontsector) : sector_floorheight(backsector);
#if USE_WHD
                rw_maskedtexturemid = rw_maskedtexturemid + texture_height(maskedtexture_tex) - viewz;
#else
                rw_maskedtexturemid = rw_maskedtexturemid + texture_height(maskedtexture) - viewz;
#endif
            } else {
                rw_maskedtexturemid = frontsector->rawceilingheight < backsector->rawceilingheight
                                ? sector_ceilingheight(frontsector) : sector_ceilingheight(backsector);
                rw_maskedtexturemid = rw_maskedtexturemid - viewz;
            }
            rw_maskedtexturemid += side_rowoffset(seg_sidedef(curline));
#else
            maskedtexturecol = lastopening - rw_x;
            ds_p->maskedtexturecol = maskedtexturecol;
            lastopening += rw_stopx - rw_x;
#endif
        }
    }

    // calculate rw_offset (only needed for textured lines)
    segtextured = (midtexture!=0) | (toptexture!=0) | (bottomtexture!=0) | (maskedtexture!=0);

    if (segtextured) {
        offsetangle = rw_normalangle - rw_angle1;

        if (offsetangle > ANG180)
            offsetangle = -offsetangle;

        if (offsetangle > ANG90)
            offsetangle = ANG90;

        sineval = finesine(offsetangle >> ANGLETOFINESHIFT);
        rw_offset = FixedMul(hyp, sineval);

        if (rw_normalangle - rw_angle1 < ANG180)
            rw_offset = -rw_offset;

        rw_offset += side_textureoffset(sidedef) + seg_offset(curline);
#if USE_WHD
        if (!seg_side(curline)) {
            uint loff = linedef - lines;
            uint min = 0;
            uint max = numlinespecials;
            while (min < max) {
                uint mid = (min + max) / 2;
                if (linespecialoffsetlist[mid] <= loff) {
                    if (linespecialoffsetlist[mid] == loff) {
                        rw_offset += linespecialoffset << FRACBITS;
                        break;
                    }
                    min = mid + 1;
                } else {
                    max = mid;
                }
            }
        }
#endif
        rw_centerangle = ANG90 + viewangle - rw_normalangle;

        // calculate light table
        //  use different light tables
        //  for horizontal / vertical / diagonal
        // OPTIMIZE: get rid of LIGHTSEGSHIFT globally
        if (!fixedcolormap) {
            lightnum = (frontsector->lightlevel >> LIGHTSEGSHIFT) + extralight;

            if (line_is_horiz(seg_linedef(curline)))
                lightnum--;
            else if (line_is_vert(seg_linedef(curline)))
                lightnum++;

            if (lightnum < 0)
                walllights = scalelight[0];
            else if (lightnum >= LIGHTLEVELS)
                walllights = scalelight[LIGHTLEVELS - 1];
            else
                walllights = scalelight[lightnum];
        }
    }

    // if a floor / ceiling plane is on the wrong side
    //  of the view plane, it is definitely invisible
    //  and doesn't need to be marked.


    if (sector_floorheight(frontsector) >= viewz) {
        // above view plane
        markfloor = false;
    }

    if (sector_ceilingheight(frontsector) <= viewz
        && frontsector->ceilingpic != skyflatnum) {
        // below view plane
        markceiling = false;
    }


    // calculate incremental stepping values for texture edges
    worldtop >>= WORLD_TO_HEIGHT_SHIFT;
    worldbottom >>= WORLD_TO_HEIGHT_SHIFT;

    topstep = -FixedMul(rw_scalestep, worldtop);
    topfrac = (centeryfrac >> WORLD_TO_HEIGHT_SHIFT) - FixedMul(worldtop, rw_scale);

    bottomstep = -FixedMul(rw_scalestep, worldbottom);
    bottomfrac = (centeryfrac >> WORLD_TO_HEIGHT_SHIFT) - FixedMul(worldbottom, rw_scale);

    if (backsector) {
        worldhigh >>= WORLD_TO_HEIGHT_SHIFT;
        worldlow >>= WORLD_TO_HEIGHT_SHIFT;

        if (worldhigh < worldtop) {
            pixhigh = (centeryfrac >> WORLD_TO_HEIGHT_SHIFT) - FixedMul(worldhigh, rw_scale);
            pixhighstep = -FixedMul(rw_scalestep, worldhigh);
        }

        if (worldlow > worldbottom) {
            pixlow = (centeryfrac >> WORLD_TO_HEIGHT_SHIFT) - FixedMul(worldlow, rw_scale);
            pixlowstep = -FixedMul(rw_scalestep, worldlow);
        }
    }

#if !NO_VISPLANE_GUTS
    // render it
    if (markceiling)
        ceilingplane = R_CheckPlane(ceilingplane, rw_x, rw_stopx - 1);

    if (markfloor)
        floorplane = R_CheckPlane(floorplane, rw_x, rw_stopx - 1);
#endif

    R_RenderSegLoop();


#if !NO_DRAWSEGS
    // save sprite clipping info
    if (((ds_p->silhouette & SIL_TOP) || maskedtexture)
        && !ds_p->sprtopclip) {
        memcpy(lastopening, ceilingclip + start, sizeof(*lastopening) * (rw_stopx - start));
        ds_p->sprtopclip = lastopening - start;
        lastopening += rw_stopx - start;
    }

    if (((ds_p->silhouette & SIL_BOTTOM) || maskedtexture)
        && !ds_p->sprbottomclip) {
        memcpy(lastopening, floorclip + start, sizeof(*lastopening) * (rw_stopx - start));
        ds_p->sprbottomclip = lastopening - start;
        lastopening += rw_stopx - start;
    }

    if (maskedtexture && !(ds_p->silhouette & SIL_TOP)) {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT_MIN;
    }
    if (maskedtexture && !(ds_p->silhouette & SIL_BOTTOM)) {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT_MAX;
    }
    ds_p++;
#endif
}

#if PD_COLUMNS && USE_WHD
void pd_add_column2(pd_column_type type) {
    assert(dc_source.real_id >= 0);
    int pc = whd_textures[dc_source.real_id].patch_count;
    if (!pc) {
        dc_source = make_drawcolumn(lookup_patch(whd_textures[dc_source.real_id].patch0-firstspritelump), dc_source.col);
    } else {
        uint8_t *patch_table = &((uint8_t *)whd_textures)[whd_textures[dc_source.real_id].metdata_offset];
        uint8_t *metadata = patch_table + pc * 2;

        int xx = 0;
        do {
            uint b=metadata[0];
            if (xx + 1 + (b&0x7f) > dc_source.col) break;
            xx = xx + 1 + (b&0x7f);
            metadata += 1 + 2 * (b>>7u);
        } while (true);
        if (metadata[0] & 0x80) {
            int pn = metadata[1];
            if (pn != 0xff) { // todo this was here before zero patch columns, but i can't see why (it used to fall thru to regular add column type)
              // single patch column
                assert(pn < pc);
                uint pnum = patch_table[pn*2] | (patch_table[pn*2+1] << 8);
                // note as pre R_GenerateLookup, a single patch column ignores the patch offsety
                dc_source = make_drawcolumn(lookup_patch(pnum-firstspritelump), (uint8_t)(dc_source.col - metadata[2]));
            } else {
#warning untested no patch column
                return;
            }
        }
    }
    pd_add_column(type);
}
#endif