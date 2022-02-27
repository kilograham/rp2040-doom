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
// DESCRIPTION:  none
//

#define xprintf(x, ...) ((void)0)

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "doomdef.h" 
#include "doomkeys.h"
#include "doomstat.h"

#include "deh_main.h"
#include "deh_misc.h"

#include "z_zone.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_controls.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_input.h"
#include "i_video.h"

#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"

#include "d_main.h"

#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "statdump.h"
#include "m_menu.h"
// Needs access to LFB.
#include "v_video.h"

#include "w_wad.h"

#include "p_local.h" 

#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

// SKY handling - still the wrong place.
#include "r_data.h"
#include "r_sky.h"



#include "g_game.h"

#if USE_WHD
#include "tiny_huff.h"
#include "picodoom.h"
#endif

#define SAVEGAMESIZE	0x2c000

void	G_ReadDemoTiccmd (ticcmd_t* cmd); 
void	G_WriteDemoTiccmd (ticcmd_t* cmd); 
void	G_PlayerReborn (int player); 
 
void	G_DoReborn (int playernum); 
 
void	G_DoLoadLevel (void); 
void	G_DoNewGame (boolean net);
void	G_DoPlayDemo (void); 
void	G_DoCompleted (void); 
void	G_DoVictory (void); 
void	G_DoWorldDone (void); 
void	G_DoSaveGame (void); 
 
// Gamestate the last time G_Ticker was called.

gamestate_t     oldgamestate; 
 
gameaction_t    gameaction; 
gamestate_t     gamestate; 
skill_t         gameskill; 
boolean		respawnmonsters;
isb_int8_t      gameepisode;
isb_int8_t      gamemap;

// If non-zero, exit the level after this number of minutes.

int             timelimit;

boolean         paused; 
boolean         sendpause;             	// send a pause event next tic 
boolean         sendsave;             	// send a save event next tic 
boolean         usergame;               // ok to save / end game 
 
boolean         timingdemo;             // if true, exit with report on completion
#if !FORCE_NODRAW
boolean         nodrawers;              // for comparative timing purposes
#endif
int             starttime;          	// for comparative timing purposes  	 
 
boolean         viewactive; 
 
isb_int8_t      deathmatch;           	// only if started as net death
#if !NO_USE_NET || USE_PICO_NET
boolean         netgame;                // only true if packets are broadcast
#endif
boolean         playeringame[MAXPLAYERS]; 
player_t        players[MAXPLAYERS]; 

boolean         turbodetected[MAXPLAYERS];
 
isb_int8_t      consoleplayer;          // player taking events and displaying
isb_int8_t      displayplayer;          // view being displayed
int             levelstarttic;          // gametic at level start 
int             totalkills, totalitems, totalsecret;    // for intermission
 
#if !NO_DEMO_RECORDING
char           *demoname;
boolean         demorecording;
#endif
boolean         longtics;               // cph's doom 1.91 longtics hack
boolean         lowres_turn;            // low resolution turning for longtics
boolean         demoplayback; 
boolean		netdemo;
byte*		demobuffer;
#if !USE_WHD
byte*		demo_p;
#else
struct {
    th_bit_input bi;
    uint8_t   changes_offset;
    uint8_t   fb_delta_offset;
    uint8_t   strafe_offset;
    uint8_t   turn_delta_offset;
    uint8_t   buttons_offset;
    int8_t    last_fb;
    int8_t    last_turn;
    uint16_t *decoders;
} demo_decode;
#endif
byte*		demoend;
boolean         singledemo;            	// quit after playing a demo from cmdline 
 
boolean         precache = true;        // if true, load all graphics at start 

#if !DOOM_TINY
boolean         testcontrols = false;    // Invoked by setup to test controls
int             testcontrols_mousespeed;
#endif

 
wbstartstruct_t wminfo;               	// parms for world map / intermission 
 
byte		consistancy[MAXPLAYERS][BACKUPTICS]; 
 
#define MAXPLMOVE		(forwardmove[1]) 
 
#define TURBOTHRESHOLD	0x32

#if !NO_USE_ARGS
// there is --turbo option that hackily changes these
fixed_t         forwardmove[2] = {0x19, 0x32};
fixed_t         sidemove[2] = {0x18, 0x28};
fixed_t         angleturn[3] = {640, 1280, 320};    // + slow turn
#else
static const int16_t         forwardmove[2] = {0x19, 0x32};
static const int16_t         sidemove[2] = {0x18, 0x28};
static const int16_t         angleturn[3] = {640, 1280, 320};    // + slow turn
#endif

static const key_type_t * const weapon_keys[] = {
    &key_weapon1,
    &key_weapon2,
    &key_weapon3,
    &key_weapon4,
    &key_weapon5,
    &key_weapon6,
    &key_weapon7,
    &key_weapon8
};

// Set to -1 or +1 to switch to the previous or next weapon.

static int next_weapon = 0;

// Used for prev/next weapon keys.

static const struct
{
    weapontype_t weapon;
    weapontype_t weapon_num;
} weapon_order_table[] = {
    { wp_fist,            wp_fist },
    { wp_chainsaw,        wp_fist },
    { wp_pistol,          wp_pistol },
    { wp_shotgun,         wp_shotgun },
    { wp_supershotgun,    wp_shotgun },
    { wp_chaingun,        wp_chaingun },
    { wp_missile,         wp_missile },
    { wp_plasma,          wp_plasma },
    { wp_bfg,             wp_bfg }
};

#define SLOWTURNTICS	6 
 
#define NUMKEYS		256 
#define MAX_JOY_BUTTONS 20

#if !DOOM_SMALL
static boolean  gamekeydown[NUMKEYS];
#define is_gamekeydown(k) gamekeydown[k]
#define set_gamekeydown(k,d) gamekeydown[k] = (d)
#else
static uint8_t gamekeydown[(NUMKEYS + 7) >> 3];
#define is_gamekeydown(k) (gamekeydown[(k)>>3] & (1u << ((k)&7u)))
static inline void set_gamekeydown(unsigned k, boolean down) {
    if (down)
        gamekeydown[(k)>>3] |= (1u << ((k)&7u));
    else
        gamekeydown[(k)>>3] &= ~(1u << ((k)&7u));
}
#endif

static int      turnheld;		// for accelerative turning
 
static boolean  mousearray[MAX_MOUSE_BUTTONS + 1];
static boolean *mousebuttons = &mousearray[1];  // allow [-1]

// mouse values are used once 
int             mousex;
int             mousey;         

static int      dclicktime;
static boolean  dclickstate;
static isb_int8_t dclicks;
static int      dclicktime2;
static boolean  dclickstate2;
static isb_int8_t dclicks2;

#if !NO_USE_JOYSTICK
// joystick values are repeated 
static int      joyxmove;
static int      joyymove;
static int      joystrafemove;
static boolean  joyarray[MAX_JOY_BUTTONS + 1]; 
static boolean *joybuttons = &joyarray[1];		// allow [-1] 
#endif

static isb_int8_t savegameslot;
static char     savedescription[32]; 
 
#define	BODYQUESIZE	32

shortptr_t /*mobj_t*	*/ bodyque[BODYQUESIZE];
isb_uint8_t 		bodyqueslot;
 
isb_int8_t vanilla_savegame_limit = 1;
isb_int8_t vanilla_demo_limit = 1;
 
int G_CmdChecksum (ticcmd_t* cmd) 
{ 
    size_t		i;
    int		sum = 0; 
	 
    for (i=0 ; i< sizeof(*cmd)/4 - 1 ; i++) 
	sum += ((int *)cmd)[i]; 
		 
    return sum; 
} 

static boolean WeaponSelectable(weapontype_t weapon)
{
    // Can't select the super shotgun in Doom 1.

    if (weapon == wp_supershotgun && logical_gamemission == doom)
    {
        return false;
    }

    // These weapons aren't available in shareware.

    if ((weapon == wp_plasma || weapon == wp_bfg)
     && gamemission == doom && gamemode == shareware)
    {
        return false;
    }

    // Can't select a weapon if we don't own it.

    if (!players[consoleplayer].weaponowned[weapon])
    {
        return false;
    }

    // Can't select the fist if we have the chainsaw, unless
    // we also have the berserk pack.

    if (weapon == wp_fist
     && players[consoleplayer].weaponowned[wp_chainsaw]
     && !players[consoleplayer].powers[pw_strength])
    {
        return false;
    }

    return true;
}

