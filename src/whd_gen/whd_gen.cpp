/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "wad.h"
#include <iostream>
#include <ostream>
#include <algorithm>
#include <functional>
#include <cstring>
#include <cassert>
#include <memory>
#include <cstdarg>
#include <array>
#include <cmath>
#include "doomdata.h"
#include "whddata.h"
#include "compress_mus.h"
#include "lodepng.h"
#include "statsomizer.h"
#include "extra_patches.h"
#include <utility>
#include <vector>
#include "musx_decoder.h"
#include "image_decoder.h"

//#define USE_PIXELS_ONLY_PATCH 1 // dont use c3 on patches
#define USE_PIXELS_ONLY_FLAT 1 // dont use c3 on flats
//#define DEBUG_TEXTURE_OPTIMIZATION 1
//#define DEBUG_SAVE_PNG 1

typedef struct __packed {
    const char * const name;
    const uint8_t * const data;
    const uint8_t w;
    const uint8_t h;
} txt_font_t;
#include "../../textscreen/fonts/normal.h"


bool super_tiny = true;

using std::vector;

template<typename T, typename S> std::ostream &operator<<(std::ostream &os, const std::pair<T, S> &v);
#include "huffman.h"
#include "huff.h"
#include "huff_sink.h"

int dumped_patch_count, converted_patch_count, converted_patch_size;
int bit_addressable_patch;
std::vector<int> winners(16);
std::vector<int> fwinners(4);
std::set<int> all_linedef_flags;

statsomizer flat_have_same_savings("Flat same savings");
statsomizer side_meta("Side meta");
statsomizer side_metaz("Side meta z");
statsomizer line_meta("Line meta");
statsomizer line_metaz("Line meta z");
statsomizer line_scale("Line scale");
statsomizer vertex_x("VX");
statsomizer vertex_y("VY");
statsomizer ss_delta("subsector delta");
statsomizer demo_size_orig("demo size orig");
statsomizer demo_size("demo size");

statsomizer single_patch_metadata_size("single patch metadata");

static std::map<int,std::string> touched;
static std::vector<int> cleared_lumps;
static std::set<int> compressed;
static std::set<std::string> name_required;

// map from thing in the lump to some stat buckets
#if 0
#define TOUCHED_PATCH "Graphics"
#define TOUCHED_VPATCH "Graphics"
#define TOUCHED_FLAT "Graphics"
#define TOUCHED_SPRITE_METADATA "Graphics"
#define TOUCHED_PATCH_METADATA "Graphics"
#define TOUCHED_PNAMES "Graphics"
#define TOUCHED_TEX_METADATA "Graphics"
#define TOUCHED_DMX "Unused"
#define TOUCHED_UNUSED_GRAPHIC "Unused"
#define TOUCHED_UNUSED "Unused"
#define TOUCHED_PALETTE "Misc"
#define TOUCHED_COLORMAP "Misc"
#define TOUCHED_ENDOOM "Misc"
#define TOUCHED_SFX "SFX"
#define TOUCHED_MUSIC "Music"
#define TOUCHED_GENMIDI "Misc"
#define TOUCHED_LEVEL "Level"
#define TOUCHED_LEVEL_THINGS "Level"
#define TOUCHED_LEVEL_SIDEDEFS "Level"
#define TOUCHED_LEVEL_NODES "Level"
#define TOUCHED_LEVEL_VERTEXES "Level"
#define TOUCHED_LEVEL_LINEDEFS "Level"
#define TOUCHED_LEVEL_SECTORS "Level"
#define TOUCHED_LEVEL_SEGS "Level"
#define TOUCHED_LEVEL_REJECT "Level"
#define TOUCHED_LEVEL_SSECTORS "Level"
#define TOUCHED_LEVEL_BLOCKMAP "Level"
#else
#define TOUCHED_PATCH "Graphics Patch"
#define TOUCHED_VPATCH "Graphics V-Patch"
#define TOUCHED_FLAT "Graphics Flat"
#define TOUCHED_SPRITE_METADATA "Graphics Patch"
#define TOUCHED_PATCH_METADATA "Graphics Patch"
#define TOUCHED_PNAMES "Graphics Meta"
#define TOUCHED_TEX_METADATA "Graphics Textures"
#define TOUCHED_DMX "Unused"
#define TOUCHED_UNUSED_GRAPHIC "Unused"
#define TOUCHED_UNUSED "Unused"
#define TOUCHED_PALETTE "Misc"
#define TOUCHED_COLORMAP "Misc"
#define TOUCHED_ENDOOM "Misc"
#define TOUCHED_SFX "SFX"
#define TOUCHED_MUSIC "Music"
#define TOUCHED_GENMIDI "Misc"
#define TOUCHED_LEVEL "Level"
#define TOUCHED_LEVEL_THINGS "Level Things"
#define TOUCHED_LEVEL_SIDEDEFS "Level Sidedefs"
#define TOUCHED_LEVEL_NODES "Level Nodes"
#define TOUCHED_LEVEL_VERTEXES "Level Vertexes"
#define TOUCHED_LEVEL_LINEDEFS "Level Linedefs"
#define TOUCHED_LEVEL_SECTORS "Level Sectors"
#define TOUCHED_LEVEL_SEGS "Level Segs"
#define TOUCHED_LEVEL_REJECT "Level Reject"
#define TOUCHED_LEVEL_SSECTORS "Level Subsectors"
#define TOUCHED_LEVEL_BLOCKMAP "Level Blockmap"

#endif
// all the lump names that are referenced by doom source code... we must keep the names
static std::vector<std::string> named_lumps = {
        "PLAYPAL",
        "ENDOOM",
        "P_START",
        "P_END",
        "GENMIDI",
        "COLORMAP",
        "TEXTURE1",
        "TEXTURE2",
        "F_START",
        "F_SKY1",
        "F_END",
        "S_START",
        "S_END",
        "CREDIT",
        "HELP",
        "HELP1",
        "HELP2",
        "PFUB1",
        "PFUB2",
        "ENDPIC",
        "TITLEPIC",
        "INTERPIC",
        "VICTORY2",
        "BOSSBACK",
        "FLOOR4_8",
        "SFLR6_1",
        "MFLR8_4",
        "MFLR8_3",
        "SLIME16",
        "RROCK14",
        "RROCK07",
        "RROCK17",
        "RROCK13",
        "RROCK19",
        "SLIME16",
        "RROCK14",
        "RROCK07",
        "RROCK17",
        "RROCK13",
        "RROCK19",
        "SLIME16",
        "RROCK14",
        "RROCK07",
        "RROCK17",
        "RROCK13",
        "RROCK19",
        "E4M1",
        "E3M1",
};

// large menu graphics with lots of transparency, ecnodedd as runs
static std::vector<std::string> run16_menu_vpatches = {
        "M_RDTHIS",
        "M_OPTION",
        "M_QUITG",
        "M_NGAME",
        "M_THERMR",
        "M_THERMM",
        "M_THERML",
        "M_ENDGAM",
        "M_PAUSE",
        "M_MESSG",
        "M_MSGON",
        "M_MSGOFF",
        "M_EPISOD",
        "M_EPI1",
        "M_EPI2",
        "M_EPI3",
        "M_EPI4",
        "M_HURT",
        "M_JKILL",
        "M_ROUGH",
        "M_SKILL",
        "M_NEWG",
        "M_ULTRA",
        "M_NMARE",
        "M_SVOL",
        "M_OPTTTL",
        "M_SAVEG",
        "M_LOADG",
        "M_DISP",
        "M_MSENS",
        "M_GDHIGH",
        "M_GDLOW",
        "M_DETAIL",
        "M_DISOPT",
        "M_SCRNSZ",
        "M_SGTTL",
        "M_LGTTL",
        "M_SFXVOL",
        "M_MUSVOL",
        "M_LSLEFT",
        "M_LSCNTR",
        "M_LSRGHT",
        // these are our new network menu items
        "M_DTHMCH",
        "M_GAME",
        "M_HOST",
        "M_JOIN",
        "M_NAME",
        "M_NETWK",
        "M_TWO",
        // these seem red but have other colors
        "WISPLAT",
        "WIURH0",
        "WIURH1",
};

// these (all) red graphics share a single 16 color palette... use alpha as the runs are very short
static std::vector<std::string> alpha16_shpal_red_vpatches = {
        "STCFN033",
        "STCFN034",
        "STCFN035",
        "STCFN036",
        "STCFN037",
        "STCFN038",
        "STCFN039",
        "STCFN040",
        "STCFN041",
        "STCFN042",
        "STCFN043",
        "STCFN044",
        "STCFN045",
        "STCFN046",
        "STCFN047",
        "STCFN048",
        "STCFN049",
        "STCFN050",
        "STCFN051",
        "STCFN052",
        "STCFN053",
        "STCFN054",
        "STCFN055",
        "STCFN056",
        "STCFN057",
        "STCFN058",
        "STCFN059",
        "STCFN060",
        "STCFN061",
        "STCFN062",
        "STCFN063",
        "STCFN064",
        "STCFN065",
        "STCFN066",
        "STCFN067",
        "STCFN068",
        "STCFN069",
        "STCFN070",
        "STCFN071",
        "STCFN072",
        "STCFN073",
        "STCFN074",
        "STCFN075",
        "STCFN076",
        "STCFN077",
        "STCFN078",
        "STCFN079",
        "STCFN080",
        "STCFN081",
        "STCFN082",
        "STCFN083",
        "STCFN084",
        "STCFN085",
        "STCFN086",
        "STCFN087",
        "STCFN088",
        "STCFN089",
        "STCFN090",
        "STCFN091",
        "STCFN092",
        "STCFN093",
        "STCFN094",
        "STCFN095",
        "STCFN121",
        "STTMINUS",
        "STTNUM0",
        "STTNUM1",
        "STTNUM2",
        "STTNUM3",
        "STTNUM4",
        "STTNUM5",
        "STTNUM6",
        "STTNUM7",
        "STTNUM8",
        "STTNUM9",
        "STTPRCNT",
        "WICOLON",
        "WIENTER",
        "WIF",
        "WIFRGS",
        "WIKILRS",
        "WIMINUS",
        "WIMSTAR",
        "WIMSTT",
        "WINUM0",
        "WINUM1",
        "WINUM2",
        "WINUM3",
        "WINUM4",
        "WINUM5",
        "WINUM6",
        "WINUM7",
        "WINUM8",
        "WINUM9",
        "WIOSTF",
        "WIOSTI",
        "WIOSTK",
        "WIOSTS",
        "WIP1",
        "WIP2",
        "WIP3",
        "WIP4",
        "WIPAR",
        "WIPCNT",
        "WISCRT2",
        "WISUCKS",
        "WITIME",
        "WIVCTMS",
};

// these level name graphics share a single 16 color palette
static std::vector<std::string> alpha16_shpal_white_vpatches = {
        "WIBP1",
        "WIBP2",
        "WIBP3",
        "WIBP4",
        "WILV00",
        "WILV01",
        "WILV02",
        "WILV03",
        "WILV04",
        "WILV05",
        "WILV06",
        "WILV07",
        "WILV08",
        "WILV10",
        "WILV11",
        "WILV12",
        "WILV13",
        "WILV14",
        "WILV15",
        "WILV16",
        "WILV17",
        "WILV18",
        "WILV20",
        "WILV21",
        "WILV22",
        "WILV23",
        "WILV24",
        "WILV25",
        "WILV26",
        "WILV27",
        "WILV28",
        "WILV30",
        "WILV31",
        "WILV32",
        "WILV33",
        "WILV34",
        "WILV35",
        "WILV36",
        "WILV37",
        "WILV38",
        "CWILV00",
        "CWILV01",
        "CWILV02",
        "CWILV03",
        "CWILV04",
        "CWILV05",
        "CWILV06",
        "CWILV07",
        "CWILV08",
        "CWILV09",
        "CWILV10",
        "CWILV11",
        "CWILV12",
        "CWILV13",
        "CWILV14",
        "CWILV15",
        "CWILV16",
        "CWILV17",
        "CWILV18",
        "CWILV19",
        "CWILV20",
        "CWILV21",
        "CWILV22",
        "CWILV23",
        "CWILV24",
        "CWILV25",
        "CWILV26",
        "CWILV27",
        "CWILV28",
        "CWILV29",
        "CWILV30",
        "CWILV31",
        "CWILV32",
        "CWILV33",
        "CWILV34",
        "CWILV35",
        "CWILV36",
        "CWILV37",
        "CWILV38",
        "CWILV39",
};

// these grayscale graphics share a 16 color palette (note STBAR must use a shared palette as rendering from a shared
// palette which is cached in screen pixel format in RAM is faster). STBAR is "alpha" however it is rendered opaquely
// always for speed
static std::vector<std::string> alpha_shpal_grey_graphics = {
        "STBAR",
        "STARMS",
};

// 16 color status bar graphics, alpha for rendering simplicity/space
static std::vector<std::string> alpha16_status_vpatches = {
        "STYSNUM0",
        "STYSNUM1",
        "STYSNUM2",
        "STYSNUM3",
        "STYSNUM4",
        "STYSNUM5",
        "STYSNUM6",
        "STYSNUM7",
        "STYSNUM8",
        "STYSNUM9",
        "STKEYS0",
        "STKEYS1",
        "STKEYS2",
        "STKEYS3",
        "STKEYS4",
        "STKEYS5",
        "STGNUM0", // unused todo remove
        "STGNUM1", // unused todo remove
        "STGNUM2",
        "STGNUM3",
        "STGNUM4",
        "STGNUM5",
        "STGNUM6",
        "STGNUM7",
        "STGNUM8", // unused todo remove
        "STGNUM9", // unused todo remove
};

// these player background graphics are checked specially to see if they are a single color center with a border (they are in regular doom).
// if so they are encodded in a special vpatch type which is quicker to redner (and more compact)... the rendering speed is important
// otherwise we run out of stbar scanline time in network games
static std::vector<std::string> special_player_background_vpatches = {
        "STFB0",
        "STFB1",
        "STFB2",
        "STFB3",
        "STPB0",
        "STPB1",
        "STPB2",
        "STPB3",
};

// status bar face graphics use 64 color palettes
static std::vector<std::string> run64_face_vpatches = {
        "STFST00",
        "STFST01",
        "STFST02",
        "STFTR00",
        "STFTL00",
        "STFOUCH0",
        "STFEVL0",
        "STFKILL0",
        "STFST10",
        "STFST11",
        "STFST12",
        "STFTR10",
        "STFTL10",
        "STFOUCH1",
        "STFEVL1",
        "STFKILL1",
        "STFST20",
        "STFST21",
        "STFST22",
        "STFTR20",
        "STFTL20",
        "STFOUCH2",
        "STFEVL2",
        "STFKILL2",
        "STFST30",
        "STFST31",
        "STFST32",
        "STFTR30",
        "STFTL30",
        "STFOUCH3",
        "STFEVL3",
        "STFKILL3",
        "STFST40",
        "STFST41",
        "STFST42",
        "STFTR40",
        "STFTL40",
        "STFOUCH4",
        "STFEVL4",
        "STFKILL4",
        "STFGOD0",
        "STFDEAD0",
};

// remaining <16 color graphics ... actually less - we don't use another shared palette as the palettes are
// small and we're short of scratch_x space. these are encoded as "pixel runs" for rendering speed
static std::vector<std::string> run16_misc_vpatches = {
        "AMMNUM0",
        "AMMNUM1",
        "AMMNUM2",
        "AMMNUM3",
        "AMMNUM4",
        "AMMNUM5",
        "AMMNUM6",
        "AMMNUM7",
        "AMMNUM8",
        "AMMNUM9",
        "END0",
        "END1",
        "END2",
        "END3",
        "END4",
        "END5",
        "END6",
};

// remaining <64 color graphics ... these are encoded as "pixel runs" for rendering speed
static std::vector<std::string> run64_misc_vpatches = {
        "M_SKULL1",
        "M_SKULL2",
        "M_THERMO",
        "WIA00000",
        "WIA00001",
        "WIA00002",
        "WIA00100",
        "WIA00101",
        "WIA00102",
        "WIA00200",
        "WIA00201",
        "WIA00202",
        "WIA00300",
        "WIA00301",
        "WIA00302",
        "WIA00400",
        "WIA00401",
        "WIA00402",
        "WIA00500",
        "WIA00501",
        "WIA00502",
        "WIA00600",
        "WIA00601",
        "WIA00602",
        "WIA00700",
        "WIA00701",
        "WIA00702",
        "WIA00800",
        "WIA00801",
        "WIA00802",
        "WIA00900",
        "WIA00901",
        "WIA00902",
        "WIA10000",
        "WIA10100",
        "WIA10200",
        "WIA10300",
        "WIA10400",
        "WIA10500",
        "WIA10600",
        "WIA10700",
        "WIA10701",
        "WIA10702",
        "WIA20000",
        "WIA20001",
        "WIA20002",
        "WIA20100",
        "WIA20101",
        "WIA20102",
        "WIA20200",
        "WIA20201",
        "WIA20202",
        "WIA20300",
        "WIA20301",
        "WIA20302",
        "WIA20400",
        "WIA20401",
        "WIA20402",
        "WIA20500",
        "WIA20501",
        "WIA20502",
};

// 256 color graphics ... note "raw" refers to the fact that we encode them with alpha rather than pixel runs
static std::vector<std::string> run256_misc_vpatches = {
        "M_DOOM",
};

// splash screen patches (which need to be encoded as real patches)
static std::vector<std::string> splash_graphics = {
        "HELP",
        "HELP1",
        "HELP2",
        "CREDIT",
        "ENDPIC",
        "TITLEPIC",
        "INTERPIC",
        "VICTORY2",
        "BOSSBACK",
        "PFUB1",
        "PFUB2",
};
#if __BIG_ENDIAN__
#error didn not bother
#endif

const std::vector<std::string> sprite_names = {
        "TROO", "SHTG", "PUNG", "PISG", "PISF", "SHTF", "SHT2", "CHGG", "CHGF", "MISG",
        "MISF", "SAWG", "PLSG", "PLSF", "BFGG", "BFGF", "BLUD", "PUFF", "BAL1", "BAL2",
        "PLSS", "PLSE", "MISL", "BFS1", "BFE1", "BFE2", "TFOG", "IFOG", "PLAY", "POSS",
        "SPOS", "VILE", "FIRE", "FATB", "FBXP", "SKEL", "MANF", "FATT", "CPOS", "SARG",
        "HEAD", "BAL7", "BOSS", "BOS2", "SKUL", "SPID", "BSPI", "APLS", "APBX", "CYBR",
        "PAIN", "SSWV", "KEEN", "BBRN", "BOSF", "ARM1", "ARM2", "BAR1", "BEXP", "FCAN",
        "BON1", "BON2", "BKEY", "RKEY", "YKEY", "BSKU", "RSKU", "YSKU", "STIM", "MEDI",
        "SOUL", "PINV", "PSTR", "PINS", "MEGAFGA", "SUIT", "PMAP", "PVIS", "CLIP", "AMMO",
        "ROCK", "BROK", "CELL", "CELP", "SHEL", "SBOX", "BPAK", "BFUG", "MGUN", "CSAW",
        "LAUN", "PLAS", "SHOT", "SGN2", "COLU", "SMT2", "GOR1", "POL2", "POL5", "POL4",
        "POL3", "POL1", "POL6", "GOR2", "GOR3", "GOR4", "GOR5", "SMIT", "COL1", "COL2",
        "COL3", "COL4", "CAND", "CBRA", "COL6", "TRE1", "TRE2", "ELEC", "CEYE", "FSKU",
        "COL5", "TBLU", "TGRN", "TRED", "SMBT", "SMGT", "SMRT", "HDB1", "HDB2", "HDB3",
        "HDB4", "HDB5", "HDB6", "POB1", "POB2", "BRS1", "TLMP", "TLP2"
};

// these special textures will be placed first, as they are handled specially in the code, and must be looked up by index/enum value
// note they also delibrately have small indexes
std::vector<std::string> special_textures = {
        NAMED_TEXTURE_LIST
};

// these special textures will be placed first, as they are handled specially in the code, and must be looked up by index/enum value
// note they also delibrately have small indexes
std::vector<std::string> special_flats = {
        NAMED_FLAT_LIST
};

std::vector<std::string> vpatch_names = {
        VPATCH_LIST
};

typedef struct {
    const char *first;
    const char *last;
} anim_range_t;

const anim_range_t flat_animdefs[] =
{
    {FLAT_NAME(NUKAGE1), FLAT_NAME(NUKAGE3)},
    {FLAT_NAME(FWATER1), FLAT_NAME(FWATER4)},
    {FLAT_NAME(SWATER1), FLAT_NAME(SWATER4)},
    {FLAT_NAME(LAVA1),   FLAT_NAME(LAVA4)},
    {FLAT_NAME(BLOOD1),  FLAT_NAME(BLOOD3)},

    // DOOM II flat animations.
    { FLAT_NAME(RROCK05), FLAT_NAME(RROCK08) },
    { FLAT_NAME(SLIME01), FLAT_NAME(SLIME04) },
    { FLAT_NAME(SLIME05), FLAT_NAME(SLIME08) },
    { FLAT_NAME(SLIME09), FLAT_NAME(SLIME12) },
};

const anim_range_t tex_animdefs[] = {
    { TEXTURE_NAME(SLADRIP1), TEXTURE_NAME(SLADRIP3) },

    { TEXTURE_NAME(BLODGR1), TEXTURE_NAME(BLODGR4) },
    { TEXTURE_NAME(BLODRIP1), TEXTURE_NAME(BLODRIP4) },
    { TEXTURE_NAME(FIREWALA), TEXTURE_NAME(FIREWALL) },
    { TEXTURE_NAME(GSTFONT1), TEXTURE_NAME(GSTFONT3) },
    { TEXTURE_NAME(FIRELAV3), TEXTURE_NAME(FIRELAVA) },
    { TEXTURE_NAME(FIREMAG1), TEXTURE_NAME(FIREMAG3) },
    { TEXTURE_NAME(FIREBLU1), TEXTURE_NAME(FIREBLU2) },
    { TEXTURE_NAME(ROCKRED1), TEXTURE_NAME(ROCKRED3) },

    { TEXTURE_NAME(BFALL1), TEXTURE_NAME(BFALL4) },
    { TEXTURE_NAME(SFALL1), TEXTURE_NAME(SFALL4) },
    { TEXTURE_NAME(WFALL1), TEXTURE_NAME(WFALL4) },
    { TEXTURE_NAME(DBRAIN1), TEXTURE_NAME(DBRAIN4) },
};

void __attribute__((noreturn)) fail(const char *msg, ...) {
    va_list va;
    va_start(va, msg);
    vprintf(msg, va);
    va_end(va);
    printf("\n");
    exit(1);
}

struct texture {
    whdtexture_t whd{0};
};

struct texture_index {
    std::map<std::string, int> lookup;
    std::vector<texture> textures;

    int find(const std::string &name) const {
        if (name == "-") return 0;
        auto it = lookup.find(to_lower(name));
        if (it == lookup.end()) {
            fail("Unable to locate texture %s\n", name.c_str());
        }
        return it->second;
    }
};

extern "C" {
#include "adpcm-lib.h"
}

template<typename T>
T get_field(const std::vector<uint8_t> &data, int offset) {
    assert(data.begin() + offset <= data.end());
    return *(const T *) (data.data() + offset);
}

template<typename T>
void append_field(std::vector<uint8_t> &data, const T &field) {
    const uint8_t *p = (uint8_t *) &field;
    data.template insert(data.end(), p, p + sizeof(T));
}

template<typename T>
T get_field_inc(const std::vector<uint8_t> &data, int &offset) {
    assert(data.begin() + offset <= data.end());
    T rc = *(const T *) (data.data() + offset);
    offset += sizeof(rc);
    return rc;
}

static void usage() {
    throw std::invalid_argument("usage: whd_gen <wad_in> <whd_out>");
}

std::set<std::string> music_lumpnames = {
        "d_e1m1",
        "d_e1m2",
        "d_e1m3",
        "d_e1m4",
        "d_e1m5",
        "d_e1m6",
        "d_e1m7",
        "d_e1m8",
        "d_e1m9",
        "d_e2m1",
        "d_e2m2",
        "d_e2m3",
        "d_e2m4",
        "d_e2m5",
        "d_e2m6",
        "d_e2m7",
        "d_e2m8",
        "d_e2m9",
        "d_e3m1",
        "d_e3m2",
        "d_e3m3",
        "d_e3m4",
        "d_e3m5",
        "d_e3m6",
        "d_e3m7",
        "d_e3m8",
        "d_e3m9",
        "d_inter",
        "d_intro",
        "d_bunny",
        "d_victor",
        "d_introa",
        "d_runnin",
        "d_stalks",
        "d_countd",
        "d_betwee",
        "d_doom",
        "d_the_da",
        "d_shawn",
        "d_ddtblu",
        "d_in_cit",
        "d_dead",
        "d_stlks2",
        "d_theda2",
        "d_doom2",
        "d_ddtbl2",
        "d_runni2",
        "d_dead2",
        "d_stlks3",
        "d_romero",
        "d_shawn2",
        "d_messag",
        "d_count2",
        "d_ddtbl3",
        "d_ampie",
        "d_theda3",
        "d_adrian",
        "d_messg2",
        "d_romer2",
        "d_tense",
        "d_shawn3",
        "d_openin",
        "d_evil",
        "d_ultima",
        "d_read_m",
        "d_dm2ttl",
        "d_dm2int",
};

