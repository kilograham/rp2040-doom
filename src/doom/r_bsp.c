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
//	BSP traversal, handling of LineSegs for rendering.
//




#include "doomdef.h"

#include "m_bbox.h"

#include "i_system.h"

#include "r_main.h"
#include "r_plane.h"
#include "r_things.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#if PICO_ON_DEVICE
#include "i_sound.h"
#include "pico/time.h"
#endif
//#include "r_local.h"



seg_t *curline;
side_t *sidedef;
line_t *linedef;
sector_t *frontsector;
sector_t *backsector;

#if !NO_DRAWSEGS
drawseg_t drawsegs[MAXDRAWSEGS];
drawseg_t *ds_p;
#endif


void
R_StoreWallRange
        (int start,
         int stop);


//
// R_ClearDrawSegs
//
void R_ClearDrawSegs(void) {
#if !NO_DRAWSEGS
    ds_p = drawsegs;
#endif
}


//
// ClipWallSegment
// Clips the given range of columns
// and includes it in the new clip list.
//
typedef struct {
    int16_t first;
    int16_t last;

} cliprange_t;

// We must expand MAXSEGS to the theoretical limit of the number of solidsegs
// that can be generated in a scene by the DOOM engine. This was determined by
// Lee Killough during BOOM development to be a function of the screensize.
// The simplest thing we can do, other than fix this bug, is to let the game
// render overage and then bomb out by detecting the overflow after the 
// fact. -haleyjd
//#define MAXSEGS 32
#define MAXSEGS (SCREENWIDTH / 2 + 1)

// todo pico possible space savings here vs runtime... a bitmap might be just as good
// newend is one past the last valid seg
cliprange_t *newend;
cliprange_t solidsegs[MAXSEGS];

//
// R_ClipSolidWallSegment
// Does handle solid walls,
//  e.g. single sided LineDefs (middle texture)
//  that entirely block the view.
// 
void
R_ClipSolidWallSegment
        (int first,
         int last) {
    cliprange_t *next;
    cliprange_t *start;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solidsegs;
    while (start->last < first - 1)
        start++;

    if (first < start->first) {
        if (last < start->first - 1) {
            // Post is entirely visible (above start),
            //  so insert a new clippost.
            R_StoreWallRange(first, last);
            next = newend;
            newend++;

            while (next != start) {
                *next = *(next - 1);
                next--;
            }
            next->first = first;
            next->last = last;
            return;
        }

        // There is a fragment above *start.
        R_StoreWallRange(first, start->first - 1);
        // Now adjust the clip size.
        start->first = first;
    }

    // Bottom contained in start?
    if (last <= start->last)
        return;

    next = start;
    while (last >= (next + 1)->first - 1) {
        // There is a fragment between two posts.
        R_StoreWallRange(next->last + 1, (next + 1)->first - 1);
        next++;

        if (last <= next->last) {
            // Bottom is contained in next.
            // Adjust the clip size.
            start->last = next->last;
            goto crunch;
        }
    }

    // There is a fragment after *next.
    R_StoreWallRange(next->last + 1, last);
    // Adjust the clip size.
    start->last = last;

    // Remove start+1 to next from the clip list,
    // because start now covers their area.
    crunch:
    if (next == start) {
        // Post just extended past the bottom of one post.
        return;
    }


    while (next++ != newend) {
        // Remove a post.
        *++start = *next;
    }

    newend = start + 1;
}


//
// R_ClipPassWallSegment
// Clips the given range of columns,
//  but does not includes it in the clip list.
// Does handle windows,
//  e.g. LineDefs with upper and lower texture.
//
void
R_ClipPassWallSegment
        (int first,
         int last) {
    cliprange_t *start;

    // Find the first range that touches the range
    //  (adjacent pixels are touching).
    start = solidsegs;
    while (start->last < first - 1)
        start++;

    if (first < start->first) {
        if (last < start->first - 1) {
            // Post is entirely visible (above start).
            R_StoreWallRange(first, last);
            return;
        }

        // There is a fragment above *start.
        R_StoreWallRange(first, start->first - 1);
    }

    // Bottom contained in start?
    if (last <= start->last)
        return;

    while (last >= (start + 1)->first - 1) {
        // There is a fragment between two posts.
        R_StoreWallRange(start->last + 1, (start + 1)->first - 1);
        start++;

        if (last <= start->last)
            return;
    }

    // There is a fragment after *next.
    R_StoreWallRange(start->last + 1, last);
}


//
// R_ClearClipSegs
//
void R_ClearClipSegs(void) {
    solidsegs[0].first = -0x7fff;
    solidsegs[0].last = -1;
    solidsegs[1].first = viewwidth;
    solidsegs[1].last = 0x7fff;
    newend = solidsegs + 2;
}