static int G_NextWeapon(int direction)
{
    weapontype_t weapon;
    int start_i, i;

    // Find index in the table.

    if (players[consoleplayer].pendingweapon == wp_nochange)
    {
        weapon = players[consoleplayer].readyweapon;
    }
    else
    {
        weapon = players[consoleplayer].pendingweapon;
    }

    for (i=0; i<arrlen(weapon_order_table); ++i)
    {
        if (weapon_order_table[i].weapon == weapon)
        {
            break;
        }
    }

    // Switch weapon. Don't loop forever.
    start_i = i;
    do
    {
        i += direction;
        i = (i + arrlen(weapon_order_table)) % arrlen(weapon_order_table);
    } while (i != start_i && !WeaponSelectable(weapon_order_table[i].weapon));

    return weapon_order_table[i].weapon_num;
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer. 
// If recording a demo, write it out 
// 
void G_BuildTiccmd (ticcmd_t* cmd, int maketic) 
{ 
    int		i; 
    boolean	strafe;
    boolean	bstrafe; 
    int		speed;
    int		tspeed; 
    int		forward;
    int		side;

    memset(cmd, 0, sizeof(ticcmd_t));

#if DEBUG_CONSISTENCY
    if (netgame) printf("MAKETIC READ CONSISTENCY %d (slot %d) %d %02x\n", maketic, maketic%BACKUPTICS, consoleplayer, consistancy[consoleplayer][maketic%BACKUPTICS]);
#endif
    cmd->consistancy = 
	consistancy[consoleplayer][maketic%BACKUPTICS]; 
 
    strafe = is_gamekeydown(key_strafe)
            || mousebuttons[mousebstrafe]
#if !NO_USE_JOYSTICK
	        || joybuttons[joybstrafe]
#endif
        ;

    // fraggle: support the old "joyb_speed = 31" hack which
    // allowed an autorun effect

    speed = key_speed >= NUMKEYS
         || is_gamekeydown(key_speed)
#if !NO_USE_JOYSTICK
         || joybspeed >= MAX_JOY_BUTTONS
         || joybuttons[joybspeed]
#endif
         ;

    forward = side = 0;
    
    // use two stage accelerative turning
    // on the keyboard and joystick
    if (is_gamekeydown(key_right)
            || is_gamekeydown(key_left)
#if !NO_USE_JOYSTICK
            || joyxmove < 0
	        || joyxmove > 0
#endif
    )
	turnheld += ticdup;
    else 
	turnheld = 0; 

    if (turnheld < SLOWTURNTICS) 
	tspeed = 2;             // slow turn 
    else 
	tspeed = speed;
    
    // let movement keys cancel each other out
    if (strafe) 
    { 
	if (is_gamekeydown(key_right))
	{
	    // fprintf(stderr, "strafe right\n");
	    side += sidemove[speed]; 
	}
	if (is_gamekeydown(key_left))
	{
	    //	fprintf(stderr, "strafe left\n");
	    side -= sidemove[speed]; 
	}
#if !NO_USE_JOYSTICK
	if (joyxmove > 0) 
	    side += sidemove[speed]; 
	if (joyxmove < 0) 
	    side -= sidemove[speed];
#endif
 
    } 
    else 
    { 
	if (is_gamekeydown(key_right))
	    cmd->angleturn -= angleturn[tspeed]; 
	if (is_gamekeydown(key_left))
	    cmd->angleturn += angleturn[tspeed];
#if !NO_USE_JOYSTICK
	if (joyxmove > 0)
	    cmd->angleturn -= angleturn[tspeed]; 
	if (joyxmove < 0) 
	    cmd->angleturn += angleturn[tspeed]; 
#endif
    }

    if (is_gamekeydown(key_up))
    {
	// fprintf(stderr, "up\n");
	forward += forwardmove[speed]; 
    }
    if (is_gamekeydown(key_down))
    {
	// fprintf(stderr, "down\n");
	forward -= forwardmove[speed]; 
    }

#if !NO_USE_JOYSTICK
    if (joyymove < 0) 
        forward += forwardmove[speed]; 
    if (joyymove > 0) 
        forward -= forwardmove[speed];
#endif

    if (is_gamekeydown(key_strafeleft)

     || mousebuttons[mousebstrafeleft]
#if !NO_USE_JOYSTICK
     || joybuttons[joybstrafeleft]
     || joystrafemove < 0
#endif
     )
    {
        side -= sidemove[speed];
    }

    if (is_gamekeydown(key_straferight)
     || mousebuttons[mousebstraferight]
#if !NO_USE_JOYSTICK
     || joybuttons[joybstraferight]
     || joystrafemove > 0
#endif
     )
    {
        side += sidemove[speed]; 
    }

#if !NO_USE_NET
    // buttons
    cmd->chatchar = HU_dequeueChatChar();
#endif
 
    if (is_gamekeydown(key_fire)
    || mousebuttons[mousebfire]
#if !NO_USE_JOYSTICK
	|| joybuttons[joybfire]
#endif
	)
	cmd->buttons |= BT_ATTACK; 
 
    if (is_gamekeydown(key_use)
#if !NO_USE_JOYSTICK
     || joybuttons[joybuse]
#endif
     || mousebuttons[mousebuse]
     )
    { 
	cmd->buttons |= BT_USE;
	// clear double clicks if hit use button 
	dclicks = 0;                   
    } 

    // If the previous or next weapon button is pressed, the
    // next_weapon variable is set to change weapons when
    // we generate a ticcmd.  Choose a new weapon.

    if (gamestate == GS_LEVEL && next_weapon != 0)
    {
        i = G_NextWeapon(next_weapon);
        cmd->buttons |= BT_CHANGE;
        cmd->buttons |= i << BT_WEAPONSHIFT;
    }
    else
    {
        // Check weapon keys.

        for (i=0; i<arrlen(weapon_keys); ++i)
        {
            int key = *weapon_keys[i];

            if (is_gamekeydown(key))
            {
                cmd->buttons |= BT_CHANGE;
                cmd->buttons |= i<<BT_WEAPONSHIFT;
                break;
            }
        }
    }

    next_weapon = 0;

    // mouse
    if (mousebuttons[mousebforward]) 
    {
	forward += forwardmove[speed];
    }
    if (mousebuttons[mousebbackward])
    {
        forward -= forwardmove[speed];
    }

    if (dclick_use)
    {
        // forward double click
        if (mousebuttons[mousebforward] != dclickstate && dclicktime > 1 ) 
        { 
            dclickstate = mousebuttons[mousebforward]; 
            if (dclickstate) 
                dclicks++; 
            if (dclicks == 2) 
            { 
                cmd->buttons |= BT_USE; 
                dclicks = 0; 
            } 
            else 
                dclicktime = 0; 
        } 
        else 
        { 
            dclicktime += ticdup; 
            if (dclicktime > 20) 
            { 
                dclicks = 0; 
                dclickstate = 0; 
            } 
        }
        
        // strafe double click
        bstrafe =
            mousebuttons[mousebstrafe]
#if !NO_USE_JOYSTICK
        || joybuttons[joybstrafe]
#endif
            ;
        if (bstrafe != dclickstate2 && dclicktime2 > 1 ) 
        { 
            dclickstate2 = bstrafe; 
            if (dclickstate2) 
                dclicks2++; 
            if (dclicks2 == 2) 
            { 
                cmd->buttons |= BT_USE; 
                dclicks2 = 0; 
            } 
            else 
                dclicktime2 = 0; 
        } 
        else 
        { 
            dclicktime2 += ticdup; 
            if (dclicktime2 > 20) 
            { 
                dclicks2 = 0; 
                dclickstate2 = 0; 
            } 
        } 
    }

    forward += mousey; 

    if (strafe) 
	side += mousex*2; 
    else 
	cmd->angleturn -= mousex*0x8;

#if !DOOM_TINY
    if (mousex == 0)
    {
        // No movement in the previous frame

        testcontrols_mousespeed = 0;
    }
#endif
    
    mousex = mousey = 0; 
	 
    if (forward > MAXPLMOVE) 
	forward = MAXPLMOVE; 
    else if (forward < -MAXPLMOVE) 
	forward = -MAXPLMOVE; 
    if (side > MAXPLMOVE) 
	side = MAXPLMOVE; 
    else if (side < -MAXPLMOVE) 
	side = -MAXPLMOVE; 
 
    cmd->forwardmove += forward; 
    cmd->sidemove += side;
    
    // special buttons
    if (sendpause) 
    { 
	sendpause = false; 
	cmd->buttons = BT_SPECIAL | BTS_PAUSE; 
    } 
 
    if (sendsave) 
    { 
	sendsave = false; 
	cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot<<BTS_SAVESHIFT); 
    } 

    // low-res turning

    if (lowres_turn)
    {
        static signed short carry = 0;
        signed short desired_angleturn;

        desired_angleturn = cmd->angleturn + carry;

        // round angleturn to the nearest 256 unit boundary
        // for recording demos with single byte values for turn

        cmd->angleturn = (desired_angleturn + 128) & 0xff00;

        // Carry forward the error from the reduced resolution to the
        // next tic, so that successive small movements can accumulate.

        carry = desired_angleturn - cmd->angleturn;
    }
#if DOOM_TINY
    cmd->ingame = true;
#endif
} 
 