std::set<std::string> sfx_lumpnames = {
        "dspistol",
        "dsshotgn",
        "dssgcock",
        "dsdshtgn",
        "dsdbopn",
        "dsdbcls",
        "dsdbload",
        "dsplasma",
        "dsbfg",
        "dssawup",
        "dssawidl",
        "dssawful",
        "dssawhit",
        "dsrlaunc",
        "dsrxplod",
        "dsfirsht",
        "dsfirxpl",
        "dspstart",
        "dspstop",
        "dsdoropn",
        "dsdorcls",
        "dsstnmov",
        "dsswtchn",
        "dsswtchx",
        "dsplpain",
        "dsdmpain",
        "dspopain",
        "dsvipain",
        "dsmnpain",
        "dspepain",
        "dsslop",
        "dsitemup",
        "dswpnup",
        "dsoof",
        "dstelept",
        "dsposit1",
        "dsposit2",
        "dsposit3",
        "dsbgsit1",
        "dsbgsit2",
        "dssgtsit",
        "dscacsit",
        "dsbrssit",
        "dscybsit",
        "dsspisit",
        "dsbspsit",
        "dskntsit",
        "dsvilsit",
        "dsmansit",
        "dspesit",
        "dssklatk",
        "dssgtatk",
        "dsskepch",
        "dsvilatk",
        "dsclaw",
        "dsskeswg",
        "dspldeth",
        "dspdiehi",
        "dspodth1",
        "dspodth2",
        "dspodth3",
        "dsbgdth1",
        "dsbgdth2",
        "dssgtdth",
        "dscacdth",
        "dsskldth",
        "dsbrsdth",
        "dscybdth",
        "dsspidth",
        "dsbspdth",
        "dsvildth",
        "dskntdth",
        "dspedth",
        "dsskedth",
        "dsposact",
        "dsbgact",
        "dsdmact",
        "dsbspact",
        "dsbspwlk",
        "dsvilact",
        "dsnoway",
        "dsbarexp",
        "dspunch",
        "dshoof",
        "dsmetal",
        "dschgun", // this is a link, does it actually exist?
        "dstink",
        "dsbdopn",
        "dsbdcls",
        "dsitmbk",
        "dsflame",
        "dsflamst",
        "dsgetpow",
        "dsbospit",
        "dsboscub",
        "dsbossit",
        "dsbospn",
        "dsbosdth",
        "dsmanatk",
        "dsmandth",
        "dssssit",
        "dsssdth",
        "dskeenpn",
        "dskeendt",
        "dsskeact",
        "dsskesit",
        "dsskeatk",
        "dsradio",
};

bool convert_sound(std::pair<const int, lump> &e);

void dump_patch(const char *name, int num, lump &patch);

struct patch_header {
    short width;                // bounding box size
    short height;
    short leftoffset;        // pixels to the left of origin
    short topoffset;        // pixels below the origin
};

statsomizer patch_meta_size("Patch meta size");
statsomizer patch_decoder_size("Patch decoder size");
statsomizer patch_orig_meta_size("Patch orig meta size");
statsomizer patch_widths("Patch width");
statsomizer patch_heights("Patch height");
statsomizer patch_left_offsets("Patch left offset");
statsomizer patch_top_offsets("Patch top offset");
statsomizer vpatch_left_offsets("VPatch left offset");
statsomizer vpatch_top_offsets("VPatch top offset");
statsomizer vpatch_left_0offsets("VPatch left zero offset");
statsomizer vpatch_top_0offsets("VPatch top zero offset");
statsomizer patch_sizes("Patch size");
statsomizer patch_colors("Patch colors");
statsomizer patch_column_colors("Patch column colors");
statsomizer patch_columns_under_colors[] = {
        statsomizer("2 color post"),
        statsomizer("4 color post"),
        statsomizer("8 color post"),
        statsomizer("16 color post"),
        statsomizer("32 color post"),
        statsomizer("64 color post"),
        statsomizer("128 color post"),
        statsomizer("256 color post"),
};
statsomizer patch_post_counts("Patch column posts");
statsomizer patch_one("Patch one post columns");
statsomizer patch_all_post_counts(" Patcl total posts");
statsomizer patch_pixels("Patch total pixels");
statsomizer patch_col_hack_huff_pixels("Patch Col Hack huff size");
statsomizer patch_hack_huff_pixels("Patch Hack huff size");

statsomizer subsector_length("Subsector length");
//#define ZLIB_HEADER 1

statsomizer texture_col_metadata("Texture column metadata size");
statsomizer texture_transparent("Textures with transparency");
statsomizer texture_transparent_patch_count("Transparent tex patch count");
statsomizer texture_column_patches("Texture column patches");
statsomizer texture_column_patches1("Texture column 1 patch");
statsomizer texture_single_patch("Texture single patch");
statsomizer texture_single_patch00("Texture single patch 00");

statsomizer sector_lightlevel("Sector Lightlevel");
statsomizer sector_floorheight("Sector Floor Height");
statsomizer sector_ceilingheight("Sector Ceiling Height");
statsomizer sector_special("Sector Special num");
statsomizer color_runs("Color runs");
statsomizer same_columns("Same columns");

statsomizer sfx_orig_size("SFX original size");
statsomizer sfx_new_size("SFX compressed size");
statsomizer patch_orig_size("Patch original size");
statsomizer patch_new_size("Patch compressed size");
statsomizer vpatch_orig_size("VPatch original size");
statsomizer vpatch_new_size("VPatch compressed size");
statsomizer tex_orig_size("Tex original size");
statsomizer tex_new_size("Tex compressed size");

uint32_t hash;

void save_png(wad &wad, std::string prefix, std::string name, uint w, uint h, std::vector<int16_t> data, const std::vector<uint32_t> palette_colors) {
    lodepng::State state;
    lodepng_state_init(&state);
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8; // one byte per pixel still
    state.encoder.auto_convert = 0;
    // hack converted global
    std::vector<uint32_t> pixels(data.size());
    for (size_t i = 0; i < data.size(); i++) {
        if (data[i] < 0) pixels[i] = 0;
        else pixels[i] = palette_colors[data[i]];
    }
    std::vector<unsigned char> png_data;
    auto error = lodepng::encode(png_data, (byte *) pixels.data(), w, h, state);
    if (!error) {
        error = lodepng::save_file(png_data, std::string("output/").append(prefix).append("/").append(name).append(
                ".png").c_str());
    }
}

void save_png(wad &wad, std::string prefix, std::string name, uint w, uint h, std::vector<int16_t> data) {
    lump palette;
    wad.get_lump("playpal", palette);
    std::vector<uint32_t> palette_colors(256);
    for (int i = 0; i < 256; i++) {
        palette_colors[i] =
                0xff000000 | (palette.data[i * 3 + 2] << 16) | (palette.data[i * 3 + 1] << 8) | (palette.data[i * 3]);
    }
    save_png(wad, prefix, name, w, h, data, palette_colors);
}

std::vector<int16_t> unpack_patch(lump &patch) {
    const patch_header *ph = (const patch_header *) patch.data.data();
    auto pixels = std::vector<int16_t>(ph->width * ph->height, -1);
    for (int col = 0; col < ph->width; col++) {
        uint32_t col_offset = *(uint32_t *) (patch.data.data() + 8 + col * 4);
        const uint8_t *post = patch.data.data() + col_offset;
        while (post[0] != 0xff) {
            for (int y = 3; y < 3 + post[1]; y++) {
                size_t index = col + (post[0] + y - 3) * ph->width;
                assert(index < pixels.size());
                pixels[index] = post[y];
            }
            post += 4 + post[1];
        }
    }
    return pixels;
}

int opaque_pixels;
int transparent_pixels;
std::vector<std::vector<uint8_t>> to_merged_posts(const std::vector<int16_t>& pix, uint width, uint height, std::vector<int>& same, bool& have_same) {
    std::vector<std::vector<uint8_t>> merged_posts(width);
    same.clear();
    same.resize(width);
    have_same = false;
#if 1
    for(int i=1;i<(int)width;i++) {
        int match = 0;
        for(int j=0;j<i;j++) {
            int diff = 0;
            for(uint y=0;y<height && !diff;y++) {
                if (pix[i + y * width] != pix[j + y * width]) diff++;
            }
            if (!diff) {
                match = j+1;
                break;
            }
        }
        if (match) {
            same[i] = match;
            have_same = true;
            int non_transparent = 0;
            for(uint y=0;y<height;y++) {
                non_transparent += pix[i + y * width] >= 0;
                //                pix[i + y * ph.width] = 0xfc;
            }
            same_columns.record(non_transparent);
        }
    }
#endif
    for(int x=0;x<(int)width;x++) {
        if (!same[x]) {
            for(int y = 0; y < (int)(width * height); y += width) {
                if (pix[x+y]>=0) {
                    merged_posts[x].push_back(pix[x+y]);
                    opaque_pixels++;
                } else {
                    transparent_pixels++;
                }
            }
        }
    }
    return merged_posts;
}

template<typename BO, typename H> void output_min_max_c3(BO &bit_output, huffman_encoding<std::pair<bool,uint8_t>, H> &huff, bool zero_flag, bool group8) {
    if (huff.empty()) {
        assert(false); // should be handled at a higher level
        return;
    }
    uint8_t min = huff.get_first_symbol().second;
    uint8_t max = min;
    for(const auto &s : huff.get_symbols()) {
        if (!s.first) {
            max = std::max(max, s.second);
        }
    };
    bit_output->write(bit_sequence(min, 8));
    bit_output->write(bit_sequence(max, 8));
    int min_cl = huff.get_min_code_length();
    int max_cl = huff.get_max_code_length();
    assert(max_cl < 16);
    assert(min_cl <= max_cl);
    bit_output->write(bit_sequence(min_cl, 4));
    bit_output->write(bit_sequence(max_cl, 4));
    if (min_cl == max_cl && zero_flag) {
        for (int val = min; val <= max; val++) {
            int length = huff.get_code_length(std::make_pair(0,val));
            assert(!length || length == min_cl);
            bit_output->write(bit_sequence(length != 0, 1));
        }
    } else {
        uint bit_count = 32 - __builtin_clz(max_cl - min_cl + (zero_flag ? 0 : 1));
        auto stats = huff.get_stats();
        if (zero_flag) {
            if (group8) {
                for (uint base_val = min; base_val <= max; base_val += 8) {
                    uint8_t mask = 0;
                    uint8_t bits = 0;
                    for (uint i = 0; i <= std::min(7u, max - base_val); i++) {
                        if (huff.get_code_length(std::make_pair(0,base_val + i))) mask |= 1u << i;
                        bits |= 1u << i;

                    }
                    if (mask == 0) {
                        bit_output->write(bit_sequence(0b01, 2));
                    } else if (mask == bits) {
                        bit_output->write(bit_sequence(0b11, 2));
                        for (uint i = 0; i <= std::min(7u, max - base_val); i++) {
                            int length = huff.get_code_length(std::make_pair(0,base_val + i));
                            assert(length);
                            length -= min_cl;
                            bit_output->write(bit_sequence(length, bit_count));
                        }
                    } else {
                        bit_output->write(bit_sequence(0b0, 1));
                        for (uint i = 0; i <= std::min(7u, max - base_val); i++) {
                            int length = huff.get_code_length(std::make_pair(0,base_val + i));
                            bit_output->write(bit_sequence(length != 0, 1));
                            if (length) {
                                length -= min_cl;
                                bit_output->write(bit_sequence(length, bit_count));
                            }
                        }
                    }
                }
            } else {
                for (int val = min; val <= max; val++) {
                    int length = huff.get_code_length(std::make_pair(0,val));
                    bit_output->write(bit_sequence(length != 0, 1));
                    if (length) {
                        length -= min_cl;
                        bit_output->write(bit_sequence(length, bit_count));
                    }
                }
            }
        } else {
            for (int val = min; val <= max; val++) {
                int length = huff.get_code_length(std::make_pair(0,val));
                if (length) {
                    length -= min_cl;
                } else {
                    length = (1u << bit_count) - 1;
                }
                bit_output->write(bit_sequence(length, bit_count));
            }
        }
    }
}

template<typename BO, typename H>
void output_min_max_best_c3(BO &bit_output, huffman_encoding<std::pair<bool,uint8_t>, H> &huff) {
    bit_output->write(bit_sequence(!huff.empty(), 1));
    if (huff.empty()) return;
    auto bo1 = std::make_shared<byte_vector_bit_output>();
    output_min_max_c3(bo1, huff, true, true);
    auto bo2 = std::make_shared<byte_vector_bit_output>();
    output_min_max_c3(bo2, huff, true, false);
    auto bo3 = std::make_shared<byte_vector_bit_output>();
    output_min_max_c3(bo3, huff, false, false);
    //    printf("BO1/2/3 %d %d %d\n", (int) bo1->bit_size(), (int) bo2->bit_size(), (int) bo3->bit_size());
    size_t min = std::min(bo1->bit_size(), std::min(bo2->bit_size(), bo3->bit_size()));
    if (bo1->bit_size() == min) {
        bit_output->write(bit_sequence(1, 1));
        bo1->write_to(bit_output);
    } else if (bo2->bit_size() == min) {
        bit_output->write(bit_sequence(0, 1));
        bo2->write_to(bit_output);
    } else {
        //        printf("WA\n"); // now seems to happen, but rare so not sure we care enough to add complexity
        if (bo1->bit_size() < bo2->bit_size()) {
            bit_output->write(bit_sequence(1, 1));
            bo1->write_to(bit_output);
        } else {
            bit_output->write(bit_sequence(0, 1));
            bo2->write_to(bit_output);
        }
        //assert(false); // doesn't seem to be ever, plan to remove
        //        bit_output->write(bit_sequence(0b11, 2));
        //        bo3->write_to(bit_output);
    }
}

template<typename H>
uint decoder_size(huffman_encoding<uint8_t, H> &huff) {
    return th_decoder_size(huff.get_symbols().size(), huff.get_max_code_length());
}

template<typename BO, typename H>
void output_raw_pixels(BO &bit_output, huffman_encoding<uint8_t, H> &huff) {
    if (huff.empty()) {
        assert(false); // should be handled at a higher level
        return;
    }
    auto stats = huff.get_stats();
    int min_cl = huff.get_min_code_length();
    int max_cl = huff.get_max_code_length();
    assert(max_cl < 16);
    assert(min_cl <= max_cl);
    bit_output->write(bit_sequence(min_cl, 4));
    bit_output->write(bit_sequence(max_cl, 4));
    int bit_count = 32 - __builtin_clz(max_cl - min_cl);
    std::vector<uint8_t> set(32);
    for(const auto &e : huff.get_symbols()) {
        set[e>>3] |= 1u << (e&7u);
    }

    for (uint p = 0; p < 32; p++) {
        bit_output->write(bit_sequence(set[p]!=0, 1));
        if (set[p]!=0) {
            for (uint bit = 0; bit < 8; bit++) {
                if (set[p] & (1u << bit)) {
                    bit_output->write(bit_sequence(1, 1));
                    auto length = huff.get_code_length(p*8 + bit);
                    assert(!max_cl || length);
                    if (min_cl == max_cl) {
                        assert(length == min_cl);
                    } else {
                        bit_output->write(bit_sequence(length - min_cl, bit_count));
                    }
                } else {
                    bit_output->write(bit_sequence(0, 1));
                }
            }
        }
    }
}

template<typename BO, typename H>
void output_raw_pixels_c3(BO &bit_output, huffman_encoding<std::pair<bool,uint8_t>, H> &huff) {
    if (huff.empty()) {
        assert(false); // should be handled at a higher level
        return;
    }
    auto stats = huff.get_stats();
    int min_cl = huff.get_min_code_length();
    int max_cl = huff.get_max_code_length();
    assert(max_cl < 16);
    assert(min_cl <= max_cl);
    bit_output->write(bit_sequence(min_cl, 4));
    bit_output->write(bit_sequence(max_cl, 4));
    int bit_count = 32 - __builtin_clz(max_cl - min_cl);
    std::vector<uint8_t> set(32);
    for(const auto &e : huff.get_symbols()) {
        if (!e.first) set[e.second>>3] |= 1u << (e.second&7u);
    }

    for (uint p = 0; p < 32; p++) {
        bit_output->write(bit_sequence(set[p]!=0, 1));
        if (set[p]!=0) {
            for (uint bit = 0; bit < 8; bit++) {
                if (set[p] & (1u << bit)) {
                    bit_output->write(bit_sequence(1, 1));
                    auto length = huff.get_code_length(std::make_pair(0,p*8 + bit));
                    assert(!max_cl || length);
                    if (min_cl == max_cl) {
                        assert(length == min_cl);
                    } else {
                        bit_output->write(bit_sequence(length - min_cl, bit_count));
                    }
                } else {
                    bit_output->write(bit_sequence(0, 1));
                }
            }
        }
    }
}


statsomizer cp1_pixels("CP1 post pixels");
statsomizer cp1_run("CP1 run");
statsomizer cp1_raw_run("CP1 raw run");
statsomizer cp1_size("CP1 size");
statsomizer cp2_size("CP2 size");
statsomizer cp_wtf_size("CP WTF size");
statsomizer cp_po_size("CP POP size");
statsomizer cp_size("CP size");
template<typename T, typename S>
std::ostream &operator<<(std::ostream &os, const std::pair<T, S> &v) {
    os << "(" << static_cast<typename symbol_adapter<uint8_t>::ostream_type>(v.first) << ", " << static_cast<typename symbol_adapter<uint8_t>::ostream_type>(v.second) << ")";
    return os;
}

uint consider_compress3(const std::string& name, const std::vector<std::vector<uint8_t>>& posts,
                        std::shared_ptr<byte_vector_bit_output>& decoder_output, std::vector<std::shared_ptr<byte_vector_bit_output>>& zposts,
                        uint width, uint height, uint& decoder_size) {
    //    enum {
    //        cp1_raw = 0,
    //        cp1_delta3 = 1,
    //        };
    symbol_sink<huffman_params<std::pair<bool,uint8_t>>> sink("pixel/delta");

    zposts.clear();
    zposts.resize(width);
    decoder_output = std::make_shared<byte_vector_bit_output>();
    int last_color = -1;
    int color_change_count = 0;
    for(int pass=0;pass<2;pass++) {
//        printf("BEGIN %s %d\n", name.c_str(), pass);
        for(int x=0;x<(int)width;x++) {
            const auto& post = posts[x];
            if (pass) {
                zposts[x] = std::make_shared<byte_vector_bit_output>();
                sink.begin_output(zposts[x]);
            }
            if (post.empty()) continue;
            sink.output(std::make_pair(false, post[0]));
            for(int y=1;y<(int)post.size();) {
                int d1 = post[y]-post[y-1];
                int code = -1;
                if (abs(d1) < 4) {
                        code = d1 + 3;
                    }
                if (code == -1) {
                    if (last_color != post[y]) {
                        last_color = post[y];
                        color_change_count++;
                    }
                    sink.output(std::make_pair(false, post[y])); y++;
                } else if (code < 7) {
                    sink.output(std::make_pair(true, code)); y++;
                } else {
                    assert(false);
                }
            }
        }
        if (!pass) {
            if (color_change_count == 0) { // very pointless but CYAN in doom2 is this
                // this is simpler than adding special case in the decode
                printf("warning: zero colors for compression, adding some dummies\n");
                sink.output(std::make_pair(false, 0));
                sink.output(std::make_pair(false, 1));
            } else if (color_change_count == 1) {
                // this is simpler than adding special case in the decode
                printf("warning: only one color for compression, adding a dummy\n");
                sink.output(std::make_pair(false, (uint8_t)(last_color + 1)));
            }

            sink.begin_output(decoder_output);
            output_raw_pixels_c3(decoder_output, sink.huff);
            uint min_cl = sink.huff.get_min_code_length();
            uint bit_count = 32 - __builtin_clz(1 + sink.huff.get_max_code_length() - sink.huff.get_min_code_length());
            for(uint8_t i=0;i<7;i++) {
                uint len = sink.huff.get_code_length(std::make_pair(1, i));
                if (len) {
                    decoder_output->write(bit_sequence(len-min_cl, bit_count));
                } else {
                    decoder_output->write(bit_sequence((1u << bit_count) -1, bit_count));
                }
            }
        }
    }

    auto check = std::make_shared<byte_vector_bit_output>();
    decoder_output->write_to(check);
    for(auto &col_bo : zposts) {
        col_bo->write_to(check);
    }
    auto result = check->get_output();
#if 1 // must be 1 now as we need to set decoder_size
    byte_vector_bit_input biv(result);
    auto bi = create_bip(biv);

    uint16_t buf[512];
    uint8_t tmp[1024];
    uint16_t *pos = buf;
    uint pos_size = count_of(buf);
    uint16_t *decoder = pos;
    pos = read_raw_pixels_decoder_c3(bi, pos, pos_size, tmp, count_of(tmp));
    decoder_size = pos - buf;
    pos_size -= (pos - buf);
    assert(pos < buf + count_of(buf));
    std::vector<std::vector<uint8_t>> decoded(width);
    for(int x=0;x<(int)width;x++) {
        auto& post = decoded[x];
        while (post.size() < posts[x].size()) {
            uint16_t p = th_decode_16(decoder, bi);
            if (p < 256) {
                post.push_back(p);
            } else {
                int prev = post.size() - 1;
                assert(prev>=0);
                assert(1 == p >> 8);
                p &= 0xff;
                assert(p<7);
                post.push_back(post[prev] + p - 3);
            }
        }
        if (!std::equal(post.begin(), post.end(), posts[x].begin(), posts[x].end())) {
            if (post.size() == posts[x].size()) {
                for(int i=0;i<(int)post.size();i++) {
                    printf("%d %02x %02x %c\n", i, posts[x][i], post[i], posts[x][i] != post[i] ? '*' : ' ');
                }
            }
            fail("Post mismatcher %d %d vs %d\n", x, (int)post.size(), (int)posts[x].size());
        }
    }
    uncreate_bip(biv, bi);
#endif

    cp1_size.record(result.size());
    return result.size();
}

uint consider_compress_pixels_only(const std::string& name, const std::vector<std::vector<uint8_t>>& posts,
                                   std::shared_ptr<byte_vector_bit_output>& decoder_output, std::vector<std::shared_ptr<byte_vector_bit_output>>& zposts, uint width, uint height, uint& decoder_size_out) {
    symbol_sink<huffman_params<uint8_t>> raw_pixel_sink("Raw Pixel");
    sink_wrappers<std::shared_ptr<byte_vector_bit_output>> wrappers{raw_pixel_sink};

    zposts.clear();
    zposts.resize(width);
    decoder_output = std::make_shared<byte_vector_bit_output>();
    int last_color = -1;
    int color_change_count = 0;
    for(int pass=0;pass<2;pass++) {
        for(int x=0;x<(int)width;x++) {
            const auto& post = posts[x];
            if (pass) {
                zposts[x] = std::make_shared<byte_vector_bit_output>();
                wrappers.begin_output(zposts[x]);
            } else {
                cp1_pixels.record(post.size());
            }
            for(uint y=0;y<post.size();y++) {
                if (last_color != post[y]) {
                    last_color = post[y];
                    color_change_count++;
                }
                raw_pixel_sink.output(post[y]);
            }
        }
        if (!pass) {
            if (color_change_count == 0) { // very pointless but CYAN in doom2 is this
                // this is simpler than adding special case in the decode
                printf("warning: zero colors for compression, adding some dummies");
                raw_pixel_sink.output(0);
                raw_pixel_sink.output(1);
            } else if (color_change_count == 1) {
                // this is simpler than adding special case in the decode
                printf("warning: only one color for compression, adding a dummy");
                raw_pixel_sink.output(last_color + 1);
            }
            wrappers.begin_output(decoder_output);
            auto bo2 = std::make_shared<byte_vector_bit_output>();
            output_raw_pixels(bo2, raw_pixel_sink.huff);
            auto bo3 = std::make_shared<byte_vector_bit_output>();
            output_min_max_best(bo3, raw_pixel_sink.huff);
            //printf( "      bo2 %u bo3 %u\n", bo2->bit_size(), bo3->bit_size());
            if (bo2->bit_size() < bo3->bit_size()) {
                decoder_output->write(bit_sequence(0, 1));
                bo2->write_to(decoder_output);
            } else {
                decoder_output->write(bit_sequence(1, 1));
                bo3->write_to(decoder_output);
            }
        }
    }

    auto check = std::make_shared<byte_vector_bit_output>();
    decoder_output->write_to(check);
    for(auto &col_bo : zposts) {
        col_bo->write_to(check);
    }
    auto result = check->get_output();
    cp_po_size.record(result.size());
#if 1 // must be 1 now as we set have to set decoder_size
    byte_vector_bit_input biv(result);
    auto bi = create_bip(biv);

    uint16_t buf[512];
    uint8_t tmp[512];
    uint16_t *pos = buf;
    uint pos_size = count_of(buf);
    uint16_t *rp_decoder = buf;
    if (th_bit(bi)) {
        pos = th_read_simple_decoder(bi, pos, pos_size, tmp, count_of(tmp));
    } else {
        pos = read_raw_pixels_decoder(bi, pos, pos_size, tmp, count_of(tmp));
    }
    decoder_size_out = pos - buf;
    th_make_prefix_length_table(rp_decoder, tmp);
    assert(pos < buf + count_of(buf));
    std::vector<std::vector<uint8_t>> decoded(width);
    for(int x=0;x<(int)width;x++) {
        auto& post = decoded[x];
        for(int i=0;i<(int)posts[x].size();i++) {
//            uint8_t pix = th_decode(rp_decoder, bi);
            uint8_t pix = th_decode_table_special(rp_decoder, tmp, bi);
            post.push_back(pix);
        }
        if (!std::equal(post.begin(), post.end(), posts[x].begin(), posts[x].end())) {
            printf("Post mismatcher %d %d vs %d\n", x, (int)post.size(), (int)posts[x].size());
            if (post.size() == posts[x].size()) {
                for(int i=0;i<(int)post.size();i++) {
                    printf("%d %02x %02x %c\n", i, posts[x][i], post[i], posts[x][i] != post[i] ? '*' : ' ');
                }
            }
        }
    }
    uncreate_bip(biv, bi);
#endif

    return result.size();
}

