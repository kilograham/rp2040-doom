#ifndef __WHDDATA__
#define __WHDDATA__

#include "doomtype.h"

#if !USE_WHD
typedef char textureorflatname_def_t[9];
typedef const char* textureorflatname_t;
#define TEXTURE_NAME(x) __STRING(x)
#define TEXTURE_NAME_NONE ""
#define FLAT_NAME(x) __STRING(x)
#define VPATCH_NAME(x) __STRING(x)
#define VPATCH_NAME_INVALID ""
#define DEH_VPATCH_NAME(x) DEH_String(VPATCH_NAME(x))
typedef const char *vpatchname_t;
#define DEH_TextureName(n) DEH_String(n)
typedef const char *textureorflatname_t;
#else
typedef int8_t textureorflatname_def_t;
typedef textureorflatname_def_t textureorflatname_t;
#define TEXTURE_NAME(x) __CONCAT(NTEX_, x)
#define TEXTURE_NAME_NONE (-1)
#define FLAT_NAME(x) __CONCAT(NFLAT_, x)
#define VPATCH_NAME(x) __CONCAT(VPATCH_, x)
#define DEH_VPATCH_NAME(x) VPATCH_NAME(x)
typedef uint8_t vpatchname_t;
#define DEH_TextureName(n) (n)
typedef int8_t textureorflatname_t;
#endif
typedef textureorflatname_def_t texturename_def_t;
typedef textureorflatname_t texturename_t;
typedef textureorflatname_t flatname_t;

typedef	struct {
    short floorheight;
    short ceilingheight;
    short floorpic;
    short ceilingpic;
    short lightlevel;
    short special;
    short tag;

//    1.5   int16_t     Xrawfloorheight;
//    1.5   int16_t     Xrawceilingheight;
//    1   uint8_t     Xfloorpic;
//    1   uint8_t     Xlightlevel;
//    1   uint8_t     Xspecial
//    2   short	tag; // immutable; how big? (CAN BE CONST)
//    2   cardinal_t	line_index;	// within linebuffer of first line (CAN BE CONST)
//    1   cardinal_t 	linecount; // seems unlikely to be more than 8 bit really (CAN BE CONST)
//    1   uint8_t     ceilingpic; // (CAN BE CONST)
//    4   uint8_t     blockbox[4]; // (CAN BE CONST)
//    4   xy_positioned_t soundorg; // middle of bbox (CAN BE CONST)

} whdsector_t;

//typedef struct {
//    int16_t floor_height:12;
//    int16_t ceiling_height:12;
//    uint8_t floor_pic;
//    uint8_t ceiling_pic;
//    uint8_t light_level;
//    uint8_t special;
//    uint8_t line_count;
//    uint16_t line_index;
//    int16_t tag;
//    uint8_t blockbox[4];
//    //xy_positioned_t soundorg;
//} foo;

typedef struct {
    short	textureoffset;
    short	rowoffset;
    short	toptexture;
    short	bottomtexture;
    short	midtexture;
    // Front sector, towards viewer.
    short	sector;
} whdsidedef_t;

// not sure where this comes in yet
//typedef struct {
//    int16_t	originx;
//    int16_t	originy;
//    lumpindex_t 	patch;
//} whdpatch_t;

typedef struct {
    uint8_t width_m1;
    uint8_t height;
    int8_t leftoffset;
    int8_t topoffset;
    uint16_t columnofs[1];
} whdpatch_t;

typedef struct {
    uint16_t width;
    uint8_t height;
    uint8_t patch_count;
//    uint8_t approx_color; // seems like it might be handy
    union {
        uint16_t patch0;
        uint16_t metdata_offset;
    };
    // todo do we need flags
} whdtexture_t;

/*
    // DOOM II flat animations.
    FLATANIM_DEF(SLIME04, SLIME01, 8),
    FLATANIM_DEF(SLIME08, SLIME05, 8),
    FLATANIM_DEF(SLIME12, SLIME09, 8),
 */

enum vpatch_type {
    vp4_solid,
    vp4_runs,
    vp4_alpha,
    vp6_runs,
    vp8_runs,
    vp_border,
    vp4_solid_clipped, // note these are in same order as above
    vp4_runs_clipped,
    vp4_alpha_clipped,
    vp6_runs_clipped,
    vp8_runs_clipped,
    vp_border_clipped,
};