//
// G_DoLoadLevel 
//
void G_DoLoadLevel (void) 
{ 
    int             i; 

    // Set the sky map.
    // First thing, we have a dummy sky texture name,
    //  a flat. The data is in the WAD only because
    //  we look for an actual index, instead of simply
    //  setting one.

#if !USE_WHD
    skyflatnum = R_FlatNumForName(DEH_String(SKYFLATNAME));
#else
    skyflatnum = W_GetNumForName(SKYFLATNAME) - firstflat;
#endif

    // The "Sky never changes in Doom II" bug was fixed in
    // the id Anthology version of doom2.exe for Final Doom.
    if ((gamemode == commercial)
     && (gameversion == exe_final2 || gameversion_is_chex(gameversion)))
    {
        texturename_t skytexturename;

        if (gamemap < 12)
        {
            skytexturename = TEXTURE_NAME(SKY1);
        }
        else if (gamemap < 21)
        {
            skytexturename = TEXTURE_NAME(SKY2);
        }
        else
        {
            skytexturename = TEXTURE_NAME(SKY3);
        }

        skytexturename = DEH_TextureName(skytexturename);

        skytexture = R_TextureNumForName(skytexturename);
    }

    levelstarttic = gametic;        // for time calculation
    
    if (wipegamestate == GS_LEVEL)
	wipegamestate = -1;             // force a wipe

    gamestate = GS_LEVEL; 

    for (i=0 ; i<MAXPLAYERS ; i++) 
    { 
	turbodetected[i] = false;
	if (playeringame[i] && players[i].playerstate == PST_DEAD) 
	    players[i].playerstate = PST_REBORN; 
	memset (players[i].frags,0,sizeof(players[i].frags)); 
    } 
		 
    P_SetupLevel (gameepisode, gamemap, 0, gameskill);
    displayplayer = consoleplayer;		// view the guy you are playing
    gameaction = ga_nothing;
    Z_CheckHeap ();
    
    // clear cmd building stuff

    memset (gamekeydown, 0, sizeof(gamekeydown));
#if !NO_USE_JOYSTICK
    joyxmove = joyymove = joystrafemove = 0;
#endif
    mousex = mousey = 0;
    sendpause = sendsave = paused = false;
    memset(mousearray, 0, sizeof(mousearray));
#if !NO_USE_JOYSTICK
    memset(joyarray, 0, sizeof(joyarray));
#endif
    if (testcontrols)
    {
        players[consoleplayer].message = "Press escape to quit.";
    }
} 

#if !NO_USE_JOYSTICK
static void SetJoyButtons(unsigned int buttons_mask)
{
    int i;

    for (i=0; i<MAX_JOY_BUTTONS; ++i)
    {
        int button_on = (buttons_mask & (1 << i)) != 0;

        // Detect button press:

        if (!joybuttons[i] && button_on)
        {
            // Weapon cycling:

            if (i == joybprevweapon)
            {
                next_weapon = -1;
            }
            else if (i == joybnextweapon)
            {
                next_weapon = 1;
            }
        }

        joybuttons[i] = button_on;
    }
}
#endif

static void SetMouseButtons(unsigned int buttons_mask)
{
    int i;

    for (i=0; i<MAX_MOUSE_BUTTONS; ++i)
    {
        unsigned int button_on = (buttons_mask & (1 << i)) != 0;

        // Detect button press:

        if (!mousebuttons[i] && button_on)
        {
            if (i == mousebprevweapon)
            {
                next_weapon = -1;
            }
            else if (i == mousebnextweapon)
            {
                next_weapon = 1;
            }
        }

	mousebuttons[i] = button_on;
    }
}

//
// G_Responder  
// Get info needed to make ticcmd_ts for the players.
// 
boolean G_Responder (event_t* ev) 
{
#if USE_FPS
    if (ev->type == ev_keydown && ev->data2 =='\\') {
        show_fps ^= 1;
        return true;
    }
#endif

    // allow spy mode changes even during the demo
    if (gamestate == GS_LEVEL && ev->type == ev_keydown 
     && ev->data1 == key_spy && (singledemo || !deathmatch) )
    {
	// spy mode 
	do 
	{ 
	    displayplayer++; 
	    if (displayplayer == MAXPLAYERS) 
		displayplayer = 0; 
	} while (!playeringame[displayplayer] && displayplayer != consoleplayer); 
	return true; 
    }
    
    // any other key pops up menu if in demos
    if (gameaction == ga_nothing && !singledemo && 
	(demoplayback || gamestate == GS_DEMOSCREEN) 
	) 
    { 
	if (ev->type == ev_keydown ||  
	    (ev->type == ev_mouse && ev->data1) || 
	    (ev->type == ev_joystick && ev->data1) ) 
	{ 
	    M_StartControlPanel (); 
	    return true; 
	} 
	return false; 
    } 

    if (gamestate == GS_LEVEL) 
    { 
#if 0 
	if (devparm && ev->type == ev_keydown && ev->data1 == ';') 
	{ 
	    G_DeathMatchSpawnPlayer (0); 
	    return true; 
	} 
	if (HU_Responder (ev))
	    return true;	// chat ate the event 
#endif
    if (ST_Responder (ev))
	    return true;	// status window ate it 
	if (AM_Responder (ev)) 
	    return true;	// automap ate it 
    } 
	 
    if (gamestate == GS_FINALE) 
    { 
	if (F_Responder (ev)) 
	    return true;	// finale ate the event 
    }

#if !DOOM_TINY
    if (testcontrols && ev->type == ev_mouse)
    {
        // If we are invoked by setup to test the controls, save the 
        // mouse speed so that we can display it on-screen.
        // Perform a low pass filter on this so that the thermometer 
        // appears to move smoothly.

        testcontrols_mousespeed = abs(ev->data2);
    }
#endif

    // If the next/previous weapon keys are pressed, set the next_weapon
    // variable to change weapons when the next ticcmd is generated.

    if (ev->type == ev_keydown && ev->data1 == key_prevweapon)
    {
        next_weapon = -1;
    }
    else if (ev->type == ev_keydown && ev->data1 == key_nextweapon)
    {
        next_weapon = 1;
    }

    switch (ev->type) 
    { 
      case ev_keydown: 
	if (ev->data1 == key_pause) 
	{ 
	    sendpause = true; 
	}
        else if (ev->data1 <NUMKEYS) 
        {
            set_gamekeydown(ev->data1, true);
        }

	return true;    // eat key down events 
 
      case ev_keyup: 
	if (ev->data1 <NUMKEYS) 
	    set_gamekeydown(ev->data1, false);
	return false;   // always let key up events filter down 
		 
      case ev_mouse: 
        SetMouseButtons(ev->data1);
	mousex = ev->data2*(mouseSensitivity+5)/10; 
	mousey = ev->data3*(mouseSensitivity+5)/10; 
	return true;    // eat events 

#if !NO_USE_JOYSTICK
      case ev_joystick:
        SetJoyButtons(ev->data1);
	joyxmove = ev->data2; 
	joyymove = ev->data3; 
        joystrafemove = ev->data4;
	return true;    // eat events
#endif
      default: 
	break; 
    } 
 
    return false; 
} 
 
 
 
