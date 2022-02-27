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
//	Created by a sound utility.
//	Kept as a sample, DOOM2 sounds.
//

#if !INCLUDE_SOUND_C_IN_S_SOUND
#include <stdlib.h>
#include <assert.h>


#include "doomtype.h"
#include "sounds.h"
//
// Information about all the music
//

#if !DOOM_SMALL
#define MUSIC(name) { name, 0, NULL, NULL }
#else
#define MUSIC(name) { name, 0, NULL }
#endif

// todo graham big waste of space
musicinfo_t S_music[] =
{
#if !USE_CONST_SFX
    MUSIC(NULL),
#else
    MUSIC(""),
#endif
    MUSIC("e1m1"),
    MUSIC("e1m2"),
    MUSIC("e1m3"),
    MUSIC("e1m4"),
    MUSIC("e1m5"),
    MUSIC("e1m6"),
    MUSIC("e1m7"),
    MUSIC("e1m8"),
    MUSIC("e1m9"),
    MUSIC("e2m1"),
    MUSIC("e2m2"),
    MUSIC("e2m3"),
    MUSIC("e2m4"),
    MUSIC("e2m5"),
    MUSIC("e2m6"),
    MUSIC("e2m7"),
    MUSIC("e2m8"),
    MUSIC("e2m9"),
    MUSIC("e3m1"),
    MUSIC("e3m2"),
    MUSIC("e3m3"),
    MUSIC("e3m4"),
    MUSIC("e3m5"),
    MUSIC("e3m6"),
    MUSIC("e3m7"),
    MUSIC("e3m8"),
    MUSIC("e3m9"),
    MUSIC("inter"),
    MUSIC("intro"),
    MUSIC("bunny"),
    MUSIC("victor"),
    MUSIC("introa"),
    MUSIC("runnin"),
    MUSIC("stalks"),
    MUSIC("countd"),
    MUSIC("betwee"),
    MUSIC("doom"),
    MUSIC("the_da"),
    MUSIC("shawn"),
    MUSIC("ddtblu"),
    MUSIC("in_cit"),
    MUSIC("dead"),
    MUSIC("stlks2"),
    MUSIC("theda2"),
    MUSIC("doom2"),
    MUSIC("ddtbl2"),
    MUSIC("runni2"),
    MUSIC("dead2"),
    MUSIC("stlks3"),
    MUSIC("romero"),
    MUSIC("shawn2"),
    MUSIC("messag"),
    MUSIC("count2"),
    MUSIC("ddtbl3"),
    MUSIC("ampie"),
    MUSIC("theda3"),
    MUSIC("adrian"),
    MUSIC("messg2"),
    MUSIC("romer2"),
    MUSIC("tense"),
    MUSIC("shawn3"),
    MUSIC("openin"),
    MUSIC("evil"),
    MUSIC("ultima"),
    MUSIC("read_m"),
    MUSIC("dm2ttl"),
    MUSIC("dm2int") 
};


//
// Information about all the sfx
//

#if !DOOM_ONLY
#define FIRST_ONE 0,
#define LAST_TWO -1, NULL
#else
#define FIRST_ONE
#define LAST_TWO
#endif

#if !USE_CONST_SFX
#define SOUND(name, priority) \
  { FIRST_ONE name, priority, NULL, -1, -1, 0, 0, LAST_TWO }
#define SOUND_LINK(name, priority, link_id, pitch, volume) \
  { FIRST_ONE name, priority, &S_sfx[link_id], pitch, volume, 0, 0, LAST_TWO }
#else
#define SOUND(name, priority) \
  { FIRST_ONE name, priority, NULL, -1, -1, LAST_TWO }
#define SOUND_LINK(name, priority, link_id, pitch, volume) \
  { FIRST_ONE name, priority, &S_sfx[link_id], pitch, volume, LAST_TWO }
#endif

