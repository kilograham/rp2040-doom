//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 1993-2008 Raven Software
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

#include <stdio.h>

#include "doomtype.h"
#include "doomkeys.h"

#include "m_config.h"
#include "m_misc.h"
#include "m_controls.h"

//
// Keyboard controls
//

key_type_t key_right = KEY_RIGHTARROW;
key_type_t key_left = KEY_LEFTARROW;

key_type_t key_up = KEY_UPARROW;
key_type_t key_down = KEY_DOWNARROW;
key_type_t key_strafeleft = ',';
key_type_t key_straferight = '.';
key_type_t key_fire = KEY_RCTRL;
key_type_t key_use = ' ';
key_type_t key_strafe = KEY_RALT;
key_type_t key_speed = KEY_RSHIFT;

// 
// Heretic keyboard controls
//
 
key_type_t key_flyup = KEY_PGUP;
key_type_t key_flydown = KEY_INS;
key_type_t key_flycenter = KEY_HOME;

key_type_t key_lookup = KEY_PGDN;
key_type_t key_lookdown = KEY_DEL;
key_type_t key_lookcenter = KEY_END;

key_type_t key_invleft = '[';
key_type_t key_invright = ']';
key_type_t key_useartifact = KEY_ENTER;

//
// Hexen key controls
//

key_type_t key_jump = '/';

key_type_t key_arti_all             = KEY_BACKSPACE;
key_type_t key_arti_health          = '\\';
key_type_t key_arti_poisonbag       = '0';
key_type_t key_arti_blastradius     = '9';
key_type_t key_arti_teleport        = '8';
key_type_t key_arti_teleportother   = '7';
key_type_t key_arti_egg             = '6';
key_type_t key_arti_invulnerability = '5';

//
// Strife key controls
//
// haleyjd 09/01/10
//

// Note: Strife also uses key_invleft, key_invright, key_jump, key_lookup, and
// key_lookdown, but with different default values.

key_type_t key_usehealth = 'h';
key_type_t key_invquery  = 'q';
key_type_t key_mission   = 'w';
key_type_t key_invpop    = 'z';
key_type_t key_invkey    = 'k';
key_type_t key_invhome   = KEY_HOME;
key_type_t key_invend    = KEY_END;
key_type_t key_invuse    = KEY_ENTER;
key_type_t key_invdrop   = KEY_BACKSPACE;


//
// Mouse controls
//

mouseb_type_t mousebfire = 0;
mouseb_type_t mousebstrafe = 1;
mouseb_type_t mousebforward = 2;

mouseb_type_t mousebjump = -1;

mouseb_type_t mousebstrafeleft = -1;
mouseb_type_t mousebstraferight = -1;
mouseb_type_t mousebbackward = -1;
mouseb_type_t mousebuse = -1;

mouseb_type_t mousebprevweapon = -1;
mouseb_type_t mousebnextweapon = -1;

key_type_t key_message_refresh = KEY_ENTER;
key_type_t key_pause = KEY_PAUSE;
key_type_t key_demo_quit = 'q';
key_type_t key_spy = KEY_F12;

// Multiplayer chat keys:

key_type_t key_multi_msg = 't';
key_type_t key_multi_msgplayer[8];

// Weapon selection keys:

key_type_t key_weapon1 = '1';
key_type_t key_weapon2 = '2';
key_type_t key_weapon3 = '3';
key_type_t key_weapon4 = '4';
key_type_t key_weapon5 = '5';
key_type_t key_weapon6 = '6';
key_type_t key_weapon7 = '7';
key_type_t key_weapon8 = '8';
key_type_t key_prevweapon = 0;
key_type_t key_nextweapon = 0;

// Map control keys:

key_type_t key_map_north     = KEY_UPARROW;
key_type_t key_map_south     = KEY_DOWNARROW;
key_type_t key_map_east      = KEY_RIGHTARROW;
key_type_t key_map_west      = KEY_LEFTARROW;
key_type_t key_map_zoomin    = '=';
key_type_t key_map_zoomout   = '-';
key_type_t key_map_toggle    = KEY_TAB;
key_type_t key_map_maxzoom   = '0';
key_type_t key_map_follow    = 'f';
key_type_t key_map_grid      = 'g';
key_type_t key_map_mark      = 'm';
key_type_t key_map_clearmark = 'c';