uint consider_compress_data(const std::string& name, std::vector<uint8_t>& input,
                                   std::shared_ptr<byte_vector_bit_output>& output) {
    symbol_sink<huffman_params<uint8_t>> byte_sink("Raw Data");
    sink_wrappers<std::shared_ptr<byte_vector_bit_output>> wrappers{byte_sink};

    output = std::make_shared<byte_vector_bit_output>();
    for(int pass=0;pass<2;pass++) {
        for(const auto & b : input) {
            byte_sink.output(b);
        }
        if (!pass) {
            wrappers.begin_output(output);
            auto bo2 = std::make_shared<byte_vector_bit_output>();
            output_raw_pixels(bo2, byte_sink.huff);
            auto bo3 = std::make_shared<byte_vector_bit_output>();
            output_min_max_best(bo3, byte_sink.huff);
            if (bo2->bit_size() < bo3->bit_size()) {
                output->write(bit_sequence(0, 1));
                bo2->write_to(output);
            } else {
                output->write(bit_sequence(1, 1));
                bo3->write_to(output);
            }
        }
    }

    auto check = std::make_shared<byte_vector_bit_output>();
    output->write_to(check);
    auto result = check->get_output();
    cp_po_size.record(result.size());
#if 1
    byte_vector_bit_input biv(result);
    auto bi = create_bip(biv);

    uint16_t buf[512];
    uint8_t tmp[512];
    uint16_t *pos = buf;
    uint pos_size = count_of(buf);
    uint16_t *rp_decoder = buf;
    if (th_bit(bi)) {
        pos = th_read_simple_decoder(bi, pos, pos_size, tmp, count_of(tmp));
    } else {
        pos = read_raw_pixels_decoder(bi, pos, pos_size, tmp, count_of(tmp));
    }
    th_make_prefix_length_table(rp_decoder, tmp);
    assert(pos < buf + count_of(buf));
    std::vector<uint8_t> decoded;
    for(int i=0;i<(int)input.size();i++) {
        uint8_t pix = th_decode_table_special(rp_decoder, tmp, bi);
        decoded.push_back(pix);
    }
    if (!std::equal(decoded.begin(), decoded.end(), input.begin(), input.end())) {
        printf("Post mismatcher  %d vs %d\n", (int)decoded.size(), (int)input.size());
        if (decoded.size() == input.size()) {
            for(int i=0; i < (int)decoded.size(); i++) {
                printf("%d %02x %02x %c\n", i, input[i], decoded[i], input[i] != decoded[i] ? '*' : ' ');
            }
        }
        fail("post mismatched");
    }
    uncreate_bip(biv, bi);
#endif

    return result.size();
}

void patch_for_each_pixel(lump &patch, std::function<void(uint8_t)> callback) {
    const patch_header *ph = (const patch_header *) patch.data.data();
    for (int col = 0; col < ph->width; col++) {
        uint32_t col_offset = *(uint32_t *) (patch.data.data() + 8 + col * 4);
        const uint8_t *post = patch.data.data() + col_offset;
        while (post[0] != 0xff) {
            for (int y = 3; y < 3 + post[1]; y++) {
                callback(post[y]);
            }
            post += 4 + post[1];
        }
    }
}

symbol_stats<uint16_t> patch_run_stats;
symbol_stats<uint16_t> patch_width_stats;
symbol_stats<uint16_t> patch_height_stats;

const uint8_t bitcount8_table[256] = {
        0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
        };

void convert_patch(wad &wad, int num, lump &patch) {
    dump_patch(patch.name.c_str(), num, patch);
    auto ph = get_field<patch_header>(patch.data, 0);
    auto pix = unpack_patch(patch);
    converted_patch_count++;
    assert(ph.height < 256);
#if DEBUG_SAVE_PNG
    save_png(wad, "raw", patch.name, ph.width, ph.height, pix);
#endif
#if 0
    const int bigoff = 6;
    for(auto& p : pix) {
        if (p >=0) {
            switch (p >> 4) {
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                    p = (p + 0x10) & 0xe0;
                    break;
                case 7:
                case 8:
                case 11:
                case 12:
                case 13:
                    p = (p & 0xf0) + bigoff;
                    break;
                case 9:
                case 10:
                    p = (p&0xf8) + 3;
                    break;
                case 15:
                    if (p < 0xf8) p = 0xc0 + bigoff;
                    else p = 0xfb;
                    break;
                default:
                    p = 0xfb;
            }
        }
    }
#endif
#if 0
    auto pix_at = [&](int x, int y) { return pix[x + y  * ph.width]; };
#define MIN_RUN 3
    for(int i=0;i<ph.width;i++) {
        std::vector<int> lengths(ph.height);
        std::vector<int> deltas(ph.height);
//#define COLORING 1 // constant rate
//#define COLORING 2 // flat
//#define COLORING 3 // small delta
//#define COLORING 4 // larger delta
#define COLORING 5 // largest delta
#if COLORING == 1
        for(int y=0;y<ph.height-1;y++) {
            if (pix_at(i, y) < 0) continue; // transparent
            int delta = pix_at(i, y+1) - pix_at(i, y);
            int y1 = y+1;
            for(;pix_at(i, y1-1) + delta == pix_at(i, y1) && y1<ph.height;y1++);
            deltas[y] = delta;
            lengths[y] = y1 - y;
        }
#elif COLORING == 2
        for(int y=0;y<ph.height-1;y++) {
            if (pix_at(i, y) < 0) continue; // transparent
            int y1 = y+1;
            for(;y1<ph.height && pix_at(i, y1-1) == pix_at(i, y1);y1++);
            deltas[y] = 0;
            lengths[y] = y1 - y;
        }
#elif COLORING == 3
for(int y=0;y<ph.height-1;y++) {
            if (pix_at(i, y) < 0) continue; // transparent
            int y1 = y+1;
            for(;y1<ph.height && abs(pix_at(i, y1-1) - pix_at(i, y1))<=1;y1++);
            deltas[y] = 0;
            lengths[y] = y1 - y;
        }
#elif COLORING == 4
for(int y=0;y<ph.height-1;y++) {
            if (pix_at(i, y) < 0) continue; // transparent
            int y1 = y+1;
            for(;y1<ph.height && abs(pix_at(i, y1-1) - pix_at(i, y1))<=2;y1++);
            deltas[y] = 0;
            lengths[y] = y1 - y;
        }
#elif COLORING == 5
        for(int y=0;y<ph.height-1;y++) {
            if (pix_at(i, y) < 0) continue; // transparent
            int y1 = y+1;
            for(;y1<ph.height && (abs(pix_at(i, y1-1) - pix_at(i, y1))<=3 || pix_at(i, y1) < 0);y1++);
            deltas[y] = 0;
            lengths[y] = y1 - y;
        }
#else
#error no
#endif
        for(int y=0;y<ph.height;y++) {
            for(int y1 = y+1; y1<ph.height;y1++) {
                if (y + lengths[y] > y1 && lengths[y1]) {
                    if (lengths[y] < lengths[y1]) {
                        lengths[y] = y1 - y;
                    } else {
                        lengths[y1] = 0;
                    }
                }
            }
        }
        for(int y=0;y<ph.height;y++) {
            if (lengths[y] >= MIN_RUN) {
                int length = lengths[y];
                for(int y1 = y; y1 < y + lengths[y]; y1++) {
                    if (pix[i + y1  * ph.width] < 0) {
                        length--;
                    }
                }
                if (length >= MIN_RUN) {
                    int col;
#if COLORING == 1
                    if (deltas[y] == 0) col = 0xfb;
                    else if (deltas[y] < 0) col = 0xf9;
                    else col = 0xc3;
#else
                    col = 0xfb;
#endif
                    for(int y1 = y; y1 < y + lengths[y]; y1++) {
                        if (pix[i + y1  * ph.width] >= 0) {
                            pix[i + y1  * ph.width] = col;
                            same_columns++;
                        }
                    }
                    same_columns--;
                    pix[i + y  * ph.width] = 0xf9;
                    color_runs.record(length);
                }
            }
        }
    }
#endif
#if DEBUG_SAVE_PNG
    save_png(wad, "flat", patch.name, ph.width, ph.height, pix);
#endif
    int choice = 0;
    uint best = std::numeric_limits<uint>::max();
    std::vector<std::shared_ptr<byte_vector_bit_output>> zposts;
    std::shared_ptr<byte_vector_bit_output> decoder_output;
    std::vector<std::shared_ptr<byte_vector_bit_output>> best_zposts;
    std::shared_ptr<byte_vector_bit_output> best_decoder_output;
    uint decoder_size;
    uint best_decoder_size;
    auto choose = [&](int c, uint size) {
        if (size < best) {
            choice = c;
            best = size;
            best_zposts = zposts;
            best_decoder_output = std::make_shared<byte_vector_bit_output>(*decoder_output);
            best_decoder_size = decoder_size;
        }
    };
    std::vector<int> same;
    bool have_same;
    auto posts = to_merged_posts(pix, ph.width, ph.height, same, have_same);
    // i think 0 and 3 may be fine on their own
    choose(0, consider_compress_pixels_only(patch.name, posts, decoder_output, zposts, ph.width, ph.height, decoder_size));
#if !USE_PIXELS_ONLY_PATCH
    choose(1, consider_compress3(patch.name, posts, decoder_output, zposts, ph.width, ph.height, decoder_size));
    //choose(2, consider_compress_wtf(patch.name, posts, decoder_output, zposts, ph.width, ph.height));
    // todo put this back if we need 5K
//    choose(3, consider_compress1(patch.name, posts, ph.width, ph.height, 2, 8));
#endif
    winners[choice]++;
    cp_size.record(best);
    converted_patch_size += ph.width * ph.height;

    // ------------------
    // start with metadata without post pixels
    int full_or_same_column_count = 0;
    uint orig_meta_size = 0;
    for (int col = 0; col < ph.width; col++) {
        uint32_t col_offset = *(uint32_t *) (patch.data.data() + 8 + col * 4);
        const uint8_t *post = patch.data.data() + col_offset;
        int post_count = 0;
        int last = 0;
        orig_meta_size++;
        while (post[0] != 0xff) {
            patch_run_stats.add(ph.height - last);
            patch_run_stats.add(ph.height - post[0]);
            last = post[0] + post[1];
            orig_meta_size+=2;

            post += 4 + post[1];
            post_count++;
        }
        if (last != ph.height) {
            assert( last < ph.height);
            patch_run_stats.add(ph.height - last);
        }
        if ((int)posts[col].size() == ph.height || same[col]) {
            full_or_same_column_count++;
        }
    }

    uint16_t w = ph.width;
    uint16_t h = ph.height;
    uint16_t lo = ph.leftoffset;
    uint16_t to = ph.topoffset;
    bool extra = (lo>>8) || (to >> 8);
    bool fully_opaque = full_or_same_column_count == ph.width;
    uint8_t flags = extra;
    flags |= fully_opaque << 1;

    std::vector<int> col_offsets;
    auto bitwidth = [](int v) {
        assert(v>=0 && v<256);
        return bitcount8_table[v];
    };

    auto write_meta = [&](bool bit_aligned) {
        auto bo = byte_vector_bit_output();
        col_offsets.clear();
        col_offsets.resize(ph.width+1);
        for(int x=0;x<ph.width;x++) {
            if (same[x]) {
                int same_col = same[x]-1;
                assert(same_col < x);
                // mark this specially as a same col - we used to just use the same_col target's col_offset,
                // however that breaks our ability to use the next columns start as our data end (we wouldn't be
                // able to determine same_col from col_offset[same_col] at runtime to get col_offset[same_col+1]).
                col_offsets[x] = -same[x];
                continue;
            } else {
                col_offsets[x] = bo.bit_size() / (bit_aligned ? 1 : 8);
            }

            // write the column data first
            best_zposts[x]->write_to(bo);

            // column post metadata is afterwards and will be reversed
            if (!fully_opaque) {
                std::vector<bit_sequence> col_metadata;
                uint32_t col_offset = *(uint32_t *) (patch.data.data() + 8 + x * 4);
                const uint8_t *post = patch.data.data() + col_offset;
                int last = 0;
                while (last < ph.height) {
                    int run = post[0] - last;
                    if (post[0] == 0xff) run = ph.height - last;
                    if (run == 0 && last != 0) {
                        // see #if/todo below ...
//                        assert(post[0] == 0xff);
                        if (post[0]==0xff) break;
                    }
                    patch_run_stats.add(run);
                    col_metadata.emplace_back(run, bitwidth(ph.height-last)); // todo could shrink range by 1 after the first - doubt it makes much difference
                    last += run;
                    if (last >= ph.height) break;
                    run = post[1];
                    // todo disabled for now as code might care
#if 0
                    // seems like there is a post limit of 128... we can remove that
                    if (post[4 + post[1]] != 0xff && post[4 + post[1]] == last + run) {
                        run += post[5 + post[1]];
                        assert(run < 256);
                        post += 4 + post[1];
                    }
#endif
                    assert(run>0);
                    col_metadata.emplace_back(run, bitwidth(ph.height-last));
                    last += run;
                    post += 4 + post[1];
                }
                assert(last == ph.height);
                int metadata_bits = 0;
                for(const auto &bs : col_metadata) metadata_bits += bs.length();
                if (!bit_aligned) {
                    // we need the reverse the bo_col_meta to end on an 8 bit boundary, so we may need to pad in the middle
                    bo.write(bit_sequence(0, (8 - bo.bit_size() - metadata_bits) & 7));
                }
                // write the metadata backwards
                for(int i=col_metadata.size()-1;i>=0;i--) {
                    bo.write(col_metadata[i]);
                }
            } else {
                if (!bit_aligned) bo.pad_to_byte();
            }
        }
        if (!bit_aligned) {
            assert(!bo.bit_index()); // should already be aligned
        }
        col_offsets[ph.width] = bo.bit_size() / (bit_aligned ? 1 : 8);
        return bo;
    };
    auto metadata = write_meta(true);
    if (metadata.bit_size() >= 0xff00) {
        // have to do it byte aligned
        metadata = write_meta(false);
        flags |= 4;
    } else {
        bit_addressable_patch++;
    }
//    patch_meta_size.record(6 + 2 * ph.width + (extra?2:0) + metadata.bit_size()/8);
    patch_meta_size.record(6 + 2 * ph.width + (extra?2:0) + metadata.bit_size()/8);
    patch_decoder_size.record(best_decoder_output->bit_size());
    patch_orig_meta_size.record(8 + 2 * ph.width + orig_meta_size);

    std::vector<uint8_t> p2;
    // we want multiples of 2, so we do 2 byte width (actually use upper 7 bits for decoder size) and assume height is always <256
    p2.push_back(flags);
    p2.push_back(w & 0xff);
    if ((best_decoder_size>>2) > 127) {
        fail("decoder size too big");
    }
    p2.push_back((w >> 8) | ((best_decoder_size>>2)<<1));
    p2.push_back(h & 0xff);
    p2.push_back(lo & 0xff);
    p2.push_back(to & 0xff);
    if (extra) {
        p2.push_back(lo >> 8);
        p2.push_back(to >> 8);
    }
    byte_vector_bit_output prefixed_decoder;
    assert(choice < 2);
    prefixed_decoder.write(bit_sequence(choice, 1));
    best_decoder_output->write_to(prefixed_decoder);
    auto decoder = prefixed_decoder.get_output();
    assert(decoder.size() < 512);
    p2.push_back(decoder.size()/2+1);
    p2.insert(p2.end(), decoder.begin(), decoder.end());
    if (!(decoder.size() & 1)) p2.push_back(0);
    assert(!(p2.size() & 1));

    assert((int)col_offsets.size() == ph.width+1);
    for(const auto &co : col_offsets) {
        assert(co < 0xff00);
        if (co < 0) {
            p2.push_back((-co - 1) & 0xff);
            p2.push_back(0xff);
        } else {
            p2.push_back(co & 0xff);
            p2.push_back(co >> 8);
        }
    }
    auto meta = metadata.get_output();
    p2.insert(p2.end(), meta.begin(), meta.end());
#if 0
    symbol_stats<uint8_t> pixel_stats;
    patch_for_each_pixel(patch, [&](uint8_t p) {
        pixel_stats.add(p);
    });
    auto pixel_huffman = pixel_stats.create_huffman_encoding();
    byte_vector_bit_output output;
    patch_for_each_pixel(patch, [&](uint8_t p) {
        output.write(pixel_huffman.encode(p));
    });
    auto c2 = output.get_output();
    p2.insert(p2.end(), c2.begin(), c2.end());
#endif
    printf("      encoding %d %d->%d ds %d\n", choice, (int)patch.data.size(), (int)p2.size(), best_decoder_size);
    patch_orig_size.record(patch.data.size());
    patch.data = p2;
    patch_new_size.record(patch.data.size());
    wad.update_lump(patch);
    compressed.insert(num);
    touched[num] = TOUCHED_PATCH;
}

// use_runs = true to do runs of pixels, false to use 0 as transparent color
void convert_vpatch(wad &wad, lump &patch, int max_colors, bool use_runs, std::set<int> colors, int shared_palette_handle, bool first) {
    touched[patch.num] = TOUCHED_VPATCH;
    compressed.insert(patch.num);
    dump_patch(patch.name.c_str(), patch.num, patch);
    auto ph = get_field<patch_header>(patch.data, 0);
    auto pix = unpack_patch(patch);
    if (colors.empty()) {
        for (const auto &p: pix) {
            colors.insert(p);
        }
    }
    lump palette;
    wad.get_lump("playpal", palette);

    // note in the case of shared_palettes we are doing the same color reduction work for every patch, but it is idempotent, and it was easier
    // than extracting/refactoring the code!!!

    if (use_runs && colors.find(-1) != colors.end()) max_colors++;
    // this only saves 500 bytes (trying 4 color textures)
//    if (max_colors == 16 || max_colors == 17) {
//        if (colors.size() <= max_colors - 12) {
//            max_colors -= 12;
//        }
//    }
    while ((int)colors.size() > max_colors) {
//        printf("Colors now:\n");
//        for(const auto &c : colors) {
//            if (c == -1) printf("  %d transparent\n", c);
//            else printf("  %d %02x,%02x,%02x\n", c,
//                   palette.data[c*3],
//                   palette.data[c*3+1],
//                   palette.data[c*3+2]);
//        }
        float lowest_score = INFINITY;
        int lowest_c0 = 0;
        int lowest_c1 = 0;
        for (int c0: colors) {
            if (c0 == -1) continue;
            for (int c1: colors) {
                if (c1 <= c0) continue;
                float dr = ((float) palette.data[c0 * 3]) - ((float) palette.data[c1 * 3]);
                float dg = ((float) palette.data[c0 * 3 + 1]) - ((float) palette.data[c1 * 3 + 1]);
                float db = ((float) palette.data[c0 * 3 + 2]) - ((float) palette.data[c1 * 3 + 2]);
                // todo we can improve heuristic
                float score = dr * dr + dg * dg + db * db;
                if (score < lowest_score) {
                    lowest_score = score;
                    lowest_c0 = c0;
                    lowest_c1 = c1;
//                    printf("potential replacement %d %02x,%02x,%02x with %d %02x,%02x,%02x %f\n",
//                           c0,
//                           palette.data[c0*3],
//                           palette.data[c0*3+1],
//                           palette.data[c0*3+2],
//                           c1,
//                           palette.data[c1*3],
//                           palette.data[c1*3+1],
//                           palette.data[c1*3+2],
//                           score);
                }
            }
        }
        // todo we should pick best of c0, c1, or another color from the palette.
        //  ACTUALLY - tried this, but it produces undesirable results... breaks stylized looks of icons, and turns status bar non grey
        int replacement = lowest_c1;
//        printf("replacing %d %02x,%02x,%02x with %d %02x,%02x,%02x\n",
//               lowest_c0,
//               palette.data[lowest_c0*3],
//               palette.data[lowest_c0*3+1],
//               palette.data[lowest_c0*3+2],
//               replacement,
//               palette.data[replacement*3],
//               palette.data[replacement*3+1],
//               palette.data[replacement*3+2]);
        for (auto &p: pix) {
            if (p == lowest_c0) p = replacement;
        }
        colors.erase(lowest_c0);
    }
    std::map<int, int> color_mapping;
    for (const auto &c: colors) {
        color_mapping[c] = color_mapping.size();
    }
    for (auto &p: pix) {
        p = color_mapping[p];
    }
#if DEBUG_SAVE_PNG
    std::vector<uint32_t> pal;
    for (const auto &c: colors) {
        uint32_t argb;
        if (c == -1) argb = 0;
        else
            argb = 0xff000000 | (palette.data[c * 3 + 2] << 16) | (palette.data[c * 3 + 1] << 8) |
                   (palette.data[c * 3]);
        pal.push_back(argb);
    }
    std::string prefix = "misc/";
    if (shared_palette_handle == -1) {
        prefix += "unshared";
    } else {
        prefix += std::to_string(shared_palette_handle);
    }
    save_png(wad, prefix, patch.name.c_str(), ph.width, ph.height, pix, pal);
#endif
    int orig_size = patch.data.size();
    vpatch_orig_size.record(orig_size);

    patch.data.clear();
    bool has_transparent = colors.find(-1) != colors.end();
    if (use_runs && has_transparent) {
        for (auto &p: pix) {
            p--;
        }
        colors.erase(-1);
    }
    patch.data.push_back(ph.width);
    patch.data.push_back(ph.height);
    if (shared_palette_handle==-1 || first) {
        patch.data.push_back(colors.size());
    } else {
        patch.data.push_back(0); // not super efficient, but makes decode easier, and we are saving a palette altogether
    }
    uint bpp = 31 - __builtin_clz(max_colors);
    int vpt = -1;
    if (bpp == 4) {
        if (use_runs) vpt = vp4_runs;
        else if (has_transparent) vpt = vp4_alpha;
        else vpt = vp4_solid;
    } else if (bpp == 6) {
        if (use_runs) vpt = vp6_runs;
    } else if (bpp == 8) {
        if (use_runs) vpt = vp8_runs;
    }
    if (vpt == -1) fail("Unknown vpt type");
    patch.data.push_back((vpt<<2) | ((ph.width &0x100)>>7) | (shared_palette_handle>=0 ? 1:0));
    // todo shrink this some; it is often zero
    patch.data.push_back(ph.topoffset);
    patch.data.push_back(ph.leftoffset);
    if (shared_palette_handle == -1 || first) {
        for (const auto &c: colors) {
            if (use_runs && c == -1) continue;
            patch.data.push_back(c);
        }
    }
    if (shared_palette_handle >= 0) {
        patch.data.push_back(shared_palette_handle);
    }
    if (pix.size() & 1) pix.push_back(0);
    for(int i=0, y=0;y<ph.height;y++) {
        int x = 0;
        for (;x < ph.width; ) {
            int xend;
            if (use_runs) {
                xend = x;
                while (xend < ph.width && pix[i+xend] == -1) xend++;
                if (xend == ph.width) {
                    patch.data.push_back(0xff);
                    break;
                } else {
                    if (xend - x >= 255) fail("gap too big");
                    patch.data.push_back(xend - x);
                    x = xend;
                    while (xend < ph.width && pix[i + xend] != -1) xend++;
                    if (xend - x > 255) fail("run too big");
                    patch.data.push_back(xend - x);
                }
            } else {
                xend = ph.width;
            }
            uint accum = 0;
            uint bits = 0;
            for (;x < xend; x++) {
                accum |= pix[i+x] << bits;
                bits += bpp;
                if (bits >= 8) {
                    patch.data.push_back(accum);
                    accum >>= 8;
                    bits -= 8;
                }
            }
            if (bits) {
                patch.data.push_back(accum);
            }
        }
        i += ph.width;
    }
    vpatch_top_offsets.record(ph.topoffset);
    vpatch_left_offsets.record(ph.leftoffset);
    vpatch_top_0offsets.record(ph.topoffset == 0);
    vpatch_left_0offsets.record(ph.leftoffset == 0);
    printf("VPATCH %s %d->%d\n", patch.name.c_str(), orig_size, (int)patch.data.size());
    vpatch_new_size.record(patch.data.size());
    wad.update_lump(patch);
}

void add_patch_colors(lump& patch, std::set<int>& colors) {
    auto pix = unpack_patch(patch);
    for (const auto &p: pix) {
        colors.insert(p);
    }
}

void convert_vpatches(wad &wad, const std::vector<std::string>& names, int max_colors, bool use_runs, int shared_palette_handle = -1) {
    std::set<int> colors;
    if (shared_palette_handle >= 0) {
        for (const auto &cg: names) {
            lump l;
            int indexp1 = wad.get_lump(cg, l);
            if (indexp1) {
                add_patch_colors(l, colors);
            }
        }
    }
    bool first = true;
    for (const auto &cg: names) {
        lump l;
        int indexp1 = wad.get_lump(cg, l);
        if (indexp1) {
            convert_vpatch(wad, l, max_colors, use_runs, colors, shared_palette_handle, first);
            first = false;
        } else {
            printf("MISSING named patch %s\n", cg.c_str());
        }
    }
}