//
// G_Ticker
// Make ticcmd_ts for the players.
//
void G_Ticker (void) 
{ 
    int		i;
    int		buf; 
    ticcmd_t*	cmd;

    // do player reborns if needed
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i] && players[i].playerstate == PST_REBORN) 
	    G_DoReborn (i);

    // do things to change the game state
    while (gameaction != ga_nothing)
    {
#if DOOM_TINY
        if (gameaction == ga_loadlevel || gameaction == ga_newgame || gameaction == ga_completed || gameaction == ga_worlddone) {
            // we prefer (real doom seems to, chocolate doesn't, but i think it looks better without the menu wiping at least)
            // to remove menu/status before wipe, so insert an extra frame so it can be drawn as an (non framebuffer) overlay instead
            if (pre_wipe_state == PRE_WIPE_EXTRA_FRAME_DONE) {
                pre_wipe_state = PRE_WIPE_NONE;
            } else {
                pre_wipe_state = PRE_WIPE_EXTRA_FRAME_NEEDED;
                break;
            }
        }
#endif
	    switch (gameaction) {
        case ga_loadlevel:
            G_DoLoadLevel();
            break;
        case ga_newgame:
            G_DoNewGame(false);
            break;
#if USE_PICO_NET
        case ga_newgamenet:
            G_DoNewGame(true);
            break;
#endif
        case ga_loadgame:
            G_DoLoadGame();
            break;
        case ga_savegame:
            G_DoSaveGame();
            break;
      case ga_playdemo:
	    G_DoPlayDemo ();
	    break;
	  case ga_completed:
	    G_DoCompleted ();
	    break; 
	  case ga_victory: 
	    F_StartFinale (); 
	    break; 
	  case ga_worlddone: 
	    G_DoWorldDone (); 
	    break;
#if !NO_SCREENSHOT
	  case ga_screenshot:
	    V_ScreenShot("DOOM%02i.%s");
            players[consoleplayer].message = DEH_String("screen shot");
	    gameaction = ga_nothing; 
	    break;
#endif
#if DOOM_TINY
    case ga_deferredquit:
        I_Quit();
        break;
#endif
//	  case ga_nothing:
        default:
	    break; 
	}
    }
    
    // get commands, check consistancy,
    // and build new consistancy check
    buf = (gametic/ticdup)%BACKUPTICS; 
 
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    cmd = &players[i].cmd;

	    memcpy(cmd, &netcmds[i], sizeof(ticcmd_t));

	    if (demoplayback) 
		G_ReadDemoTiccmd (cmd); 
	    if (demorecording) 
		G_WriteDemoTiccmd (cmd);
	    
	    // check for turbo cheats

            // check ~ 4 seconds whether to display the turbo message. 
            // store if the turbo threshold was exceeded in any tics
            // over the past 4 seconds.  offset the checking period
            // for each player so messages are not displayed at the
            // same time.

            if (cmd->forwardmove > TURBOTHRESHOLD)
            {
                turbodetected[i] = true;
            }

            if ((gametic & 31) == 0 
             && ((gametic >> 5) % MAXPLAYERS) == i
             && turbodetected[i])
            {
                static char turbomessage[80];
                extern char *player_names[4];
                M_snprintf(turbomessage, sizeof(turbomessage),
                           "%s is turbo!", player_names[i]);
                players[consoleplayer].message = turbomessage;
                turbodetected[i] = false;
            }

	    if (netgame && !netdemo && !(gametic%ticdup) ) 
	    { 
		if (gametic > BACKUPTICS 
		    && consistancy[i][buf] != cmd->consistancy) 
		{
//#warning removed consistency failure
//#if !USE_PICO_NET
#if DEBUG_CONSISTENCY
		    printf("consistency error tic %d (slot %d) %d (%02x should be %02x)\n", gametic, gametic % BACKUPTICS, i,
			     cmd->consistancy, consistancy[i][buf]);
#else
            I_Error("consistency error tic %d (slot %d) %d (%02x should be %02x)\n", gametic, gametic % BACKUPTICS, i,
                   cmd->consistancy, consistancy[i][buf]);
#endif
//#endif
		} 
		if (players[i].mo) {
#if DEBUG_CONSISTENCY
            printf("calc consistency %d (slot %d) %d %02x\n", gametic, gametic % BACKUPTICS, i, players[i].mo->xy.x&0xff);
#endif
            consistancy[i][buf] = players[i].mo->xy.x;
        } else
		    consistancy[i][buf] = rndindex; 
	    } 
	}
    }
    
    // check for special buttons
    for (i=0 ; i<MAXPLAYERS ; i++)
    {
	if (playeringame[i]) 
	{ 
	    if (players[i].cmd.buttons & BT_SPECIAL) 
	    { 
		switch (players[i].cmd.buttons & BT_SPECIALMASK) 
		{ 
		  case BTS_PAUSE: 
		    paused ^= 1; 
		    if (paused) 
			S_PauseSound (); 
		    else 
			S_ResumeSound (); 
		    break; 
					 
		  case BTS_SAVEGAME: 
		    if (!savedescription[0]) 
                    {
                        M_StringCopy(savedescription, "NET GAME",
                                     sizeof(savedescription));
                    }

		    savegameslot =  
			(players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT; 
		    gameaction = ga_savegame; 
		    break; 
		} 
	    } 
	}
    }

    // Have we just finished displaying an intermission screen?

    if (oldgamestate == GS_INTERMISSION && gamestate != GS_INTERMISSION)
    {
#if !NO_USE_WI
        WI_End();
#endif
    }

    oldgamestate = gamestate;
    
    // do main actions
    switch (gamestate) 
    { 
      case GS_LEVEL:
	P_Ticker ();
#if DOOM_TINY
    if (!pre_wipe_state)
#endif
	    ST_Ticker ();
	AM_Ticker (); 
	HU_Ticker ();            
	break; 
	 
      case GS_INTERMISSION:
#if !NO_USE_WI
	WI_Ticker ();
#else
	G_WorldDone();
#endif
	break; 
			 
      case GS_FINALE: 
	F_Ticker (); 
	break; 
 
      case GS_DEMOSCREEN: 
	D_PageTicker (); 
	break;
    }        
} 
 
 
//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_InitPlayer 
// Called at the start.
// Called by the game initialization functions.
//
void G_InitPlayer (int player) 
{
    // clear everything else to defaults
    G_PlayerReborn (player); 
}
 
 

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//
void G_PlayerFinishLevel (int player) 
{ 
    player_t*	p; 
	 
    p = &players[player]; 
	 
    memset (p->powers, 0, sizeof (p->powers)); 
    memset (p->cards, 0, sizeof (p->cards)); 
    p->mo->flags &= ~MF_SHADOW;		// cancel invisibility 
    p->extralight = 0;			// cancel gun flashes 
    p->fixedcolormap = 0;		// cancel ir gogles 
    p->damagecount = 0;			// no palette changes 
    p->bonuscount = 0; 
} 
 

//
// G_PlayerReborn
// Called after a player dies 
// almost everything is cleared and initialized 
//
void G_PlayerReborn (int player) 
{ 
    player_t*	p; 
    int		i; 
    int		frags[MAXPLAYERS]; 
    int		killcount;
    int		itemcount;
    int		secretcount; 
	 
    memcpy (frags,players[player].frags,sizeof(frags)); 
    killcount = players[player].killcount; 
    itemcount = players[player].itemcount; 
    secretcount = players[player].secretcount; 
	 
    p = &players[player]; 
    memset (p, 0, sizeof(*p)); 
 
    memcpy (players[player].frags, frags, sizeof(players[player].frags)); 
    players[player].killcount = killcount; 
    players[player].itemcount = itemcount; 
    players[player].secretcount = secretcount; 
 
    p->usedown = p->attackdown = true;	// don't do anything immediately 
    p->playerstate = PST_LIVE;       
    p->health = deh_initial_health;     // Use dehacked value
    p->readyweapon = p->pendingweapon = wp_pistol; 
    p->weaponowned[wp_fist] = true; 
    p->weaponowned[wp_pistol] = true; 
    p->ammo[am_clip] = deh_initial_bullets; 
	 
    for (i=0 ; i<NUMAMMO ; i++) 
	p->maxammo[i] = maxammo[i]; 
		 
}

//
// G_CheckSpot  
// Returns false if the player cannot be respawned
// at the given mapthing_t spot  
// because something is occupying it 
//
void P_SpawnPlayer (mapthing_t* mthing); 
 