#define VPATCH_LIST \
    VPATCH_NAME_INVALID,  \
    VPATCH_NAME(AMMNUM0), \
    VPATCH_NAME(AMMNUM1), \
    VPATCH_NAME(AMMNUM2), \
    VPATCH_NAME(AMMNUM3), \
    VPATCH_NAME(AMMNUM4), \
    VPATCH_NAME(AMMNUM5), \
    VPATCH_NAME(AMMNUM6), \
    VPATCH_NAME(AMMNUM7), \
    VPATCH_NAME(AMMNUM8), \
    VPATCH_NAME(AMMNUM9), \
    VPATCH_NAME(M_LOADG), \
    VPATCH_NAME(M_LSLEFT), \
    VPATCH_NAME(M_LSCNTR), \
    VPATCH_NAME(M_LSRGHT), \
    VPATCH_NAME(M_SVOL), \
    VPATCH_NAME(M_SKILL), \
    VPATCH_NAME(M_DOOM), \
    VPATCH_NAME(M_NEWG), \
    VPATCH_NAME(M_THERML), \
    VPATCH_NAME(M_THERMM), \
    VPATCH_NAME(M_THERMO), \
    VPATCH_NAME(M_THERMR), \
    VPATCH_NAME(M_EPISOD),\
    VPATCH_NAME(M_OPTTTL),\
    VPATCH_NAME(M_MSGON), \
    VPATCH_NAME(M_MSGOFF), \
    VPATCH_NAME(M_NGAME), \
    VPATCH_NAME(M_OPTION), \
    VPATCH_NAME(M_SAVEG), \
    VPATCH_NAME(M_RDTHIS), \
    VPATCH_NAME(M_QUITG), \
    VPATCH_NAME(M_EPI1), \
    VPATCH_NAME(M_EPI2), \
    VPATCH_NAME(M_EPI3), \
    VPATCH_NAME(M_EPI4), \
    VPATCH_NAME(M_JKILL), \
    VPATCH_NAME(M_ROUGH), \
    VPATCH_NAME(M_HURT), \
    VPATCH_NAME(M_ULTRA), \
    VPATCH_NAME(M_NMARE), \
    VPATCH_NAME(M_ENDGAM), \
    VPATCH_NAME(M_MESSG), \
    VPATCH_NAME(M_DETAIL), \
    VPATCH_NAME(M_SCRNSZ), \
    VPATCH_NAME(M_MSENS), \
    VPATCH_NAME(M_SFXVOL), \
    VPATCH_NAME(M_MUSVOL), \
    VPATCH_NAME(M_SKULL1), \
    VPATCH_NAME(M_SKULL2), \
    VPATCH_NAME(M_PAUSE), \
    VPATCH_NAME(STTMINUS), \
    VPATCH_NAME(STFST00), \
    VPATCH_NAME(STFST01), \
    VPATCH_NAME(STFST02), \
    VPATCH_NAME(STFTR00), \
    VPATCH_NAME(STFTL00), \
    VPATCH_NAME(STFOUCH0), \
    VPATCH_NAME(STFEVL0), \
    VPATCH_NAME(STFKILL0), \
    VPATCH_NAME(STFST10), \
    VPATCH_NAME(STFST11), \
    VPATCH_NAME(STFST12), \
    VPATCH_NAME(STFTR10), \
    VPATCH_NAME(STFTL10), \
    VPATCH_NAME(STFOUCH1), \
    VPATCH_NAME(STFEVL1), \
    VPATCH_NAME(STFKILL1), \
    VPATCH_NAME(STFST20), \
    VPATCH_NAME(STFST21), \
    VPATCH_NAME(STFST22), \
    VPATCH_NAME(STFTR20), \
    VPATCH_NAME(STFTL20), \
    VPATCH_NAME(STFOUCH2), \
    VPATCH_NAME(STFEVL2), \
    VPATCH_NAME(STFKILL2), \
    VPATCH_NAME(STFST30), \
    VPATCH_NAME(STFST31), \
    VPATCH_NAME(STFST32), \
    VPATCH_NAME(STFTR30), \
    VPATCH_NAME(STFTL30), \
    VPATCH_NAME(STFOUCH3), \
    VPATCH_NAME(STFEVL3), \
    VPATCH_NAME(STFKILL3), \
    VPATCH_NAME(STFST40), \
    VPATCH_NAME(STFST41), \
    VPATCH_NAME(STFST42), \
    VPATCH_NAME(STFTR40), \
    VPATCH_NAME(STFTL40), \
    VPATCH_NAME(STFOUCH4), \
    VPATCH_NAME(STFEVL4), \
    VPATCH_NAME(STFKILL4),\
    VPATCH_NAME(STFGOD0),\
    VPATCH_NAME(STFDEAD0),\
    VPATCH_NAME(STFB0), \
    VPATCH_NAME(STFB1), \
    VPATCH_NAME(STFB2), \
    VPATCH_NAME(STFB3), \
    VPATCH_NAME(STTNUM0), \
    VPATCH_NAME(STTNUM1), \
    VPATCH_NAME(STTNUM2), \
    VPATCH_NAME(STTNUM3), \
    VPATCH_NAME(STTNUM4), \
    VPATCH_NAME(STTNUM5), \
    VPATCH_NAME(STTNUM6), \
    VPATCH_NAME(STTNUM7), \
    VPATCH_NAME(STTNUM8), \
    VPATCH_NAME(STTNUM9), \
    VPATCH_NAME(STTPRCNT), \
    VPATCH_NAME(STYSNUM0), \
    VPATCH_NAME(STYSNUM1), \
    VPATCH_NAME(STYSNUM2), \
    VPATCH_NAME(STYSNUM3), \
    VPATCH_NAME(STYSNUM4), \
    VPATCH_NAME(STYSNUM5), \
    VPATCH_NAME(STYSNUM6), \
    VPATCH_NAME(STYSNUM7), \
    VPATCH_NAME(STYSNUM8), \
    VPATCH_NAME(STYSNUM9), \
    VPATCH_NAME(STGNUM0), \
    VPATCH_NAME(STGNUM1), \
    VPATCH_NAME(STGNUM2), \
    VPATCH_NAME(STGNUM3), \
    VPATCH_NAME(STGNUM4), \
    VPATCH_NAME(STGNUM5), \
    VPATCH_NAME(STGNUM6), \
    VPATCH_NAME(STGNUM7), \
    VPATCH_NAME(STGNUM8), \
    VPATCH_NAME(STGNUM9), \
    VPATCH_NAME(STKEYS0), \
    VPATCH_NAME(STKEYS1), \
    VPATCH_NAME(STKEYS2), \
    VPATCH_NAME(STKEYS3), \
    VPATCH_NAME(STKEYS4), \
    VPATCH_NAME(STKEYS5), \
    VPATCH_NAME(STARMS),  \
    VPATCH_NAME(STBAR),   \
    VPATCH_NAME(WINUM0), \
    VPATCH_NAME(WINUM1), \
    VPATCH_NAME(WINUM2), \
    VPATCH_NAME(WINUM3), \
    VPATCH_NAME(WINUM4), \
    VPATCH_NAME(WINUM5), \
    VPATCH_NAME(WINUM6), \
    VPATCH_NAME(WINUM7), \
    VPATCH_NAME(WINUM8), \
    VPATCH_NAME(WINUM9), \
    VPATCH_NAME(WIMINUS), \
    VPATCH_NAME(WIPCNT), \
    VPATCH_NAME(WIF), \
    VPATCH_NAME(WIENTER), \
    VPATCH_NAME(WIOSTK), \
    VPATCH_NAME(WIOSTS), \
    VPATCH_NAME(WISCRT2), \
    VPATCH_NAME(WIOSTI), \
    VPATCH_NAME(WIFRGS), \
    VPATCH_NAME(WICOLON), \
    VPATCH_NAME(WITIME), \
    VPATCH_NAME(WISUCKS), \
    VPATCH_NAME(WIPAR), \
    VPATCH_NAME(WIKILRS), \
    VPATCH_NAME(WIVCTMS), \
    VPATCH_NAME(WIMSTT), \
    VPATCH_NAME(WIURH0), \
    VPATCH_NAME(WIURH1),  \
    VPATCH_NAME(WISPLAT), \
    VPATCH_NAME(WIBP0), \
    VPATCH_NAME(WIBP1), \
    VPATCH_NAME(WIBP2), \
    VPATCH_NAME(WIBP3), \
    VPATCH_NAME(STPB0), \
    VPATCH_NAME(STPB1), \
    VPATCH_NAME(STPB2), \
    VPATCH_NAME(STPB3), \
    VPATCH_NAME(M_GAME), \
    VPATCH_NAME(M_HOST), \
    VPATCH_NAME(M_JOIN), \
    VPATCH_NAME(M_NAME), \
    VPATCH_NAME(M_NETWK), \
    VPATCH_NAME(M_DTHMCH), \
    VPATCH_NAME(M_TWO), \
    VPATCH_NAME(STCFN033), \
    VPATCH_NAME(STCFN034), \
    VPATCH_NAME(STCFN035), \
    VPATCH_NAME(STCFN036), \
    VPATCH_NAME(STCFN037), \
    VPATCH_NAME(STCFN038), \
    VPATCH_NAME(STCFN039), \
    VPATCH_NAME(STCFN040), \
    VPATCH_NAME(STCFN041), \
    VPATCH_NAME(STCFN042), \
    VPATCH_NAME(STCFN043), \
    VPATCH_NAME(STCFN044), \
    VPATCH_NAME(STCFN045), \
    VPATCH_NAME(STCFN046), \
    VPATCH_NAME(STCFN047), \
    VPATCH_NAME(STCFN048), \
    VPATCH_NAME(STCFN049), \
    VPATCH_NAME(STCFN050), \
    VPATCH_NAME(STCFN051), \
    VPATCH_NAME(STCFN052), \
    VPATCH_NAME(STCFN053), \
    VPATCH_NAME(STCFN054), \
    VPATCH_NAME(STCFN055), \
    VPATCH_NAME(STCFN056), \
    VPATCH_NAME(STCFN057), \
    VPATCH_NAME(STCFN058), \
    VPATCH_NAME(STCFN059), \
    VPATCH_NAME(STCFN060), \
    VPATCH_NAME(STCFN061), \
    VPATCH_NAME(STCFN062), \
    VPATCH_NAME(STCFN063), \
    VPATCH_NAME(STCFN064), \
    VPATCH_NAME(STCFN065), \
    VPATCH_NAME(STCFN066), \
    VPATCH_NAME(STCFN067), \
    VPATCH_NAME(STCFN068), \
    VPATCH_NAME(STCFN069), \
    VPATCH_NAME(STCFN070), \
    VPATCH_NAME(STCFN071), \
    VPATCH_NAME(STCFN072), \
    VPATCH_NAME(STCFN073), \
    VPATCH_NAME(STCFN074), \
    VPATCH_NAME(STCFN075), \
    VPATCH_NAME(STCFN076), \
    VPATCH_NAME(STCFN077), \
    VPATCH_NAME(STCFN078), \
    VPATCH_NAME(STCFN079), \
    VPATCH_NAME(STCFN080), \
    VPATCH_NAME(STCFN081), \
    VPATCH_NAME(STCFN082), \
    VPATCH_NAME(STCFN083), \
    VPATCH_NAME(STCFN084), \
    VPATCH_NAME(STCFN085), \
    VPATCH_NAME(STCFN086), \
    VPATCH_NAME(STCFN087), \
    VPATCH_NAME(STCFN088), \
    VPATCH_NAME(STCFN089), \
    VPATCH_NAME(STCFN090), \
    VPATCH_NAME(STCFN091), \
    VPATCH_NAME(STCFN092), \
    VPATCH_NAME(STCFN093), \
    VPATCH_NAME(STCFN094), \
    VPATCH_NAME(STCFN095), \
    /* WILV, CWILV, WIA are all vpatch_sequence_t which is a 16 bit handle, so these can go at index > 256 */ \
    VPATCH_NAME(WILV00), \
    VPATCH_NAME(WILV01), \
    VPATCH_NAME(WILV02), \
    VPATCH_NAME(WILV03), \
    VPATCH_NAME(WILV04), \
    VPATCH_NAME(WILV05), \
    VPATCH_NAME(WILV06), \
    VPATCH_NAME(WILV07), \
    VPATCH_NAME(WILV08), \
    VPATCH_NAME(WILV10), \
    VPATCH_NAME(WILV11), \
    VPATCH_NAME(WILV12), \
    VPATCH_NAME(WILV13), \
    VPATCH_NAME(WILV14), \
    VPATCH_NAME(WILV15), \
    VPATCH_NAME(WILV16), \
    VPATCH_NAME(WILV17), \
    VPATCH_NAME(WILV18), \
    VPATCH_NAME(WILV20), \
    VPATCH_NAME(WILV21), \
    VPATCH_NAME(WILV22), \
    VPATCH_NAME(WILV23), \
    VPATCH_NAME(WILV24), \
    VPATCH_NAME(WILV25), \
    VPATCH_NAME(WILV26), \
    VPATCH_NAME(WILV27), \
    VPATCH_NAME(WILV28), \
    VPATCH_NAME(WILV30), \
    VPATCH_NAME(WILV31), \
    VPATCH_NAME(WILV32), \
    VPATCH_NAME(WILV33), \
    VPATCH_NAME(WILV34), \
    VPATCH_NAME(WILV35), \
    VPATCH_NAME(WILV36), \
    VPATCH_NAME(WILV37), \
    VPATCH_NAME(WILV38), \
    VPATCH_NAME(CWILV00), \
    VPATCH_NAME(CWILV01), \
    VPATCH_NAME(CWILV02), \
    VPATCH_NAME(CWILV03), \
    VPATCH_NAME(CWILV04), \
    VPATCH_NAME(CWILV05), \
    VPATCH_NAME(CWILV06), \
    VPATCH_NAME(CWILV07), \
    VPATCH_NAME(CWILV08), \
    VPATCH_NAME(CWILV09), \
    VPATCH_NAME(CWILV10), \
    VPATCH_NAME(CWILV11), \
    VPATCH_NAME(CWILV12), \
    VPATCH_NAME(CWILV13), \
    VPATCH_NAME(CWILV14), \
    VPATCH_NAME(CWILV15), \
    VPATCH_NAME(CWILV16), \
    VPATCH_NAME(CWILV17), \
    VPATCH_NAME(CWILV18), \
    VPATCH_NAME(CWILV19), \
    VPATCH_NAME(CWILV20), \
    VPATCH_NAME(CWILV21), \
    VPATCH_NAME(CWILV22), \
    VPATCH_NAME(CWILV23), \
    VPATCH_NAME(CWILV24), \
    VPATCH_NAME(CWILV25), \
    VPATCH_NAME(CWILV26), \
    VPATCH_NAME(CWILV27), \
    VPATCH_NAME(CWILV28), \
    VPATCH_NAME(CWILV29), \
    VPATCH_NAME(CWILV30), \
    VPATCH_NAME(CWILV31), \
    VPATCH_NAME(CWILV32), \
    VPATCH_NAME(CWILV33), \
    VPATCH_NAME(CWILV34), \
    VPATCH_NAME(CWILV35), \
    VPATCH_NAME(CWILV36), \
    VPATCH_NAME(CWILV37), \
    VPATCH_NAME(CWILV38), \
    VPATCH_NAME(WIA00000),  \
    VPATCH_NAME(WIA00001),  \
    VPATCH_NAME(WIA00002),  \
    VPATCH_NAME(WIA00100),  \
    VPATCH_NAME(WIA00101),  \
    VPATCH_NAME(WIA00102),  \
    VPATCH_NAME(WIA00200),  \
    VPATCH_NAME(WIA00201),  \
    VPATCH_NAME(WIA00202),  \
    VPATCH_NAME(WIA00300),  \
    VPATCH_NAME(WIA00301),  \
    VPATCH_NAME(WIA00302),  \
    VPATCH_NAME(WIA00400),  \
    VPATCH_NAME(WIA00401),  \
    VPATCH_NAME(WIA00402),  \
    VPATCH_NAME(WIA00500),  \
    VPATCH_NAME(WIA00501),  \
    VPATCH_NAME(WIA00502),  \
    VPATCH_NAME(WIA00600),  \
    VPATCH_NAME(WIA00601),  \
    VPATCH_NAME(WIA00602),  \
    VPATCH_NAME(WIA00700),  \
    VPATCH_NAME(WIA00701),  \
    VPATCH_NAME(WIA00702),  \
    VPATCH_NAME(WIA00800),  \
    VPATCH_NAME(WIA00801),  \
    VPATCH_NAME(WIA00802),  \
    VPATCH_NAME(WIA00900),  \
    VPATCH_NAME(WIA00901),  \
    VPATCH_NAME(WIA00902),  \
    VPATCH_NAME(WIA10000), \
    VPATCH_NAME(WIA10100), \
    VPATCH_NAME(WIA10200), \
    VPATCH_NAME(WIA10300), \
    VPATCH_NAME(WIA10400), \
    VPATCH_NAME(WIA10500), \
    VPATCH_NAME(WIA10600), \
    VPATCH_NAME(WIA10700), \
    VPATCH_NAME(WIA10701), \
    VPATCH_NAME(WIA10702), \
    VPATCH_NAME(WIA20000), \
    VPATCH_NAME(WIA20001), \
    VPATCH_NAME(WIA20002), \
    VPATCH_NAME(WIA20100), \
    VPATCH_NAME(WIA20101), \
    VPATCH_NAME(WIA20102), \
    VPATCH_NAME(WIA20200), \
    VPATCH_NAME(WIA20201), \
    VPATCH_NAME(WIA20202), \
    VPATCH_NAME(WIA20300), \
    VPATCH_NAME(WIA20301), \
    VPATCH_NAME(WIA20302), \
    VPATCH_NAME(WIA20400), \
    VPATCH_NAME(WIA20401), \
    VPATCH_NAME(WIA20402), \
    VPATCH_NAME(WIA20500), \
    VPATCH_NAME(WIA20501), \
    VPATCH_NAME(WIA20502),\
    VPATCH_NAME(END0), \
    VPATCH_NAME(END1), \
    VPATCH_NAME(END2), \
    VPATCH_NAME(END3), \
    VPATCH_NAME(END4), \
    VPATCH_NAME(END5), \
    VPATCH_NAME(END6)