// menu keys:

key_type_t key_menu_activate  = KEY_ESCAPE;
key_type_t key_menu_up        = KEY_UPARROW;
key_type_t key_menu_down      = KEY_DOWNARROW;
key_type_t key_menu_left      = KEY_LEFTARROW;
key_type_t key_menu_right     = KEY_RIGHTARROW;
key_type_t key_menu_back      = KEY_BACKSPACE;
key_type_t key_menu_forward   = KEY_ENTER;
key_type_t key_menu_confirm   = 'y';
key_type_t key_menu_abort     = 'n';

key_type_t key_menu_help      = KEY_F1;
key_type_t key_menu_save      = KEY_F2;
key_type_t key_menu_load      = KEY_F3;
key_type_t key_menu_volume    = KEY_F4;
key_type_t key_menu_detail    = KEY_F5;
key_type_t key_menu_qsave     = KEY_F6;
key_type_t key_menu_endgame   = KEY_F7;
key_type_t key_menu_messages  = KEY_F8;
key_type_t key_menu_qload     = KEY_F9;
key_type_t key_menu_quit      = KEY_F10;
key_type_t key_menu_gamma     = KEY_F11;

key_type_t key_menu_incscreen = KEY_EQUALS;
key_type_t key_menu_decscreen = KEY_MINUS;
key_type_t key_menu_screenshot = 0;

#if !NO_USE_JOYSTICK
//
// Joystick controls
//

int joybfire = 0;
int joybstrafe = 1;
int joybuse = 3;
int joybspeed = 2;

int joybstrafeleft = -1;
int joybstraferight = -1;

int joybjump = -1;

int joybprevweapon = -1;
int joybnextweapon = -1;

int joybmenu = -1;
int joybautomap = -1;
#endif

// Control whether if a mouse button is double clicked, it acts like 
// "use" has been pressed

isb_int8_t dclick_use = 1;
 
// 
// Bind all of the common controls used by Doom and all other games.
//

void M_BindBaseControls(void)
{
    M_BindKeyVariable("key_right",          &key_right);
    M_BindKeyVariable("key_left",           &key_left);
    M_BindKeyVariable("key_up",             &key_up);
    M_BindKeyVariable("key_down",           &key_down);
    M_BindKeyVariable("key_strafeleft",     &key_strafeleft);
    M_BindKeyVariable("key_straferight",    &key_straferight);
    M_BindKeyVariable("key_fire",           &key_fire);
    M_BindKeyVariable("key_use",            &key_use);
    M_BindKeyVariable("key_strafe",         &key_strafe);
    M_BindKeyVariable("key_speed",          &key_speed);

    M_BindMouseBVariable("mouseb_fire",        &mousebfire);
    M_BindMouseBVariable("mouseb_strafe",      &mousebstrafe);
    M_BindMouseBVariable("mouseb_forward",     &mousebforward);

#if !NO_USE_JOYSTICK
    M_BindIntVariable("joyb_fire",          &joybfire);
    M_BindIntVariable("joyb_strafe",        &joybstrafe);
    M_BindIntVariable("joyb_use",           &joybuse);
    M_BindIntVariable("joyb_speed",         &joybspeed);

    M_BindIntVariable("joyb_menu_activate", &joybmenu);
    M_BindIntVariable("joyb_toggle_automap", &joybautomap);

    // Extra controls that are not in the Vanilla versions:

    M_BindIntVariable("joyb_strafeleft",     &joybstrafeleft);
    M_BindIntVariable("joyb_straferight",    &joybstraferight);
#endif

    M_BindMouseBVariable("mouseb_strafeleft",   &mousebstrafeleft);
    M_BindMouseBVariable("mouseb_straferight",  &mousebstraferight);
    M_BindMouseBVariable("mouseb_use",          &mousebuse);
    M_BindMouseBVariable("mouseb_backward",     &mousebbackward);
    M_BindIntVariable("dclick_use",          &dclick_use);
    M_BindKeyVariable("key_pause",           &key_pause);
    M_BindKeyVariable("key_message_refresh", &key_message_refresh);
}