boolean
G_CheckSpot
( int		playernum,
  mapthing_t*	mthing ) 
{ 
    fixed_t		x;
    fixed_t		y; 
    subsector_t*	ss; 
    mobj_t*		mo; 
    int			i;
	
    if (!players[playernum].mo)
    {
	// first spawn of level, before corpses
	for (i=0 ; i<playernum ; i++)
	    if (players[i].mo->xy.x == mthing->x << FRACBITS
		&& players[i].mo->xy.y == mthing->y << FRACBITS)
		return false;	
	return true;
    }
		
    x = mthing->x << FRACBITS; 
    y = mthing->y << FRACBITS; 
	 
    if (!P_CheckPosition (players[playernum].mo, x, y) ) 
	return false; 
 
    // flush an old corpse if needed 
    if (bodyqueslot >= BODYQUESIZE) 
	P_RemoveMobj (shortptr_to_mobj(bodyque[bodyqueslot%BODYQUESIZE]));
    bodyque[bodyqueslot%BODYQUESIZE] = mobj_to_shortptr(players[playernum].mo);
    bodyqueslot++; 

    // spawn a teleport fog
    ss = R_PointInSubsector (x,y);


    // The code in the released source looks like this:
    //
    //    an = ( ANG45 * (((unsigned int) mthing->angle)/45) )
    //         >> ANGLETOFINESHIFT;
    //    mo = P_SpawnMobj (x+20*finecosine(an), y+20*finesine(an)
    //                     , ss->sector->floorheight
    //                     , MT_TFOG);
    //
    // But 'an' can be a signed value in the DOS version. This means that
    // we get a negative index and the lookups into finecosine/finesine
    // end up dereferencing values in finetangent().
    // A player spawning on a deathmatch start facing directly west spawns
    // "silently" with no spawn fog. Emulate this.
    //
    // This code is imported from PrBoom+.

    {
        fixed_t xa, ya;
        signed int an;

        // This calculation overflows in Vanilla Doom, but here we deliberately
        // avoid integer overflow as it is undefined behavior, so the value of
        // 'an' will always be positive.
        an = (ANG45 >> ANGLETOFINESHIFT) * ((signed int) mthing->angle / 45);

        switch (an)
        {
            case 4096:  // -4096:
                xa = finetangent(2048);    // finecosine(-4096)
                ya = finetangent(0);       // finesine(-4096)
                break;
            case 5120:  // -3072:
                xa = finetangent(3072);    // finecosine(-3072)
                ya = finetangent(1024);    // finesine(-3072)
                break;
            case 6144:  // -2048:
                xa = finesine(0);          // finecosine(-2048)
                ya = finetangent(2048);    // finesine(-2048)
                break;
            case 7168:  // -1024:
                xa = finesine(1024);       // finecosine(-1024)
                ya = finetangent(3072);    // finesine(-1024)
                break;
            case 0:
            case 1024:
            case 2048:
            case 3072:
                xa = finecosine(an);
                ya = finesine(an);
                break;
            default:
                I_Error("G_CheckSpot: unexpected angle %d\n", an);
                xa = ya = 0;
                break;
        }
        mo = P_SpawnMobj(x + 20 * xa, y + 20 * ya,
                         sector_floorheight(subsector_sector(ss)), MT_TFOG);
    }

    if (players[consoleplayer].viewz != 1) 
	S_StartObjSound (mo, sfx_telept);	// don't start sound on first frame
 
    return true; 
} 


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//
void G_DeathMatchSpawnPlayer (int playernum) 
{ 
    int             i,j; 
    int				selections; 
	 
    selections = deathmatch_p - deathmatchstarts; 
    if (selections < 4) 
	I_Error ("Only %i deathmatch spots, 4 required", selections); 
 
    for (j=0 ; j<20 ; j++) 
    { 
	i = P_Random() % selections; 
	if (G_CheckSpot (playernum, &deathmatchstarts[i]) ) 
	{ 
	    deathmatchstarts[i].type = playernum+1; 
	    P_SpawnPlayer (&deathmatchstarts[i]); 
	    return; 
	} 
    } 
 
    // no good spot, so the player will probably get stuck 
    P_SpawnPlayer (&playerstarts[playernum]); 
} 

//
// G_DoReborn 
// 
void G_DoReborn (int playernum) 
{ 
    int                             i; 
	 
    if (!netgame)
    {
	// reload the level from scratch
	gameaction = ga_loadlevel;  
    }
    else 
    {
	// respawn at the start

	// first dissasociate the corpse 
	mobj_full(players[playernum].mo)->sp_player =0;
		 
	// spawn at random spot if in death match 
	if (deathmatch) 
	{ 
	    G_DeathMatchSpawnPlayer (playernum); 
	    return; 
	} 
		 
	if (G_CheckSpot (playernum, &playerstarts[playernum]) ) 
	{ 
	    P_SpawnPlayer (&playerstarts[playernum]); 
	    return; 
	}
	
	// try to spawn at one of the other players spots 
	for (i=0 ; i<MAXPLAYERS ; i++)
	{
	    if (G_CheckSpot (playernum, &playerstarts[i]) ) 
	    { 
		playerstarts[i].type = playernum+1;	// fake as other player 
		P_SpawnPlayer (&playerstarts[i]); 
		playerstarts[i].type = i+1;		// restore 
		return; 
	    }	    
	    // he's going to be inside something.  Too bad.
	}
	P_SpawnPlayer (&playerstarts[playernum]); 
    } 
} 
 

#if !NO_SCREENSHOT
void G_ScreenShot (void) 
{ 
    gameaction = ga_screenshot; 
}
#endif
 


// DOOM Par Times
static const isb_int16_t pars[4][10] =
{ 
    {0}, 
    {0,30,75,120,90,165,180,180,30,165}, 
    {0,90,90,90,120,90,360,240,30,170}, 
    {0,90,45,90,150,90,90,165,30,135} 
}; 

// DOOM II Par Times
static const isb_int16_t cpars[32] =
{
    30,90,120,120,90,150,120,120,270,90,	//  1-10
    210,150,150,150,210,150,420,150,210,150,	// 11-20
    240,150,180,150,150,300,330,420,300,180,	// 21-30
    120,30					// 31-32
};
 

//
// G_DoCompleted 
//
boolean		secretexit; 

void G_ExitLevel (void) 
{ 
    secretexit = false; 
    gameaction = ga_completed; 
} 

// Here's for the german edition.
void G_SecretExitLevel (void) 
{ 
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ( (gamemode == commercial)
      && (W_CheckNumForName("map31")<0))
	secretexit = false;
    else
	secretexit = true; 
    gameaction = ga_completed; 
} 
 
void G_DoCompleted (void) 
{ 
    int             i; 
	 
    gameaction = ga_nothing; 
 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	if (playeringame[i]) 
	    G_PlayerFinishLevel (i);        // take away cards and stuff 
	 
    if (automapactive) 
	AM_Stop (); 
	
    if (gamemode != commercial)
    {
        // Chex Quest ends after 5 levels, rather than 8.

        if (gameversion_is_chex(gameversion))
        {
            if (gamemap == 5)
            {
                gameaction = ga_victory;
                return;
            }
        }
        else
        {
            switch(gamemap)
            {
#if !HACK_FINALE_E1M1
              case 8:
#else
              case 1:
#if !HACK_FINALE_SHAREWARE
                  gameepisode = 3; // for ultimate
#endif
#endif
                gameaction = ga_victory;
                return;
              case 9: 
                for (i=0 ; i<MAXPLAYERS ; i++) 
                    players[i].didsecret = true; 
                break;
            }
        }
    }

//#if 0  Hmmm - why?
    if ( (gamemap == 8)
	 && (gamemode != commercial) ) 
    {
	// victory 
	gameaction = ga_victory; 
	return; 
    } 
	 
    if ( (gamemap == 9)
	 && (gamemode != commercial) ) 
    {
	// exit secret level 
	for (i=0 ; i<MAXPLAYERS ; i++) 
	    players[i].didsecret = true; 
    } 
//#endif
    
	 
    wminfo.didsecret = players[consoleplayer].didsecret; 
    wminfo.epsd = gameepisode -1; 
    wminfo.last = gamemap -1;
    
    // wminfo.next is 0 biased, unlike gamemap
    if ( gamemode == commercial)
    {
	if (secretexit)
	    switch(gamemap)
	    {
	      case 15: wminfo.next = 30; break;
	      case 31: wminfo.next = 31; break;
	    }
	else
	    switch(gamemap)
	    {
	      case 31:
	      case 32: wminfo.next = 15; break;
	      default: wminfo.next = gamemap;
	    }
    }
    else
    {
	if (secretexit) 
	    wminfo.next = 8; 	// go to secret level 
	else if (gamemap == 9) 
	{
	    // returning from secret level 
	    switch (gameepisode) 
	    { 
	      case 1: 
		wminfo.next = 3; 
		break; 
	      case 2: 
		wminfo.next = 5; 
		break; 
	      case 3: 
		wminfo.next = 6; 
		break; 
	      case 4:
		wminfo.next = 2;
		break;
	    }                
	} 
	else 
	    wminfo.next = gamemap;          // go to next level 
    }
		 
    wminfo.maxkills = totalkills; 
    wminfo.maxitems = totalitems; 
    wminfo.maxsecret = totalsecret; 
    wminfo.maxfrags = 0; 

    // Set par time. Exceptions are added for purposes of
    // statcheck regression testing.
    if (gamemode == commercial)
    {
        // map33 has no official time: initialize to zero
        if (gamemap == 33)
        {
            wminfo.partime = 0;
        }
        else
        {
            wminfo.partime = TICRATE*cpars[gamemap-1];
        }
    }
    // Doom episode 4 doesn't have a par time, so this
    // overflows into the cpars array.
    else if (gameepisode < 4)
    {
        wminfo.partime = TICRATE*pars[gameepisode][gamemap];
    }
    else
    {
        wminfo.partime = TICRATE*cpars[gamemap];
    }

    wminfo.pnum = consoleplayer; 
 
    for (i=0 ; i<MAXPLAYERS ; i++) 
    { 
	wminfo.plyr[i].in = playeringame[i]; 
	wminfo.plyr[i].skills = players[i].killcount; 
	wminfo.plyr[i].sitems = players[i].itemcount; 
	wminfo.plyr[i].ssecret = players[i].secretcount; 
	wminfo.plyr[i].stime = leveltime; 
	memcpy (wminfo.plyr[i].frags, players[i].frags 
		, sizeof(wminfo.plyr[i].frags)); 
    } 
 
    gamestate = GS_INTERMISSION; 
    viewactive = false; 
    automapactive = false; 

    StatCopy(&wminfo);

#if !NO_USE_WI
    WI_Start (&wminfo);
#endif
} 