void convert_patches(wad &wad, std::string from_name, std::string to_name) {
    lump pnames;
    int start = wad.get_lump_index(from_name);
    int end = wad.get_lump_index(to_name);
    std::set<int> pname_patches;

    std::set<std::string> temp_hack;
    for (int num = start + 1; num < end; num++) {
        lump patch;
        if (wad.get_lump(num, patch)) {
            if (wad.get_lump_index(patch.name) != num) {
                // seems to be a duplicate patch, and not the right one
                patch.data.clear();
                wad.update_lump(patch);
                continue;
            }

            // for now remove the data
            //wad.remove_lump(name);

            convert_patch(wad, num, patch);
        } else {
            printf("  %d - not found\n", num);
        }
    }
}

void dump_patch(const char *name, int num, lump &patch) {
    static std::set<int> dumped;
    if (!dumped.insert(num).second) return;
    dumped_patch_count++;
#if 1
    const patch_header *ph = (const patch_header *)patch.data.data();
    patch_widths.record(ph->width);
    patch_heights.record(ph->height);
    patch_width_stats.add(ph->width);
    patch_height_stats.add(ph->height);
    patch_left_offsets.record(ph->leftoffset);
    patch_top_offsets.record(ph->topoffset);
    patch_sizes.record(ph->width * ph->height);
    std::set<uint8_t> patch_color_set;
    std::vector<uint8_t> patch_raw_pixels;
    std::vector<uint8_t> column_raw_pixels;
    int all_posts_count=0;
    for(int col=0;col<ph->width;col++) {
        column_raw_pixels.clear();
        uint32_t col_offset = *(uint32_t *)(patch.data.data() + 8 + col * 4);
        const uint8_t *post = patch.data.data() + col_offset;
        int post_count = 0;
        int pixel_count = 0;
        while (post[0] != 0xff) {
            std::set<uint8_t> post_color_set;
            post_count++;
            pixel_count += post[1];
            for(int y = 3; y < 3 + post[1]; y++) {
                post_color_set.insert(post[y]);
                patch_raw_pixels.push_back(post[y]);
                column_raw_pixels.push_back(post[y]);
            }
            post += 4 + post[1];
            patch_column_colors.record(post_color_set.size());
            patch_color_set.insert(post_color_set.begin(), post_color_set.end());
            uint x = post_color_set.size();
            if (x) x--;
            if (x) x = 31 - __builtin_clz(x);
            assert(x < 8);
            patch_columns_under_colors[x].record(post_color_set.size());
        }
        auto lengths = huff(column_raw_pixels.data(), column_raw_pixels.size());
        int huffl = 0;
        for(const auto &p : column_raw_pixels) huffl += lengths[p];
        patch_col_hack_huff_pixels.record((huffl+7)/8);
        if (post_count > 1 || pixel_count != ph->height) {
            post = patch.data.data() + col_offset;
            while (post[0] != 0xff) {
                post += 4 + post[1];
            }
        }
        if (post_count == 1) {
            patch_one.record(1);
        }
        all_posts_count += post_count;
        patch_post_counts.record(post_count);
    }
    patch_colors.record(patch_color_set.size());
    patch_all_post_counts.record(all_posts_count);
    auto lengths = huff(patch_raw_pixels.data(), patch_raw_pixels.size());
    int huffl = 0;
    for(const auto &p : patch_raw_pixels) huffl += lengths[p];
    patch_hack_huff_pixels.record((huffl+7)/8);
    patch_pixels.record(patch_raw_pixels.size());

    symbol_stats<uint8_t> pixel_stats;
    patch_for_each_pixel(patch, [&](uint8_t p) {
       pixel_stats.add(p);
    });
    auto pixel_huffman = pixel_stats.create_huffman_encoding<huffman_params<uint8_t>>();
    byte_vector_bit_output output;
    patch_for_each_pixel(patch, [&](uint8_t p) {
        output.write(pixel_huffman.encode(p));
    });
    auto c2 = output.get_output();
#endif
    printf("  %d %s - %dx%d +%d,%d colors %d\n", num, name, ph->width, ph->height, ph->leftoffset, ph->topoffset, (int)patch_color_set.size());
    if (ph->width >= 256) {
        printf("        wide\n");
    }
}

std::vector<int> convert_sidedefs(wad &wad, const texture_index &tex_index, lump &lump) {
    assert(lump.data.size() % sizeof(mapsidedef_t) == 0);
    int count = lump.data.size() / sizeof(mapsidedef_t);
    printf("Converting %d sidedefs in lump %s\n", count, lump.name.c_str());
    std::vector<int> sidedef_mapping;
    int offset = 0;
    hash = hash * 31 + count;
    if (super_tiny) {
        std::vector<uint8_t> sides_z;
        for (int i = 0; i < count; i++) {
            uint s0 = sides_z.size();
            auto msd = get_field_inc<mapsidedef_t>(lump.data, offset);
            assert(sides_z.size() < 65536);
            sidedef_mapping.push_back(sides_z.size());

            short toptexture = tex_index.find(wad::wad_string(msd.toptexture));
            short midtexture = tex_index.find(wad::wad_string(msd.midtexture));
            short bottomtexture = tex_index.find(wad::wad_string(msd.bottomtexture));
            if (toptexture < 0 || midtexture < 0 || bottomtexture < 0) {
                fail("Missing sidedef texture");
            }

            // some textures may be 0, we consider the following combinations of none or the same textures...
            // we want to encode as few of A, B, C as possible.
            //
            // ENC : T M O
            // ----:------
            // 0   | - - -
            // 1   | - - C
            // 2   | - B -
            // 3   | - B B
            // 4   | - B C
            // 5   | A - -
            // 6   | A - A
            // 7   | A - C
            // 8   | A A -
            // 9   | A A A
            // 10  | A A B
            // 11  | A A C
            // 12  | A B -
            // 13  | A B A
            // 14  | A B B
            // 15  | A B C
            static uint8_t hasA[16] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
            static uint8_t hasB[16] = { 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1};
            static uint8_t hasC[16] = { 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1};
            static uint8_t posM[26] = { 0, 0, 2, 2, 2, 0, 0, 0, 2, 2, 2, 2, 3, 3, 3, 3};
            static uint8_t posO[26] = { 0, 2, 0, 2, 3, 0, 2, 3, 0, 2, 3, 4, 0, 2, 3, 4};
            static uint8_t offset_offset[26] = {2, 3, 3, 3, 4, 3, 3, 4, 3, 3, 4, 4, 4, 4, 4, 5};

            bool onebyte_texture = (!toptexture || toptexture < 256) &&
                    (!midtexture || midtexture < 256) &&
                    (!bottomtexture || bottomtexture < 256);

            uint enc;
            if (toptexture == 0) {
                if (midtexture == 0) {
                    // 0   | - - -
                    // 1   | - - C
                    enc = bottomtexture == 0 ? 0 : 1;
                } else {
                    // 2   | - B -
                    // 3   | - B B
                    // 4   | - B C
                    enc = bottomtexture == 0 ? 2 : ((bottomtexture == toptexture) ? 3 : 4);
                }
            } else {
                if (!midtexture) {
                    // 5   | A - -
                    // 6   | A - A
                    // 7   | A - C
                    enc = bottomtexture == 0 ? 5 : ((bottomtexture == toptexture) ? 6 : 7);
                } else if (midtexture == toptexture) {
                    // 8   | A A -
                    // 9   | A A A
                    // 10  | A A B
                    // 11  | A A C
                    if (!bottomtexture) enc = 8;
                    else if (bottomtexture == toptexture) enc = 9;
                    else if (bottomtexture == midtexture) enc = 10;
                    else enc = 11;
                } else {
                    // 12  | A B -
                    // 13  | A B A
                    // 14  | A B B
                    // 15  | A B C
                    if (!bottomtexture) enc = 12;
                    else if (bottomtexture == toptexture) enc = 13;
                    else if (bottomtexture == midtexture) enc = 14;
                    else enc = 15;
                }
            }
            bool have_rowoff = msd.rowoffset != 0;
            uint toff_size;
            if (msd.textureoffset == 0) {
                toff_size = 0;
            } else if (msd.textureoffset >= 0 && !(msd.textureoffset & 1) && (msd.textureoffset / 2 < 256)) {
                toff_size = 1;
            } else {
                toff_size = 2;
            }
            bool onebyte_sector = msd.sector < 256;
            bool onebyte = onebyte_sector | onebyte_texture;
            sides_z.push_back(enc | (!(onebyte) << 7) | (have_rowoff << 6) | (toff_size << 4));
            sides_z.push_back(msd.sector);
            assert(onebyte);
            if (hasA[enc]) {
                sides_z.push_back(toptexture);
            }
            if (hasB[enc]) {
                sides_z.push_back(midtexture);
            }
            if (hasC[enc]) {
                sides_z.push_back(bottomtexture);
            }
            if (toff_size == 1) {
                sides_z.push_back(msd.textureoffset / 2);
            } else if (toff_size == 2) {
                sides_z.push_back(msd.textureoffset >> 8);
                sides_z.push_back(msd.textureoffset & 0xff);
            }
            if (have_rowoff) {
                if (msd.rowoffset >= 0 && !(msd.rowoffset & 1) && (msd.rowoffset / 2 < 128)) {
                    sides_z.push_back(msd.rowoffset / 2);
                } else {
                    assert(msd.rowoffset >= -16384 && msd.rowoffset < 13684);
                    sides_z.push_back(0x80 | (msd.rowoffset >> 8));
                    sides_z.push_back(msd.rowoffset & 0xff);
                }
            }
            assert(msd.sector < 512);
            {
                uint enc2 = sides_z[s0] & 0xf;
                int tex_t = enc2 < 5 ? 0 : sides_z[s0+2];
                int tex_m = posM[enc2] ? sides_z[s0+posM[enc2]] : 0;
                int tex_b = posO[enc2] ? sides_z[s0+posO[enc2]] : 0;
                if (tex_t != toptexture || tex_m != midtexture || tex_b != bottomtexture) {
                    fail("sidedefs are not compatible with super tiny\n");
                }
                int texoff = 0;
                if (sides_z[s0] & 48) {
                    if (sides_z[s0] & 32) {
                        texoff = (sides_z[s0 + offset_offset[enc2]] << 8) + sides_z[s0 + 1 + offset_offset[enc2]];
                    } else {
                        texoff = sides_z[s0 + offset_offset[enc2]] << 1;
                    }
                }
                assert(msd.textureoffset == texoff);
                int rowoff = 0;
                if (sides_z[s0] & 64) {
                    uint pos = offset_offset[enc2] + ((sides_z[s0] >> 4)&3);
                    rowoff = sides_z[s0 + pos];
                    if (rowoff & 128) {
                        rowoff = (int16_t)(((rowoff << 8) | sides_z[s0 + pos + 1])<<1);
                        rowoff /= 2;
                    } else {
                        rowoff <<= 1;
                    }
                }
                assert(msd.rowoffset == rowoff);
            }
            side_meta.record(sizeof(whdsidedef_t));
            side_metaz.record(sides_z.size() - s0);
        }
        lump.data = sides_z;
    } else {
        std::vector<uint8_t> whd_sides;
        for (int i = 0; i < count; i++) {
            sidedef_mapping.push_back(i);
            auto msd = get_field_inc<mapsidedef_t>(lump.data, offset);

            short toptexture = tex_index.find(wad::wad_string(msd.toptexture));
            short midtexture = tex_index.find(wad::wad_string(msd.midtexture));
            short bottomtexture = tex_index.find(wad::wad_string(msd.bottomtexture));

            if (toptexture < 0 || midtexture < 0 || bottomtexture < 0) {
                fail("Missing sidedef texture");
            }
            whdsidedef_t sd = {
                    .textureoffset = msd.textureoffset,
                    .rowoffset = msd.rowoffset,
                    .toptexture = toptexture,
                    .bottomtexture = bottomtexture,
                    .midtexture = midtexture,
                    .sector = msd.sector,
                    };
            side_meta.record(sizeof(sd));
            append_field(whd_sides, sd);
        }
        lump.data = whd_sides;
    }
    wad.update_lump(lump);
    return sidedef_mapping;
}

// order here is important
#define ML_NO_PREDICT_SIDE      256
#define ML_NO_PREDICT_V1        512 // can share with ML_MAPPED
#define ML_NO_PREDICT_V2        1024
#define ML_HAS_SPECIAL          2048
#define ML_HAS_TAG              4096
#define ML_SIDE_MASK            0xe000u

#define line_onesided(l) ((l[1] & (ML_SIDE_MASK >> 8)) == 0)
#define line_predict_side(l) ((l[1] & (ML_NO_PREDICT_SIDE >> 8)) == 0)
#define line_predict_v1(l) ((l[1] & (ML_NO_PREDICT_V1 >> 8)) == 0)
#define line_predict_v2(l) ((l[1] & (ML_NO_PREDICT_V2 >> 8)) == 0)

static inline int line_sidenum(const uint8_t *lines, uint16_t whd_sidemul, const uint8_t *l, int side) {
    if (side && line_onesided(l)) return -1;
    int s;
    if (line_predict_side(l)) {
        s = ((l - lines) * whd_sidemul) >> 16;
        s += (int8_t)l[2];
    } else {
        s = l[2] + (l[3] << 8);
    }
    if (side) {
        s += l[1] >> 5;
    }
    return s;
}

static inline int line_v1(const uint8_t *lines, uint16_t whd_vmul, const uint8_t *l) {
    const static uint8_t v1pos[2] = { 3, 4 };
    int v;
    uint pos = v1pos[l[1]&1];
    if (line_predict_v1(l)) {
        v = ((l - lines) * whd_vmul) >> 16;
        v += (int8_t)l[pos];
    } else {
        v = l[pos] + (l[pos+1] << 8);
    }
    return v;
}

static inline int line_v2(const uint8_t *lines, uint16_t whd_vmul, const uint8_t *l) {
    const static uint8_t v2pos[4] = { 4, 5, 5, 6 };
    uint pos = v2pos[l[1]&3];
    int v;
    if (line_predict_v2(l)) {
        v = ((l - lines) * whd_vmul) >> 16;
        v += (int8_t)l[pos];
    } else {
        v = l[pos] + (l[pos+1] << 8);
    }
    return v;
}

static inline int line_special(const uint8_t *lines, const uint8_t *l) {
    int special = 0;
    if ((l[1] & (ML_HAS_SPECIAL >> 8)) != 0) {
        const static uint8_t special_pos[8] = { 5, 6, 6, 7, 6, 7, 7, 8 };
        uint pos = special_pos[l[1]&7];
        special = l[pos];
    }
    return special;
}

static inline int line_tag(const uint8_t *lines, const uint8_t *l) {
    int tag = 0;
    if ((l[1] & (ML_HAS_TAG >> 8)) != 0) {
        const static uint8_t special_pos[8] = { 5, 6, 6, 7, 6, 7, 7, 8 };
        uint pos = special_pos[l[1]&7] + ((l[1] & (ML_HAS_SPECIAL >> 8)) != 0);
        tag = l[pos];
    }
    return tag;
}

std::vector<int> convert_linedefs(wad &wad, lump &lump, const std::vector<int>& sidedef_mapping, const std::vector<std::pair<int,int>>& vertexes) {
    assert(lump.data.size() % sizeof(maplinedef_t) == 0);
    int count = lump.data.size() / sizeof(maplinedef_t);
    hash = hash * 31 + count;
    if (super_tiny) {
        compressed.insert(lump.num);
        printf("Converting %d linedefs in lump %s\n", count, lump.name.c_str());
        int offset = 0;
        int max_side_num = 0;
        int max_v = 0;
        for (int i = 0; i < count; i++) {
            auto msd = get_field_inc<maplinedef_t>(lump.data, offset);
            all_linedef_flags.insert(msd.flags);
            assert(msd.sidenum[0] >= 0);
            assert(msd.sidenum[1] == -1 || msd.sidenum[1] == msd.sidenum[0]+1);
            if (msd.sidenum[0] >= 0) {
                msd.sidenum[0] = sidedef_mapping[msd.sidenum[0]];
                max_side_num = std::max(max_side_num, (int)(uint16_t)msd.sidenum[0]);
            }
            if (msd.sidenum[1] >= 0) {
                msd.sidenum[1] = sidedef_mapping[msd.sidenum[1]];
                max_side_num = std::max(max_side_num, (int)(uint16_t)msd.sidenum[1]);
            }
            max_v = std::max((int)msd.v1, max_v);
        }
        auto try_encode = [&](uint size_guess) {
            printf("Guess %d\n", size_guess);
            uint vmul = 65536 * max_v / size_guess;
            assert(vmul < 65536);
            uint smul = 65536 * max_side_num / size_guess;
            assert(smul < 65536);
            offset = 0;
            std::vector<uint8_t> result;

            statsomizer side_num_range("Side num range");
            statsomizer side_num_range_v("Side num range");
            statsomizer v1_range("V1 range");
            statsomizer v1_range_v("V1 range");
            statsomizer v2_range("V2 range");
            statsomizer v2_range_v("V2 range");

            std::vector<int> offsets;
            for (int i = 0; i < count; i++) {
                offsets.push_back(result.size());
                auto msd = get_field_inc<maplinedef_t>(lump.data, offset);
                //            short		v1;
                //            short		v2;
                //            short		flags;
                //            short		special;
                //            short		tag;
                //            // sidenum[1] will be -1 if one sided
                //            short		sidenum[2];

                uint base = result.size();
                uint flags = msd.flags;
                assert(flags < 256);
                result.push_back(flags);
                result.push_back(0); // to fill in later

                int sidenum = sidedef_mapping[msd.sidenum[0]];
                if (msd.sidenum[1] != -1) {
                    uint delta = sidedef_mapping[msd.sidenum[1]] - sidenum;
                    assert(delta > 0 && delta <= 7);
                    flags |= delta << 13;
                }

                int s_est = (int)((base * smul) >> 16u);
                bool s_in_range = abs(sidenum - s_est) <= 127;
                side_num_range.record(s_in_range);
                side_num_range_v.record(sidenum - s_est);
                if (s_in_range) {
                    result.push_back(sidenum - s_est);
                } else {
                    flags |= ML_NO_PREDICT_SIDE;
                    assert(sidenum < 65536);
                    result.push_back(sidenum & 0xff);
                    result.push_back(sidenum >> 8);
                }

                int v_est = (int)((base * vmul) >> 16u);
                bool v1_in_range = abs(msd.v1 - v_est) <= 127;
                v1_range.record(v1_in_range);
                v1_range_v.record(msd.v1 - v_est);
                if (v1_in_range) {
                    result.push_back(msd.v1 - v_est);
                } else {
                    flags |= ML_NO_PREDICT_V1;
                    result.push_back(msd.v1 & 0xff);
                    result.push_back(msd.v1 >> 8);
                }

                bool v2_in_range = abs(msd.v2 - v_est) <= 127;
                v2_range.record(v2_in_range);
                v2_range_v.record(msd.v2 - v_est);
                if (v2_in_range) {
                    result.push_back(msd.v2 - v_est);
                } else {
                    flags |= ML_NO_PREDICT_V2;
                    result.push_back(msd.v2 & 0xff);
                    result.push_back(msd.v2 >> 8);
                }
                if (msd.special) {
                    assert(msd.special < 256);
                    flags |= ML_HAS_SPECIAL;
                    result.push_back(msd.special);
                }
                if (msd.tag) {
                    assert(msd.tag < 256);
                    flags |= ML_HAS_TAG;
                    result.push_back(msd.tag);
                }

                result[base+1] = flags >> 8;
            }
            side_num_range.print_summary();
            side_num_range_v.print_summary();
            v1_range.print_summary();
            v1_range_v.print_summary();
            v2_range.print_summary();
            v2_range_v.print_summary();
            printf("%d -> %d\n", (int)lump.data.size(), (int)result.size());
            return std::make_pair(result, offsets);
        };
        uint size_guess = 5 * count;
        auto last_encoding = try_encode(size_guess);
        while (true) {
            auto encoding = try_encode(last_encoding.first.size());
            if (encoding.first.size() >= last_encoding.first.size()) break;
            size_guess = last_encoding.first.size();
            last_encoding = encoding;
        }
        line_metaz.record(last_encoding.first.size());
        line_meta.record(lump.data.size());

#if 1
        // temp until we fix that above
        offset = 0;
        const uint8_t *lines = last_encoding.first.data();
        const auto &offsets = last_encoding.second;
        uint smul = 65536 * max_side_num / size_guess;
        uint vmul = 65536 * max_v / size_guess;
        for (int i = 0; i < count; i++) {
            auto msd = get_field_inc<maplinedef_t>(lump.data, offset);
            all_linedef_flags.insert(msd.flags);
            if (msd.sidenum[0] >= 0) {
                msd.sidenum[0] = sidedef_mapping[msd.sidenum[0]];
                max_side_num = std::max(max_side_num, (int)(uint16_t)msd.sidenum[0]);
            }
            if (msd.sidenum[1] >= 0) {
                msd.sidenum[1] = sidedef_mapping[msd.sidenum[1]];
                max_side_num = std::max(max_side_num, (int)(uint16_t)msd.sidenum[1]);
            }
            max_v = std::max((int)msd.v1, std::max((int)msd.v2, max_v));
            int s0 = line_sidenum(lines, smul, lines + offsets[i], 0);
            int s1 = line_sidenum(lines, smul, lines + offsets[i], 1);
            assert(msd.sidenum[0] == s0);
            assert(msd.sidenum[1] == s1);
            int v1 = line_v1(lines, vmul, lines + offsets[i]);
            int v2 = line_v2(lines, vmul, lines + offsets[i]);
            assert(msd.v1 == v1);
            assert(msd.v2 == v2);
            assert(msd.tag == line_tag(lines, lines + offsets[i]));
            assert(msd.special == line_special(lines, lines + offsets[i]));
        }
#endif
        lump.data.clear();
        uint16_t tmp = count; append_field(lump.data, tmp);
        tmp = smul; append_field(lump.data, tmp);
        tmp = vmul; append_field(lump.data, tmp);
        assert(last_encoding.first.size() < 65536);
        lump.data.insert(lump.data.end(), last_encoding.first.begin(), last_encoding.first.end());
        wad.update_lump(lump);
        line_scale.record(100 * lump.data.size() / count);
        return last_encoding.second;
    } else {
        // unchanged, so return an identity mapping
        std::vector<int> rc;
        for (int i = 0; i < count; i++) {
            rc.push_back(i);
        }
        return rc;
    }
}

std::vector<int> convert_segs(wad &wad, lump &lump, const std::vector<int>& linedef_mapping) {
    assert(lump.data.size() % sizeof(mapseg_t) == 0);
    int count = lump.data.size() / sizeof(mapseg_t);
    hash = hash * 31 + count;
    std::vector<uint8_t> newdata;
    printf("Converting %d segs in lump %s\n", count, lump.name.c_str());
    std::vector<int> rc;
    if (!super_tiny) {
        int offset = 0;
            for (int i = 0; i < count; i++) {
            rc.push_back(i);
            auto ms = get_field_inc<mapseg_t>(lump.data, offset);
                ms.linedef = linedef_mapping[ms.linedef];
                append_field(newdata, ms);
            }
        rc.push_back(count); // we need an end marker too
        } else {
        compressed.insert(lump.num);
        int offset = 0;
        for (int i = 0; i < count; i++) {
            rc.push_back(newdata.size());
            auto ms = get_field_inc<mapseg_t>(lump.data, offset);
            uint16_t l = linedef_mapping[ms.linedef];
            assert(ms.v1 < 2048);
            assert(ms.v2 < 2048);
            assert(ms.side == 0 || ms.side == 1);
            newdata.push_back(ms.side << 7 | ((ms.offset != 0) << 6) | ((ms.v2 >> 5)&0x38) | ((ms.v1 >> 8)&0x7));
            newdata.push_back(ms.v1 & 0xff);
            newdata.push_back(ms.v2 & 0xff);
            newdata.push_back(l & 0xff);
            newdata.push_back(l >> 8);
            if (ms.offset) {
                if (!(ms.offset & 3) && ms.offset >= 0 && ms.offset < 512) {
                    newdata.push_back((ms.offset>>2)&0x7f);
                } else {
                    assert(abs(ms.offset)<16384);
                    newdata.push_back(128 | (ms.offset>>8));
                    newdata.push_back(ms.offset&0xff);
                }
            }
        }
        offset = 0;
        int pos = 0;
        for (int i = 0; i < count; i++) {
            auto ms = get_field_inc<mapseg_t>(lump.data, offset);
            uint16_t l = newdata[pos+3] + (newdata[pos+4] << 8);
            assert(l == linedef_mapping[ms.linedef]);
            assert((newdata[pos] >> 7) == ms.side);
            uint16_t v1 = newdata[pos+1] | ((newdata[pos]&7)<<8);
            uint16_t v2 = newdata[pos+2] | ((newdata[pos]&0x38)<<5);
            assert(v1 == ms.v1);
            assert(v2 == ms.v2);
            short soffset = 0;
            if (newdata[pos] & 64) {
                if (newdata[pos+5] < 128) {
                    soffset = newdata[pos+5] * 4;
                    pos++;
                } else {
                    soffset = (newdata[pos+5] << 9) | (newdata[pos+6] << 1);
                    soffset /= 2;
                    pos+=2;
                }
            }
            pos += 5;
            assert(soffset == ms.offset);
        }
        rc.push_back(newdata.size()); // we need an end marker too
    }
    lump.data = newdata;
    wad.update_lump(lump);
    return rc;
}