// note we must include intermediate flats even if not directly references (since we deal with consecutive ranges)
#define NAMED_FLAT_LIST \
    FLAT_NAME(NUKAGE1), \
    FLAT_NAME(NUKAGE2), \
    FLAT_NAME(NUKAGE3), \
    FLAT_NAME(FWATER1), \
    FLAT_NAME(FWATER2), \
    FLAT_NAME(FWATER3), \
    FLAT_NAME(FWATER4), \
    FLAT_NAME(SWATER1), \
    FLAT_NAME(SWATER4), \
    FLAT_NAME(LAVA1), \
    FLAT_NAME(LAVA2), \
    FLAT_NAME(LAVA3), \
    FLAT_NAME(LAVA4), \
    FLAT_NAME(BLOOD1), \
    FLAT_NAME(BLOOD2), \
    FLAT_NAME(BLOOD3), \
    FLAT_NAME(RROCK05), \
    FLAT_NAME(RROCK06), \
    FLAT_NAME(RROCK07), \
    FLAT_NAME(RROCK08), \
    FLAT_NAME(SLIME01), \
    FLAT_NAME(SLIME02), \
    FLAT_NAME(SLIME03), \
    FLAT_NAME(SLIME04), \
    FLAT_NAME(SLIME05), \
    FLAT_NAME(SLIME06), \
    FLAT_NAME(SLIME07), \
    FLAT_NAME(SLIME08), \
    FLAT_NAME(SLIME09), \
    FLAT_NAME(SLIME10), \
    FLAT_NAME(SLIME11), \
    FLAT_NAME(SLIME12), \
    FLAT_NAME(F_SKY1)