//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//
void R_AddLine(seg_t *line) {
    int x1;
    int x2;
    angle_t angle1;
    angle_t angle2;
    angle_t span;
    angle_t tspan;

    curline = line;

    // OPTIMIZE: quickly reject orthogonal back sides.
    angle1 = R_PointToAngle(vertex_x(seg_v1(line)), vertex_y(seg_v1(line)));
    angle2 = R_PointToAngle(vertex_x(seg_v2(line)), vertex_y(seg_v2(line)));

    // Clip to view edges.
    // OPTIMIZE: make constant out of 2*clipangle (FIELDOFVIEW).
    span = angle1 - angle2;

    // Back side? I.e. backface culling?
    if (span >= ANG180)
        return;

    // Global angle needed by segcalc.
    rw_angle1 = angle1;
    angle1 -= viewangle;
    angle2 -= viewangle;

    tspan = angle1 + clipangle;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;

        angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;
        angle2 = -clipangle;
    }

    // The seg is in the view range,
    // but not necessarily visible.
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    x1 = viewangletox[angle1];
    x2 = viewangletox[angle2];

    // Does not cross a pixel?
    if (x1 == x2)
        return;

    backsector = seg_backsector(line);

    // Single sided line?
    if (!backsector)
        goto clipsolid;

    // Closed door.
    if (backsector->rawceilingheight <= frontsector->rawfloorheight
        || backsector->rawfloorheight >= frontsector->rawceilingheight)
        goto clipsolid;

    // Window.
    if (backsector->rawceilingheight != frontsector->rawceilingheight
        || backsector->rawfloorheight != frontsector->rawfloorheight)
        goto clippass;

    // Reject empty lines used for triggers
    //  and special events.
    // Identical floor and ceiling on both sides,
    // identical light levels on both sides,
    // and no middle texture.
    if (backsector->ceilingpic == frontsector->ceilingpic
        && backsector->floorpic == frontsector->floorpic
        && backsector->lightlevel == frontsector->lightlevel
        && side_midtexture(seg_sidedef(curline)) == 0) {
        return;
    }


    clippass:
    R_ClipPassWallSegment(x1, x2 - 1);
    return;

    clipsolid:
    R_ClipSolidWallSegment(x1, x2 - 1);
}


//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//
isb_int8_t checkcoord[12][4] =
        {
                {3, 0, 2, 1},
                {3, 0, 2, 0},
                {3, 1, 2, 0},
                {0},
                {2, 0, 2, 1},
                {0, 0, 0, 0},
                {3, 1, 3, 0},
                {0},
                {2, 0, 3, 1},
                {2, 1, 3, 1},
                {2, 1, 3, 0}
        };

boolean R_CheckBBox(const node_coord_t *bspcoord) {
    int boxx;
    int boxy;
    int boxpos;

    fixed_t x1;
    fixed_t y1;
    fixed_t x2;
    fixed_t y2;

    angle_t angle1;
    angle_t angle2;
    angle_t span;
    angle_t tspan;

    cliprange_t *start;

    int sx1;
    int sx2;

    // Find the corners of the box
    // that define the edges from current viewpoint.
    if (viewx <= node_coord_to_fixed(bspcoord[BOXLEFT]))
        boxx = 0;
    else if (viewx < node_coord_to_fixed(bspcoord[BOXRIGHT]))
        boxx = 1;
    else
        boxx = 2;

    if (viewy >= node_coord_to_fixed(bspcoord[BOXTOP]))
        boxy = 0;
    else if (viewy > node_coord_to_fixed(bspcoord[BOXBOTTOM]))
        boxy = 1;
    else
        boxy = 2;

    boxpos = (boxy << 2) + boxx;
    if (boxpos == 5)
        return true;

    x1 = node_coord_to_fixed(bspcoord[checkcoord[boxpos][0]]);
    y1 = node_coord_to_fixed(bspcoord[checkcoord[boxpos][1]]);
    x2 = node_coord_to_fixed(bspcoord[checkcoord[boxpos][2]]);
    y2 = node_coord_to_fixed(bspcoord[checkcoord[boxpos][3]]);

    // check clip list for an open space
    angle1 = R_PointToAngle(x1, y1) - viewangle;
    angle2 = R_PointToAngle(x2, y2) - viewangle;

    span = angle1 - angle2;

    // Sitting on a line?
    if (span >= ANG180)
        return true;

    tspan = angle1 + clipangle;

    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return false;

        angle1 = clipangle;
    }
    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle) {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return false;

        angle2 = -clipangle;
    }


    // Find the first clippost
    //  that touches the source post
    //  (adjacent pixels are touching).
    angle1 = (angle1 + ANG90) >> ANGLETOFINESHIFT;
    angle2 = (angle2 + ANG90) >> ANGLETOFINESHIFT;
    sx1 = viewangletox[angle1];
    sx2 = viewangletox[angle2];

    // Does not cross a pixel.
    if (sx1 == sx2)
        return false;
    sx2--;

    start = solidsegs;
    while (start->last < sx2)
        start++;

    if (sx1 >= start->first
        && sx2 <= start->last) {
        // The clippost contains the new span.
        return false;
    }

    return true;
}