void M_BindHereticControls(void)
{
    M_BindKeyVariable("key_flyup",          &key_flyup);
    M_BindKeyVariable("key_flydown",        &key_flydown);
    M_BindKeyVariable("key_flycenter",      &key_flycenter);

    M_BindKeyVariable("key_lookup",         &key_lookup);
    M_BindKeyVariable("key_lookdown",       &key_lookdown);
    M_BindKeyVariable("key_lookcenter",     &key_lookcenter);

    M_BindKeyVariable("key_invleft",        &key_invleft);
    M_BindKeyVariable("key_invright",       &key_invright);
    M_BindKeyVariable("key_useartifact",    &key_useartifact);
}

void M_BindHexenControls(void)
{
    M_BindKeyVariable("key_jump",           &key_jump);
    M_BindMouseBVariable("mouseb_jump",        &mousebjump);
#if !NO_USE_JOYSTICK
    M_BindIntVariable("joyb_jump",          &joybjump);
#endif

    M_BindKeyVariable("key_arti_all",             &key_arti_all);
    M_BindKeyVariable("key_arti_health",          &key_arti_health);
    M_BindKeyVariable("key_arti_poisonbag",       &key_arti_poisonbag);
    M_BindKeyVariable("key_arti_blastradius",     &key_arti_blastradius);
    M_BindKeyVariable("key_arti_teleport",        &key_arti_teleport);
    M_BindKeyVariable("key_arti_teleportother",   &key_arti_teleportother);
    M_BindKeyVariable("key_arti_egg",             &key_arti_egg);
    M_BindKeyVariable("key_arti_invulnerability", &key_arti_invulnerability);
}

void M_BindStrifeControls(void)
{
    // These are shared with all games, but have different defaults:
    key_message_refresh = '/';

    // These keys are shared with Heretic/Hexen but have different defaults:
    key_jump     = 'a';
    key_lookup   = KEY_PGUP;
    key_lookdown = KEY_PGDN;
    key_invleft  = KEY_INS;
    key_invright = KEY_DEL;

    M_BindKeyVariable("key_jump",           &key_jump);
    M_BindKeyVariable("key_lookUp",         &key_lookup);
    M_BindKeyVariable("key_lookDown",       &key_lookdown);
    M_BindKeyVariable("key_invLeft",        &key_invleft);
    M_BindKeyVariable("key_invRight",       &key_invright);

    // Custom Strife-only Keys:
    M_BindKeyVariable("key_useHealth",      &key_usehealth);
    M_BindKeyVariable("key_invquery",       &key_invquery);
    M_BindKeyVariable("key_mission",        &key_mission);
    M_BindKeyVariable("key_invPop",         &key_invpop);
    M_BindKeyVariable("key_invKey",         &key_invkey);
    M_BindKeyVariable("key_invHome",        &key_invhome);
    M_BindKeyVariable("key_invEnd",         &key_invend);
    M_BindKeyVariable("key_invUse",         &key_invuse);
    M_BindKeyVariable("key_invDrop",        &key_invdrop);

    // Strife also supports jump on mouse and joystick, and in the exact same
    // manner as Hexen!
    M_BindMouseBVariable("mouseb_jump",        &mousebjump);
#if !NO_USE_JOYSTICK
    M_BindIntVariable("joyb_jump",          &joybjump);
#endif
}

void M_BindWeaponControls(void)
{
    M_BindKeyVariable("key_weapon1",        &key_weapon1);
    M_BindKeyVariable("key_weapon2",        &key_weapon2);
    M_BindKeyVariable("key_weapon3",        &key_weapon3);
    M_BindKeyVariable("key_weapon4",        &key_weapon4);
    M_BindKeyVariable("key_weapon5",        &key_weapon5);
    M_BindKeyVariable("key_weapon6",        &key_weapon6);
    M_BindKeyVariable("key_weapon7",        &key_weapon7);
    M_BindKeyVariable("key_weapon8",        &key_weapon8);

    M_BindKeyVariable("key_prevweapon",     &key_prevweapon);
    M_BindKeyVariable("key_nextweapon",     &key_nextweapon);

#if !NO_USE_JOYSTICK
    M_BindIntVariable("joyb_prevweapon",    &joybprevweapon);
    M_BindIntVariable("joyb_nextweapon",    &joybnextweapon);
#endif

    M_BindMouseBVariable("mouseb_prevweapon",  &mousebprevweapon);
    M_BindMouseBVariable("mouseb_nextweapon",  &mousebnextweapon);
}