std::vector<std::pair<int,int>> convert_vertexes(wad &wad, lump &lump) {
    // todo 4->3
    assert(lump.data.size() % sizeof(mapvertex_t) == 0);
    int count = lump.data.size() / sizeof(mapvertex_t);
    hash = hash * 31 + count;
    printf("Converting %d vertexes in lump %s\n", count, lump.name.c_str());
    int offset = 0;
    std::vector<std::pair<int,int>> vertexes;
    for (int i = 0; i < count; i++) {
        auto mv = get_field_inc<mapvertex_t>(lump.data, offset);
        int x = mv.x;
        int y = mv.y;
        vertex_x.record(x);
        vertex_y.record(y);
        vertexes.emplace_back(x, y);
    }
    return vertexes;
}

static inline bool is_leaf(const mapnode_t &node, int which) {
    return node.children[which] >= 32768;
}

enum {
    BOXTOP,
    BOXBOTTOM,
    BOXLEFT,
    BOXRIGHT
};        // bbox coordinates

void convert_nodes(wad &wad, lump &lump) {
    assert(lump.data.size() % sizeof(mapnode_t) == 0);
    int count = lump.data.size() / sizeof(mapnode_t);
    hash = hash * 31 + count;
    if (super_tiny) {
        std::vector<uint8_t> newdata;
        printf("Converting %d nodes in lump %s\n", count, lump.name.c_str());
        int offset = 0;
        // TODO we could do without encoding children, it only saves 2 bytes per node, but still it works
        //  in all the WADs we care about for now
        std::vector<mapnode_t> mapnodes(count);
        std::vector<whdnode_t> whdnodes(count);

        for (int i = 0; i < count; i++) {
            mapnodes[i] = get_field_inc<mapnode_t>(lump.data, offset);
        }
        std::function<void(int, int16_t *)> push_bbox = [&](int i, int16_t *bbox) {
            const auto &mnode = mapnodes[i];
            whdnodes[i].x = mnode.x;
            whdnodes[i].y = mnode.y;
            whdnodes[i].dx = mnode.dx;
            whdnodes[i].dy = mnode.dy;
            if (is_leaf(mnode, 1)) {
                // L is leaf
                if (is_leaf(mnode, 0)) {
                    // L is leaf, R is leaf
                    int deltaL = i - (mnode.children[1] & 0x7fffu);
                    int deltaR = i - (mnode.children[0] & 0x7fffu);
                    if (deltaL < -64 || deltaL > 63 || deltaR < -64 || deltaR > 63) {
                        fail("Out of range/non standard node tree, node %d has L leaf delta %d, R leaf delta %d (require -64 <= x <= 63)\n",
                             i, deltaL, deltaR);
                    }
                    whdnodes[i].coded_children = 0xc000 | ((deltaL & 0x7fu) << 7u) | (deltaR & 0x7fu);
                } else {
                    // L is leaf, R is node
                    if (mnode.children[0] != i - 1) {
                        fail("Non standard tree, node %d, expected R node child to be N-1 i.e. %d but was %d\n", i,
                             i - 1,
                             mnode.children[0]);
                    }
                    int deltaL = i - (mnode.children[1] & 0x7fffu);
                    if (deltaL < -64 || deltaL > 63) {
                        fail("Out of range/non standard node tree, node %d has L leaf delta %d (require -64 <= x <= 63)\n",
                             deltaL);
                    }
                    whdnodes[i].coded_children = 0x8000 | ((deltaL & 0x7fu) << 7u);
                }
            } else {
                // L is node
                if (mnode.children[1] != i - 1) {
                    fail("Non standard tree, node %d, expected L node child to be N-1 i.e. %d but was %d\n", i, i - 1,
                         mnode.children[1]);
                }
                if (is_leaf(mnode, 0)) {
                    // L is node, R is leaf
                    int deltaR = i - (mnode.children[0] & 0x7fffu);
                    if (deltaR < -64 || deltaR > 63) {
                        fail("Out of range/non standard node tree, node %d has R leaf delta %d (require -64 <= x <= 63)\n",
                             deltaR);
                    }
                    whdnodes[i].coded_children = 0x4000 | (deltaR & 0x7fu);
                } else {
                    // L is node, R is node
                    int deltaR = i - (mnode.children[0] & 0x7fffu);
                    if (deltaR < 2 || deltaR > 16383) {
                        fail("Out of range/non standard node tree, node %d has R node delta %d (require 2 <= x <= 16383) \n",
                             i, deltaR);
                    }
                    whdnodes[i].coded_children = (deltaR & 0x3fffu);
                }
            }
#if VERIFY_ENCODING
            // code is 11ll llll lrrr rrrr  : l/r are 7 bit signed values; left leaf = 0x8000 + n - l; right leaf = 0x8000 + n - r
            //         10ll llll l--- ----  : right node is n- 1, left leaf via l as above
            //         01-- ---- -rrr rrrr  : left node is n - 1, right leaf via r is as above,
            //         00rr rrrr rrrr rrrr  : left node is n - 1, right node is n - r
            int v[2];
            if (whdnodes[i].coded_children & 0x8000) {
                v[1] = 0x8000 + i - (((int32_t) (whdnodes[i].coded_children << 18)) >> 25);
                if (whdnodes[i].coded_children & 0x4000) {
                    v[0] = 0x8000 + i - (((int32_t) (whdnodes[i].coded_children << 25)) >> 25);
                } else {
                    v[0] = i - 1;
                }
            } else {
                v[1] = i - 1;
                if (whdnodes[i].coded_children & 0x4000) {
                    v[0] = 0x8000 + i - (((int32_t) (whdnodes[i].coded_children << 25)) >> 25);
                } else {
                    v[0] = i - (whdnodes[i].coded_children & 0x3fffu);
                }
            }
            if (v[0] != mapnodes[i].children[0] || v[1] != mapnodes[i].children[1]) {
                fail("Encoding error");
            }
#endif
            for (int s = 0; s < 2; s++) {
                assert(bbox[BOXRIGHT] != bbox[BOXLEFT]); // saw this in E4M7... keeping as a reminder
                int l = 16 * (mnode.bbox[s][BOXLEFT] - bbox[BOXLEFT]) / (bbox[BOXRIGHT] - bbox[BOXLEFT]);
                if (l == 16) l = 15;
                int ldash = bbox[BOXLEFT] + (l * (bbox[BOXRIGHT] - bbox[BOXLEFT])) / 16;
                //            printf("  L %d->%d->%d = %d (%d/16))\n", bbox[BOXLEFT], mnode.bbox[s][BOXLEFT], bbox[BOXRIGHT], ldash, l);
                int w;
                if (bbox[BOXRIGHT] == ldash) {
                    w = 0;
                } else {
                    w = (16 * (mnode.bbox[s][BOXRIGHT] - ldash) - 1) / (bbox[BOXRIGHT] - ldash);
                }
                int rdash = ldash + ((bbox[BOXRIGHT] - ldash) * (1 + w)) / 16;
                //            printf("  R %d->%d->%d = %d (%d/16))\n", ldash, mnode.bbox[s][BOXRIGHT], bbox[BOXRIGHT], rdash, w);
                assert(bbox[BOXBOTTOM] != bbox[BOXTOP]); // saw this in E4M7... keeping as a reminder
                int b = 16 * (mnode.bbox[s][BOXBOTTOM] - bbox[BOXBOTTOM]) / (bbox[BOXTOP] - bbox[BOXBOTTOM]);
                if (b == 16) b = 15;
                int bdash = bbox[BOXBOTTOM] + (b * (bbox[BOXTOP] - bbox[BOXBOTTOM])) / 16;
                int h;
                if (bbox[BOXTOP] == bdash) {
                    h = 0;
                } else {
                    h = (16 * (mnode.bbox[s][BOXTOP] - bdash) - 1) / (bbox[BOXTOP] - bdash);
                }
                int tdash = bdash + ((bbox[BOXTOP] - bdash) * (1 + h)) / 16;
                if (l < 0 || l > 15 || b < 0 || b > 15 || w < 0 || w > 15 || h < 0 || h >> 15) {
                    fail("Bad bounding box conversion %d.%d  %d->%d,%d->%d becomes %d->%d,%d->%d, but lwbh is %d,%d %d,%d\n",
                         i, s,
                         mnode.bbox[s][BOXLEFT], mnode.bbox[s][BOXRIGHT], mnode.bbox[s][BOXBOTTOM],
                         mnode.bbox[s][BOXTOP],
                         ldash, rdash, bdash, tdash, l, w, b, h);
                }
                if (ldash > mnode.bbox[s][BOXLEFT] ||
                    rdash < mnode.bbox[s][BOXRIGHT] ||
                    bdash > mnode.bbox[s][BOXBOTTOM] ||
                    tdash < mnode.bbox[s][BOXTOP]) {
                    fail("Bad bounding box conversion %d.%d  %d->%d,%d->%d becomes %d->%d,%d->%d\n", i, s,
                         mnode.bbox[s][BOXLEFT], mnode.bbox[s][BOXRIGHT], mnode.bbox[s][BOXBOTTOM],
                         mnode.bbox[s][BOXTOP],
                         ldash, rdash, bdash, tdash);
                }
                //            printf("%d.%d x,y %d->%d,%d->%d becomes %d->%d,%d->%d\n", i, s,
                //                   mnode.bbox[s][BOXLEFT], mnode.bbox[s][BOXRIGHT], mnode.bbox[s][BOXBOTTOM], mnode.bbox[s][BOXTOP],
                //                   ldash, rdash, bdash, tdash);
                whdnodes[i].bbox_lw[s] = (l << 4u) | w;
                whdnodes[i].bbox_th[s] = (b << 4u) | h;
                //            whdnodes[i].children[s] = mnode.children[s];
                if (mnode.children[s] < 32768) {
                    int16_t childbbox[4];
                    childbbox[BOXLEFT] = ldash;
                    childbbox[BOXRIGHT] = rdash;
                    childbbox[BOXTOP] = tdash;
                    childbbox[BOXBOTTOM] = bdash;
                    push_bbox(mnode.children[s], childbbox);
                }
            }
        };
        int16_t initial_bbox[4] = {32767, -32768, -32768, 32767};
        push_bbox(count - 1, initial_bbox);
        lump.data.clear();
        for (int i = 0; i < count; i++) {
            append_field(lump.data, whdnodes[i]);
        }
        wad.update_lump(lump);
    }
}

void convert_sectors(wad &wad, lump &lump) {
    assert(lump.data.size() % sizeof(mapsector_t) == 0);
    int count = lump.data.size() / sizeof(mapsector_t);
    hash = hash * 31 + count;
    std::vector<uint8_t> newdata;
    printf("Converting %d sidedefs in lump %s\n", count, lump.name.c_str());
    int offset = 0;
    int fstart = wad.get_lump_index("f_start")+1;

    for (int i = 0; i < count; i++) {
        auto ms = get_field_inc<mapsector_t>(lump.data, offset);
        int floorpic = wad.get_lump_index(wad::wad_string(ms.floorpic));
        int ceilingpic = wad.get_lump_index(wad::wad_string(ms.ceilingpic));
        if (super_tiny) {
            if (!(ms.tag == 999 || ms.tag == 666 || ms.tag == 667 || ms.tag < 254)) {
                // note we will fail for existing tags of 254, 255 which we could fix by renumbering
                fail("Tag out of range %d\n", ms.tag);
            }
        }

        if (floorpic < 0 || ceilingpic < 0) {
            fail("Missing sector flat");
        }
        floorpic -= fstart;
        ceilingpic -= fstart;
        if (floorpic < 0 || floorpic > 255 || ceilingpic < 0 || ceilingpic > 255) {
            fail("Expect <256 flats");
        }
        sector_lightlevel.record(ms.lightlevel);
        sector_floorheight.record(ms.floorheight);
        sector_ceilingheight.record(ms.ceilingheight);
        sector_special.record(ms.special);
        short tag = ms.tag;
        if (super_tiny) {
            if (tag == 666) tag = 254;
            if (tag == 667) tag = 255;
            if (tag == 999) tag = 0;
        }

        whdsector_t se = {
                .floorheight = ms.floorheight,
                .ceilingheight = ms.ceilingheight,
                .floorpic = (uint8_t) floorpic,
                .ceilingpic = (uint8_t) ceilingpic,
                .lightlevel = ms.lightlevel,
                .special = ms.special,
                .tag = tag
        };
        append_field(newdata, se);
    }
    lump.data = newdata;
    wad.update_lump(lump);
}

void convert_subsectors(wad &wad, lump &lump, const std::vector<int>& seg_mapping) {
    assert(lump.data.size() % sizeof(mapsubsector_t) == 0);
    int count = lump.data.size() / sizeof(mapsubsector_t);
    hash = hash * 31 + count;
    std::vector<uint8_t> newdata;
    printf("Converting %d subsectors in lump %s\n", count, lump.name.c_str());
    int offset = 0;
    int expected = 0;
    auto appender= [&](int val) {
        if (val > 65535) fail("segment no %d is too big", val);
        uint16_t v = val;
        append_field(newdata, v);
    };
        for (int i = 0; i < count; i++) {
            auto mss = get_field_inc<mapsubsector_t>(lump.data, offset);
        assert(mss.firstseg == expected);
            expected = mss.firstseg + mss.numsegs;
            subsector_length.record(mss.numsegs);
            appender(seg_mapping[mss.firstseg]);
        }
        assert((int) seg_mapping.size() == expected + 1);
        appender(seg_mapping[expected]);
    lump.data = newdata;
    wad.update_lump(lump);
}

statsomizer blockmap_empty("blockmap empty");
statsomizer blockmap_one("blockmap one");
statsomizer blockmap_length("blockmap length");
statsomizer blockmap_row_size("blockmap row size");
statsomizer block_map_deltas("blockmap deltas");
statsomizer block_map_deltas_1byte("blockmap delta 1byte");
statsomizer blockmap_sizes("blockmap sizes");
statsomizer blockmap_blocks("blockmap blocks");

void write_hword(std::vector<uint8_t> &bytes, size_t pos, size_t word) {
    if (pos + 2 > bytes.size()) {
        bytes.resize(pos + 2);
    }
    bytes[pos] = word & 0xff;
    bytes[pos + 1] = (word >> 8) & 0xff;
}

void write_word(std::vector<uint8_t> &bytes, size_t pos, size_t word) {
    if (pos + 4 > bytes.size()) {
        bytes.resize(pos + 4);
    }
    bytes[pos] = word & 0xff;
    bytes[pos + 1] = (word >> 8) & 0xff;
    bytes[pos + 2] = (word >> 16) & 0xff;
    bytes[pos + 3] = (word >> 24) & 0xff;
}

void convert_blockmap(wad &wad, lump &lump, const std::vector<int>& linedef_mapping) {
    const short *bm = (const short *) lump.data.data();
    int w = bm[2];
    int h = bm[3];
//    printf("blockmap %dx%d\n", w, h);
    size_t header_span_size = 2 + (w + 7) / 8;
    std::vector<uint8_t> new_bm(header_span_size * h);
    // note this isn't super efficient storage-wise, but done this way for the sake of sanity checking
    std::vector<std::vector<std::vector<int16_t>>> blockmap;
    blockmap.resize(h);
    blockmap_blocks.record(w * h);
    for (int y = 0; y < h; y++) {
        blockmap[y].resize(w);
        int per_row_non_empty_count = 0;
        for (int x = 0; x < w; x++) {
            int i = bm[y * w + x + 4];
            // note this check assumes sorted, which we currently, do ... we could sort obviously
            if (bm[i] != 0) {
                fail("block map cell without zero");
            }
            i++; // skip the zero.
            int per_cell_non_empty_count = 0;
            auto &cell = blockmap[y][x];
            if (bm[i] == -1) {
                blockmap_empty.record(1);
            } else {
                if (bm[i + 1] == -1) {
                    blockmap_one.record(1);
                    cell.push_back(linedef_mapping[bm[i]]);
                } else {
                    for (int last = 0; bm[i] != -1; i++) {
                        int delta = bm[i] - last;
                        if (delta == 0) {
                            printf("  duplicate %d\n", last);
                            continue;
                        }
                        cell.push_back(linedef_mapping[bm[i]]);
                        if (delta < 0) fail("out of order delta");
                        delta--;
//                        printf("%d ", bm[i]);
                        block_map_deltas.record(delta);
                        block_map_deltas_1byte.record(delta >= 0 && delta < 128);
                    }
                }
            }
            if (!cell.empty()) per_row_non_empty_count++;
            blockmap_length.record(per_cell_non_empty_count);
//            printf("\n");
        }
        blockmap_row_size.record(per_row_non_empty_count);

        // main header points to sparse per row stuff
        size_t row_start_pos = new_bm.size();
        write_hword(new_bm, y * header_span_size, row_start_pos);
        // leave space for 2 bytes per non-empty cell in the row
        new_bm.resize(row_start_pos + per_row_non_empty_count * 2);
        int non_empty_cell_index = 0;
        for (int x = 0; x < w; x++) {
            const auto &cell = blockmap[y][x];
            if (!cell.empty()) {
                // set the bitmap bit
                new_bm[y * header_span_size + 2 + (x / 8)] |= 1u << (x & 7u);
                size_t cell_metadata_offset = row_start_pos + non_empty_cell_index * 2;
                bool use_list = true;
                if (cell.size() == 1) {
                    // single occupancy cell, so don't bother redirecting unless it won't fit here
                    if (cell[0] <= 0xfff) {
                        write_hword(new_bm, cell_metadata_offset, 0xf000 | cell[0]);
                        use_list = false;
                    }
                }
                if (use_list) {
                    // multi occupancy cell, so redirect to variable length list (note the length
                    // will be filledin later, below... 6:12 length:offset)
                    size_t values_list_offset = new_bm.size() - row_start_pos;
                    if (values_list_offset > 0x3ff) {
                        fail("blockmap sparse data too big");
                    }
                    // write the link to the values list
                    write_hword(new_bm, cell_metadata_offset, values_list_offset);
                    int last = 0;
                    for (const auto &v : cell) {
                        int delta = v - last;
                        if (delta == 0) {
                            continue;
                        }
                        delta--;
                        assert(v);
                        per_row_non_empty_count++;
                        if (delta < 127) {
                            new_bm.push_back(delta);
                        } else {
                            if (delta > 37267) fail("duh");
                            new_bm.push_back(0x80 | (delta >> 8));
                            new_bm.push_back(delta & 0xff);
                        }
                        last = v;
                    }
                    // we currently use 4 top bits set to mean single cell
                    if (cell.size() >= 0b111100 + 1) {
                        fail("block has too many lines");
                    }
                    new_bm[cell_metadata_offset + 1] |= ((cell.size() - 1) << 2);
                }
                non_empty_cell_index++;
            }
        }
    }
    blockmap_sizes.record(new_bm.size());
#if VERIFY_ENCODING
    auto popcount8 = [](uint n) {
        assert(n < 256);
        return __builtin_popcount(n);
    };
    auto popcount8_2 = [](uint8_t v) {
        static const uint8_t table[128] = {
                0x10, 0x21, 0x21, 0x32, 0x21, 0x32, 0x32, 0x43, 0x21, 0x32, 0x32, 0x43, 0x32, 0x43, 0x43, 0x54,
                0x21, 0x32, 0x32, 0x43, 0x32, 0x43, 0x43, 0x54, 0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65,
                0x21, 0x32, 0x32, 0x43, 0x32, 0x43, 0x43, 0x54, 0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65,
                0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65, 0x43, 0x54, 0x54, 0x65, 0x54, 0x65, 0x65, 0x76,
                0x21, 0x32, 0x32, 0x43, 0x32, 0x43, 0x43, 0x54, 0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65,
                0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65, 0x43, 0x54, 0x54, 0x65, 0x54, 0x65, 0x65, 0x76,
                0x32, 0x43, 0x43, 0x54, 0x43, 0x54, 0x54, 0x65, 0x43, 0x54, 0x54, 0x65, 0x54, 0x65, 0x65, 0x76,
                0x43, 0x54, 0x54, 0x65, 0x54, 0x65, 0x65, 0x76, 0x54, 0x65, 0x65, 0x76, 0x65, 0x76, 0x76, 0x87,
        };
        return (v & 1) ? (table[v / 2] >> 4) : (table[v / 2] & 0xf);
    };
    for (int i = 0; i < 256; i++) {
        assert(popcount8(i) == popcount8_2(i));
    }

    std::vector<int16_t> check;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint row_header_offset = y * header_span_size;
            uint data_offset = new_bm[row_header_offset] | (new_bm[row_header_offset + 1] << 8u);
            check.clear();
            int bmx = x / 8;
            if (new_bm[row_header_offset + 2 + bmx] & (1u << (x & 7))) {
                int cell_metadata_index = popcount8(new_bm[row_header_offset + 2 + bmx] & ((1u << (x & 7)) - 1));
                for (int xx = 0; xx < bmx; xx++) {
                    cell_metadata_index += popcount8(new_bm[row_header_offset + 2 + xx]);
                }
                uint cell_metadata = new_bm[data_offset + cell_metadata_index * 2] |
                                     (new_bm[data_offset + cell_metadata_index * 2 + 1] << 8u);
                if ((cell_metadata & 0xf000) == 0xf000) {
                    check.push_back(cell_metadata & 0xfffu);
                } else {
                    int count = (cell_metadata >> 10u) + 1;
                    uint element_offset = data_offset + (cell_metadata & 0x3ff);
                    int last = 0;
                    while (count--) {
                        uint b = new_bm[element_offset++];
                        if (b & 0x80) {
                            b = ((b & 0x7f) << 8) + new_bm[element_offset++];
                        }
                        last += b + 1;
                        check.push_back(last);
                    }
                }
            }
            const auto &cell = blockmap[y][x];
            if (!std::equal(check.begin(), check.end(), cell.begin(), cell.end())) {
                printf("Expected: ");
                for (auto &e : cell) printf("%d ", e);
                printf("\nGot: ");
                for (auto &e : check) printf("%d ", e);
                printf("\n");
                fail("(Mismatched blockmap decoding at cell %d,%d)\n", x, y);
            }
        }
    }
#endif
    lump.data.resize(8); // keep the x,y,w,h
    lump.data.insert(lump.data.end(), new_bm.begin(), new_bm.end());
    wad.update_lump(lump);
}


//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//
typedef PACKED_STRUCT (
        {
            short originx;
            short originy;
            short patch;
            short stepdir;
            short colormap;
        }) mappatch_t;


//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
typedef PACKED_STRUCT (
        {
            char name[8];
            int masked;
            short width;
            short height;
            int obsolete;
            short patchcount;
        }) maptexture_t;

struct sprite_frame {
    std::array<int, 9> rotations = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
};

void convert_sprites(wad &wad) {
    int s_start = wad.get_lump_index("s_start");
    int s_end = wad.get_lump_index("s_end");

    int sprite_frames = 0;
    std::vector<std::vector<sprite_frame>> sprite_info(sprite_names.size());
    const int FLAG = (1u << 15);
    for (int l = s_start + 1; l < s_end; l++) {
        lump sprite;
        if (wad.get_lump(l, sprite)) {
            auto it = std::find(sprite_names.begin(), sprite_names.end(), to_upper(sprite.name.substr(0, 4)));
            if (it == sprite_names.end() || (sprite.name.length() != 6 && sprite.name.length() != 8)) {
                printf("Unexpected sprite name %s\n", sprite.name.c_str());
            } else {
                int num = (int) (it - sprite_names.begin());
//                printf("Found sprite frame %d %s\n", num, sprite.name.c_str());
                sprite_frames++;
                uint frame = toupper(sprite.name[4]) - 'A';
                uint rotation = sprite.name[5] - '0';
                if (sprite_info[num].size() <= frame) {
                    sprite_info[num].resize(frame + 1);
                }
                sprite_info[num][frame].rotations[rotation] = l - (s_start + 1);
                if (sprite.name.length() == 8) {
                    frame = toupper(sprite.name[6]) - 'A';
                    rotation = sprite.name[7] - '0';
                    if (!rotation)
                        fail("didn't expect a flipped sprite for non rotated version"); // currently disabled in the WHD code though we could use a different bit for flipped vs rotatable
                    if (sprite_info[num].size() <= frame) {
                        sprite_info[num].resize(frame + 1);
                    }
                    sprite_info[num][frame].rotations[rotation] = FLAG | (l - (s_start + 1));
                }
            }
        }
    }
    std::vector<uint16_t> sprite_offset;
    std::vector<uint16_t> frame0_or_rotation_offset;
    std::vector<uint16_t> rotation_frames;
    for (uint i = 0; i < sprite_names.size(); i++) {
        sprite_offset.push_back(frame0_or_rotation_offset.size());
        for (uint j = 0; j < sprite_info[i].size(); j++) {
            const auto &rotations = sprite_info[i][j].rotations;
            int c = 0;
            for (int k = 1; k < 9; k++) {
                if (rotations[k] != -1) {
                    c++;
                }
            }
            if (rotations[0] != -1) {
                if (c != 0) {
                    fail("%d %d EXTRA FRAMES FOR SINGLE FRAME\n", i, j);
                } else {
//                    printf("%d %d SINGLE FRAME OK\n", i, j);
                    frame0_or_rotation_offset.push_back(rotations[0]);
                }
            } else {
                switch (c) {
                    case 0:
                        fail("%d %d MISSING\n", i, j);
                        break;
                    case 8:
                        frame0_or_rotation_offset.push_back(FLAG | rotation_frames.size());
                        for (int k = 1; k < 9; k++) {
                            rotation_frames.push_back(rotations[k]);
                        }
                        break;
                    default:
                        fail("%d %d INVALID WITH %d ANGLES\n", i, j, c);
                        break;
                }
            }
        }
    }
    printf("Sprite frames %d\n", sprite_frames);
    sprite_offset.push_back(frame0_or_rotation_offset.size());
    std::vector<uint8_t> data;
    uint16_t off = sprite_offset.size();
    for (uint16_t e : sprite_offset) {
        e += off;
        append_field(data, e);
    }
    off += frame0_or_rotation_offset.size();
    for (uint16_t e : frame0_or_rotation_offset) {
        if (e & FLAG) e += off;
        append_field(data, e);
    }
    for (uint16_t e : rotation_frames) {
        append_field(data, e);
    }
    printf("Sprite table count = %d\n", (int) sprite_offset.size());
    printf("Frame info count = %d\n", (int) frame0_or_rotation_offset.size());
    printf("Rotation frame count = %d\n", (int) rotation_frames.size());
    printf("Total sprite frame metadata size = %d\n", (int) data.size());
    lump s_end_lump;
    wad.get_lump("S_END", s_end_lump);
    touched[s_end_lump.num] = TOUCHED_SPRITE_METADATA;

    // put metadata in s_end as we're using s_start for per sprite patch info
    s_end_lump.data = data;
    wad.update_lump(s_end_lump);
}