//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
    gameaction = ga_worlddone; 

    if (secretexit) 
	players[consoleplayer].didsecret = true;

    if ( gamemode == commercial )
    {
	switch (gamemap)
	{
	  case 15:
	  case 31:
	    if (!secretexit)
		break;
#if HACK_FINALE_E1M1
        case 1:
            gamemap=30;
            // fall thru
#endif
	  case 6:
	  case 11:
	  case 20:
	  case 30:
	    F_StartFinale ();
	    break;
	}
    }
} 
 
void G_DoWorldDone (void) 
{        
    gamestate = GS_LEVEL; 
    gamemap = wminfo.next+1; 
    G_DoLoadLevel (); 
    gameaction = ga_nothing; 
    viewactive = true; 
} 
 


//
// G_InitFromSavegame
// Can be called by the startup code or the menu task. 
//
extern boolean setsizeneeded;
void R_ExecuteSetViewSize (void);

char	savename[256];

void G_LoadGame (char* name) 
{
#if !NO_FILE_ACCESS
    M_StringCopy(savename, name, sizeof(savename));
#endif
    gameaction = ga_loadgame;
}

void G_DoLoadGame (void) {
#if !NO_USE_LOAD
    int savedleveltime;

    gameaction = ga_nothing;

#if !NO_FILE_ACCESS
    save_stream = fopen(savename, "rb");

    if (save_stream == NULL) {
        I_Error("Could not load savegame %s", savename);
    }
#endif
#if LOAD_COMPRESSED
    th_bit_input bi;
    uint32_t size;
#if !NO_FILE_ACCESS
    uint8_t *load_buffer = pd_get_work_area(&size);
    fseek(save_stream, 0, SEEK_END);
    uint fsize = ftell(save_stream);
    if (fsize > size) {
        return;
    }
    fseek(save_stream, 0, SEEK_SET);
    fread(load_buffer, 1, fsize, save_stream);
#else
    flash_slot_info_t slots[g_load_slot+1];
    P_SaveGameGetExistingFlashSlotAddresses(slots, g_load_slot+1);
    if (!slots[g_load_slot].data) return;
    const uint8_t *load_buffer = slots[g_load_slot].data;
#endif
    sg_bi = &bi;
    th_bit_input_init(sg_bi, load_buffer);
#endif

    savegame_error = false;

    if (!P_ReadSaveGameHeader())
    {
#if !NO_FILE_ACCESS
        fclose(save_stream);
#endif
#if DOOM_TINY
        players[consoleplayer].message = "Load failed: invalid game or WAD mismatch.";
#endif
        return;
    }

    savedleveltime = leveltime;
    
    // load a base level 
    G_InitNew (gameskill, gameepisode, gamemap); 
 
    leveltime = savedleveltime;

    // dearchive all the modifications
    xprintf("PL AT %08x\n", (sg_bi->cur - load_buffer) * 8 - sg_bi->bits);
    P_UnArchivePlayers ();
    xprintf("WO AT %08x\n", (sg_bi->cur - load_buffer) * 8 - sg_bi->bits);
    P_UnArchiveWorld ();
    xprintf("TH %08x\n", (sg_bi->cur - load_buffer) * 8 - sg_bi->bits);
    P_UnArchiveThinkers ();
    xprintf("SP %08x\n", (sg_bi->cur - load_buffer) * 8 - sg_bi->bits);
    P_UnArchiveSpecials ();
 
    if (!P_ReadSaveGameEOF())
	I_Error ("Bad savegame");

#if !NO_FILE_ACCESS
    fclose(save_stream);
#endif

    if (setsizeneeded)
	R_ExecuteSetViewSize ();

#if !NO_RDRAW
    // draw the pattern into the back screen
    R_FillBackScreen ();
#endif
#endif
}
 

#if PICO_ON_DEVICE
static boolean save_game_clear(int key) {
    if (key == key_menu_confirm) {
        uint32_t size;
        uint8_t *tmp_buffer = pd_get_work_area(&size);
        P_SaveGameWriteFlashSlot(savegameslot, NULL, 0, tmp_buffer);
        M_SaveGame(0);
        return true;
    }
    return false;
}
#endif
//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string 
//
void
G_SaveGame
( int	slot,
  char*	description )
{
    savegameslot = slot;
    M_StringCopy(savedescription, description, sizeof(savedescription));
    sendsave = true;
}

void G_DoSaveGame (void) 
{
#if !NO_USE_SAVE
#if !NO_FILE_ACCESS
    char *savegame_file;
    char *temp_savegame_file;
    char *recovery_savegame_file;

    recovery_savegame_file = NULL;
    temp_savegame_file = P_TempSaveGameFile();
    savegame_file = P_SaveGameFile(savegameslot);

    // Open the savegame file for writing.  We write to a temporary file
    // and then rename it at the end if it was successfully written.
    // This prevents an existing savegame from being overwritten by
    // a corrupted one, or if a savegame buffer overrun occurs.
    save_stream = fopen(temp_savegame_file, "wb");

    if (save_stream == NULL)
    {
        // Failed to save the game, so we're going to have to abort. But
        // to be nice, save to somewhere else before we call I_Error().
        recovery_savegame_file = M_TempFile("recovery.dsg");
        save_stream = fopen(recovery_savegame_file, "wb");
        if (save_stream == NULL)
        {
            I_Error("Failed to open either '%s' or '%s' to write savegame.",
                    temp_savegame_file, recovery_savegame_file);
        }
    }
#endif
#if SAVE_COMPRESSED
    th_bit_output bo;
    uint32_t size;
    uint8_t *save_buffer = pd_get_work_area(&size);
    save_buffer += 4096; size -= 4096; // we need 4k for flash writing
    sg_bo = &bo;
    th_bit_output_init(sg_bo, save_buffer, size);
#endif

    savegame_error = false;

    P_WriteSaveGameHeader(savedescription);

    xprintf("PL AT %08x\n", (sg_bo->cur - save_buffer) * 8 + sg_bo->bits);
    P_ArchivePlayers ();
    xprintf("WO AT %08x\n", (sg_bo->cur - save_buffer) * 8 + sg_bo->bits);
    P_ArchiveWorld ();
    xprintf("TH AT %08x\n", (sg_bo->cur - save_buffer) * 8 + sg_bo->bits);
    P_ArchiveThinkers ();
    xprintf("SP AT %08x\n", (sg_bo->cur - save_buffer) * 8 + sg_bo->bits);
    P_ArchiveSpecials ();

    P_WriteSaveGameEOF();

    boolean resume = true;
#if SAVE_COMPRESSED
    if (bo.bits) th_write_bits(&bo, 0, 8-bo.bits);
#if !NO_FILE_ACCESS
    fwrite(save_buffer, 1, bo.cur - save_buffer, save_stream);
#endif
    printf("SAVE GAME SIZE %d\n", (int)(bo.cur - save_buffer));
#if PICO_ON_DEVICE
    if (!P_SaveGameWriteFlashSlot(savegameslot, save_buffer, (int)(bo.cur - save_buffer), save_buffer - 4096)) {
        M_StartMessage("There was not enough space to save the game.\nWould you like to clear this slot and\n try saving again in a different slot?\n\npress y or n.",save_game_clear,true);
        resume = false;
    }
#endif
#endif
    // Enforce the same savegame size limit as in Vanilla Doom,
    // except if the vanilla_savegame_limit setting is turned off.

#if !NO_FILE_ACCESS
    if (vanilla_savegame_limit && ftell(save_stream) > SAVEGAMESIZE)
    {

        I_Error("Savegame buffer overrun");
    }
#endif

    // Finish up, close the savegame file.

#if !NO_FILE_ACCESS
    fclose(save_stream);
#endif

#if !DOOM_TINY
    if (recovery_savegame_file != NULL)
    {
        // We failed to save to the normal location, but we wrote a
        // recovery file to the temp directory. Now we can bomb out
        // with an error.
        I_Error("Failed to open savegame file '%s' for writing.\n"
                "But your game has been saved to '%s' for recovery.",
                temp_savegame_file, recovery_savegame_file);
    }
#endif

#if !NO_FILE_ACCESS
    // Now rename the temporary savegame file to the actual savegame
    // file, overwriting the old savegame if there was one there.

    remove(savegame_file);
    rename(temp_savegame_file, savegame_file);
#endif

    M_StringCopy(savedescription, "", sizeof(savedescription));
    gameaction = ga_nothing;
    if (resume) {
        players[consoleplayer].message = DEH_String(GGSAVED);
    }

#if !NO_RDRAW
    // draw the pattern into the back screen
    R_FillBackScreen ();
#endif
#endif
}
 

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set. 
//
skill_t	d_skill; 
static isb_int8_t     d_episode;
static isb_int8_t     d_map;
 