// These are all the textures referenced by the game code (it won't compile otherwise because of TEXTURE_NAME)
//
// these are also therefore all the texture names that are used in animations, so we have the WHD use these offsets directly for texture numbers, so
// we can keep some shorter arrays for dealing with mutable info for changed textures
#define NAMED_TEXTURE_LIST \
    TEXTURE_NAME(_), /* 0 is not a valid texture number */ \
    TEXTURE_NAME(SKY4), /* this is not a switch texture, but oh well, here it keeps the evenness ok */ \
    /* switches start on an even number */                 \
    TEXTURE_NAME(SW1BRCOM), \
    TEXTURE_NAME(SW2BRCOM),\
                           \
    TEXTURE_NAME(SW1BRN1), \
    TEXTURE_NAME(SW2BRN1), \
                           \
    TEXTURE_NAME(SW1BRN2), \
    TEXTURE_NAME(SW2BRN2), \
                           \
    TEXTURE_NAME(SW1BRNGN), \
    TEXTURE_NAME(SW2BRNGN), \
                           \
    TEXTURE_NAME(SW1BROWN), \
    TEXTURE_NAME(SW2BROWN), \
                           \
    TEXTURE_NAME(SW1COMM), \
    TEXTURE_NAME(SW2COMM), \
                           \
    TEXTURE_NAME(SW1COMP), \
    TEXTURE_NAME(SW2COMP), \
                           \
    TEXTURE_NAME(SW1DIRT), \
    TEXTURE_NAME(SW2DIRT), \
                           \
    TEXTURE_NAME(SW1EXIT), \
    TEXTURE_NAME(SW2EXIT), \
                           \
    TEXTURE_NAME(SW1GRAY), \
    TEXTURE_NAME(SW2GRAY), \
                           \
    TEXTURE_NAME(SW1GRAY1), \
    TEXTURE_NAME(SW2GRAY1), \
                           \
    TEXTURE_NAME(SW1METAL), \
    TEXTURE_NAME(SW2METAL), \
                           \
    TEXTURE_NAME(SW1PIPE), \
    TEXTURE_NAME(SW2PIPE), \
                           \
    TEXTURE_NAME(SW1SLAD), \
    TEXTURE_NAME(SW2SLAD), \
                           \
    TEXTURE_NAME(SW1STON2), \
    TEXTURE_NAME(SW2STON2), \
                           \
    TEXTURE_NAME(SW1STARG), \
    TEXTURE_NAME(SW2STARG), \
                           \
    TEXTURE_NAME(SW1STON1), \
    TEXTURE_NAME(SW2STON1), \
                           \
    TEXTURE_NAME(SW1GARG), \
    TEXTURE_NAME(SW2GARG), \
                           \
    TEXTURE_NAME(SW1HOT), \
    TEXTURE_NAME(SW2HOT), \
                           \
    TEXTURE_NAME(SW1BLUE), \
    TEXTURE_NAME(SW2BLUE), \
                           \
    TEXTURE_NAME(SW1SATYR), \
    TEXTURE_NAME(SW2SATYR), \
                           \
    TEXTURE_NAME(SW1SKIN), \
    TEXTURE_NAME(SW2SKIN), \
                           \
    TEXTURE_NAME(SW1VINE), \
    TEXTURE_NAME(SW2VINE), \
                           \
    TEXTURE_NAME(SW1WOOD), \
    TEXTURE_NAME(SW2WOOD), \
                           \
    TEXTURE_NAME(SW1PANEL), \
    TEXTURE_NAME(SW2PANEL), \
                           \
    TEXTURE_NAME(SW1ROCK), \
    TEXTURE_NAME(SW2ROCK), \
                           \
    TEXTURE_NAME(SW1MET2), \
    TEXTURE_NAME(SW2MET2), \
                           \
    TEXTURE_NAME(SW1WDMET), \
    TEXTURE_NAME(SW2WDMET), \
                           \
    TEXTURE_NAME(SW1BRIK), \
    TEXTURE_NAME(SW2BRIK), \
                           \
    TEXTURE_NAME(SW1MOD1), \
    TEXTURE_NAME(SW2MOD1), \
                           \
    TEXTURE_NAME(SW1ZIM), \
    TEXTURE_NAME(SW2ZIM), \
                           \
    TEXTURE_NAME(SW1STON6), \
    TEXTURE_NAME(SW2STON6), \
                           \
    TEXTURE_NAME(SW1TEK), \
    TEXTURE_NAME(SW2TEK), \
                           \
    TEXTURE_NAME(SW1MARB), \
    TEXTURE_NAME(SW2MARB), \
                           \
    TEXTURE_NAME(SW1SKULL), \
    TEXTURE_NAME(SW2SKULL), \
                           \
    TEXTURE_NAME(SW1STONE), \
    TEXTURE_NAME(SW2STONE), \
                           \
    TEXTURE_NAME(SW1STRTN), \
    TEXTURE_NAME(SW2STRTN), \
                           \
    TEXTURE_NAME(SW1CMT), \
    TEXTURE_NAME(SW2CMT), \
                           \
    TEXTURE_NAME(SW1GSTON), \
    TEXTURE_NAME(SW2GSTON), \
                           \
    TEXTURE_NAME(SW1LION), \
    TEXTURE_NAME(SW2LION), \
    /* end of switch textures - note SW2LION is used as a constant */ \
    TEXTURE_NAME(BLODGR1), \
    TEXTURE_NAME(BLODGR2), \
    TEXTURE_NAME(BLODGR3), \
    TEXTURE_NAME(BLODGR4), \
    TEXTURE_NAME(SLADRIP1), \
    TEXTURE_NAME(SLADRIP2), \
    TEXTURE_NAME(SLADRIP3), \
    TEXTURE_NAME(BLODRIP1), \
    TEXTURE_NAME(BLODRIP2), \
    TEXTURE_NAME(BLODRIP3), \
    TEXTURE_NAME(BLODRIP4), \
    TEXTURE_NAME(FIREWALA), \
    TEXTURE_NAME(FIREWALB), \
    TEXTURE_NAME(FIREWALL), \
    TEXTURE_NAME(GSTFONT1), \
    TEXTURE_NAME(GSTFONT2), \
    TEXTURE_NAME(GSTFONT3), \
    TEXTURE_NAME(FIRELAV3), \
    /* TEXTURE_NAME(FIRELAV2), */ \
    TEXTURE_NAME(FIRELAVA), \
    TEXTURE_NAME(FIREMAG1), \
    TEXTURE_NAME(FIREMAG2), \
    TEXTURE_NAME(FIREMAG3), \
    TEXTURE_NAME(FIREBLU1), \
    TEXTURE_NAME(FIREBLU2), \
    TEXTURE_NAME(ROCKRED1), \
    TEXTURE_NAME(ROCKRED2), \
    TEXTURE_NAME(ROCKRED3), \
    TEXTURE_NAME(BFALL1), \
    TEXTURE_NAME(BFALL2), \
    TEXTURE_NAME(BFALL3), \
    TEXTURE_NAME(BFALL4), \
    TEXTURE_NAME(SFALL1), \
    TEXTURE_NAME(SFALL2), \
    TEXTURE_NAME(SFALL3), \
    TEXTURE_NAME(SFALL4), \
    TEXTURE_NAME(WFALL1), \
    TEXTURE_NAME(WFALL2), \
    TEXTURE_NAME(WFALL3), \
    TEXTURE_NAME(WFALL4), \
    TEXTURE_NAME(DBRAIN1), \
    TEXTURE_NAME(DBRAIN2), \
    TEXTURE_NAME(DBRAIN3), \
    TEXTURE_NAME(DBRAIN4), \
    TEXTURE_NAME(SKY1), \
    TEXTURE_NAME(SKY2), \
    TEXTURE_NAME(SKY3)

