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

#ifndef __M_CONTROLS_H__
#define __M_CONTROLS_H__
 
extern key_type_t key_right;
extern key_type_t key_left;

extern key_type_t key_up;
extern key_type_t key_down;
extern key_type_t key_strafeleft;
extern key_type_t key_straferight;
extern key_type_t key_fire;
extern key_type_t key_use;
extern key_type_t key_strafe;
extern key_type_t key_speed;

extern key_type_t key_jump;
 
extern key_type_t key_flyup;
extern key_type_t key_flydown;
extern key_type_t key_flycenter;
extern key_type_t key_lookup;
extern key_type_t key_lookdown;
extern key_type_t key_lookcenter;
extern key_type_t key_invleft;
extern key_type_t key_invright;
extern key_type_t key_useartifact;

// villsa [STRIFE] strife keys
extern key_type_t key_usehealth;
extern key_type_t key_invquery;
extern key_type_t key_mission;
extern key_type_t key_invpop;
extern key_type_t key_invkey;
extern key_type_t key_invhome;
extern key_type_t key_invend;
extern key_type_t key_invuse;
extern key_type_t key_invdrop;

extern key_type_t key_message_refresh;
extern key_type_t key_pause;

extern key_type_t key_multi_msg;
extern key_type_t key_multi_msgplayer[8];

extern key_type_t key_weapon1;
extern key_type_t key_weapon2;
extern key_type_t key_weapon3;
extern key_type_t key_weapon4;
extern key_type_t key_weapon5;
extern key_type_t key_weapon6;
extern key_type_t key_weapon7;
extern key_type_t key_weapon8;

extern key_type_t key_arti_all;
extern key_type_t key_arti_health;
extern key_type_t key_arti_poisonbag;
extern key_type_t key_arti_blastradius;
extern key_type_t key_arti_teleport;
extern key_type_t key_arti_teleportother;
extern key_type_t key_arti_egg;
extern key_type_t key_arti_invulnerability;

extern key_type_t key_demo_quit;
extern key_type_t key_spy;
extern key_type_t key_prevweapon;
extern key_type_t key_nextweapon;

extern key_type_t key_map_north;
extern key_type_t key_map_south;
extern key_type_t key_map_east;
extern key_type_t key_map_west;
extern key_type_t key_map_zoomin;
extern key_type_t key_map_zoomout;
extern key_type_t key_map_toggle;
extern key_type_t key_map_maxzoom;
extern key_type_t key_map_follow;
extern key_type_t key_map_grid;
extern key_type_t key_map_mark;
extern key_type_t key_map_clearmark;

// menu keys:

extern key_type_t key_menu_activate;
extern key_type_t key_menu_up;
extern key_type_t key_menu_down;
extern key_type_t key_menu_left;
extern key_type_t key_menu_right;
extern key_type_t key_menu_back;
extern key_type_t key_menu_forward;
extern key_type_t key_menu_confirm;
extern key_type_t key_menu_abort;

extern key_type_t key_menu_help;
extern key_type_t key_menu_save;
extern key_type_t key_menu_load;
extern key_type_t key_menu_volume;
extern key_type_t key_menu_detail;
extern key_type_t key_menu_qsave;
extern key_type_t key_menu_endgame;
extern key_type_t key_menu_messages;
extern key_type_t key_menu_qload;
extern key_type_t key_menu_quit;
extern key_type_t key_menu_gamma;

extern key_type_t key_menu_incscreen;
extern key_type_t key_menu_decscreen;
extern key_type_t key_menu_screenshot;

extern mouseb_type_t mousebfire;
extern mouseb_type_t mousebstrafe;
extern mouseb_type_t mousebforward;

extern mouseb_type_t mousebjump;

extern mouseb_type_t mousebstrafeleft;
extern mouseb_type_t mousebstraferight;
extern mouseb_type_t mousebbackward;
extern mouseb_type_t mousebuse;

extern mouseb_type_t mousebprevweapon;
extern mouseb_type_t mousebnextweapon;

#if !NO_USE_JOYSTICK
extern int joybfire;
extern int joybstrafe;
extern int joybuse;
extern int joybspeed;

extern int joybjump;

extern int joybstrafeleft;
extern int joybstraferight;

extern int joybprevweapon;
extern int joybnextweapon;

extern int joybmenu;
extern int joybautomap;
#endif

extern isb_int8_t dclick_use;

void M_BindBaseControls(void);
void M_BindHereticControls(void);
void M_BindHexenControls(void);
void M_BindStrifeControls(void);
void M_BindWeaponControls(void);
void M_BindMapControls(void);
void M_BindMenuControls(void);
void M_BindChatControls(unsigned int num_players);

void M_ApplyPlatformDefaults(void);

#endif /* #ifndef __M_CONTROLS_H__ */