void
G_DeferedInitNew
( skill_t	skill,
  int		episode,
  int		map,
  boolean   net)
{ 
    d_skill = skill;
    d_episode = (isb_int8_t)episode;
    d_map = (isb_int8_t)map;
#if USE_PICO_NET
    netgame = net;
    gameaction = net ? ga_newgamenet : ga_newgame;
#else
    gameaction = ga_newgame;
#endif
}


void G_DoNewGame(boolean net)
{
    demoplayback = false;
    netdemo = false;
    if (!net) {
#if !NO_USE_NET || USE_PICO_NET // var doesn't exist otherwise
        netgame = false;
#endif
        deathmatch = false;
        playeringame[0] = 1;
        playeringame[1] = playeringame[2] = playeringame[3] = 0;
        consoleplayer = 0;
    }
    respawnparm = false;
    fastparm = false;
    nomonsters = false;
    G_InitNew (d_skill, d_episode, d_map);
    gameaction = ga_nothing; 
} 


void
G_InitNew
( skill_t	skill,
  int		episode,
  int		map )
{
    texturename_t skytexturename;
    int             i;

    if (paused)
    {
	paused = false;
	S_ResumeSound ();
    }

    /*
    // Note: This commented-out block of code was added at some point
    // between the DOS version(s) and the Doom source release. It isn't
    // found in disassemblies of the DOS version and causes IDCLEV and
    // the -warp command line parameter to behave differently.
    // This is left here for posterity.

    // This was quite messy with SPECIAL and commented parts.
    // Supposedly hacks to make the latest edition work.
    // It might not work properly.
    if (episode < 1)
      episode = 1;

    if ( gamemode == retail )
    {
      if (episode > 4)
	episode = 4;
    }
    else if ( gamemode == shareware )
    {
      if (episode > 1)
	   episode = 1;	// only start episode 1 on shareware
    }
    else
    {
      if (episode > 3)
	episode = 3;
    }
    */

    if (skill > sk_nightmare)
	skill = sk_nightmare;

    if (gameversion >= exe_ultimate)
    {
        if (episode == 0)
        {
            episode = 4;
        }
    }
    else
    {
        if (episode < 1)
        {
            episode = 1;
        }
        if (episode > 3)
        {
            episode = 3;
        }
    }

    if (episode > 1 && gamemode == shareware)
    {
        episode = 1;
    }

    if (map < 1)
	map = 1;

    if ( (map > 9)
	 && ( gamemode != commercial) )
      map = 9;

    M_ClearRandom ();

    if (skill == sk_nightmare || respawnparm )
	respawnmonsters = true;
    else
	respawnmonsters = false;

#if !DOOM_CONST
    if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare) )
    {
	for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
	    states[i].tics >>= 1;
	mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT;
	mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT;
	mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT;
    }
    else if (skill != sk_nightmare && gameskill == sk_nightmare)
    {
	for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
	    states[i].tics <<= 1;
	mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT;
	mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT;
	mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT;
    }
#else
    nightmare_speeds = fastparm || skill == sk_nightmare;
#endif

    // force players to be initialized upon first level load
    for (i=0 ; i<MAXPLAYERS ; i++)
	players[i].playerstate = PST_REBORN;

    usergame = true;                // will be set false if a demo
    paused = false;
    demoplayback = false;
    automapactive = false;
    viewactive = true;
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;

    viewactive = true;

    // Set the sky to use.
    //
    // Note: This IS broken, but it is how Vanilla Doom behaves.
    // See http://doomwiki.org/wiki/Sky_never_changes_in_Doom_II.
    //
    // Because we set the sky here at the start of a game, not at the
    // start of a level, the sky texture never changes unless we
    // restore from a saved game.  This was fixed before the Doom
    // source release, but this IS the way Vanilla DOS Doom behaves.

    if (gamemode == commercial)
    {
        if (gamemap < 12)
            skytexturename = TEXTURE_NAME(SKY1);
        else if (gamemap < 21)
            skytexturename = TEXTURE_NAME(SKY2);
        else
            skytexturename = TEXTURE_NAME(SKY3);
    }
    else
    {
        switch (gameepisode)
        {
          default:
          case 1:
            skytexturename = TEXTURE_NAME(SKY1);
            break;
          case 2:
            skytexturename = TEXTURE_NAME(SKY2);
            break;
          case 3:
            skytexturename = TEXTURE_NAME(SKY3);
            break;
          case 4:        // Special Edition sky
            skytexturename = TEXTURE_NAME(SKY4);
            break;
        }
    }

    skytexturename = DEH_TextureName(skytexturename);

    skytexture = R_TextureNumForName(skytexturename);

    G_DoLoadLevel ();
}


//
// DEMO RECORDING 
// 
#define DEMOMARKER		0x80


void G_ReadDemoTiccmd (ticcmd_t* cmd) 
{
#if !USE_WHD
    if (*demo_p == DEMOMARKER) 
    {
	// end of demo data stream 
	G_CheckDemoStatus (); 
	return; 
    } 
    cmd->forwardmove = ((signed char)*demo_p++); 
    cmd->sidemove = ((signed char)*demo_p++); 

    // If this is a longtics demo, read back in higher resolution

    if (longtics)
    {
        cmd->angleturn = *demo_p++;
        cmd->angleturn |= (*demo_p++) << 8;
    }
    else
    {
        cmd->angleturn = ((unsigned char) *demo_p++)<<8; 
    }

    cmd->buttons = (unsigned char)*demo_p++;
#else
    uint changes = th_decode(demo_decode.decoders + demo_decode.changes_offset, &demo_decode.bi);
    assert(changes <= 16);
    if (changes == 16) {
        // end of demo data stream
        G_CheckDemoStatus();
        return;
    }
    if (changes & 1) {
        demo_decode.last_fb += from_zig(th_decode(demo_decode.decoders + demo_decode.fb_delta_offset, &demo_decode.bi));
    }
    cmd->forwardmove = demo_decode.last_fb;
    if (changes & 2) {
        cmd->sidemove = from_zig(th_decode(demo_decode.decoders + demo_decode.strafe_offset, &demo_decode.bi));
    } else {
        cmd->sidemove = 0;
    }
    assert(!longtics);
    if (changes & 4) {
        demo_decode.last_turn += from_zig(th_decode(demo_decode.decoders + demo_decode.turn_delta_offset, &demo_decode.bi));
    }
    cmd->angleturn = demo_decode.last_turn << 8;
    if (changes & 8) {
        cmd->buttons = th_decode(demo_decode.decoders + demo_decode.buttons_offset, &demo_decode.bi);
    } else {
        cmd->buttons = 0;
    }
#endif
} 

// Increase the size of the demo buffer to allow unlimited demos

#if !NO_DEMO_RECORDING
static void IncreaseDemoBuffer(void)
{
    int current_length;
    byte *new_demobuffer;
    byte *new_demop;
    int new_length;

    // Find the current size

    current_length = demoend - demobuffer;
    
    // Generate a new buffer twice the size
    new_length = current_length * 2;
    
    new_demobuffer = Z_Malloc(new_length, PU_STATIC, 0);
    new_demop = new_demobuffer + (demo_p - demobuffer);

    // Copy over the old data

    memcpy(new_demobuffer, demobuffer, current_length);

    // Free the old buffer and point the demo pointers at the new buffer.

    Z_Free(demobuffer);

    demobuffer = new_demobuffer;
    demo_p = new_demop;
    demoend = demobuffer + new_length;
}

void G_WriteDemoTiccmd (ticcmd_t* cmd) 
{
    byte *demo_start;

    if (is_gamekeydown(key_demo_quit))           // press q to end demo recording
	G_CheckDemoStatus (); 

    demo_start = demo_p;

    *demo_p++ = cmd->forwardmove; 
    *demo_p++ = cmd->sidemove; 

    // If this is a longtics demo, record in higher resolution
 
    if (longtics)
    {
        *demo_p++ = (cmd->angleturn & 0xff);
        *demo_p++ = (cmd->angleturn >> 8) & 0xff;
    }
    else
    {
        *demo_p++ = cmd->angleturn >> 8; 
    }

    *demo_p++ = cmd->buttons; 

    // reset demo pointer back
    demo_p = demo_start;

    if (demo_p > demoend - 16)
    {
        if (vanilla_demo_limit)
        {
            // no more space 
            G_CheckDemoStatus (); 
            return; 
        }
        else
        {
            // Vanilla demo limit disabled: unlimited
            // demo lengths!

            IncreaseDemoBuffer();
        }
    } 
	
    G_ReadDemoTiccmd (cmd);         // make SURE it is exactly the same 
}