#if USE_WHD
#define LAST_SWITCH_TEXTURE NTEX_SW2LION
enum {
    NAMED_TEXTURE_LIST,
    NUM_SPECIAL_TEXTURES
};
enum {
    NAMED_FLAT_LIST,
    NUM_SPECIAL_FLATS
};
enum {
    VPATCH_LIST,
    NUM_VPATCHES,
};
#include <assert.h>
static_assert(NUM_VPATCHES < 512, "");
#endif

typedef PACKED_STRUCT({
    // Partition line from (x,y) to x+dx,y+dy)
    short		x;
    short		y;
    short		dx;
    short		dy;
    //uint16_t            children[2];
    uint16_t            coded_children;

    // Bounding box for each child,
    // clip against view frustum.
    uint8_t             bbox_lw[2];
    uint8_t             bbox_th[2];
}) whdnode_t;
#include <assert.h>
static_assert(sizeof(whdnode_t)==14, "");

typedef PACKED_STRUCT({
    uint32_t size;
    uint32_t hash;
    char name[14];
    uint16_t num_named_lumps;
}) whdheader_t;
static_assert(sizeof(whdheader_t)==24, "");
extern const whdheader_t *whdheader;

#define WHD_MAX_COL_SEGS 8 // todo may be smaller
#define WHD_MAX_COL_UNIQUE_PATCHES 4  // 4 * 128 = 512 which is how big we like to keep the decoder_tmp in pd_render_nh (when used for decoding)

#define WHD_COL_SEG_MEMCPY              0x80 // segment is a memcpy
#define WHD_COL_SEG_EXPLICIT_Y          0x40 // segment has an explcit start y rather than starting from the bottom of the previous seg
#define WHD_COL_SEG_MEMCPY_SOURCE       0x20 // segment is targeted by a memcpy (which means at runtime we cannot skip rendering it just because it is out side the visible y range)
#define WHD_COL_SEG_MEMCPY_IS_BACKWARDS 0x10 // direction for memcpy is explicit to get the right effect for overlapping (sometimes we want to interfere, sometimes not)

#define WHD_PATCH_MAX_WIDTH 257
#define WHD_FLAT_DECODER_MAX_SIZE 512
#endif