void M_BindMapControls(void)
{
    M_BindKeyVariable("key_map_north",      &key_map_north);
    M_BindKeyVariable("key_map_south",      &key_map_south);
    M_BindKeyVariable("key_map_east",       &key_map_east);
    M_BindKeyVariable("key_map_west",       &key_map_west);
    M_BindKeyVariable("key_map_zoomin",     &key_map_zoomin);
    M_BindKeyVariable("key_map_zoomout",    &key_map_zoomout);
    M_BindKeyVariable("key_map_toggle",     &key_map_toggle);
    M_BindKeyVariable("key_map_maxzoom",    &key_map_maxzoom);
    M_BindKeyVariable("key_map_follow",     &key_map_follow);
    M_BindKeyVariable("key_map_grid",       &key_map_grid);
    M_BindKeyVariable("key_map_mark",       &key_map_mark);
    M_BindKeyVariable("key_map_clearmark",  &key_map_clearmark);
}

void M_BindMenuControls(void)
{
    M_BindKeyVariable("key_menu_activate",  &key_menu_activate);
    M_BindKeyVariable("key_menu_up",        &key_menu_up);
    M_BindKeyVariable("key_menu_down",      &key_menu_down);
    M_BindKeyVariable("key_menu_left",      &key_menu_left);
    M_BindKeyVariable("key_menu_right",     &key_menu_right);
    M_BindKeyVariable("key_menu_back",      &key_menu_back);
    M_BindKeyVariable("key_menu_forward",   &key_menu_forward);
    M_BindKeyVariable("key_menu_confirm",   &key_menu_confirm);
    M_BindKeyVariable("key_menu_abort",     &key_menu_abort);

    M_BindKeyVariable("key_menu_help",      &key_menu_help);
    M_BindKeyVariable("key_menu_save",      &key_menu_save);
    M_BindKeyVariable("key_menu_load",      &key_menu_load);
    M_BindKeyVariable("key_menu_volume",    &key_menu_volume);
    M_BindKeyVariable("key_menu_detail",    &key_menu_detail);
    M_BindKeyVariable("key_menu_qsave",     &key_menu_qsave);
    M_BindKeyVariable("key_menu_endgame",   &key_menu_endgame);
    M_BindKeyVariable("key_menu_messages",  &key_menu_messages);
    M_BindKeyVariable("key_menu_qload",     &key_menu_qload);
    M_BindKeyVariable("key_menu_quit",      &key_menu_quit);
    M_BindKeyVariable("key_menu_gamma",     &key_menu_gamma);

    M_BindKeyVariable("key_menu_incscreen", &key_menu_incscreen);
    M_BindKeyVariable("key_menu_decscreen", &key_menu_decscreen);
    M_BindKeyVariable("key_menu_screenshot",&key_menu_screenshot);
    M_BindKeyVariable("key_demo_quit",      &key_demo_quit);
    M_BindKeyVariable("key_spy",            &key_spy);
}

void M_BindChatControls(unsigned int num_players)
{
    char name[32];  // haleyjd: 20 not large enough - Thank you, come again!
    unsigned int i; // haleyjd: signedness conflict

    M_BindKeyVariable("key_multi_msg",     &key_multi_msg);

    for (i=0; i<num_players; ++i)
    {
        M_snprintf(name, sizeof(name), "key_multi_msgplayer%i", i + 1);
        M_BindKeyVariable(name, &key_multi_msgplayer[i]);
    }
}

//
// Apply custom patches to the default values depending on the
// platform we are running on.
//

void M_ApplyPlatformDefaults(void)
{
    // no-op. Add your platform-specific patches here.
}