//
// G_RecordDemo
//
void G_RecordDemo (char *name)
{
    size_t demoname_size;
    int i;
    int maxsize;

    usergame = false;
    demoname_size = strlen(name) + 5;
    demoname = Z_Malloc(demoname_size, PU_STATIC, 0);
    M_snprintf(demoname, demoname_size, "%s.lmp", name);
    maxsize = 0x20000;

    //!
    // @arg <size>
    // @category demo
    // @vanilla
    //
    // Specify the demo buffer size (KiB)
    //

#if !NO_USE_ARGS
    i = M_CheckParmWithArgs("-maxdemo", 1);
    if (i)
	maxsize = atoi(myargv[i+1])*1024;
#endif
    demobuffer = Z_Malloc (maxsize,PU_STATIC, 0);
    demoend = demobuffer + maxsize;
	
    demorecording = true; 
}
#endif

// Get the demo version code appropriate for the version set in gameversion.
int G_VanillaVersionCode(void)
{
    switch (gameversion)
    {
        case exe_doom_1_2:
            I_Error("Doom 1.2 does not have a version code!");
        case exe_doom_1_666:
            return 106;
        case exe_doom_1_7:
            return 107;
        case exe_doom_1_8:
            return 108;
        case exe_doom_1_9:
        default:  // All other versions are variants on v1.9:
            return 109;
    }
}

#if !NO_DEMO_RECORDING
void G_BeginRecording (void) 
{ 
    int             i; 

    demo_p = demobuffer;

    //!
    // @category demo
    //
    // Record a high resolution "Doom 1.91" demo.
    //

    longtics = D_NonVanillaRecord(M_ParmExists("-longtics"),
                                  "Doom 1.91 demo format");

    // If not recording a longtics demo, record in low res
    lowres_turn = !longtics;

    if (longtics)
    {
        *demo_p++ = DOOM_191_VERSION;
    }
    else
    {
        *demo_p++ = G_VanillaVersionCode();
    }

    *demo_p++ = gameskill; 
    *demo_p++ = gameepisode; 
    *demo_p++ = gamemap; 
    *demo_p++ = deathmatch; 
    *demo_p++ = respawnparm;
    *demo_p++ = fastparm;
    *demo_p++ = nomonsters;
    *demo_p++ = consoleplayer;
	 
    for (i=0 ; i<MAXPLAYERS ; i++) 
	*demo_p++ = playeringame[i]; 		 
} 
#endif

//
// G_PlayDemo 
//

static const char *defdemoname;
 
void G_DeferedPlayDemo(const char *name)
{ 
    defdemoname = name; 
    gameaction = ga_playdemo; 
} 

// Generate a string describing a demo version

static const char *DemoVersionDescription(int version)
{
    static char resultbuf[16];

    switch (version)
    {
        case 104:
            return "v1.4";
        case 105:
            return "v1.5";
        case 106:
            return "v1.6/v1.666";
        case 107:
            return "v1.7/v1.7a";
        case 108:
            return "v1.8";
        case 109:
            return "v1.9";
        case 111:
            return "v1.91 hack demo?";
        default:
            break;
    }

    // Unknown version.  Perhaps this is a pre-v1.4 IWAD?  If the version
    // byte is in the range 0-4 then it can be a v1.0-v1.2 demo.

    if (version >= 0 && version <= 4)
    {
        return "v1.0/v1.1/v1.2";
    }
    else
    {
        M_snprintf(resultbuf, sizeof(resultbuf),
                   "%i.%i (unknown)", version / 100, version % 100);
        return resultbuf;
    }
}

void G_DoPlayDemo (void)
{
    skill_t skill;
    int i, lumpnum, episode, map;
    int demoversion;

    lumpnum = W_GetNumForName(defdemoname);
    gameaction = ga_nothing;
    should_be_const byte *demobuffer = W_CacheLumpNum(lumpnum, PU_STATIC);
#if USE_WHD
    const byte* demo_p;
#endif
    demo_p = (byte *)demobuffer;

    demoversion = *demo_p++;

    longtics = false;

    // Longtics demos use the modified format that is generated by cph's
    // hacked "v1.91" doom exe. This is a non-vanilla extension.
    if (D_NonVanillaPlayback(demoversion == DOOM_191_VERSION, lumpnum,
                             "Doom 1.91 demo format"))
    {
        longtics = true;
    }
    else if (demoversion != G_VanillaVersionCode())
    {
        const char *message = "Demo is from a different game version!\n"
                              "(read %i, should be %i)\n"
                              "\n"
                              "*** You may need to upgrade your version "
                                  "of Doom to v1.9. ***\n"
                              "    See: https://www.doomworld.com/classicdoom"
                                        "/info/patches.php\n"
                              "    This appears to be %s.";

        I_Error(message, demoversion, G_VanillaVersionCode(),
                         DemoVersionDescription(demoversion));
    }

    skill = *demo_p++; 
    episode = *demo_p++; 
    map = *demo_p++; 
    deathmatch = *demo_p++;
    respawnparm = *demo_p++;
    fastparm = *demo_p++;
    nomonsters = *demo_p++;
    consoleplayer = *demo_p++;

    for (i=0 ; i<MAXPLAYERS ; i++)
	playeringame[i] = *demo_p++;

#if !NO_USE_NET
    if (playeringame[1] || M_CheckParm("-solo-net") > 0
                        || M_CheckParm("-netdemo") > 0)
    {
	netgame = true;
	netdemo = true;
    }
#endif

    // don't spend a lot of time in loadlevel
    precache = false;
    G_InitNew (skill, episode, map); 
#if USE_WHD
    uint8_t decode_words = *demo_p++;
    memset(&demo_decode, 0, sizeof(demo_decode));
    th_bit_input_init(&demo_decode.bi, demo_p);
    demo_decode.decoders = Z_Malloc(decode_words * 2, PU_LEVEL, 0);
    int idx=0;
    uint8_t tmp_buffer[512];
#define READ_DECODER(name, func) demo_decode.name##_offset = idx, idx = func(&demo_decode.bi, demo_decode.decoders + idx, decode_words - idx, tmp_buffer, sizeof(tmp_buffer)) - demo_decode.decoders
    READ_DECODER(changes, th_read_simple_decoder);
    READ_DECODER(fb_delta, th_read_simple_decoder);
    READ_DECODER(strafe, th_read_simple_decoder);
    READ_DECODER(turn_delta, th_read_simple_decoder);
    READ_DECODER(buttons, th_read_simple_decoder);
    assert(decode_words == idx);
#endif
    precache = true;
    starttime = I_GetTime (); 

    usergame = false; 
    demoplayback = true; 
} 

//
// G_TimeDemo 
//
void G_TimeDemo (char* name) 
{
    //!
    // @category video
    // @vanilla
    //
    // Disable rendering the screen entirely.
    //

#if !FORCE_NODRAW
    nodrawers = M_CheckParm ("-nodraw");
#endif

    timingdemo = true; 
    singletics = true; 

    defdemoname = name; 
    gameaction = ga_playdemo; 
} 
 
 
/* 
=================== 
= 
= G_CheckDemoStatus
= 
= Called after a death or level completion to allow demos to be cleaned up 
= Returns true if a new demo loop action will take place 
=================== 
*/ 
 
boolean G_CheckDemoStatus (void) 
{ 

    if (timingdemo)
    { 
        // Prevent recursive calls
        timingdemo = false;
        demoplayback = false;

#if !NO_USE_FLOAT
        int endtime = I_GetTime ();
        int realtics = endtime - starttime;
        float fps = ((float) gametic * TICRATE) / realtics;
	I_Error ("timed %i gametics in %i realtics (%f fps)",
                 gametic, realtics, fps);
#endif
    }

    if (demoplayback)
    { 
        W_ReleaseLumpName(defdemoname);
	demoplayback = false; 
	netdemo = false;
#if !NO_USE_NET
	netgame = false;
#endif
	deathmatch = false;
	playeringame[1] = playeringame[2] = playeringame[3] = 0;
	respawnparm = false;
	fastparm = false;
	nomonsters = false;
	consoleplayer = 0;
        
        if (singledemo) 
            I_Quit (); 
        else 
            D_AdvanceDemo (); 

	return true; 
    }

#if !NO_DEMO_RECORDING
    if (demorecording) 
    { 
	*demo_p++ = DEMOMARKER; 
	M_WriteFile (demoname, demobuffer, demo_p - demobuffer); 
	Z_Free (demobuffer); 
	demorecording = false; 
	I_Error ("Demo %s recorded",demoname); 
    }
#endif
	 
    return false; 
} 
 
 
 