//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
void R_Subsector(int num) {
    seg_t *line;
    seg_t *line_limit;
    subsector_t *sub;

#ifdef RANGECHECK
    if (num >= numsubsectors)
        I_Error("R_Subsector: ss %i with numss = %i",
                num,
                numsubsectors);
#endif

//    sscount++;
    sub = &subsectors[num];
    frontsector = subsector_sector(sub);
    line_limit = subsector_linelimit(sub);
    line = subsector_firstline(sub);

#if !NO_VISPLANES
    if (sector_floorheight(frontsector) < viewz) {
        floorplane = R_FindPlane(sector_floorheight(frontsector),
                                 frontsector->floorpic,
                                 frontsector->lightlevel);
        assert(frontsector->floorpic == skyflatnum ||
               (floorplane->height == sector_floorheight(frontsector) &&
                floorplane->picnum == frontsector->floorpic));
    } else
        floorplane = NULL;

    if (sector_ceilingheight(frontsector) > viewz
        || frontsector->ceilingpic == skyflatnum) {
        ceilingplane = R_FindPlane(sector_ceilingheight(frontsector),
                                   frontsector->ceilingpic,
                                   frontsector->lightlevel);
        assert(frontsector->ceilingpic == skyflatnum ||
               (ceilingplane->height == sector_ceilingheight(frontsector) &&
                ceilingplane->picnum == frontsector->ceilingpic));
    } else
        ceilingplane = NULL;
#endif
    
    R_AddSprites(frontsector);

    while (line < line_limit) {
        R_AddLine(line);
        line += seg_next_step(line);
    }

    // check for solidsegs overflow - extremely unsatisfactory!
    if (newend > &solidsegs[32])
        I_Error("R_Subsector: solidsegs overflow (vanilla may crash here)\n");
}


//
// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.
#if !WHD_SUPER_TINY
void R_RenderBSPNode(int bspnum)
#else
void R_RenderBSPNode (int bspnum, node_coord_t *bbox)
#endif
{
    node_t *bsp;
    int side;

    // todo graham we should loop not recurse perhaps?

    // Found a subsector?
    if (bspnum & NF_SUBSECTOR) {
//        printf("SS %d\n", bspnum);
        if (bspnum == -1)
            R_Subsector(0);
        else
            R_Subsector(bspnum & (~NF_SUBSECTOR));
        return;
    }

#if PICO_ON_DEVICE
    static int t0;
    uint32_t t = time_us_32();
    if ((t - t0) > 3000) {
        I_UpdateSound();
        t0 = t;
    }
#endif
    bsp = &nodes[bspnum];

    // Decide which side the view point is on.
    side = R_PointOnSide(viewx, viewy, bsp);
//    printf("NODE %d v %d,%d p %d,%d dir %d,%d\n", bspnum, viewx, viewy, bsp->x, bsp->y, bsp->dx, bsp->dy);

    // Recursively divide front space.
#if !WHD_SUPER_TINY
    R_RenderBSPNode(bsp->children[side]);
#else
    node_coord_t subbox[4];
    subbox[BOXLEFT] = bbox[BOXLEFT] + (((bsp->bbox_lw[side] & 0xf0u) * (bbox[BOXRIGHT] - bbox[BOXLEFT])) >> 8u);
    subbox[BOXRIGHT] = subbox[BOXLEFT] + ((((bsp->bbox_lw[side] & 0xfu) + 1) * (bbox[BOXRIGHT] - subbox[BOXLEFT])) >> 4u);
    subbox[BOXBOTTOM] = bbox[BOXBOTTOM] + (((bsp->bbox_th[side] & 0xf0u) * (bbox[BOXTOP] - bbox[BOXBOTTOM])) >> 8u);
    subbox[BOXTOP] = subbox[BOXBOTTOM] + ((((bsp->bbox_th[side] & 0xfu) + 1) * (bbox[BOXTOP] - subbox[BOXBOTTOM])) >> 4u);
    R_RenderBSPNode(bsp_child(bspnum, side), subbox);
#endif

    // Possibly divide back space.
#if !WHD_SUPER_TINY
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    if (R_CheckBBox(bsp->bbox[side ^ 1]))
        R_RenderBSPNode(bsp->children[side ^ 1]);
#pragma GCC diagnostic pop
#else
    subbox[BOXLEFT] = bbox[BOXLEFT] + (((bsp->bbox_lw[side^1] & 0xf0u) * (bbox[BOXRIGHT] - bbox[BOXLEFT])) >> 8u);
    subbox[BOXRIGHT] = subbox[BOXLEFT] + ((((bsp->bbox_lw[side^1] & 0xfu) + 1) * (bbox[BOXRIGHT] - subbox[BOXLEFT])) >> 4u);
    subbox[BOXBOTTOM] = bbox[BOXBOTTOM] + (((bsp->bbox_th[side^1] & 0xf0u) * (bbox[BOXTOP] - bbox[BOXBOTTOM])) >> 8u);
    subbox[BOXTOP] = subbox[BOXBOTTOM] + ((((bsp->bbox_th[side^1] & 0xfu) + 1) * (bbox[BOXTOP] - subbox[BOXBOTTOM])) >> 4u);
    if (R_CheckBBox(subbox))
        R_RenderBSPNode(bsp_child(bspnum, side^1), subbox);
#endif
}