void clear_lump(wad& wad, std::string name, std::string touched_name) {
    lump l;
    if (wad.get_lump(name, l)) {
        l.data.clear();
        cleared_lumps.push_back(l.num);
        wad.update_lump(l);
        touched[l.num] = touched_name;
    }
}

lump get_free_lump(wad& wad) {
    lump l;
    if (!cleared_lumps.empty()) {
        l.num = cleared_lumps[cleared_lumps.size() - 1];
        printf("get_free_lump resuing %d\n", l.num);
        cleared_lumps.pop_back();
    } else {
        auto it = wad.get_lumps().end();
        it--;
        l.num = it->first + 1;
        printf("get_free_lump allocating new %d\n", l.num);
    }
    return l;
}

std::vector<uint8_t> optimize_column(std::string name, int col, std::vector<uint8_t> cmds, const std::vector<int> local_patches, int height, int &seg_count) {
    using seg = std::pair<int, std::vector<uint8_t>>;

    std::vector<seg> original_segs;
    assert(!(cmds.size()&3)); // everything starts as a regular 4 byte command
    int y = 0;
    for(int i=0;i<(int)cmds.size();i+=4) {
        std::vector<uint8_t> cmd;
        cmd.insert(cmd.end(), cmds.begin() + i, cmds.begin() + i + 4);
        cmd[1] = (cmd[1] & 0x7f) + 1;
        original_segs.emplace_back(y, cmd);
        y += cmd[1];
    }
    assert(seg_count == (int)original_segs.size());

    // helper method to re-render a column for comparison
    auto draw_column = [&](const std::vector<seg> &segs, bool leniant = false) {
        std::vector<std::pair<int, int>> patch_and_y(height);
        std::fill(patch_and_y.begin(), patch_and_y.end(), std::make_pair(-1,-1));
        for(const auto &e : segs) {
            const auto& cmds = e.second;
            int y = e.first;
            // 0 is local patch + flags
            // 1 is length - 1 + end flag
            int i = 0;
            int m0 = cmds[i];
            int length = cmds[i + 1];
            i += 2;
            if (m0 & WHD_COL_SEG_EXPLICIT_Y) {
                y = cmds[i++];
            }
            if (y + length > height) {
                if (leniant) return std::vector<std::pair<int, int>>(); // given garbage return something that wont match
                assert(false);
            }
            if (m0 & WHD_COL_SEG_MEMCPY) {
                // memcpy
                int from = cmds[i++];
                if (!(m0 & WHD_COL_SEG_MEMCPY_IS_BACKWARDS)) {
                    for (int j = 0; j < length; j++) {
                        patch_and_y[y + j] = patch_and_y[from + j];
                    }
                } else {
                    for (int j = length-1; j >= 0; j--) {
                        patch_and_y[y + j] = patch_and_y[from + j];
                    }
                }
            } else {
                int lp = m0 & 0xf;
                for (int j = 0; j < length; j++) {
                    patch_and_y[y + j] = std::make_pair(lp, j + cmds[i + 1]);
                }
            }
        }
        return patch_and_y;
    };

#if DEBUG_TEXTURE_OPTIMIZATION
    bool had_dump = false;
    std::function<void(void)> dump_original;
    auto dump_segs = [&](std::vector<seg>& segs_to_dump, const char *msg, ...) {
        if (!had_dump) {
            had_dump = true;
            dump_original();
        }
        printf("Tex %s col %d: ", name.c_str(), col);
        va_list va;
        va_start(va, msg);
        vprintf(msg, va);
        va_end(va);
        printf("\n");
        int y= 0;
        for(int i=0;i<(int)segs_to_dump.size(); i++) {
            const auto &ey = segs_to_dump[i].first;
            const auto &ecmds = segs_to_dump[i].second;
            int length = ecmds[1];
            printf("%d%s ", ey, y == ey ? " ":"*");
            if (ecmds[0] & 0x80) {
                printf("copy %s for %d from %d", ecmds[0]& WHD_COL_SEG_MEMCPY_IS_BACKWARDS ? "backwards" : "forwards", length, ecmds[2]);
            } else {
                printf("%d for %d +/- %d,%d", ecmds[0]&0xf, length, ecmds[2], ecmds[3]);
            }
            if (ecmds[0] & WHD_COL_SEG_MEMCPY_SOURCE) printf(" (copyable)");
            printf("\n");
            y = ey + length;
        }
    };
    dump_original = [&]() {
        dump_segs(original_segs, "original");
    };
#endif
    // first try and find patches that have been split by one or more patches in front... these fragments can be drawn
    // as a single run, then the one or more patches drawn over the front. (funnily enough this is how things are
    // specified in the first place!!!)
    auto segs = original_segs;
    auto original_result = draw_column(original_segs);
    for(int candidate=1; candidate < (int)segs.size(); ) {
        bool candidate_removed = false;
        for (int match=0; match < candidate && !candidate_removed; match++) {
            if (segs[candidate].second[0] == segs[match].second[0] &&
                segs[candidate].second[2] == segs[match].second[2]) {
                // ^ we are the same column and xoffset
                if (segs[candidate].second[3] == segs[match].second[3] + segs[candidate].first - segs[match].first) {
                    // ^ we have what seems like an obscured piece.... can we just draw the top one bigger underneath?
                    auto new_segs = std::vector<seg>();
                    new_segs.insert(new_segs.end(), segs.begin(), segs.begin() + candidate);
                    new_segs.insert(new_segs.end(), segs.begin() + candidate + 1, segs.end());
                    new_segs[match].second[1] = segs[candidate].first + segs[candidate].second[1] - segs[match].first;
                    if (original_result == draw_column(new_segs)) {
                        segs = new_segs;
#if DEBUG_TEXTURE_OPTIMIZATION
                        dump_segs(segs, "coalesce same part of same obscured %d and %d", match, candidate);
#endif
                        candidate_removed = true;
                    }
                }
            }
        }
        if (!candidate_removed) candidate++;
    }

    // we often repeat patches vertically, so look to replace segments with memcpy from another one
    for(int candidate=1; candidate < (int)segs.size(); candidate++) {
        for (int match=0; match < candidate; match++) {
            if (!(segs[candidate].second[0] & 0x80) && !(segs[match].second[0] & 0x80) &&
                (segs[candidate].second[0] & 0xf) == (segs[match].second[0] & 0xf) &&
                segs[candidate].second[2] == segs[match].second[2]) {
                // ^ we are the same column and xoffset and neither is a memcpy
                int to_data_top = segs[candidate].second[3];
                int to_data_bottom = to_data_top + segs[candidate].second[1];
                int from_data_top = segs[match].second[3];
                int from_data_bottom = from_data_top + segs[match].second[1];
                if (to_data_top >= from_data_top) {
                    auto new_segs = std::vector<seg>();
                    new_segs.insert(new_segs.end(), segs.begin(), segs.begin() + candidate);
                    if (to_data_bottom <= from_data_bottom) {
//                        printf("EASY COPY POSSIBILITY\n");
                    } else {
//                        printf("HARD COPY POSSIBILITY\n");
                        new_segs[match].second[1] = to_data_bottom - from_data_top;
                    }
                    new_segs[match].second[0] |= WHD_COL_SEG_MEMCPY_SOURCE;
                    new_segs.emplace_back(segs[candidate].first, std::vector<uint8_t>{
                        0x80, segs[candidate].second[1], (uint8_t)(segs[match].first + to_data_top - from_data_top)
                    });
                    new_segs.insert(new_segs.end(), segs.begin() + candidate + 1, segs.end());
                    if (original_result == draw_column(new_segs)) {
#if DEBUG_TEXTURE_OPTIMIZATION
                        dump_segs(segs, "memcpy from %d to %d", match, candidate);
#endif
                        segs = new_segs;
                    } else {
                        new_segs[candidate].second[0] |= WHD_COL_SEG_MEMCPY_IS_BACKWARDS;
                        if (original_result == draw_column(new_segs)) {
#if DEBUG_TEXTURE_OPTIMIZATION
                            dump_segs(segs, "backwards memcpy from %d to %d", match, candidate);
#endif
                            segs = new_segs;
                        } else if (candidate > match + 1){
                            // ok lets try doing the copy straight after
                            new_segs.clear();
                            new_segs.insert(new_segs.end(), segs.begin(), segs.begin() + match + 1);
                            if (to_data_bottom <= from_data_bottom) {
//                                printf("EASY COPY POSSIBILITY\n");
                            } else {
//                                printf("HARD COPY POSSIBILITY\n");
                                new_segs[match].second[1] = segs[candidate].first + segs[candidate].second[1] - segs[match].first;
                            }
                            new_segs[match].second[0] |= WHD_COL_SEG_MEMCPY_SOURCE;
                            new_segs.emplace_back(segs[candidate].first, std::vector<uint8_t>{
                                    0x80, segs[candidate].second[1], (uint8_t)(segs[match].first + to_data_top - from_data_top)
                            });
                            new_segs.insert(new_segs.end(), segs.begin() + match + 1 , segs.begin() + candidate);
                            new_segs.insert(new_segs.end(), segs.begin() + candidate + 1, segs.end());
                            if (original_result == draw_column(new_segs)) {
#if DEBUG_TEXTURE_OPTIMIZATION
                                dump_segs(segs, "memcpy from %d to %d but with the latter moved next to the former", match, candidate);
#endif
                                segs = new_segs;
                            } else {
                                new_segs[candidate].second[0] |= WHD_COL_SEG_MEMCPY_IS_BACKWARDS;
                                if (original_result == draw_column(new_segs)) {
#if DEBUG_TEXTURE_OPTIMIZATION
                                    dump_segs(segs, "memcpy backwards from %d to %d but with the latter moved next to the former", match, candidate);
#endif
                                    segs = new_segs;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // todo use flags here
    // we often have things like 16 of the same patch tiled vertically. this will now be 1 regular segment and 15 memcpy segments.
    //   we can often do better by collapsing this into fewer memcpys (in this case 1)
    for(int candidate=2; candidate < (int)segs.size(); ) {
        bool candidate_removed = false;
        if (segs[candidate].second[0] & 128) {
            for(int earlier=1; earlier<candidate; earlier++) {
                if (segs[earlier].second[0] & 128) {
                    // note candidates are not necessarily in monotonic y-order due to previous optimizations
                    if ((segs[candidate].first > segs[earlier].first)) {
                        // don't think too much just try it
                        auto new_segs = std::vector<seg>();
                        new_segs.insert(new_segs.end(), segs.begin(), segs.begin() + candidate);
                        new_segs.insert(new_segs.end(), segs.begin() + candidate + 1, segs.end());
                        new_segs[earlier].second[1] += segs[candidate].second[1];
                        // call with second arg = true (lenient) as we may produce an invalid memcpy here, but it is hard to check without doing the same work that draw_column does
                        if (original_result == draw_column(new_segs, true)) {
                            // hail mary success!
                            segs = new_segs;
#if DEBUG_TEXTURE_OPTIMIZATION
                            dump_segs(segs, "memcpy coalesce!");
#endif
                            candidate_removed = true;
                            break;
                        }
                    }
                }
            }
        }
        if (!candidate_removed) candidate++;
    }
    if (segs != original_segs) {
#if DEBUG_TEXTURE_OPTIMIZATION
        dump_segs(segs, "optimized");
#endif
        assert(original_result == draw_column(segs));
        cmds.clear();
        y = 0;
        for(int i=0;i<(int)segs.size();i++) {
            const auto &ey = segs[i].first;
            const auto &ecmds = segs[i].second;
            int length = ecmds[1];
            cmds.push_back(ecmds[0] | (y != ey ? 0x40 : 0));
            cmds.push_back((length-1) | (i == (int)segs.size() - 1 ? 0x80 : 0));
            if (y != ey) {
                cmds.push_back(ey);
            }
            cmds.insert(cmds.end(), ecmds.begin() + 2, ecmds.end());
            y = ey + length;
        }
        seg_count = segs.size();
    }
    return cmds;
}

std::vector<uint8_t> image_to_patch(std::vector<int> image, int w, int h) {
    std::vector<uint8_t> patch_data;
    std::vector<uint8_t> offset_data;
    patch_header header = {
            .width = (short)w,
            .height = (short)h,
            .leftoffset = 0,
            .topoffset = 0,
    };
    append_field(patch_data, header);
    std::vector<uint8_t> post_data;
    for (int col = 0; col < w; col++) {
        uint32_t off = 8 + 4 * w + post_data.size();
        append_field(offset_data, off);
        int y=0;
        do {
            while (y < h && image[y*w+col] == -1) y++;
            if (y == h) {
                post_data.push_back(0xff);
                break;
            }
            int base = y;
            while (y < h && image[y*w+col] != -1) y++;
            post_data.push_back(base);
            assert(y > base);
            post_data.push_back(y - base);
            post_data.push_back(0);
            for(int yy=base; yy<y; yy++) {
                post_data.push_back(image[col  + yy*w]);
            }
            while (y < h && image[y*w+col] == -1) y++;
            post_data.push_back(0);
        } while (true);
    }
    patch_data.insert(patch_data.end(), offset_data.begin(), offset_data.end());
    patch_data.insert(patch_data.end(), post_data.begin(), post_data.end());
    return patch_data;
}

texture_index convert_textures(wad &wad) {
    lump pnames;
    std::vector<int> pname_lookup;
    if (wad.get_lump("pnames", pnames)) {
        int count = get_field<int>(pnames.data, 0);
        char *data = (char *) pnames.data.data();
        printf("Patch count %d\n", count);
        pname_lookup.resize(count);
        for (int i = 0; i < count; i++) {
            auto name = wad::wad_string(data + 4 + i * 8);
            pname_lookup[i] = wad.get_lump_index(name);
//            printf("patch %d %s\n", i, name.c_str());
        }
        touched[pnames.num]=TOUCHED_PNAMES;
    } else {
        fail("No PNAMES lump");
    }

    statsomizer widths("texture width");
    statsomizer heights("texture height");
    texture_index tex_index;
    lump tex1_lump, tex2_lump;
    if (!wad.get_lump("texture1", tex1_lump)) {
        fail("no texture1 lump");
    }
    touched[tex1_lump.num]=TOUCHED_TEX_METADATA;
    auto numtextures1 = get_field<int>(tex1_lump.data, 0);
    int numtextures = numtextures1;
    if (wad.get_lump("texture2", tex2_lump)) {
        numtextures += get_field<int>(tex2_lump.data, 0);
        clear_lump(wad, "texture2", TOUCHED_TEX_METADATA);
    }

    int directory_offset = 4;
    lump *tex_lump = &tex1_lump;
    printf("Num textures: %d\n", numtextures);
    int new_texture_count = special_textures.size();
    std::vector<uint8_t> all_metadata;
    std::map<std::string, int> orig_tex_numbers;
    for (int i = 0; i < numtextures; i++, directory_offset += 4) {
        if (i == numtextures1) {
            tex_lump = &tex2_lump;
            directory_offset = 4;
        }

        int offset = get_field<int>(tex_lump->data, directory_offset);
        if (offset > (int) tex_lump->data.size())
            fail("bad texture directory");

        auto mtexture = get_field_inc<maptexture_t>(tex_lump->data, offset);
        std::string name = to_lower(wad::wad_string(mtexture.name));
        orig_tex_numbers[name] = i;
        auto it = tex_index.lookup.find(name);
        if (it != tex_index.lookup.end()) {
            fail("Duplicate texture %s\n", name.c_str());
        }

        // we want to reassign the texture indexes;
        auto it2 = std::find(special_textures.begin(), special_textures.end(), to_upper(name));
        int index;
        if (it2 != special_textures.end()) {
            index = it2 - special_textures.begin();
        } else {
            index = new_texture_count++;
        }
        tex_index.lookup[name] = index;
        tex_index.textures.resize(new_texture_count);
        auto &tex_whd = tex_index.textures[index].whd;
        tex_whd.width = mtexture.width;
        int pow2 = 1u << (31 - __builtin_clz(tex_whd.width));
        // engine actually rounds down texture sizes to power of 2 (notably all textures are power of 2 except AASTINKY
        if (mtexture.width != pow2) {
//            printf("WARNING non pow2 tex width truncated %s\n", name.c_str());
            tex_whd.width = pow2;
        }
        if (mtexture.height >= 256) {
            fail("Texture too tall");
        }
        tex_whd.height = mtexture.height;
        tex_whd.patch0 = 0;
        tex_whd.patch_count = mtexture.patchcount;
        printf("Texture %d %s %dx%d, %d patches\n", index, name.c_str(), mtexture.width, mtexture.height, mtexture.patchcount);
        widths.record(mtexture.width);
        heights.record(mtexture.height);
        lump patch;
        std::array<int, 256> colorCounts{};
        std::vector<mappatch_t> mpatches;

#if TEXTURE_PIXEL_STATS
        static std::vector<int> pixel_usage;
        static std::vector<int> column_usage;
        pixel_usage.clear();
        column_usage.clear();
        pixel_usage.resize(tex_whd.width * tex_whd.height);
        column_usage.resize(tex_whd.width);
#endif
        int solid_patches = 0;
        std::vector<std::vector<int>> column_pixels(mtexture.width);
        std::vector<int> pixel_patch(mtexture.width * mtexture.height);
        std::fill(pixel_patch.begin(), pixel_patch.end(), -1);

        for (int j = 0; j < mtexture.patchcount; j++) {
            auto mpatch = get_field_inc<mappatch_t>(tex_lump->data, offset);
            mpatches.push_back(mpatch);
            if (pname_lookup[mpatch.patch] == -1) {
                fail("Missing pname mapping for patch %d in texture %d\n", pname_lookup[j], j);
            }
            if (wad.get_lump(pname_lookup[mpatch.patch], patch)) {
                patch_for_each_pixel(patch, [&](uint8_t pixel) {
                    colorCounts[pixel]++;
                });
                bool patch_solid = true;
#if TEXTURE_PIXEL_STATS
                // render every pixel
                const patch_header *ph = (const patch_header *) patch.data.data();
                for (int col = 0; col < ph->width; col++) {
                    int xp = col + mpatch.originx;
                    if (xp < 0) continue;
                    if (xp >= tex_whd.width) break;
                    if (column_pixels[xp].empty()) {
                        column_pixels[xp].resize(tex_whd.height);
                        std::fill(column_pixels[xp].begin(), column_pixels[xp].end(), -1);
                    }
                    uint32_t col_offset = *(uint32_t *) (patch.data.data() + 8 + col * 4);
                    const uint8_t *post = patch.data.data() + col_offset;
                    bool hit_column = false;
                    int touched_pixels = 0;
                    int y0 = mpatch.originy;
                    if (y0 < -128 || y0 > 127) {
                        fail("originY is %d\n", y0);
                    }
                    if (y0 < 0) y0 = 0;
                    if (y0 > tex_whd.height) y0 = tex_whd.height;
                    int y1 = mpatch.originy + ph->height;
                    if (y1 < 0) y1 = 0;
                    if (y1 > tex_whd.height) y1 = tex_whd.height;
                    while (post[0] != 0xff) {
                        int yp = post[0] + mpatch.originy;
                        for (int k = 3; k < 3 + post[1]; k++, yp++) {
                            if (yp >= tex_whd.height) break;
                            if (yp < 0) continue;
                            pixel_usage[yp * tex_whd.width + xp]++;
                            pixel_patch[yp * tex_whd.width + xp] = j;
                            column_pixels[xp][yp] = post[k];

                            touched_pixels++;
                            if (!hit_column) {
                                column_usage[xp]++;
                                hit_column = true;
                            }
                        }
                        post += 4 + post[1];
                    }
                    if (touched_pixels != y1 - y0) {
                        patch_solid = false;
                    }
                }
                if (patch_solid) solid_patches++;
#endif
                printf("  Patch %d (%d): %d at %d,%d %dx%d solid = %d\n", j, pname_lookup[mpatch.patch], mpatch.patch,
                       mpatch.originx, mpatch.originy, ph->width, ph->height, patch_solid);
            } else {
                // seems unlike!y!
                fail("Patch %d missing\n", pname_lookup[j]);
            }
        }
#if TEXTURE_PIXEL_STATS
        for (int x = 0; x < tex_whd.width; x++) {
            texture_column_patches.record(column_usage[x]);
            texture_column_patches1.record(column_usage[x] == 1);
        }
        bool had_transparent = false;
        for (auto &xx : pixel_usage) {
            if (!xx) {
                had_transparent = true;
                break;
            }
        }
        texture_transparent.record(had_transparent);
        if (had_transparent) {
            texture_transparent_patch_count.record(mpatches.size());
        }
#endif

        if (mpatches.size() == 1) {
            texture_single_patch.record(1);
            if (mpatches[0].originx == 0 && mpatches[0].originy == 0) {
                texture_single_patch00.record(1);
                tex_whd.patch_count = 0; // special marker for unmoved patch
                tex_whd.patch0 = pname_lookup[mpatches[0].patch];
            } else {
                texture_single_patch00.record(0);
                if (had_transparent) {
                    printf("WARNING: Found a transparent not at offset 0,0 %s\n", name.c_str());
                }
            }
        } else {
            texture_single_patch.record(0);
            if (had_transparent) {
                printf("Found a transparent with > 1 patch\n");
            }
        }
        //tex_whd.approx_color = std::max_element(colorCounts.begin(), colorCounts.end()) - colorCounts.begin();
        int max_unique_col_patches = 0;
        int max_seg_count = 0;
        std::vector<uint8_t> metadata;
        if (tex_whd.patch_count) {
            std::vector<int> local_patches;
            std::vector<std::vector<uint8_t>> patch_runs(tex_whd.width);
            for (int x = 0; x < tex_whd.width; x++) {
                auto &col_patch_runs = patch_runs[x];
                std::set<int> unique_col_patches;
                int last = -1;
                int run = 0;
                int localp;
                bool had_col_transparent = false;
                for (int y = 0; y < tex_whd.height; y++) {
                    int p = pixel_patch[y * tex_whd.width + x];
                    if (p != last) {
                        if (run) {
                            if (localp == 0xff) {
                                had_col_transparent = true;
                            } else {
                                col_patch_runs.push_back(localp); // which patch
                                if (run > 128) fail("run is too long %d\n", run);
                                col_patch_runs.push_back(run - 1);
                                col_patch_runs.push_back((uint8_t) mpatches[last].originx);
                                col_patch_runs.push_back((int8_t) (y-run)-mpatches[last].originy);
                            }
                        }
                        last = p;
                        if (p != -1) {
                            auto f = std::find(local_patches.begin(), local_patches.end(), pname_lookup[mpatches[p].patch]);
                            localp = f - local_patches.begin();
                            if (f == local_patches.end()) {
                                local_patches.push_back( pname_lookup[mpatches[p].patch]);
                            }
                            unique_col_patches.insert(localp);
                        } else {
                            localp = 0xff;
                        }
                        run = 1;
                    } else {
                        run++;
                    }
                }
                if (run) {
                    // hack for sky - we allow transparency at the bottom of a texture
                    if (localp == 0xff) {
//                        had_col_transparent = true;
                        if (!col_patch_runs.empty()) {
                            col_patch_runs[col_patch_runs.size() - 3] |= 0x80;
                        }
                    } else {
                        col_patch_runs.push_back(localp); // which patch
                        if (run > 128) fail("run is too long %d\n", run);
                        col_patch_runs.push_back(128 | (run - 1));
                        col_patch_runs.push_back((uint8_t) mpatches[last].originx);
                        col_patch_runs.push_back((int8_t) (tex_whd.height-run)-mpatches[last].originy);
                    }
                }
                if (had_col_transparent) {
                    if (col_patch_runs.size() != 4 && !col_patch_runs.empty()) {
                        printf("WARNING: Can't mix transparency with anything but single patch column, will be undefined. tex=%s col %d\n", name.c_str(), x);
                    }
                }
                if (col_patch_runs.size() == 4) {
                    col_patch_runs[3] = 0; // force originy to 0 for single patch column to match DOOM R_GenerateLookup behavior
                } else if (!had_transparent) {
                    // only interested in duplicate columns if they aren't already single patch (which are de-duplicated nyway)
                    for (int x2 = 0; x2 < x; x2++) {
                        if (column_pixels[x] == column_pixels[x2]) {
                            // todo encode "same" columns? ...
                            //printf("OOH %d == %d\n", x, x2);
                            break;
                        }
                    }
                }
                max_unique_col_patches = std::max(max_unique_col_patches, (int)unique_col_patches.size());
            }
            assert(local_patches.size() && local_patches.size() <= tex_whd.patch_count);
            printf("   local patch size %d\n", (int)local_patches.size());
            tex_whd.patch_count = local_patches.size();
            if (local_patches.size() > 16) {
                fail("too many local patches %d\n", (int) local_patches.size());
            }
            for(int n=0;n<tex_whd.patch_count;n++) {
                printf("      %d: %d\n", n, local_patches[n]);
                metadata.push_back(local_patches[n]&0xff);
                metadata.push_back(local_patches[n]>>8);
            }
            std::vector<uint8_t> single_patch_metadata;
            int single_patch_last = -1;
            int single_patch_start = 0;
            int single_patch_xoffset_last = 0;
            int single_patch_xoffset = 0;
            int single_patch_lpn = 0;
            int single_patch_lpn_last = 0;
            for(int x = 0; x<tex_whd.width; x++) {
                bool zero_patch = patch_runs[x].empty();
                int single_or_zero_patch = zero_patch || !!(patch_runs[x][1] & 0x80);
                single_patch_xoffset = zero_patch ? 0 : patch_runs[x][2] * single_or_zero_patch; // we set single_patch_xoffset to 0 when not single patch since we don't care about its value (and that changing)
                single_patch_lpn = zero_patch ? -1 : patch_runs[x][0];
                if (single_or_zero_patch != single_patch_last || single_patch_xoffset != single_patch_xoffset_last || single_patch_lpn != single_patch_lpn_last || x - single_patch_start == 0x80) {
                    if (x != single_patch_start) {
                        single_patch_metadata.push_back(single_patch_last << 7 | ((x-single_patch_start-1)&0x7f));
                        if (single_patch_last) {
                            single_patch_metadata.push_back(single_patch_lpn_last);
                            single_patch_metadata.push_back(single_patch_xoffset_last);
                        }
                        single_patch_start = x;
                    }
                    single_patch_last = single_or_zero_patch;
                    single_patch_xoffset_last = single_patch_xoffset;
                    single_patch_lpn_last = single_patch_lpn;
                }
            }
            if (single_patch_start < tex_whd.width) {
                single_patch_metadata.push_back(single_patch_last << 7 | ((tex_whd.width - single_patch_start - 1) & 0x7f));
                if (single_patch_last) {
                    single_patch_metadata.push_back(single_patch_lpn_last);
                    single_patch_metadata.push_back(single_patch_xoffset_last);
                }
            }
            single_patch_metadata_size.record(single_patch_metadata.size());
            metadata.insert(metadata.end(), single_patch_metadata.begin(), single_patch_metadata.end());
            for(int x = 0; x < (int)patch_runs.size(); ) {
                int x0 = x;
                // collect any zero/single patch columns
                for (; x0 < (int) patch_runs.size(); x0++) {
                    if (!(patch_runs[x0].empty() || (patch_runs[x0][1] & 0x80))) {
                        break;
                    }
                }
                if (x0 != x) {
                    metadata.push_back(x0 - x - 1);
                    // bit of a waste of space, but keeps things length and "end" bit in the same place
                    metadata.push_back(0xff);
                    metadata.push_back(0x80);
//                    printf("%d + %d single patch col(s))\n", x, x0-x);
                    x = x0;
                }
                int x2 = x + 1;
                if (x2 > (int) patch_runs.size()) break;
                for (; x2 < (int) patch_runs.size(); x2++) {
                    if (patch_runs[x] != patch_runs[x2]) break;
                }
                if (x2 - x > 256) x2 = x + 256;
                for (int e = 0; e < (int) patch_runs[x].size(); e += 4) {
                    int xoffset = (x - patch_runs[x][e + 2]) & 0xff;
                    patch_runs[x][e + 2] = xoffset;
                }
                bool have_repeat = false; // same source column from same patch multiple times in the same texture colum
                // lets not go crazy with re-ordering or anything
                for (int e = 0; e < (int) patch_runs[x].size(); e += 4) {
                    for (int e2 = 0; e2 < e; e2 += 4) {
                        if (patch_runs[x][e2] == patch_runs[x][e] && patch_runs[x][e2 + 2] == patch_runs[x][e + 2]) {
                            have_repeat = true;
                            break;
                        }
                    }
                }
                int seg_count = patch_runs[x].size() / 4;
                if (have_repeat) {
                    patch_runs[x] = optimize_column(name, x, patch_runs[x], local_patches, tex_whd.height, seg_count);
                }
                max_seg_count = std::max(seg_count, max_seg_count);
                metadata.push_back(x2 - x - 1);
                metadata.insert(metadata.end(), patch_runs[x].begin(), patch_runs[x].end());
                x = x2;
            }
        }
        printf("  Solids %d/%d max segs %d max unique col patches %d\n", solid_patches, mtexture.patchcount, max_seg_count, max_unique_col_patches);
        if (tex_whd.patch_count && solid_patches != mtexture.patchcount) {
            printf("warning: multi patch transparent texture tex=%s\n", name.c_str()); // todo is this actually allowed on a per column basis (i.e. mix compostie cols with transparent non composite cols?)
        }
        if (max_seg_count > WHD_MAX_COL_SEGS || max_unique_col_patches > WHD_MAX_COL_UNIQUE_PATCHES) {
            auto new_patch = get_free_lump(wad);
            char lname[32];
            sprintf(lname, "_SYN%d", new_patch.num);
            new_patch.name = lname;
            printf("warning: overly complex texture %s rendering as new patch %d\n", name.c_str(), new_patch.num);
            tex_whd.patch_count = 0;
            tex_whd.patch0 = new_patch.num;
            metadata.clear();
            std::vector<int> pixels(tex_whd.width * tex_whd.height);
            for(uint x=0;x<tex_whd.width;x++) {
                for (uint y = 0; y < tex_whd.height; y++) {
                    pixels[x+y*tex_whd.width] = column_pixels[x][y];
                }
            }
            new_patch.data = image_to_patch(pixels, tex_whd.width, tex_whd.height);
            convert_patch(wad, new_patch.num, new_patch);
        } else {
            texture_col_metadata.record((int) metadata.size());
        }
        if (tex_whd.patch_count) {
            assert(metadata.size());
            tex_whd.metdata_offset = all_metadata.size();
            all_metadata.insert(all_metadata.end(), metadata.begin(), metadata.end());
        }
    }
    static_assert(sizeof(whdtexture_t) == 6, ""); // want to check it is naturally aligned
    std::vector<uint8_t> whd_textures;//( tex_index.textures.size() * sizeof(whdtexture_t));
    whd_textures.push_back(tex_index.textures.size() & 0xff);
    whd_textures.push_back(tex_index.textures.size() >> 8);
    uint metadata_start = tex_index.textures.size() * sizeof(whdtexture_t); // relative to the texture array not the beginning of whd_textures
    for (auto &t : tex_index.textures) {
        if (t.whd.patch_count) {
            int new_offset = metadata_start + t.whd.metdata_offset;
            assert(new_offset < 65536);
            t.whd.metdata_offset = new_offset;
        }
        append_field(whd_textures, t);
    }
    assert(whd_textures.size() == metadata_start + 2);
    whd_textures.insert(whd_textures.end(), all_metadata.begin(), all_metadata.end());
    tex_orig_size.record(tex1_lump.data.size());
    tex1_lump.data = whd_textures;
    tex_new_size.record(tex1_lump.data.size());
    printf("WHD TEXTURE DATA SIZE %d\n", (int) tex1_lump.data.size());
    wad.update_lump(tex1_lump);
    widths.print_summary();
    heights.print_summary();
    for(const auto & e: tex_index.lookup) {
        printf("%s : %d\n", e.first.c_str(), e.second);
    }
    for(auto &e : tex_animdefs) {
        auto it1 = tex_index.lookup.find(to_lower(e.first));
        auto it2 = tex_index.lookup.find(to_lower(e.last));
        if (it1 != tex_index.lookup.end() && it2 != tex_index.lookup.end()) {
            int remapped_range = it2->second - it1->second;
            int orig_range = orig_tex_numbers[to_lower(e.last)] - orig_tex_numbers[to_lower(e.first)];
            printf("HAVE TEX ANIM %s -> %s range %d\n", e.first, e.last, remapped_range);
            if (remapped_range != orig_range) {
                fail("Mismatch in anim textures original range %d not equal to remapped range %d", orig_range, remapped_range);
            }
        } else {
            printf("DONT HAVE TEX ANIM %s -> %s\n", e.first, e.last);
        }
    }
    return tex_index;
}

statsomizer flat_rawsize("Flat raw size");
statsomizer flat_c2size("Flat c2 size");
statsomizer flat_colors("Flat colors");
statsomizer flat_under_colors[] = {
        statsomizer("2 color flat"),
        statsomizer("4 color flat"),
        statsomizer("8 color flat"),
        statsomizer("16 color flat"),
        statsomizer("32 color flat"),
        statsomizer("64 color flat"),
        statsomizer("128 color flat"),
        statsomizer("256 color flat"),
        };

void convert_flats(wad &wad) {
    int fstart = wad.get_lump_index("f_start");
    int fend = wad.get_lump_index("f_end");
    if (fstart == -1 || fend == -1) {
        fail("missing f_start/f_end marker");
    }
    // it is too painful to reorder flats, so we keep a mapping from the special_flat indexes to the real indexes
    std::vector<uint8_t> special_to_flat(special_flats.size());
    std::fill(special_to_flat.begin(), special_to_flat.end(), 0xff);
    std::vector<uint8_t> flat_to_special;
    for (int f = fstart+1; f < fend; f++) {
        // todo we only need flats mentioned in sectors (or well known)
        lump lump;
        if (wad.get_lump(f, lump)) {
            auto ff = std::find(special_flats.begin(), special_flats.end(), to_upper(lump.name));
            if (ff != special_flats.end()) {
//                printf("FOUND SPECIAL %d %s at %d\n", (int)(ff - special_flats.begin()), lump.name.c_str(), f-fstart-1);
                special_to_flat[ff - special_flats.begin()] = f - fstart - 1;
                flat_to_special.push_back(ff - special_flats.begin());
            } else {
                flat_to_special.push_back(0xff);
            }
            std::set<uint8_t> colors;
            for (const auto &p: lump.data) colors.insert(p);
            assert(lump.data.size() == 64 * 64);
            std::vector<int16_t> pix;
            for (int y = 0; y < 64; y++) {
                for (int x = 0; x < 64; x++) {
                    pix.push_back(lump.data[y * 64 + x]);
                }
            }
            uint best = std::numeric_limits<uint>::max();
            std::vector<std::shared_ptr<byte_vector_bit_output>> zposts;
            std::vector<std::shared_ptr<byte_vector_bit_output>> best_zposts;
            std::shared_ptr<byte_vector_bit_output> decoder_output;
            std::shared_ptr<byte_vector_bit_output> best_decoder_output;
            uint decoder_size;
            uint best_decoder_size;
            int choice = 0;
            auto choose = [&](int c, uint size) {
                if (size < best) {
                    choice = c;
                    best = size;
                    best_zposts = zposts;
                    best_decoder_output = std::make_shared<byte_vector_bit_output>(*decoder_output);
                    best_decoder_size = decoder_size;
                }
                return size;
            };
            std::vector<int> same;
            bool have_same;
            auto posts = to_merged_posts(pix, 64, 64, same, have_same);
            uint s1 = choose(0, consider_compress_pixels_only(lump.name, posts, decoder_output, zposts, 64, 64, decoder_size)); ((void)s1);
#if !USE_PIXELS_ONLY_FLAT
            uint s2 = choose(1, consider_compress3(lump.name, posts, decoder_output, zposts, 64, 64));
#endif
            if (best_decoder_size > WHD_FLAT_DECODER_MAX_SIZE) {
                fail("flat decoder is too big %s %d", lump.name.c_str(), best_decoder_size);
            }
            fwinners[choice]++;
            flat_rawsize.record(4096);
            if (have_same) {
                int savings = 0;
                for (int i = 0; i < 64; i++) {
                    if (same[i]) {
                        savings += zposts[same[i] - 1]->bit_size();
                        savings -= 1 + bitcount8_table[i];
                    }
                }
                if (savings < 0) have_same = false;
                flat_have_same_savings.record(have_same);
            }
            byte_vector_bit_output final_bo;
#if !USE_PIXELS_ONLY_FLAT
            assert(choice < 2);
            final_bo.write(bit_sequence(choice, 1));
#endif
            decoder_output->write_to(final_bo);
            final_bo.write(bit_sequence(have_same, 1));
            for (int x = 0; x < 64; x++) {
                if (have_same) {
                    final_bo.write(bit_sequence(same[x] != 0, 1));
                    if (same[x]) {
                        assert(!zposts[x]->bit_size());
                        assert(same[x] - 1 < x);
                        assert(!same[same[x] - 1]);
                        // todo down one
                        final_bo.write(bit_sequence(same[x] - 1, bitcount8_table[x]));
                    } else {
                        zposts[x]->write_to(final_bo);
                    }
                } else {
                    zposts[x]->write_to(final_bo);
                }
            }
            compressed.insert(f);
            touched[f] = TOUCHED_FLAT;
            lump.data = final_bo.get_output();
            flat_c2size.record(lump.data.size());
            wad.update_lump(lump);
            flat_colors.record(colors.size());
            uint x = colors.size();
            if (x) x--;
            x = 31 - __builtin_clz(x);
            assert(x < 8);
            flat_under_colors[x].record(colors.size());
        } else {
            flat_to_special.push_back(0xff);
        }
    }
    lump fstart_lump;
    wad.get_lump("f_start", fstart_lump);
    fstart_lump.data = special_to_flat;
    fstart_lump.data.insert(fstart_lump.data.end(), flat_to_special.begin(), flat_to_special.end());
    wad.update_lump(fstart_lump);
}

void convert_demo(wad & wad, lump& l) {
    touched[l.num] = "Demo";
    compressed.insert(l.num);
    name_required.insert(l.name);
    using wap = symbol_sink<huffman_params<uint8_t>, std::shared_ptr<byte_vector_bit_output>,true>;
    wap fb_delta("FwdBack Delta");
    wap strafes("Strafe");
    wap turn_delta("Turn Delta");
    wap buttonss("Buttons");
    wap changes("Changes");
    sink_wrappers<std::shared_ptr<byte_vector_bit_output>> wrappers{
                                                                    fb_delta,
                                                                    strafes,
                                                                    turn_delta,
                                                                    buttonss,
                                                                    changes};

    auto bo = std::make_shared<byte_vector_bit_output>();
    statsomizer all_same("All same");
    statsomizer all_same_1("All sameNB");
    for(int pass=0;pass<2;pass++) {
        uint pos = 13;
        const auto &data = l.data;
        int last_fb = 0;
        int last_turn = 0;
        while (pos < data.size()) {
            int8_t fb = data[pos++];
            if (fb == -128) {
                changes.output(16); break;
                break;
            }
            int8_t strafe = data[pos++];
            int8_t turn = data[pos++];
            uint8_t buttons = data[pos++];
            changes.output((last_fb != fb) | ((strafe!=0)<<1) | ((last_turn!=turn)<<2) | ((buttons!=0)<<3));
            if (last_fb != fb) {
                fb_delta.output(to_zig(fb - last_fb));
            }
            if (strafe != 0) {
                strafes.output(to_zig(strafe));
            }
            if (last_turn != turn) {
                turn_delta.output(to_zig(turn - last_turn));
            }
            if (buttons != 0) {
                buttonss.output(buttons);
            }
            last_fb = fb;
            last_turn = turn;
        }
        assert(pos == data.size());
        if (!pass) {
            wrappers.begin_output(bo);
            output_min_max_best(bo, changes.huff);
            output_min_max_best(bo, fb_delta.huff);
            output_min_max_best(bo, strafes.huff);
            output_min_max_best(bo, turn_delta.huff);
            output_min_max_best(bo, buttonss.huff);
        }
    }
    all_same_1.print_summary();
    all_same.print_summary();
    auto result = bo->get_output();
    printf("%d -> %d\n", (int)l.data.size(), (int)result.size());
    demo_size_orig.record(l.data.size());
    demo_size.record(result.size());
    uint dsize = decoder_size(changes.huff) + decoder_size(fb_delta.huff) + decoder_size(strafes.huff) + decoder_size(turn_delta.huff) + decoder_size(buttonss.huff);
    assert(dsize < 256); // could make this bigger (oh no an extra 3 bytes), or divide by 2 but seems fine for now
    l.data.resize(13); // original header
    l.data.push_back(dsize);
    l.data.insert(l.data.end(), result.begin(), result.end());
    wad.update_lump(l);
}

int mus_total1, mus_total2;

// Structure to hold MUS file header
typedef struct {
    byte id[4];
    unsigned short scorelength;
    unsigned short scorestart;
} mus_header;

void convert_music(std::pair<const int, lump> &e) {
    touched[e.first] = TOUCHED_MUSIC;
    name_required.insert(e.second.name);
#if USE_MUSX
    auto &h = e.second.data;
    if (h[0] == 'M' && h[1] == 'U' && h[2] == 'S' && h[3] == 26) {
        auto new_mus = compress_mus(e);
        int original_size = e.second.data.size();
        h.clear();
        h.push_back('M');
        h.push_back('U');
        h.push_back('S');
        h.push_back('X');
        printf("Compress %s MUS %d -> %d\n", e.second.name.c_str(), original_size, (int) new_mus.size());
        mus_total1 += original_size;
        mus_total2 += new_mus.size();
        write_word(h, 4, new_mus.size());
        h.insert(h.end(), new_mus.begin(), new_mus.end());
        compressed.insert(e.first);
    } else {
        fail("Expected MUS track %s\n", e.second.name.c_str());
    }
#else
#error no longer supported
#endif
}


static int
adpcm_encode_data(const std::vector<int16_t> &in, std::vector<uint8_t> &out, int num_channels, int samples_per_block,
                  int lookahead, int noise_shaping) {
    size_t block_size = (samples_per_block - 1) / (num_channels ^ 3) + (num_channels * 4);
    int16_t *pcm_block = (int16_t *) malloc(samples_per_block * num_channels * 2);
    uint8_t *adpcm_block = (uint8_t *) malloc(block_size);
    void *adpcm_cnxt = NULL;

    if (!pcm_block || !adpcm_block) {
        fprintf(stderr, "could not allocate memory for buffers!\n");
        return -1;
    }

    int num_samples = in.size();
    int off = 0;
    while (num_samples) {
        int this_block_adpcm_samples = samples_per_block;
        int this_block_pcm_samples = samples_per_block;
        size_t num_bytes;

        if (this_block_pcm_samples > num_samples) {
            this_block_adpcm_samples = ((num_samples + 6) & ~7) + 1;
            block_size = (this_block_adpcm_samples - 1) / (num_channels ^ 3) + (num_channels * 4);
            this_block_pcm_samples = num_samples;
        }

        // if this is the last block and it's not full, duplicate the last sample(s) so we don't
        // create problems for the lookahead

        memcpy(pcm_block, in.data() + off * num_channels, this_block_pcm_samples * num_channels * 2);
        if (this_block_adpcm_samples > this_block_pcm_samples) {
            int16_t *dst = pcm_block + this_block_pcm_samples * num_channels, *src = dst - num_channels;
            int dups = (this_block_adpcm_samples - this_block_pcm_samples) * num_channels;
            while (dups--)
                *dst++ = *src++;
        }

        // if this is the first block, compute a decaying average (in reverse) so that we can let the
        // encoder know what kind of initial deltas to expect (helps initializing index)
        if (!adpcm_cnxt) {
            int32_t average_deltas[2];
            int i;

            average_deltas[0] = average_deltas[1] = 0;

            for (i = this_block_adpcm_samples * num_channels; i -= num_channels;) {
                average_deltas[0] -= average_deltas[0] >> 3;
                average_deltas[0] += abs((int32_t) pcm_block[i] - pcm_block[i - num_channels]);

                if (num_channels == 2) {
                    average_deltas[1] -= average_deltas[1] >> 3;
                    average_deltas[1] += abs((int32_t) pcm_block[i - 1] - pcm_block[i + 1]);
                }
            }

            average_deltas[0] >>= 3;
            average_deltas[1] >>= 3;

            adpcm_cnxt = adpcm_create_context(num_channels, lookahead, noise_shaping, average_deltas);
        }

        adpcm_encode_block(adpcm_cnxt, adpcm_block, &num_bytes, pcm_block, this_block_adpcm_samples);

        if (num_bytes != block_size) {
            fprintf(stderr, "\radpcm_encode_block() did not return expected value (expected %d, got %d)!\n",
                    (int) block_size, (int) num_bytes);
            return -1;
        }
        out.insert(out.end(), adpcm_block, adpcm_block + num_bytes);

        num_samples -= this_block_pcm_samples;
        off += this_block_pcm_samples;
    }

    if (adpcm_cnxt)
        adpcm_free_context(adpcm_cnxt);

    free(adpcm_block);
    free(pcm_block);
    return 0;
}

bool convert_sound(std::pair<const int, lump> &e) {
    auto &lump = e.second;
    const int lumplen = lump.data.size();
    const uint8_t *data = lump.data.data();
    // Check the header, and ensure this is a valid sound

    if (lumplen < 8
        || data[0] != 0x03 || data[1] != 0x00) {
        // Invalid sound
        return false;
    }

    name_required.insert(lump.name);
    // 16 bit sample rate field, 32 bit length field

    int samplerate = (data[3] << 8) | data[2];
    if (samplerate != 11025) {
        printf("oou %s %d\n", lump.name.c_str(), samplerate);
    }
    int length = (data[7] << 24) | (data[6] << 16) | (data[5] << 8) | data[4];

    // If the header specifies that the length of the sound is greater than
    // the length of the lump itself, this is an invalid sound lump

    // We also discard sound lumps that are less than 49 samples long,
    // as this is how DMX behaves - although the actual cut-off length
    // seems to vary slightly depending on the sample rate.  This needs
    // further investigation to better understand the correct
    // behavior.

    if (length > lumplen - 8 || length <= 48) {
        return false;
    }

    // The DMX sound library seems to skip the first 16 and last 16
    // bytes of the lump - reason unknown.

    std::vector<uint8_t> out;
    out.insert(out.end(), data, data + 8); // copy the original header
    out[1] = 0x80; // something difference

    data += 16;
    length -= 32;
    std::vector<int16_t> in(length);
    for (int i = 0; i < length; i++) {
        in[i] = (data[i] ^ 0x80) << 8;
    }

    int block_size = 128;
    int num_channels = 1;
    int lookahead = LOOKAHEAD;
    int samples_per_block = (block_size - num_channels * 4) * (num_channels ^ 3) + 1;
    if (adpcm_encode_data(in, out, num_channels, samples_per_block, lookahead,
            //NOISE_SHAPING_DYNAMIC
                          NOISE_SHAPING_OFF // noise shaping sounds worse at this low frequency (and we low pass
            // after upscalinga at runtime later anyway)
    ) < 0) {
        return false;
    }
    compressed.insert(e.first);
    sfx_orig_size.record(e.second.data.size());
    e.second.data = out;
    sfx_new_size.record(e.second.data.size());
    return true;
}

void ColorShiftPalette (byte *inpal, byte *outpal
        , int r, int g, int b, int shift, int steps)
{
    int	i;
    int	dr, dg, db;
    byte	*in_p, *out_p;

    in_p = inpal;
    out_p = outpal;

    for (i=0 ; i<256 ; i++)
    {
        dr = r - in_p[0];
        dg = g - in_p[1];
        db = b - in_p[2];

        out_p[0] = in_p[0] + dr*shift/steps;
        out_p[1] = in_p[1] + dg*shift/steps;
        out_p[2] = in_p[2] + db*shift/steps;

        in_p += 3;
        out_p += 3;
    }
}

#if 0
std::vector<uint8_t> png_to_patch(wad& wad, const char *prefix, const char *name) {
    std::vector<unsigned char> buffer;
    std::vector<unsigned char> image32;
    unsigned w, h;

    std::string filename = prefix;
    filename += name;
    filename += ".png";
    lodepng::load_file(buffer, filename); //load the image file with given filename
    lodepng::State state;
    unsigned error = lodepng::decode(image32, w, h, state, buffer);
    lump palette;
    std::map<uint32_t,int> palette_lookup;
    wad.get_lump("playpal", palette);
    for(int i=0;i<256;i++) {
        uint32_t pixel = palette.data[i*3] + (palette.data[i*3+1] << 8) + (palette.data[i*3+2] << 16) + 0xff000000u;
        palette_lookup[pixel]=i;
    }
    auto image = std::vector<int>(w*h);
    for(uint i = 0; i < w*h; i++) {
        if (image[i*4+3] == 0xff) {
            uint32_t col = image32[i*4] + (image32[i*4+1]<<8) + (image32[i*4+2]<<16) + (image32[i*4+3]<<24);
            image[i] = palette_lookup[col];
        } else {
            image[i] = -1;
        }
    }
    auto patch_data = image_to_patch(image, w, h);
    printf("const uint8_t lump_%s[] = {\n", name);
    for(int i=0;i<(int)patch_data.size();i+=24) {
        printf("    ");
        for(int j=i;j<std::min(i+24, (int)patch_data.size());j++) {
            printf("0x%02x, ", patch_data[j]);
        }
        printf("\n");
    }
    printf("}\n\n");
    return patch_data;
}
#endif

int main(int argc, const char **argv) {
    hash = 0;
    int argn = 1;
    auto next_arg = [&](bool required = true) {
        if (argn >= argc) {
            if (required) usage();
            return (const char *)NULL;
        }
        if (!strcmp(argv[argn], "-no-super-tiny")) {
            super_tiny = false;
        }
        return argv[argn++];
    };
    try {
        const char *wad_name = next_arg();
        auto wad = wad::read(wad_name);
        int size = 0;
        for(const auto &e : wad.get_lumps()) {
            size += e.second.data.size();
        }
        printf("LUMPS ORIG SIZE %d\n", size);
        auto output_filename = next_arg();
        next_arg(false); // check for more options
        const char *pos = std::max(strrchr(wad_name, '\\'), strrchr(wad_name, '/'));
        if (pos) pos++;
        else pos = wad_name;
        wad.set_name(pos);
        for (auto &e : wad.get_lumps()) {
            std::string dpsound = to_lower(e.second.name);
            if (dpsound[1] == 'p') {
                dpsound[1] = 's';
                if (sfx_lumpnames.find(dpsound) != sfx_lumpnames.end()) {
                    e.second.data.clear();
                    cleared_lumps.push_back(e.first);
                    touched[e.first] = TOUCHED_SFX;
                }
            }
        }
        clear_lump(wad, "dmxgus", TOUCHED_DMX);
        clear_lump(wad, "dmxgusc", TOUCHED_DMX);
        clear_lump(wad, "stdisk", TOUCHED_UNUSED_GRAPHIC);
        clear_lump(wad, "stcdrom", TOUCHED_UNUSED_GRAPHIC);

        lump palette;
        wad.get_lump("playpal", palette);
        static uint8_t outpal[768];
        bool mismatch = false;
        for(int i=1;i<14;i++) {
            if (i < 9) ColorShiftPalette(palette.data.data(), outpal, 255, 0, 0, i, 9);
            else if (i < 13) ColorShiftPalette(palette.data.data(), outpal, 215, 186, 69, i-8, 8);
            else ColorShiftPalette(palette.data.data(), outpal, 0, 256, 0, 1, 8);
            if (!memcmp(palette.data.data()+i*768, outpal, 768)) {
//                printf("Palette %d matches\n", i);
            } else {
//                printf("Palette %d mismatch\n", i);
                mismatch = true;
            }
        }
        touched[palette.num] = TOUCHED_PALETTE;
        lump lmisc;
        wad.get_lump("colormap", lmisc);
        touched[lmisc.num] = TOUCHED_COLORMAP;
        wad.get_lump("genmidi", lmisc);
        touched[lmisc.num] = TOUCHED_GENMIDI;


        if (!mismatch) {
            // truncate the palette to a single copy
            palette.data.resize(768);
            wad.update_lump(palette);
            compressed.insert(palette.num);
        } else {
            printf("warning: palettes are not standard\n");
        }

#if 0
        std::vector<uint8_t> font;
        font.insert(font.begin(), normal_font_data, normal_font_data + sizeof(normal_font_data));
        auto fontz = std::make_shared<byte_vector_bit_output>();
        printf("FONTO %d %d\n", (int)font.size(), consider_compress_data("font", font, fontz));
        auto fontzd = fontz->get_output();
        printf("static const uint8_t normal_font_data_z[%d] = {\n", (int)fontzd.size());
        for(int i=0;i<fontzd.size();i+=24) {
            printf("  ");
            for(int j=i;j<std::min(i+24, (int)fontzd.size());j++) {
                printf("0x%02x, ", fontzd[j]);
            }
            printf("\n");
        }
        printf("}; \n");
#endif
        lump endoom;
        if (wad.get_lump("ENDOOM", endoom)) {
            touched[endoom.num] = TOUCHED_ENDOOM;
            compressed.insert(endoom.num);
            auto endoomz = std::make_shared<byte_vector_bit_output>();
            printf("ENDOOMALL %d %d\n", (int)endoom.data.size(), consider_compress_data("endoom", endoom.data, endoomz));
            std::vector<uint8_t> attr;
            std::vector<uint8_t> text;
            for(int i=0;i<(int)endoom.data.size();i+=2) {
                text.push_back(endoom.data[i]);
                attr.push_back(endoom.data[i+1]);
            }
            auto textz = std::make_shared<byte_vector_bit_output>();
            auto attrz = std::make_shared<byte_vector_bit_output>();
            printf("ENDOOM TEXT %d %d\n", (int)text.size(), consider_compress_data("text", text, textz));
            printf("ENDOOM ATTR %d %d\n", (int)attr.size(), consider_compress_data("attr", attr, attrz));
            byte_vector_bit_output combined;
            textz->write_to(combined);
            attrz->write_to(combined);
            endoom.data = combined.get_output();
            wad.update_lump(endoom);
        }

        // need to get these in before vpatch renumbering
        // insert our network menu items, reusing some of the freeed beep sound ids
        for(int i=0;i<(int)count_of(extra_patches);i++) {
            lump l = get_free_lump(wad);
            l.name = extra_patches[i].name;
            l.data.insert(l.data.end(), extra_patches[i].data, extra_patches[i].data + extra_patches[i].len);
            wad.update_lump(l);
        }

        // do this before patches for now
        auto tex_index = convert_textures(wad);


        for(auto &s : named_lumps) name_required.insert(s);
        convert_patches(wad, "p_start", "p_end");
        std::vector<uint8_t> vpatch_lookup;
        int vp_num=0;
        for(const auto &n : vpatch_names) {
            int index = wad.get_lump_index(n);
            if (index < 0) {
                if (!n.empty()) printf("Missing vpatch %s\n", n.c_str());
                index = 0;
            }
            printf("VPATCH %d %s lump=%d\n", vp_num++, n.c_str(), index);
            vpatch_lookup.push_back(index & 0xff);
            vpatch_lookup.push_back(index >> 8);
        }
        lump pstart;
        wad.get_lump("p_start", pstart);
        touched[pstart.num] = TOUCHED_PATCH_METADATA;
        assert(pstart.data.empty());
        pstart.data = vpatch_lookup;
        wad.update_lump(pstart);

        convert_sprites(wad);
        int s_start = wad.get_lump_index("s_start");
        int s_end = wad.get_lump_index("s_end");
        if (s_start < 0 || s_end < 0) fail("missing s_start/s_end");
        std::vector<uint8_t> sprite_metadata;
        for (int s = s_start + 1; s < s_end; s++) {
            uint32_t metadata = 0;
            lump patch;
            if (wad.get_lump(s, patch)) {
                auto ph = get_field<patch_header>(patch.data, 0);
                if (ph.width > 0x3ff) fail("patch width %d too big", ph.width);
                if (ph.leftoffset < -0x400 || ph.leftoffset > 0x3ff)
                    fail("patch left offset %d out of range", ph.leftoffset);
                if (ph.topoffset < -0x400 || ph.topoffset > 0x3ff)
                    fail("patch top offset %d out of range", ph.topoffset);
                metadata = ph.width | (ph.leftoffset << 21u) | ((0x7ffu & ph.topoffset) << 10u);
            }
            append_field(sprite_metadata, metadata);
        }
        lump s_start_lump;
        // hack to insert for now
        wad.get_lump("S_START", s_start_lump);
        touched[s_start_lump.num] = TOUCHED_SPRITE_METADATA;
        s_start_lump.data = sprite_metadata;
        wad.update_lump(s_start_lump);
        convert_patches(wad, "s_start", "s_end");
        for (const auto &cg : splash_graphics) {
            lump l;
            int indexp1 = wad.get_lump(cg, l);
            if (indexp1) {
                convert_patch(wad, indexp1 - 1, l);
            }
        }
        convert_vpatches(wad, run16_misc_vpatches, 16, true);
        convert_vpatches(wad, run64_misc_vpatches, 64, true);
        convert_vpatches(wad, run256_misc_vpatches, 256, true);
        convert_vpatches(wad, alpha_shpal_grey_graphics, 16, false, 0); // share palettes
        convert_vpatches(wad, alpha16_shpal_red_vpatches, 16, false, 1); // share palettes
        convert_vpatches(wad, alpha16_shpal_white_vpatches, 16, false, 2); // share palettes
        // special case player background to see if they are rectangle with broder which we'll ecncode specially,
        // not so much as to save space (it does) but we don't quite have enough time to draw the status bar background
        // always, and rendering a flat color is quIcker
        for(const auto& s : special_player_background_vpatches) {
            lump patch;
            int indexp1 = wad.get_lump(s, patch);
            if (indexp1) {
                auto ph = get_field<patch_header>(patch.data, 0);
                auto pix = unpack_patch(patch);
                std::set<int> colors;
                bool match = true;
                // each line is encoded as 3 palette indexes; one for first pixel, one for middle pixels and one for last pixels
                // this is obviously not crazy efficient, but is easy for clipping etc when rendering, and is more compact than
                // the raw data
                std::vector<uint8_t> lines;
                for(int y=0;y<ph.height && match;y++) {
                    lines.push_back(pix[y*ph.width]);
                    int c = pix[y*ph.width+1];
                    lines.push_back(c);
                    for(int x=1;x<ph.width-1;x++) {
                        if (pix[y*ph.width+x] != c) {
                            match = false;
                            break;
                        }
                    }
                    lines.push_back(pix[y*ph.width + ph.width -1]);
                }
                if (match) {
                    printf("Border patch %s\n", patch.name.c_str());
                    touched[patch.num] = TOUCHED_VPATCH;
                    compressed.insert(patch.num);
                    patch.data.clear();
                    patch.data.push_back(ph.width);
                    patch.data.push_back(ph.height);
                    patch.data.push_back(0); // not super efficient, but makes decode easier, and we are saving a palette altogether
                    int vpt = vp_border;
                    patch.data.push_back((vpt<<2) | ((ph.width &0x100)>>7));
                    // todo shrink this some; it is often zero
                    patch.data.push_back(ph.topoffset);
                    patch.data.push_back(ph.leftoffset);
                    patch.data.insert(patch.data.end(), lines.begin(), lines.end());
                    wad.update_lump(patch);
                } else {
                    convert_vpatch(wad, patch, 16, false, colors, -1, false);
                }
            }
        }
        convert_vpatches(wad, run64_face_vpatches, 64, true);
        convert_vpatches(wad, alpha16_status_vpatches, 16, false);
        convert_vpatches(wad, run16_menu_vpatches, 16, true);

        convert_flats(wad);
        // filter again
        for (auto &e : wad.get_lumps()) {
            if (sfx_lumpnames.find(to_lower(e.second.name)) != sfx_lumpnames.end()) {
                if (!convert_sound(e)) {
                    printf("Failed to convert sound %s\n", e.second.name.c_str());
                    // todo remove?
                }
                touched[e.first] = TOUCHED_SFX;
            }
        }
        for (auto &e : wad.get_lumps()) {
            if (e.second.name.substr(0, 5) == "WIMAP") {
                //dump_patch(e.second.name.c_str(), e.first, e.second);
                touched[e.first] = TOUCHED_PATCH;
                convert_patch(wad, e.first, e.second);
                name_required.insert(e.second.name);
            }
        }
        for (auto &e : wad.get_lumps()) {
            if (e.second.name.length() > 4 && e.second.name.substr(0, 4) == "BRDR") {
                touched[e.first] = TOUCHED_UNUSED_GRAPHIC;
                e.second.data.clear();
            }
        }
        int total_size = 0;
        std::vector<std::string> level_data = {
                "THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS", "SSECTORS", "NODES", "SECTORS", "REJECT",
                "BLOCKMAP"
        };
        std::vector<size_t> level_data_struct_size = {
                sizeof(mapthing_t),
                sizeof(maplinedef_t),
                sizeof(mapsidedef_t),
                sizeof(mapvertex_t),
                sizeof(mapseg_t),
                sizeof(mapsubsector_t),
                sizeof(mapnode_t),
                sizeof(mapsector_t),
                1,
                2
        };
        int level_data_size = 0;
        std::vector<statsomizer> level_data_orig_sizes;
        std::vector<statsomizer> level_data_sizes;
        std::transform(level_data.begin(), level_data.end(), std::back_inserter(level_data_orig_sizes),
                       [](auto &name) { return statsomizer(name + " orig size"); });
        std::transform(level_data.begin(), level_data.end(), std::back_inserter(level_data_sizes),
                       [](auto &name) { return statsomizer(name + " size"); });

        // little old, but keep for now
        for (auto &e : wad.get_lumps()) {
            auto it = std::find(level_data.begin(), level_data.end(), e.second.name);
            if (it != level_data.end()) {
                int which = it - level_data.begin();
                level_data_orig_sizes[which].record(e.second.data.size());
            }
        }

        auto convert_level = [&](::wad& wad, std::string name) {
            int index = wad.get_lump_index(name);
            if (index >= 0) {
                printf("Converting level %s\n", name.c_str());
            } else {
                return;
            }
            name_required.insert(name);
            touched[index] = TOUCHED_LEVEL;

            lump l;
            if (!wad.get_lump(index+ML_THINGS, l) || l.name != "THINGS") {
                fail("missing THINGS for %s", name.c_str());
            }
            printf("Convert THINGS in lump %d\n", index+ML_THINGS);
            touched[index+ML_THINGS] = TOUCHED_LEVEL_THINGS;
            // todo

            if (!wad.get_lump(index+ML_SIDEDEFS, l) || l.name != "SIDEDEFS") {
                fail("missing SIDEDEFS for %s", name.c_str());
            }
            printf("Convert SIDEDEF in lump %d\n", index+ML_SIDEDEFS);
            compressed.insert(index+ML_SIDEDEFS);
            touched[index+ML_SIDEDEFS] = TOUCHED_LEVEL_SIDEDEFS;
            auto sidedef_mapping = convert_sidedefs(wad, tex_index, l);

            if (!wad.get_lump(index+ML_VERTEXES, l) || l.name != "VERTEXES") {
                fail("missing VERTEXES for %s", name.c_str());
            }
            printf("Convert VERTEXES in lump %d\n", index+ML_VERTEXES);
            touched[index+ML_VERTEXES] = TOUCHED_LEVEL_VERTEXES;
            auto vertexes = convert_vertexes(wad, l);

            if (!wad.get_lump(index+ML_LINEDEFS, l) || l.name != "LINEDEFS") {
                fail("missing LINEDEFS for %s", name.c_str());
            }
            printf("Convert LINEDEFS in lump %d\n", index+ML_LINEDEFS);
            touched[index+ML_LINEDEFS] = TOUCHED_LEVEL_LINEDEFS;
            auto linedef_mapping = convert_linedefs(wad, l, sidedef_mapping, vertexes);

            if (!wad.get_lump(index+ML_SEGS, l) || l.name != "SEGS") {
                fail("missing SEGS for %s", name.c_str());
            }
            printf("Convert SEGS in lump %d\n", index+ML_SEGS);
            touched[index+ML_SEGS] = TOUCHED_LEVEL_SEGS;
            auto seg_mapping = convert_segs(wad, l, linedef_mapping);

            if (!wad.get_lump(index+ML_SSECTORS, l) || l.name != "SSECTORS") {
                fail("missing SSECTORS for %s", name.c_str());
            }
            printf("Convert SSECTORS in lump %d\n", index+ML_SSECTORS);
            touched[index+ML_SSECTORS] = TOUCHED_LEVEL_SSECTORS;
            convert_subsectors(wad, l, seg_mapping);

            if (!wad.get_lump(index+ML_NODES, l) || l.name != "NODES") {
                fail("missing NODES for %s", name.c_str());
            }
            printf("Convert NODES in lump %d\n", index+ML_NODES);
            compressed.insert(index+ML_NODES);
            touched[index+ML_NODES] = TOUCHED_LEVEL_NODES;
            convert_nodes(wad, l);

            if (!wad.get_lump(index+ML_SECTORS, l) || l.name != "SECTORS") {
                fail("missing SECTORS for %s", name.c_str());
            }
            printf("Convert SECTORS in lump %d\n", index+ML_SECTORS);
            touched[index+ML_SECTORS] = TOUCHED_LEVEL_SECTORS;
            convert_sectors(wad, l);

            if (!wad.get_lump(index+ML_REJECT, l) || l.name != "REJECT") {
                fail("missing REJECT for %s", name.c_str());
            }
            printf("Convert REJECT in lump %d\n", index+ML_REJECT);
            // todo convert_reject(wad, l);
            touched[index+ML_REJECT] = TOUCHED_LEVEL_REJECT;

            if (!wad.get_lump(index+ML_BLOCKMAP, l) || l.name != "BLOCKMAP") {
                fail("missing BLOCKMAP for %s", name.c_str());
            }
            printf("Convert BLOCKMAP in lump %d\n", index+ML_BLOCKMAP);
            touched[index+ML_BLOCKMAP] = TOUCHED_LEVEL_BLOCKMAP;
            compressed.insert(index+ML_BLOCKMAP);
            convert_blockmap(wad, l, linedef_mapping);
        };
        for(int e=1; e<=4; e++) {
            for(int m=1; m<=9; m++) {
                convert_level(wad, "E"+std::to_string(e)+"M"+std::to_string(m));
            }
        }
        for(int m=1; m<=32; m++) {
            char name[10];
            sprintf(name, "MAP%02d", m);
            convert_level(wad, name);
        }
        for (auto &e : wad.get_lumps()) {
            auto it = std::find(level_data.begin(), level_data.end(), e.second.name);
            if (it != level_data.end()) {
                int which = it - level_data.begin();
                level_data_sizes[which].record(e.second.data.size());
                // this doesn't much make sense now
//                level_data_counts[which].record(e.second.data.size() / level_data_struct_size[which]);
                level_data_size += e.second.data.size();
            }
        }

        for (auto &e : wad.get_lumps()) {
            if (music_lumpnames.find(to_lower(e.second.name)) != music_lumpnames.end()) {
                convert_music(e);
            }
            total_size += e.second.data.size();
            //printf("%s %08x\n", e.second.name.c_str(), (int)e.second.data.size());
        }

        lump tmp;
        if (wad.get_lump("f_sky1", tmp)) {
            touched[tmp.num] = TOUCHED_UNUSED_GRAPHIC;
            tmp.data.clear(); // we don't need data in here
            wad.update_lump(tmp);
        }
        lump demo;
        if (wad.get_lump("demo1", demo)) convert_demo(wad, demo);
        if (wad.get_lump("demo2", demo)) convert_demo(wad, demo);
        if (wad.get_lump("demo3", demo)) convert_demo(wad, demo);
        if (wad.get_lump("demo4", demo)) convert_demo(wad, demo);
        int total = 0;
        for (const auto &e : wad.get_lumps()) {
            if (touched.find(e.first) == touched.end()) {
                printf("UNTOUCHED %d %s (%d)\n", e.first, e.second.name.c_str(), (int) e.second.data.size());
                total += e.second.data.size();
                touched[e.first] = TOUCHED_UNUSED;
            }
        }
        printf("UNTOUCHED TOTAL %d\n", total);
        total = 0;
//        for (const auto &e : wad.get_lumps()) {
//            if (compressed.find(e.first) == compressed.end() && e.second.data.size()) {
//                printf("UNCOMPRESSED %d %s (%d)\n", e.first, e.second.name.c_str(), (int) e.second.data.size());
//                total += e.second.data.size();
//            }
//        }
//        printf("UNCOMPRESSED TOTAL %d\n", total);

        patch_widths.print_summary();
        patch_heights.print_summary();
        patch_left_offsets.print_summary();
        patch_top_offsets.print_summary();
        vpatch_left_offsets.print_summary();
        vpatch_top_offsets.print_summary();
        vpatch_left_0offsets.print_summary();
        vpatch_top_0offsets.print_summary();
        patch_sizes.print_summary();
        patch_colors.print_summary();
        patch_column_colors.print_summary();
        for (const auto &s : patch_columns_under_colors) {
            s.print_summary();
        }
        patch_post_counts.print_summary();
        patch_one.print_summary();
        patch_all_post_counts.print_summary();
        patch_col_hack_huff_pixels.print_summary();
        patch_hack_huff_pixels.print_summary();
        patch_meta_size.print_summary();
        patch_orig_meta_size.print_summary();
        patch_decoder_size.print_summary();

        texture_column_patches.print_summary();
        texture_column_patches1.print_summary();
        texture_single_patch.print_summary();
        texture_single_patch00.print_summary();
        texture_transparent.print_summary();
        texture_transparent_patch_count.print_summary();
        texture_col_metadata.print_summary();
        printf("MUS  %d\n", mus_total1);
        printf("MUSX %d\n", mus_total2);
        musx_decoder_space.print_summary();
        // todo this should be dynamic and stored in WAD
        if (musx_decoder_space.max > MUSX_MAX_DECODER_SPACE) {
            fail("MUSX decoder space exceeded (max %d)\n", MUSX_MAX_DECODER_SPACE);
        }
        int i = 0;
        int t=0;
        for (const auto &s : level_data_orig_sizes) {
            printf("%2d ", (int) level_data_struct_size[i++]);
            s.print_summary();
            t += s.total;
        }
        printf("TOTAL %d %08x\n", t, t);
        printf("\n");
        t = 0;
        for (const auto &s : level_data_sizes) {
            s.print_summary();
            t += s.total;
        }
        printf("TOTAL %d %08x\n", t, t);
        printf("\n");
        subsector_length.print_summary();
        blockmap_empty.print_summary();
        blockmap_one.print_summary();
        blockmap_length.print_summary();
        blockmap_row_size.print_summary();
        block_map_deltas.print_summary();
        block_map_deltas_1byte.print_summary();
        blockmap_sizes.print_summary();
        blockmap_blocks.print_summary();

        sector_lightlevel.print_summary();
        sector_floorheight.print_summary();
        sector_ceilingheight.print_summary();
        sector_special.print_summary();

        printf("level data size %08x\n", level_data_size);
        printf("total size %08x\n", total_size);
        sfx_orig_size.print_summary();
        sfx_new_size.print_summary();
        patch_orig_size.print_summary();
        patch_new_size.print_summary();
        vpatch_orig_size.print_summary();
        vpatch_new_size.print_summary();
        tex_orig_size.print_summary();
        tex_new_size.print_summary();

        same_columns.print_summary();

        for(i=0;i<(int)winners.size();i++) {
            printf("WIN %d %d\n", i, winners[i]);
        }
        patch_pixels.print_summary();
        cp1_pixels.print_summary();
        cp1_size.print_summary();
        cp2_size.print_summary();
        cp_wtf_size.print_summary();
        cp_po_size.print_summary();
        cp1_run.print_summary();
        cp1_raw_run.print_summary();
        cp_size.print_summary();
        printf("Bit addressable %d\n", bit_addressable_patch);
        printf("Dumped patches %d Converted patches %d Size %d\n", dumped_patch_count, converted_patch_count, converted_patch_size);
        printf("Opaque %d Transparent %d total %d\n", opaque_pixels, transparent_pixels, opaque_pixels + transparent_pixels);
        flat_rawsize.print_summary();
        flat_c2size.print_summary();
        flat_have_same_savings.print_summary();
        flat_colors.print_summary();
        for (const auto &s : flat_under_colors) {
            s.print_summary();
        }
        for(i=0;i<(int)fwinners.size();i++) {
            printf("FWIN %d %d\n", i, fwinners[i]);
        }

        color_runs.print_summary();
        side_meta.print_summary();
        side_metaz.print_summary();
        line_meta.print_summary();
        line_metaz.print_summary();
        line_scale.print_summary();
        ss_delta.print_summary();
        demo_size_orig.print_summary();
        demo_size.print_summary();
        single_patch_metadata_size.print_summary();
        wad.write_whd(output_filename, name_required, hash, super_tiny);
        size = 0;
        for(const auto &e : wad.get_lumps()) {
            size += e.second.data.size();
        }
        printf("LUMPS NEW SIZE %d\n", size);

        // just dump some extra stats
#if 1
        printf("WHD -------------\n");
        std::map<std::string, int> ltype_size;
        std::map<std::string, std::string> lname_to_ltype;
        for(const auto &e : touched) {
            lump l;
            wad.get_lump(e.first, l);
            lname_to_ltype[l.name] = e.second;
            ltype_size[e.second] += l.data.size();
        }
        total=0;
        for(const auto &e : ltype_size) {
            printf("%s: %d (%dK)\n", e.first.c_str(), e.second, (e.second + 512) / 1024);
            total += e.second;
        }
        printf("TOTAL %d (%dK)\n", total, (total+512)/1024);

        printf("WAD -------------\n");
        ltype_size.clear();
        auto wad2 = wad::read(wad_name);
        for(const auto &e : wad2.get_lumps()) {
            std::string ltype = lname_to_ltype[e.second.name];
            if (ltype.empty()) ltype = TOUCHED_UNUSED;
            ltype_size[ltype] += e.second.data.size();
        }
        total=0;
        for(const auto &e : ltype_size) {
            printf("%s: %d (%dK)\n", e.first.c_str(), e.second, (e.second + 512) / 1024);
            total += e.second;
        }
        printf("TOTAL %d (%dK)\n", total, (total+512)/1024);
#endif
    } catch (std::exception &e) {
        std::cerr << e.what();
        return -1;
    }
}

int count_codes(vector<uint8_t> &items, vector<int> &sizes) {
    int s = 0;
    for (const auto &e : items) {
        s += sizes[e];
    }
    return s;
}

void th_bit_overrun(th_bit_input *bi) {
    fail("Bit overrun in decoding");
}