sfxinfo_t S_sfx[NUM_SFX] =
{
  // S_sfx[0] needs to be a dummy for odd reasons.
  SOUND("none",   0),
  SOUND("pistol", 64),
  SOUND("shotgn", 64),
  SOUND("sgcock", 64),
  SOUND("dshtgn", 64),
  SOUND("dbopn",  64),
  SOUND("dbcls",  64),
  SOUND("dbload", 64),
  SOUND("plasma", 64),
  SOUND("bfg",    64),
  SOUND("sawup",  64),
  SOUND("sawidl", 118),
  SOUND("sawful", 64),
  SOUND("sawhit", 64),
  SOUND("rlaunc", 64),
  SOUND("rxplod", 70),
  SOUND("firsht", 70),
  SOUND("firxpl", 70),
  SOUND("pstart", 100),
  SOUND("pstop",  100),
  SOUND("doropn", 100),
  SOUND("dorcls", 100),
  SOUND("stnmov", 119),
  SOUND("swtchn", 78),
  SOUND("swtchx", 78),
  SOUND("plpain", 96),
  SOUND("dmpain", 96),
  SOUND("popain", 96),
  SOUND("vipain", 96),
  SOUND("mnpain", 96),
  SOUND("pepain", 96),
  SOUND("slop",   78),
  SOUND("itemup", 78),
  SOUND("wpnup",  78),
  SOUND("oof",    96),
  SOUND("telept", 32),
  SOUND("posit1", 98),
  SOUND("posit2", 98),
  SOUND("posit3", 98),
  SOUND("bgsit1", 98),
  SOUND("bgsit2", 98),
  SOUND("sgtsit", 98),
  SOUND("cacsit", 98),
  SOUND("brssit", 94),
  SOUND("cybsit", 92),
  SOUND("spisit", 90),
  SOUND("bspsit", 90),
  SOUND("kntsit", 90),
  SOUND("vilsit", 90),
  SOUND("mansit", 90),
  SOUND("pesit",  90),
  SOUND("sklatk", 70),
  SOUND("sgtatk", 70),
  SOUND("skepch", 70),
  SOUND("vilatk", 70),
  SOUND("claw",   70),
  SOUND("skeswg", 70),
  SOUND("pldeth", 32),
  SOUND("pdiehi", 32),
  SOUND("podth1", 70),
  SOUND("podth2", 70),
  SOUND("podth3", 70),
  SOUND("bgdth1", 70),
  SOUND("bgdth2", 70),
  SOUND("sgtdth", 70),
  SOUND("cacdth", 70),
  SOUND("skldth", 70),
  SOUND("brsdth", 32),
  SOUND("cybdth", 32),
  SOUND("spidth", 32),
  SOUND("bspdth", 32),
  SOUND("vildth", 32),
  SOUND("kntdth", 32),
  SOUND("pedth",  32),
  SOUND("skedth", 32),
  SOUND("posact", 120),
  SOUND("bgact",  120),
  SOUND("dmact",  120),
  SOUND("bspact", 100),
  SOUND("bspwlk", 100),
  SOUND("vilact", 100),
  SOUND("noway",  78),
  SOUND("barexp", 60),
  SOUND("punch",  64),
  SOUND("hoof",   70),
  SOUND("metal",  70),
  SOUND_LINK("chgun", 64, sfx_pistol, 150, 0),
  SOUND("tink",   60),
  SOUND("bdopn",  100),
  SOUND("bdcls",  100),
  SOUND("itmbk",  100),
  SOUND("flame",  32),
  SOUND("flamst", 32),
  SOUND("getpow", 60),
  SOUND("bospit", 70),
  SOUND("boscub", 70),
  SOUND("bossit", 70),
  SOUND("bospn",  70),
  SOUND("bosdth", 70),
  SOUND("manatk", 70),
  SOUND("mandth", 70),
  SOUND("sssit",  70),
  SOUND("ssdth",  70),
  SOUND("keenpn", 70),
  SOUND("keendt", 70),
  SOUND("skeact", 70),
  SOUND("skesit", 70),
  SOUND("skeatk", 70),
  SOUND("radio",  60),
};

#if USE_CONST_SFX
sfxinfo_mut_t S_sfx_mut[NUM_SFX];

sfxinfo_mut_t *get_mut_sfxinfo_t(const sfxinfo_t *sfxinfo) {
    // todo graham maybe pass indexes around instead of pointers anyway
    unsigned int index = sfxinfo - S_sfx;
    assert(index < NUM_SFX);
    return S_sfx_mut + index;
}

#endif

#endif