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
//	DOOM selection menu, options, episode etc.
//	Sliders and icons. Kinda widget stuff.
//


#include <stdlib.h>
#include <ctype.h>


#include "doomdef.h"
#include "doomkeys.h"
#include "dstrings.h"

#include "d_main.h"
#include "deh_main.h"

#include "i_input.h"
#include "i_swap.h"
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_misc.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"

#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_controls.h"
#include "p_saveg.h"
#include "p_setup.h"

#include "s_sound.h"

#include "doomstat.h"

// Data.
#include "sounds.h"

#include "m_menu.h"

#if PICO_DOOM && USE_PICO_NET
#include "piconet.h"
#include "net_client.h"

#define NET_MENU 1
#endif

#define DEFAULTPLAYERNAME "DOOMGUY"

extern vpatch_sequence_t 	hu_font;
extern boolean		message_dontfuckwithme;

extern boolean		chat_on;		// in heads-up code

//
// defaulted values
//
isb_int8_t		mouseSensitivity = 5;

// Show messages fdefault, 0 = off, 1 = on
isb_int8_t		showMessages = 1;
	

// Blocky mode, has default, 0 = high, 1 = normal
isb_int8_t		detailLevel = 0;
#if DOOM_TINY
isb_int8_t		screenblocks = 10; // default to preferred size as we don't have setting right now
#else
isb_int8_t		screenblocks = 9;
#endif

// temp for screenblocks (0-9)
static isb_int8_t 	screenSize;

// -1 = no quicksave slot picked!
static isb_int8_t 	quickSaveSlot;

 // 1 = message to be printed
int			messageToPrint;
// ...and here is the message string!
const char		*messageString;
const char      *messageString2;

// message x & y
int			messx;
int			messy;
int			messageLastMenuActive;

// timed message = no input from user
boolean			messageNeedsInput;

boolean    (*messageRoutine)(int response);

const char gammamsg[5][26] =
{
    GAMMALVL0,
    GAMMALVL1,
    GAMMALVL2,
    GAMMALVL3,
    GAMMALVL4
};

// we are going to be entering a savegame (or other) string
isb_int8_t			stringEntry;
isb_int8_t            	saveSlot;	// which slot to save in
isb_int8_t			stringEntryIndex;	// which char we're editing
isb_int8_t          stringEntryMax;
char *stringEntryBuffer;

#if NET_MENU
char player_name[MAXPLAYERNAME];
#endif
#if !NO_USE_SAVE
static boolean          joypadSave = false; // was the save action initiated by joypad?
#endif
// old save description before edit
char			stringEntryOldString[MAX(SAVESTRINGSIZE,MAXPLAYERNAME)];

uint8_t			inhelpscreens;
boolean			menuactive;

#define SKULLXOFF		-32
#define LINEHEIGHT		16

extern boolean		sendpause;
char			savegamestrings[6][SAVESTRINGSIZE];
#if !DOOM_TINY
char	endstring[160];
#endif

#if !DOOM_TINY
static boolean opldev;
#else
#define opldev false
#endif

//
// MENU TYPEDEFS
//
typedef struct
{
    // 0 = no cursor here, 1 = ok, 2 = arrows ok
    short	status;

    vpatchname_t name;

    // hotkey in menu
    char	alphaKey;

    // choice = menu item #.
    // if status = 2,
    //   choice=0:leftarrow,1:rightarrow
    void	(*routine)(int choice);
    
} menuitem_t;



typedef struct menu_s
{
    short		numitems;	// # of menu items
    struct menu_s*	prevMenu;	// previous menu
    const menuitem_t*		menuitems;	// menu items
    void		(*routine)();	// draw routine
    short		x;
    short		y;		// x,y of menu
    short		lastOn;		// last item user was on in menu
} menu_t;

short		itemOn;			// menu item skull is on
short		skullAnimCounter;	// skull animation counter
short		whichSkull;		// which skull to draw

// graphic name of skulls
// warning: initializer-string for array of chars is too long
vpatchname_t skullName[2] = {VPATCH_NAME(M_SKULL1), VPATCH_NAME(M_SKULL2)};

// current menudef
menu_t*	currentMenu;                          
//
// PROTOTYPES
//
static void M_NewGame(int choice);
static void M_Episode(int choice);
static void M_ChooseSkill(int choice);
#if !NO_USE_LOAD
static void M_LoadGame(int choice);
#endif
static void M_Options(int choice);
#if NET_MENU
static void M_NetGame(int choice);
static void M_NetName(int choice);
static void M_HostGame(int choice);
static void M_JoinGame(int choice);
static void M_NetGameStart(int choice);
#endif
static void M_EndGame(int choice);
static void M_ReadThis(int choice);
static void M_ReadThis2(int choice);
static void M_QuitDOOM(int choice);

static void M_ChangeMessages(int choice);
static void M_ChangeSensitivity(int choice);
static void M_SfxVol(int choice);
static void M_MusicVol(int choice);
static void M_ChangeDetail(int choice);
static void M_SizeDisplay(int choice);
static void M_Sound(int choice);

static void M_FinishReadThis(int choice);
#if !NO_USE_LOAD
static void M_LoadSelect(int choice);
static void M_QuickLoad(void);
#endif
#if !NO_USE_SAVE
static void M_SaveSelect(int choice);
static void M_QuickSave(void);
#endif
#if !NO_USE_LOAD
static void M_ReadSaveStrings(void);
#endif

static void M_DrawMainMenu(void);
static void M_DrawReadThis1(void);
static void M_DrawReadThis2(void);
static void M_DrawNewGame(void);
static void M_DrawEpisode(void);
static void M_DrawOptions(void);
#if NET_MENU
static void M_DrawNetGame(void);
static void M_DrawNetFoyer(void);
#endif
static void M_DrawSound(void);
#if !NO_USE_LOAD
static void M_DrawLoad(void);
#endif
#if !NO_USE_SAVE
static void M_DrawSave(void);
#endif

#if !NO_USE_LOAD || !NO_USE_SAVE
static void M_DrawSaveLoadBorder(int x,int y, int l);
#endif
static void M_SetupNextMenu(menu_t *menudef);
static void M_DrawThermo(int x,int y,int thermWidth,int thermDot);
static void M_WriteText(int x, int y, const char *string);
static int  M_StringWidth(const char *string);
static int  M_StringHeight(const char *string);




//
// DOOM MENU
//
enum
{
    newgame = 0,
    options,
#if !NO_USE_LOAD
    loadgame,
#endif
#if !NO_USE_SAVE
    savegame,
#endif
    readthis,
    quitdoom,
    main_end
} main_e;

static menuitem_t MainMenu[]=
{
    {1,VPATCH_NAME(M_NGAME),'n', M_NewGame},
    {1,VPATCH_NAME(M_OPTION), 'o', M_Options},
#if !NO_USE_LOAD
    {1,VPATCH_NAME(M_LOADG), 'l', M_LoadGame},
#endif
#if !NO_USE_SAVE
    {1,VPATCH_NAME(M_SAVEG), 's', M_SaveGame},
#endif
    // Another hickup with Special edition.
    {1,VPATCH_NAME(M_RDTHIS), 'r', M_ReadThis},
    {1,VPATCH_NAME(M_QUITG),'q',M_QuitDOOM}
};

menu_t  MainDef =
{
    main_end,
    NULL,
    MainMenu,
    M_DrawMainMenu,
    97,64,
    0
};


//
// EPISODE SELECT
//
enum
{
    ep1,
    ep2,
    ep3,
    ep4,
    ep_end
} episodes_e;

static const menuitem_t EpisodeMenu[]=
{
    {1,VPATCH_NAME(M_EPI1),'k', M_Episode},
    {1,VPATCH_NAME(M_EPI2),'t', M_Episode},
    {1,VPATCH_NAME(M_EPI3),'i', M_Episode},
    {1,VPATCH_NAME(M_EPI4),'t', M_Episode}
};

menu_t  EpiDef =
{
    ep_end,		// # of menu items
    &MainDef,		// previous menu
    EpisodeMenu,	// menuitem_t ->
    M_DrawEpisode,	// drawing routine ->
    48,63,              // x,y
    ep1			// lastOn
};

//
// NEW GAME
//
enum
{
    killthings,
    toorough,
    hurtme,
    violence,
    nightmare,
    newg_end
} newgame_e;

static const menuitem_t NewGameMenu[]=
{
    {1,VPATCH_NAME(M_JKILL), 'i',	M_ChooseSkill},
    {1,VPATCH_NAME(M_ROUGH), 'h',	M_ChooseSkill},
    {1,VPATCH_NAME(M_HURT), 'h',	M_ChooseSkill},
    {1,VPATCH_NAME(M_ULTRA), 'u',	M_ChooseSkill},
    {1,VPATCH_NAME(M_NMARE), 'n',	M_ChooseSkill}
};

menu_t  NewDef =
{
    newg_end,		// # of menu items
    &EpiDef,		// previous menu
    NewGameMenu,	// menuitem_t ->
    M_DrawNewGame,	// drawing routine ->
    48,63,              // x,y
    hurtme		// lastOn
};



//
// OPTIONS MENU
//
enum
{
#if NET_MENU
    networkgame,
#endif
    endgame,
    messages,
#if !DOOM_TINY
    detail,
    scrnsize,
    option_empty1,
#endif
#if !NO_USE_MOUSE
    mousesens,
    option_empty2,
#endif
    soundvol,
    opt_end
} options_e;

static const menuitem_t OptionsMenu[]=
{
#if NET_MENU
    {1,VPATCH_NAME(M_NETWK),'n',	M_NetGame},
#endif
    {1,VPATCH_NAME(M_ENDGAM),'e',	M_EndGame},
    {1,VPATCH_NAME(M_MESSG),	'm',M_ChangeMessages},
#if !DOOM_TINY
    {1,VPATCH_NAME(M_DETAIL),'g',	M_ChangeDetail},
    {2,VPATCH_NAME(M_SCRNSZ),'s',	M_SizeDisplay},
    {-1,VPATCH_NAME_INVALID,'\0',0},
#endif
#if !NO_USE_MOUSE
    {2,VPATCH_NAME(M_MSENS),'m',	M_ChangeSensitivity},
    {-1,VPATCH_NAME_INVALID,'\0',0},
#endif
    {1,VPATCH_NAME(M_SVOL),'s',	M_Sound}
};

menu_t  OptionsDef =
{
    opt_end,
    &MainDef,
    OptionsMenu,
    M_DrawOptions,
    60,37,
    0
};

#if NET_MENU
static const menuitem_t NetGameMenu[]=
        {
                {1,VPATCH_NAME(M_NAME),'n',	M_NetName},
                {1,VPATCH_NAME(M_HOST),'h',	M_HostGame},
                {1,VPATCH_NAME(M_HOST),	'd',M_HostGame},
                {1,VPATCH_NAME(M_HOST),	'f',M_HostGame},
                {1,VPATCH_NAME(M_JOIN),	'j',M_JoinGame},
        };

enum
{
    ng_name,
    ng_host_normal,
    ng_host_deathmatch,
    ng_host_deathmatch2,
    ng_join,
    ng_end,
} netgame_e;

static int8_t netgame_choice;

menu_t NetGameDef =
        {
                ng_end,
                &OptionsDef,
                NetGameMenu,
                M_DrawNetGame,
                56,47,
                0
        };

//
// Read This! MENU 1 & 2
//
enum
{
    nfoyerempty1,
    nfoyer_end
} net_foyer_e;

static menuitem_t NetFoyerMenu[] =
        {
                {1,VPATCH_NAME_INVALID,0, M_NetGameStart}
        };

menu_t  NetFoyerDef =
        {
                nfoyer_end,
                &NewDef,
                NetFoyerMenu,
                M_DrawNetFoyer,
                -1,0, // force hide of skull
                0
        };

#endif

//
// Read This! MENU 1 & 2
//
enum
{
    rdthsempty1,
    read1_end
} read_e;

static menuitem_t ReadMenu1[] =
{
    {1,VPATCH_NAME_INVALID,0, M_ReadThis2}
};

menu_t  ReadDef1 =
{
    read1_end,
    &MainDef,
    ReadMenu1,
    M_DrawReadThis1,
    280,185,
    0
};

enum
{
    rdthsempty2,
    read2_end
} read_e2;

static const menuitem_t ReadMenu2[]=
{
    {1,VPATCH_NAME_INVALID,0, M_FinishReadThis}
};

menu_t  ReadDef2 =
{
    read2_end,
    &ReadDef1,
    ReadMenu2,
    M_DrawReadThis2,
    330,175,
    0
};

//
// SOUND VOLUME MENU
//
enum
{
    sfx_vol,
    sfx_empty1,
    music_vol,
    sfx_empty2,
    sound_end
} sound_e;

static const menuitem_t SoundMenu[]=
{
    {2,VPATCH_NAME(M_SFXVOL), 's', M_SfxVol},
    {-1,VPATCH_NAME_INVALID,'\0',0,},
    {2,VPATCH_NAME(M_MUSVOL), 'm', M_MusicVol},
    {-1,VPATCH_NAME_INVALID,'\0',0,},
};

menu_t  SoundDef =
{
    sound_end,
    &OptionsDef,
    SoundMenu,
    M_DrawSound,
    80,64,
    0
};

#if !NO_USE_LOAD
//
// LOAD GAME MENU
//
enum
{
    load1,
    load2,
    load3,
    load4,
    load5,
    load6,
    load_end
} load_e;

static_assert(count_of(savegamestrings) == load_end, "");

// this is the only one which is mutated
menuitem_t LoadMenu[]=
{
    {1,VPATCH_NAME_INVALID, '1', M_LoadSelect},
    {1,VPATCH_NAME_INVALID, '2', M_LoadSelect},
    {1,VPATCH_NAME_INVALID, '3', M_LoadSelect},
    {1,VPATCH_NAME_INVALID, '4', M_LoadSelect},
    {1,VPATCH_NAME_INVALID, '5', M_LoadSelect},
    {1,VPATCH_NAME_INVALID, '6', M_LoadSelect}
};

menu_t  LoadDef =
{
    load_end,
    &MainDef,
    LoadMenu,
    M_DrawLoad,
    80,54,
    0
};
#endif

#if !NO_USE_SAVE
//
// SAVE GAME MENU
//
menuitem_t SaveMenu[]=
{
    {1,VPATCH_NAME_INVALID, '1', M_SaveSelect},
    {1,VPATCH_NAME_INVALID, '2', M_SaveSelect},
    {1,VPATCH_NAME_INVALID, '3', M_SaveSelect},
    {1,VPATCH_NAME_INVALID, '4', M_SaveSelect},
    {1,VPATCH_NAME_INVALID, '5', M_SaveSelect},
    {1,VPATCH_NAME_INVALID, '6', M_SaveSelect}
};

menu_t  SaveDef =
{
    load_end,
    &MainDef,
    SaveMenu,
    M_DrawSave,
    80,54,
    0
};

#endif
#if !NO_USE_LOAD
//
// M_ReadSaveStrings
//  read the strings from the savegame files
//
void M_ReadSaveStrings(void)
{
#if !NO_FILE_ACCESS
    FILE   *handle;
    int     i;
    char    name[256];

    for (i = 0;i < load_end;i++)
    {
        int retval;
        M_StringCopy(name, P_SaveGameFile(i), sizeof(name));

	handle = fopen(name, "rb");
        if (handle == NULL)
        {
            M_StringCopy(savegamestrings[i], EMPTYSTRING, SAVESTRINGSIZE);
            LoadMenu[i].status = 0;
            continue;
        }
        retval = fread(&savegamestrings[i], 1, SAVESTRINGSIZE, handle);
	fclose(handle);
        LoadMenu[i].status = retval == SAVESTRINGSIZE;
    }
#elif PICO_ON_DEVICE
    flash_slot_info_t slots[load_end];
    P_SaveGameGetExistingFlashSlotAddresses(slots, count_of(slots));
    for (int i = 0;i < load_end;i++) {
        if (slots[i].data) {
            M_StringCopy(savegamestrings[i], (const char *)slots[i].data, SAVESTRINGSIZE);
            LoadMenu[i].status = 1;
        } else {
            M_StringCopy(savegamestrings[i], EMPTYSTRING, SAVESTRINGSIZE);
            LoadMenu[i].status = 0;
        }
    }
#endif
}
#endif


#if !NO_USE_LOAD
//
// M_LoadGame & Cie.
//
void M_DrawLoad(void)
{
    int             i;
	
    V_DrawPatchDirect(72, 28, VPATCH_HANDLE(VPATCH_NAME(M_LOADG)));

    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i, 24);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }
}



#endif

#if !NO_USE_LOAD || !NO_USE_SAVE

static char tempstring[90];

//
// Draw border for the savegame description
//
void M_DrawSaveLoadBorder(int x,int y, int l)
{
    int             i;
	
    V_DrawPatchDirect(x - 8, y + 7,
                      VPATCH_HANDLE(VPATCH_NAME(M_LSLEFT)));

#if !DOOM_TINY
    for (i = 0;i < l;i++)
    {
        V_DrawPatchDirect(x, y + 7,
                          W_CacheLumpName(DEH_String("M_LSCNTR"), PU_CACHE));
        x += 8;
    }
#else
    V_DrawPatchDirectN(x, y + 7,
                      VPATCH_HANDLE(VPATCH_NAME(M_LSCNTR)), l-1);
    x += l * 8;
#endif

    V_DrawPatchDirect(x, y + 7, 
                      VPATCH_HANDLE(VPATCH_NAME(M_LSRGHT)));
}

#endif

#if !NO_USE_LOAD


#if PICO_ON_DEVICE
uint8_t g_load_slot;
#endif
//
// User wants to load this game
//
void M_LoadSelect(int choice)
{
    char    name[256];
	
    M_StringCopy(name, P_SaveGameFile(choice), sizeof(name));
#if PICO_ON_DEVICE
    g_load_slot = choice;
#endif
    G_LoadGame (name);
    M_ClearMenus ();
}

//
// Selected from DOOM menu
//
void M_LoadGame (int choice)
{
    if (netgame)
    {
	M_StartMessage(DEH_String(LOADNET),NULL,false);
	return;
    }
	
    M_SetupNextMenu(&LoadDef);
    M_ReadSaveStrings();
}

#endif

#if !NO_USE_SAVE

//
//  M_SaveGame & Cie.
//
void M_DrawSave(void)
{
    int             i;
	
    V_DrawPatchDirect(72, 28, VPATCH_HANDLE(VPATCH_NAME(M_SAVEG)));
    for (i = 0;i < load_end; i++)
    {
	M_DrawSaveLoadBorder(LoadDef.x,LoadDef.y+LINEHEIGHT*i, 24);
	M_WriteText(LoadDef.x,LoadDef.y+LINEHEIGHT*i,savegamestrings[i]);
    }
	
    if (stringEntry)
    {
	i = M_StringWidth(savegamestrings[saveSlot]);
	M_WriteText(LoadDef.x + i,LoadDef.y+LINEHEIGHT*saveSlot,"_");
    }
}

//
// M_Responder calls this when user is finished
//
void M_DoSave(int slot)
{
    G_SaveGame (slot,savegamestrings[slot]);
    M_ClearMenus ();

    // PICK QUICKSAVE SLOT_RENDER YET?
    if (quickSaveSlot == -2)
	quickSaveSlot = slot;
}

//
// Generate a default save slot name when the user saves to
// an empty slot via the joypad.
//
static void SetDefaultSaveName(int slot)
{
#if !DOOM_TINY
    // map from IWAD or PWAD?
    if (W_IsIWADLump(maplumpinfo))
    {
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE,
                   "%s", maplumpinfo->name);
    }
    else
    {
        M_snprintf(savegamestrings[itemOn], SAVESTRINGSIZE,
                   "%s: %s", W_WadNameForLump(maplumpinfo),
                   maplumpinfo->name);
    }
#else
    strcpy(savegamestrings[itemOn], "TEST");
#endif
    M_ForceUppercase(savegamestrings[itemOn]);
    joypadSave = false;
}

//
// User wants to save. Start string input for M_Responder
//
void M_SaveSelect(int choice)
{
    int x, y;

    // we are going to be intercepting all chars
    stringEntry = 1;

    // We need to turn on text input:
    x = LoadDef.x - 11;
    y = LoadDef.y + choice * LINEHEIGHT - 4;
    I_StartTextInput(x, y, x + 8 + 24 * 8 + 8, y + LINEHEIGHT - 2);

    saveSlot = choice;
    M_StringCopy(stringEntryOldString, savegamestrings[choice], SAVESTRINGSIZE);
    if (!strcmp(savegamestrings[choice], EMPTYSTRING))
    {
        savegamestrings[choice][0] = 0;

        if (joypadSave)
        {
            SetDefaultSaveName(choice);
        }
    }
    stringEntryBuffer = savegamestrings[choice];
    stringEntryIndex = strlen(savegamestrings[choice]);
    stringEntryMax = SAVESTRINGSIZE;
}

static void M_StringEntryEnd() {
    // save using another variable since we only have two usages and they have differnet max lengths
    if (stringEntryMax == SAVESTRINGSIZE) {
        if (stringEntryBuffer[0]) M_DoSave(saveSlot);
#if NET_MENU
    } else if (!player_name[0]) {
        strcpy(player_name, DEFAULTPLAYERNAME);
#endif
    }
}

//
// Selected from DOOM menu
//
void M_SaveGame (int choice)
{
    if (!usergame)
    {
	M_StartMessage(DEH_String(SAVEDEAD),NULL,false);
	return;
    }
	
    if (gamestate != GS_LEVEL)
	return;
	
    M_SetupNextMenu(&SaveDef);
    M_ReadSaveStrings();
}



//
//      M_QuickSave
//

boolean M_QuickSaveResponse(int key)
{
    if (key == key_menu_confirm)
    {
	M_DoSave(quickSaveSlot);
	S_StartUnpositionedSound( sfx_swtchx);
    }
    return false;
}

void M_QuickSave(void)
{
    if (!usergame)
    {
	S_StartUnpositionedSound( sfx_oof);
	return;
    }

    if (gamestate != GS_LEVEL)
	return;
	
    if (quickSaveSlot < 0)
    {
	M_StartControlPanel();
	M_ReadSaveStrings();
	M_SetupNextMenu(&SaveDef);
	quickSaveSlot = -2;	// means to pick a slot now
	return;
    }
    DEH_snprintf(tempstring, sizeof(tempstring),
                 QSPROMPT, savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring, M_QuickSaveResponse, true);
}

#endif

#if !NO_USE_LOAD


//
// M_QuickLoad
//
boolean M_QuickLoadResponse(int key)
{
    if (key == key_menu_confirm)
    {
	M_LoadSelect(quickSaveSlot);
	S_StartUnpositionedSound( sfx_swtchx);
    }
    return false;
}


void M_QuickLoad(void)
{
    if (netgame)
    {
	M_StartMessage(DEH_String(QLOADNET),NULL,false);
	return;
    }
	
    if (quickSaveSlot < 0)
    {
	M_StartMessage(DEH_String(QSAVESPOT),NULL,false);
	return;
    }
    DEH_snprintf(tempstring, sizeof(tempstring),
                 QLPROMPT, savegamestrings[quickSaveSlot]);
    M_StartMessage(tempstring, M_QuickLoadResponse, true);
}

#endif


//
// Read This Menus
// Had a "quick hack to fix romero bug"
//
void M_DrawReadThis1(void)
{
#if DOOM_TINY
    if (gamestate == GS_FINALE) {
        M_ClearMenus();
        return;
    }
#endif
    inhelpscreens = 2;

#if !USE_WHD
    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP2"), PU_CACHE));
#endif
}



//
// Read This Menus - optional second page.
//
void M_DrawReadThis2(void)
{
#if DOOM_TINY
    if (gamestate == GS_FINALE) {
        M_ClearMenus();
        return;
    }
#endif
    inhelpscreens = 1;

    // We only ever draw the second page if this is 
    // gameversion == exe_doom_1_9 and gamemode == registered

#if !USE_WHD
    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP1"), PU_CACHE));
#endif
}

void M_DrawReadThisCommercial(void)
{
#if DOOM_TINY
    if (gamestate == GS_FINALE) {
        M_ClearMenus();
        return;
    }
#endif
    inhelpscreens = 3;

#if !USE_WHD
    V_DrawPatchDirect(0, 0, W_CacheLumpName(DEH_String("HELP"), PU_CACHE));
#endif
}


//
// Change Sfx & Music volumes
//
void M_DrawSound(void)
{
    V_DrawPatchDirect (60, 38, VPATCH_HANDLE(VPATCH_NAME(M_SVOL)));

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(sfx_vol+1),
		 16,sfxVolume);

    M_DrawThermo(SoundDef.x,SoundDef.y+LINEHEIGHT*(music_vol+1),
		 16,musicVolume);
}

void M_Sound(int choice)
{
    M_SetupNextMenu(&SoundDef);
}

void M_SfxVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (sfxVolume)
	    sfxVolume--;
	break;
      case 1:
	if (sfxVolume < 15)
	    sfxVolume++;
	break;
    }
	
    S_SetSfxVolume(sfxVolume * 8);
}

void M_MusicVol(int choice)
{
    switch(choice)
    {
      case 0:
	if (musicVolume)
	    musicVolume--;
	break;
      case 1:
	if (musicVolume < 15)
	    musicVolume++;
	break;
    }
	
    S_SetMusicVolume(musicVolume * 8);
}




//
// M_DrawMainMenu
//
void M_DrawMainMenu(void)
{
    V_DrawPatchDirect(94, 2,
                      VPATCH_HANDLE(VPATCH_NAME(M_DOOM)));
}




//
// M_NewGame
//
void M_DrawNewGame(void)
{
    V_DrawPatchDirect(96, 14, VPATCH_HANDLE(VPATCH_NAME(M_NEWG)));
    V_DrawPatchDirect(54, 38, VPATCH_HANDLE(VPATCH_NAME(M_SKILL)));
}

void M_NewGame(int choice)
{
    if (netgame && !demoplayback)
    {
	M_StartMessage(DEH_String(NEWGAME),NULL,false);
	return;
    }

    // Chex Quest disabled the episode select screen, as did Doom II.

#if NET_MENU
    EpiDef.prevMenu = &MainDef;
    netgame_choice = -1;
#endif
    if (gamemode == commercial || gameversion_is_chex(gameversion))
	M_SetupNextMenu(&NewDef);
    else
	M_SetupNextMenu(&EpiDef);
}


//
//      M_Episode
//
isb_int8_t   epi;
isb_uint8_t  skill;

void M_DrawEpisode(void)
{
    V_DrawPatchDirect(54, 38, VPATCH_HANDLE(VPATCH_NAME(M_EPISOD)));
}

boolean M_FinishGameSelection() {
#if NET_MENU
    if (netgame_choice >= 0) {
#if USE_PICO_NET
        piconet_start_host(netgame_choice-ng_host_normal, epi+1, skill);
#endif
        M_SetupNextMenu(&NetFoyerDef);
        return true;
    }
#endif
    G_DeferedInitNew(skill,epi+1,1, false);
    M_ClearMenus ();
    return false;
}

boolean M_VerifyNightmare(int key)
{
    if (key != key_menu_confirm)
	return false;
    skill = nightmare;
    return M_FinishGameSelection();
}

void M_ChooseSkill(int choice)
{
    if (choice == nightmare)
    {
	M_StartMessage(DEH_String(NIGHTMARE),M_VerifyNightmare,true);
	return;
    }

    skill = choice;
    M_FinishGameSelection();
}

void M_Episode(int choice)
{
    if ( (gamemode == shareware)
	 && choice)
    {
	M_StartMessage(DEH_String(SWSTRING),NULL,false);
	M_SetupNextMenu(&ReadDef1);
	return;
    }

    epi = choice;
    M_SetupNextMenu(&NewDef);
}



//
// M_Options
//
#if !DOOM_TINY
static const char *detailNames[2] = {"M_GDHIGH","M_GDLOW"};
#endif
vpatchname_t msgNames[2] = {VPATCH_NAME(M_MSGOFF), VPATCH_NAME(M_MSGON)};

void M_DrawOptions(void)
{
    V_DrawPatchDirect(108, 15, VPATCH_HANDLE(VPATCH_NAME(M_OPTTTL)));

#if !DOOM_TINY
    V_DrawPatchDirect(OptionsDef.x + 175, OptionsDef.y + LINEHEIGHT * detail,
		      W_CacheLumpName(DEH_String(detailNames[detailLevel]),
			              PU_CACHE));
#else
    // "Game" for Network
    V_DrawPatchDirect(OptionsDef.x + 105, OptionsDef.y + LINEHEIGHT * networkgame,
                      VPATCH_HANDLE(VPATCH_NAME(M_GAME)));

#endif

    V_DrawPatchDirect(OptionsDef.x + 120, OptionsDef.y + LINEHEIGHT * messages,
                      VPATCH_HANDLE(msgNames[showMessages]));

#if !NO_USE_MOUSE
    M_DrawThermo(OptionsDef.x, OptionsDef.y + LINEHEIGHT * (mousesens + 1),
		 10, mouseSensitivity);
#endif

#if !DOOM_TINY
    M_DrawThermo(OptionsDef.x,OptionsDef.y+LINEHEIGHT*(scrnsize+1),
		 9,screenSize);
#endif
}

void M_Options(int choice)
{
    M_SetupNextMenu(&OptionsDef);
}

#if NET_MENU

extern net_module_t net_loop_server_module;
void M_NetGameStart(int choice) {
    if (choice == 0) {
        // we came from enter in the menu
        if (netgame_choice == ng_join) return; // ignore ENTER from join menu
    }
    lobby_state_t ls;
    /**
     * I'm dreaming of a COVID christmas
     * Just like the ones I used to know
     * Where the friends are missin'
     * and there ain't no kissin'
     * and it goes so slow, oh no.
     */
    int pnum = piconet_get_lobby_state(&ls);
    if (pnum >= 0) {
        consoleplayer = pnum;
        netgame = true;
        deathmatch = ls.deathmatch;
        int i = 0;
        for (; i < ls.nplayers; i++) {
            playeringame[i] = 1;
        }
        for (; i < NET_MAXPLAYERS; i++) {
            playeringame[i] = 0;
        }
#if PICO_DOOM_INFO
        printf("PLAYERS: %d\n", ls.nplayers);
#endif
        G_DeferedInitNew(ls.skill,ls.epi,1,true);
        M_ClearMenus ();
        D_StartPicoNetGame();
        piconet_start_game();
    }
}

void M_DrawNetGame(void)
{
    V_DrawPatchDirect(68, 15, VPATCH_HANDLE(VPATCH_NAME(M_NETWK)));
    // "Game" for Title
    V_DrawPatchDirect(173, 15,
                      VPATCH_HANDLE(VPATCH_NAME(M_GAME)));

    M_DrawSaveLoadBorder(NetGameDef.x+81,NetGameDef.y+LINEHEIGHT*ng_name+5, MAXPLAYERNAME);
    M_WriteText(NetGameDef.x+81,NetGameDef.y+LINEHEIGHT*ng_name+5,player_name);
    if (stringEntry) {
        int i = M_StringWidth(player_name);
        M_WriteText(NetGameDef.x+81 + i,NetGameDef.y+LINEHEIGHT*ng_name+5,"_");
    }

    V_DrawPatchDirect(NetGameDef.x + 64, NetGameDef.y + LINEHEIGHT * ng_host_normal,
                      VPATCH_HANDLE(VPATCH_NAME(M_GAME)));

    V_DrawPatchDirect(NetGameDef.x + 63, NetGameDef.y + LINEHEIGHT * ng_host_deathmatch,
                      VPATCH_HANDLE(VPATCH_NAME(M_DTHMCH)));

    V_DrawPatchDirect(NetGameDef.x + 63, NetGameDef.y + LINEHEIGHT * ng_host_deathmatch2,
                      VPATCH_HANDLE(VPATCH_NAME(M_DTHMCH)));

    V_DrawPatchDirect(NetGameDef.x + 203, NetGameDef.y + LINEHEIGHT * ng_host_deathmatch2,
                      VPATCH_HANDLE(VPATCH_NAME(M_TWO)));

    V_DrawPatchDirect(NetGameDef.x + 56, NetGameDef.y + LINEHEIGHT * ng_join,
                      VPATCH_HANDLE(VPATCH_NAME(M_GAME)));
}

void M_DrawNetFoyer(void) {
    V_DrawPatchDirect(72, 15, VPATCH_HANDLE(VPATCH_NAME(M_NETWK)));
    // "Game" for Title
    V_DrawPatchDirect(177, 15,
                      VPATCH_HANDLE(VPATCH_NAME(M_GAME)));
    int x = 40;
    int y = 50;
    char buf[40];
    lobby_state_t ls;
    int pnum = piconet_get_lobby_state(&ls);
    if (ls.status == lobby_no_connection) {
        M_WriteText(46, y + LINEHEIGHT * 1, "There is no game to connect to.");
    } else if (ls.status == lobby_game_started) {
        if (!netgame) {
            if (pnum > 0) {
                M_NetGameStart(-1);
            } else {
                M_WriteText(50, y + LINEHEIGHT * 1, "The game has already started.");
            }
        }
    } else if (ls.status == lobby_game_not_compatible) {
        M_WriteText(29, y + LINEHEIGHT * 1, "The network game is not compatible");
        M_WriteText(70, y + LINEHEIGHT * 2, "with your WAD or client.");
    } else {
        for(int i=0;i<NET_MAXPLAYERS;i++) {
            sprintf(buf, "%d.", i+1);
            M_WriteText(x, y + i*LINEHEIGHT, buf);
            if (ls.players[i].client_id) {
                M_StringCopy(buf, ls.players[i].name, MAXPLAYERNAME);
                if (i == pnum) {
                    sprintf(buf, "%s (you)", player_name);
                } else if (netgame_choice == ng_join) {
                    if (!i) strcat(buf, " (host)");
                }
                M_WriteText(x+16, y + i*LINEHEIGHT, buf);
            }
        }
    }
    y += LINEHEIGHT * 5;
    if (netgame_choice != ng_join) {
        M_WriteText(61, y, "Press Enter to start game");
        y += LINEHEIGHT;
    }
    M_WriteText(85, y, "Press ESC to cancel");
}

void M_NetGame(int choice)
{
    M_SetupNextMenu(&NetGameDef);
}

void M_NetName(int choice)
{
    stringEntryBuffer = player_name;
    stringEntryIndex = strlen(player_name);
    M_StringCopy(stringEntryOldString, player_name, MAXPLAYERNAME);
    stringEntryMax = MAXPLAYERNAME;
    stringEntry = 1;
}

static bool M_CheckNetGame() {
    if (netgame) {
        M_StartMessage("you can't do this when already in a net game!\n\n"PRESSKEY, NULL, false);
        return false;
    }
    return true;
}

void M_HostGame(int choice)
{
    if (M_CheckNetGame()) {
        netgame_choice = choice;
        EpiDef.prevMenu = &NetGameDef;
        M_SetupNextMenu(&EpiDef);
    }
}

void M_JoinGame(int choice)
{
    if (M_CheckNetGame()) {
        netgame_choice = choice;
        M_SetupNextMenu(&NetFoyerDef);
#if USE_PICO_NET
        piconet_start_client();
#endif
    }
}

#endif

//
//      Toggle messages on/off
//
void M_ChangeMessages(int choice)
{
    // warning: unused parameter `int choice'
    choice = 0;
    showMessages = 1 - showMessages;
	
    if (!showMessages)
	players[consoleplayer].message = DEH_String(MSGOFF);
    else
	players[consoleplayer].message = DEH_String(MSGON);

    message_dontfuckwithme = true;
}


//
// M_EndGame
//
boolean M_EndGameResponse(int key)
{
    if (key != key_menu_confirm)
	return false;

#if USE_PICO_NET
    piconet_stop();
    net_client_connected = false;
    netgame = false;
#endif
    currentMenu->lastOn = itemOn;
    M_ClearMenus ();
    D_StartTitle ();
    return false;
}

void M_EndGame(int choice)
{
    choice = 0;
    if (!usergame)
    {
	S_StartUnpositionedSound( sfx_oof);
	return;
    }

#if !USE_PICO_NET
    if (netgame)
    {
	M_StartMessage(DEH_String(NETEND),NULL,false);
	return;
    }
#endif

    M_StartMessage(DEH_String(ENDGAME),M_EndGameResponse,true);
}




//
// M_ReadThis
//
void M_ReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef1);
}

void M_ReadThis2(int choice)
{
    choice = 0;
    M_SetupNextMenu(&ReadDef2);
}

void M_FinishReadThis(int choice)
{
    choice = 0;
    M_SetupNextMenu(&MainDef);
}




//
// M_QuitDOOM
//
static const sfxenum_t quitsounds[8] =
{
    sfx_pldeth,
    sfx_dmpain,
    sfx_popain,
    sfx_slop,
    sfx_telept,
    sfx_posit1,
    sfx_posit3,
    sfx_sgtatk
};

static const sfxenum_t quitsounds2[8] =
{
    sfx_vilact,
    sfx_getpow,
    sfx_boscub,
    sfx_slop,
    sfx_skeswg,
    sfx_kntdth,
    sfx_bspact,
    sfx_sgtatk
};



boolean M_QuitResponse(int key)
{
    if (key != key_menu_confirm)
	return false;
    if (!netgame)
    {
	if (gamemode == commercial)
	    S_StartUnpositionedSound( quitsounds2[(gametic>>2)&7]);
	else
	    S_StartUnpositionedSound( quitsounds[(gametic>>2)&7]);
#if !PICO_DOOM
	I_WaitVBL(105);
#endif
    }
#if !DOOM_TINY
    I_Quit ();
#else
    // quitting acquires a display frame semaphore, and we may be called from within pd_end_frame which
    // means we may not be able to get it, so punt to the main loop
    gameaction = ga_deferredquit;
#endif
    return false;
}


static const char *M_SelectEndMessage(void)
{
    const constcharstar *endmsg;

    if (logical_gamemission == doom)
    {
        // Doom 1

        endmsg = doom1_endmsg;
    }
    else
    {
        // Doom 2
        
        endmsg = doom2_endmsg;
    }

    return endmsg[gametic % NUM_QUITMESSAGES];
}


void M_QuitDOOM(int choice)
{
#if !DOOM_TINY
    DEH_snprintf(endstring, sizeof(endstring), "%s\n\n" DOSY,
                 DEH_String(M_SelectEndMessage()));

    M_StartMessage(endstring,M_QuitResponse,true);
#else
    // one less \n as M_StartMessage2 adds a newline between
    M_StartMessage2(M_SelectEndMessage(),M_QuitResponse,true, "\n" DOSY);
#endif
}

void M_ChangeSensitivity(int choice)
{
    switch(choice)
    {
      case 0:
	if (mouseSensitivity)
	    mouseSensitivity--;
	break;
      case 1:
	if (mouseSensitivity < 9)
	    mouseSensitivity++;
	break;
    }
}




void M_ChangeDetail(int choice)
{
    choice = 0;
    detailLevel = 1 - detailLevel;

    R_SetViewSize (screenblocks, detailLevel);

    if (!detailLevel)
	players[consoleplayer].message = DEH_String(DETAILHI);
    else
	players[consoleplayer].message = DEH_String(DETAILLO);
}




void M_SizeDisplay(int choice)
{
    switch(choice)
    {
      case 0:
	if (screenSize > 0)
	{
	    screenblocks--;
	    screenSize--;
	}
	break;
      case 1:
	if (screenSize < 8)
	{
	    screenblocks++;
	    screenSize++;
	}
	break;
    }
	

    R_SetViewSize (screenblocks, detailLevel);
}




//
//      Menu Functions
//
void
M_DrawThermo
( int	x,
  int	y,
  int	thermWidth,
  int	thermDot )
{
    int		xx;
    int		i;

    xx = x;
    V_DrawPatchDirect(xx, y, VPATCH_HANDLE(VPATCH_NAME(M_THERML)));
    xx += 8;
    if (thermWidth) {
#if !DOOM_TINY
        for (i=0;i<thermWidth;i++)
        {
            V_DrawPatchDirect (xx,y,VPATCH_HANDLE(VPATCH_NAME(M_THERMM)));
            xx += 8;
        }
#else
	    V_DrawPatchDirectN(xx, y, VPATCH_HANDLE(VPATCH_NAME(M_THERMM)), thermWidth-1);
	    xx += thermWidth * 8;
#endif
    }
    V_DrawPatchDirect(xx, y, VPATCH_HANDLE(VPATCH_NAME(M_THERMR)));

    V_DrawPatchDirect((x + 8) + thermDot * 8, y,
		      VPATCH_HANDLE(VPATCH_NAME(M_THERMO)));
}


void M_StartMessage2
( const char	*string,
  boolean (*routine)(int),
  boolean	input,
  const char *string2)
{
    messageLastMenuActive = menuactive;
    messageToPrint = 1;
    messageString = string;
    messageString2 = string2;
    messageRoutine = routine;
    messageNeedsInput = input;
    menuactive = true;
}

void M_StartMessage( const char	*string,
          boolean (*routine)(int),
          boolean	input )
{
    M_StartMessage2(string, routine, input, NULL);
}

//
// Find string width from hu_font chars
//
int M_StringWidth(const char *string)
{
    size_t             i;
    int             w = 0;
    int             c;
	
    for (i = 0;i < strlen(string);i++)
    {
	c = toupper(string[i]) - HU_FONTSTART;
	if (c < 0 || c >= HU_FONTSIZE)
	    w += 4;
	else
	    w += vpatch_width(resolve_vpatch_handle(vpatch_n(hu_font, c)));
    }
		
    return w;
}



//
//      Find string height from hu_font chars
//
int M_StringHeight(const char* string)
{
    size_t             i;
    int             h;
    int             height = vpatch_height(resolve_vpatch_handle(vpatch_n(hu_font, 0)));
	
    h = height;
    for (i = 0;i < strlen(string);i++)
	if (string[i] == '\n')
	    h += height;
		
    return h;
}


//
//      Write a string using the hu_font
//
void
M_WriteText
( int		x,
  int		y,
  const char *string)
{
    int		w;
    const char *ch;
    int		c;
    int		cx;
    int		cy;
		

    ch = string;
    cx = x;
    cy = y;
	
    while(1)
    {
	c = *ch++;
	if (!c)
	    break;
	if (c == '\n')
	{
	    cx = x;
	    cy += 12;
	    continue;
	}
		
	c = toupper(c) - HU_FONTSTART;
	if (c < 0 || c>= HU_FONTSIZE)
	{
	    cx += 4;
	    continue;
	}
		
	w = vpatch_width(resolve_vpatch_handle(vpatch_n(hu_font,c)));
	if (cx+w > SCREENWIDTH)
	    break;
	V_DrawPatchDirect(cx, cy, vpatch_n(hu_font, c));
	cx+=w;
    }
}

// These keys evaluate to a "null" key in Vanilla Doom that allows weird
// jumping in the menus. Preserve this behavior for accuracy.

static boolean IsNullKey(int key)
{
    return key == KEY_PAUSE || key == KEY_CAPSLOCK
        || key == KEY_SCRLCK || key == KEY_NUMLOCK;
}

//
// CONTROL PANEL
//

//
// M_Responder
//
boolean M_Responder (event_t* ev)
{
    int             ch;
    int             key;
    int             i;
    static  int     mousewait = 0;
    static  int     mousey = 0;
    static  int     lasty = 0;
    static  int     mousex = 0;
    static  int     lastx = 0;

    // In testcontrols mode, none of the function keys should do anything
    // - the only key is escape to quit.

    if (testcontrols)
    {
        if (ev->type == ev_quit
         || (ev->type == ev_keydown
          && (ev->data1 == key_menu_activate || ev->data1 == key_menu_quit)))
        {
            I_Quit();
            return true;
        }

        return false;
    }

    // "close" button pressed on window?
    if (ev->type == ev_quit)
    {
        // First click on close button = bring up quit confirm message.
        // Second click on close button = confirm quit

        if (menuactive && messageToPrint && messageRoutine == M_QuitResponse)
        {
            M_QuitResponse(key_menu_confirm);
        }
        else
        {
            S_StartUnpositionedSound( sfx_swtchn);
            M_QuitDOOM(0);
        }

        return true;
    }

    // key is the key pressed, ch is the actual character typed
  
    ch = 0;
    key = -1;

#if !NO_USE_JOYSTICK
    if (ev->type == ev_joystick)
    {
        // Simulate key presses from joystick events to interact with the menu.

        if (menuactive)
        {
            if (ev->data3 < 0)
            {
                key = key_menu_up;
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 > 0)
            {
                key = key_menu_down;
                joywait = I_GetTime() + 5;
            }
            if (ev->data2 < 0)
            {
                key = key_menu_left;
                joywait = I_GetTime() + 2;
            }
            else if (ev->data2 > 0)
            {
                key = key_menu_right;
                joywait = I_GetTime() + 2;
            }

#define JOY_BUTTON_MAPPED(x) ((x) >= 0)
#define JOY_BUTTON_PRESSED(x) (JOY_BUTTON_MAPPED(x) && (ev->data1 & (1 << (x))) != 0)

            if (JOY_BUTTON_PRESSED(joybfire))
            {
                // Simulate a 'Y' keypress when Doom show a Y/N dialog with Fire button.
                if (messageToPrint && messageNeedsInput)
                {
                    key = key_menu_confirm;
                }
                // Simulate pressing "Enter" when we are supplying a save slot name
                else if (stringEntry)
                {
                    key = KEY_ENTER;
                }
                else
                {
#if !NO_USE_SAVE
                    // if selecting a save slot via joypad, set a flag
                    if (currentMenu == &SaveDef)
                    {
                        joypadSave = true;
                    }
#endif
                    key = key_menu_forward;
                }
                joywait = I_GetTime() + 5;
            }
            if (JOY_BUTTON_PRESSED(joybuse))
            {
                // Simulate a 'N' keypress when Doom show a Y/N dialog with Use button.
                if (messageToPrint && messageNeedsInput)
                {
                    key = key_menu_abort;
                }
                // If user was entering a save name, back out
                else if (stringEntry)
                {
                    key = KEY_ESCAPE;
                }
                else
                {
                    key = key_menu_back;
                }
                joywait = I_GetTime() + 5;
            }
        }
        if (JOY_BUTTON_PRESSED(joybmenu))
        {
            key = key_menu_activate;
            joywait = I_GetTime() + 5;
        }
    }
    else
#endif
    {
#if !NO_USE_MOUSE
	if (ev->type == ev_mouse && mousewait < I_GetTime())
	{
	    mousey += ev->data3;
	    if (mousey < lasty-30)
	    {
		key = key_menu_down;
		mousewait = I_GetTime() + 5;
		mousey = lasty -= 30;
	    }
	    else if (mousey > lasty+30)
	    {
		key = key_menu_up;
		mousewait = I_GetTime() + 5;
		mousey = lasty += 30;
	    }
		
	    mousex += ev->data2;
	    if (mousex < lastx-30)
	    {
		key = key_menu_left;
		mousewait = I_GetTime() + 5;
		mousex = lastx -= 30;
	    }
	    else if (mousex > lastx+30)
	    {
		key = key_menu_right;
		mousewait = I_GetTime() + 5;
		mousex = lastx += 30;
	    }
		
	    if (ev->data1&1)
	    {
		key = key_menu_forward;
		mousewait = I_GetTime() + 15;
	    }
			
	    if (ev->data1&2)
	    {
		key = key_menu_back;
		mousewait = I_GetTime() + 15;
	    }
	}
	else
#endif
	{
	    if (ev->type == ev_keydown)
	    {
		key = ev->data1;
		ch = ev->data2;
	    }
	}
    }
    
    if (key == -1)
	return false;

#if !NO_USE_SAVE
    // Save Game string input
    if (stringEntry)
    {
	switch(key)
	{
	  case KEY_BACKSPACE:
	    if (stringEntryIndex > 0)
	    {
		stringEntryIndex--;
		stringEntryBuffer[stringEntryIndex] = 0;
	    }
	    break;

          case KEY_ESCAPE:
              stringEntry = 0;
            I_StopTextInput();
            M_StringCopy(stringEntryBuffer, stringEntryOldString,
                         stringEntryMax);
            break;

	  case KEY_ENTER:
          stringEntry = 0;
            I_StopTextInput();
            M_StringEntryEnd();
	    break;

	  default:
            // Savegame name entry. This is complicated.
            // Vanilla has a bug where the shift key is ignored when entering
            // a savegame name. If vanilla_keyboard_mapping is on, we want
            // to emulate this bug by using ev->data1. But if it's turned off,
            // it implies the user doesn't care about Vanilla emulation:
            // instead, use ev->data3 which gives the fully-translated and
            // modified key input.

            if (ev->type != ev_keydown)
            {
                break;
            }
            if (vanilla_keyboard_mapping)
            {
                ch = ev->data1;
            }
            else
            {
                ch = ev->data3;
            }

            ch = toupper(ch);

            if (ch != ' '
             && (ch - HU_FONTSTART < 0 || ch - HU_FONTSTART >= HU_FONTSIZE))
            {
                break;
            }

	    if (ch >= 32 && ch <= 127 &&
            stringEntryIndex < stringEntryMax - 1 &&
		M_StringWidth(stringEntryBuffer) <
		(stringEntryMax-2)*8)
	    {
		stringEntryBuffer[stringEntryIndex++] = ch;
		stringEntryBuffer[stringEntryIndex] = 0;
	    }
	    break;
	}
	return true;
    }
#endif
    // Take care of any messages that need input
    if (messageToPrint)
    {
	if (messageNeedsInput)
        {
            if (key != ' ' && key != KEY_ESCAPE
             && key != key_menu_confirm && key != key_menu_abort)
            {
                return false;
            }
	}

	menuactive = messageLastMenuActive;
	messageToPrint = 0;
	if (messageRoutine) {
        menuactive = messageRoutine(key);
    } else {
        menuactive = false;
    }
	S_StartUnpositionedSound( sfx_swtchx);
	return true;
    }

#if !NO_SCREENSHOT
    if ((devparm && key == key_menu_help) ||
        (key != 0 && key == key_menu_screenshot))
    {
	G_ScreenShot ();
	return true;
    }
#endif

    // F-Keys
    if (!menuactive)
    {
#if !DOOM_TINY
	if (key == key_menu_decscreen)      // Screen size down
        {
	    if (automapactive || chat_on)
		return false;
	    M_SizeDisplay(0);
	    S_StartUnpositionedSound( sfx_stnmov);
	    return true;
	}
        else if (key == key_menu_incscreen) // Screen size up
        {
	    if (automapactive || chat_on)
		return false;
	    M_SizeDisplay(1);
	    S_StartUnpositionedSound( sfx_stnmov);
	    return true;
	}
        else
#endif
        if (key == key_menu_help)     // Help key
        {
	    M_StartControlPanel ();

	    if (gameversion >= exe_ultimate)
	      currentMenu = &ReadDef2;
	    else
	      currentMenu = &ReadDef1;

	    itemOn = 0;
	    S_StartUnpositionedSound( sfx_swtchn);
	    return true;
	}
#if !NO_USE_SAVE
        else if (key == key_menu_save)     // Save
        {
	    M_StartControlPanel();
	    S_StartUnpositionedSound( sfx_swtchn);
	    M_SaveGame(0);
	    return true;
        }
#endif
#if !NO_USE_LOAD
        else if (key == key_menu_load)     // Load
        {
	    M_StartControlPanel();
	    S_StartUnpositionedSound( sfx_swtchn);
	    M_LoadGame(0);
	    return true;
        }
#endif
        else if (key == key_menu_volume)   // Sound Volume
        {
	    M_StartControlPanel ();
	    currentMenu = &SoundDef;
	    itemOn = sfx_vol;
	    S_StartUnpositionedSound( sfx_swtchn);
	    return true;
	}
#if !DOOM_TINY
        else if (key == key_menu_detail)   // Detail toggle
        {
	    M_ChangeDetail(0);
	    S_StartUnpositionedSound( sfx_swtchn);
	    return true;
        }
#endif
#if !NO_USE_SAVE
        else if (key == key_menu_qsave)    // Quicksave
        {
	    S_StartUnpositionedSound( sfx_swtchn);
	    M_QuickSave();
	    return true;
        }
#endif
        else if (key == key_menu_endgame)  // End game
        {
	    S_StartUnpositionedSound( sfx_swtchn);
	    M_EndGame(0);
	    return true;
        }
        else if (key == key_menu_messages) // Toggle messages
        {
	    M_ChangeMessages(0);
	    S_StartUnpositionedSound( sfx_swtchn);
	    return true;
        }
#if !NO_USE_LOAD
        else if (key == key_menu_qload)    // Quickload
        {
	    S_StartUnpositionedSound( sfx_swtchn);
	    M_QuickLoad();
	    return true;
        }
#endif
        else if (key == key_menu_quit)     // Quit DOOM
        {
	    S_StartUnpositionedSound( sfx_swtchn);
	    M_QuitDOOM(0);
	    return true;
        }
        else if (key == key_menu_gamma)    // gamma toggle
        {
	    usegamma++;
	    if (usegamma > 4)
		usegamma = 0;
	    players[consoleplayer].message = DEH_String(gammamsg[usegamma]);
#if !USE_WHD
            I_SetPalette (W_CacheLumpName (DEH_String("PLAYPAL"),PU_CACHE));
#else
        I_SetPaletteNum(0);
#endif
	    return true;
	}
    }

    // Pop-up menu?
    if (!menuactive)
    {
	if (key == key_menu_activate)
	{
	    M_StartControlPanel ();
	    S_StartUnpositionedSound( sfx_swtchn);
	    return true;
	}
	return false;
    }

    // Keys usable within menu

    if (key == key_menu_down)
    {
        // Move down to next item

        do
	{
	    if (itemOn+1 > currentMenu->numitems-1)
		itemOn = 0;
	    else itemOn++;
	    S_StartUnpositionedSound( sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);

	return true;
    }
    else if (key == key_menu_up)
    {
        // Move back up to previous item

	do
	{
	    if (!itemOn)
		itemOn = currentMenu->numitems-1;
	    else itemOn--;
	    S_StartUnpositionedSound( sfx_pstop);
	} while(currentMenu->menuitems[itemOn].status==-1);

	return true;
    }
    else if (key == key_menu_left)
    {
        // Slide slider left

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartUnpositionedSound( sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(0);
	}
	return true;
    }
    else if (key == key_menu_right)
    {
        // Slide slider right

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status == 2)
	{
	    S_StartUnpositionedSound( sfx_stnmov);
	    currentMenu->menuitems[itemOn].routine(1);
	}
	return true;
    }
    else if (key == key_menu_forward)
    {
        // Activate menu item

	if (currentMenu->menuitems[itemOn].routine &&
	    currentMenu->menuitems[itemOn].status)
	{
	    currentMenu->lastOn = itemOn;
	    if (currentMenu->menuitems[itemOn].status == 2)
	    {
		currentMenu->menuitems[itemOn].routine(1);      // right arrow
		S_StartUnpositionedSound( sfx_stnmov);
	    }
	    else
	    {
		currentMenu->menuitems[itemOn].routine(itemOn);
		S_StartUnpositionedSound( sfx_pistol);
	    }
	}
	return true;
    }
    else if (key == key_menu_activate)
    {
        // Deactivate menu

	currentMenu->lastOn = itemOn;
	M_ClearMenus ();
	S_StartUnpositionedSound( sfx_swtchx);
	return true;
    }
    else if (key == key_menu_back)
    {
        // Go back to previous menu

	currentMenu->lastOn = itemOn;
	if (currentMenu->prevMenu)
	{
	    currentMenu = currentMenu->prevMenu;
	    itemOn = currentMenu->lastOn;
	    S_StartUnpositionedSound( sfx_swtchn);
	}
	return true;
    }

    // Keyboard shortcut?
    // Vanilla Doom has a weird behavior where it jumps to the scroll bars
    // when the certain keys are pressed, so emulate this.

    else if (ch != 0 || IsNullKey(key))
    {
	for (i = itemOn+1;i < currentMenu->numitems;i++)
        {
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartUnpositionedSound( sfx_pstop);
		return true;
	    }
        }

	for (i = 0;i <= itemOn;i++)
        {
	    if (currentMenu->menuitems[i].alphaKey == ch)
	    {
		itemOn = i;
		S_StartUnpositionedSound( sfx_pstop);
		return true;
	    }
        }
    }

    return false;
}



//
// M_StartControlPanel
//
void M_StartControlPanel (void)
{
    // intro might call this repeatedly
    if (menuactive)
	return;
    
    menuactive = 1;
    currentMenu = &MainDef;         // JDC
    itemOn = currentMenu->lastOn;   // JDC
}

// Display OPL debug messages - hack for GENMIDI development.

static void M_DrawOPLDev(void)
{
    extern void I_OPL_DevMessages(char *, size_t);
    char debug[1024];
    char *curr, *p;
    int line;

    I_OPL_DevMessages(debug, sizeof(debug));
    curr = debug;
    line = 0;

    for (;;)
    {
        p = strchr(curr, '\n');

        if (p != NULL)
        {
            *p = '\0';
        }

        M_WriteText(0, line * 8, curr);
        ++line;

        if (p == NULL)
        {
            break;
        }

        curr = p + 1;
    }
}

//
// M_Drawer
// Called after the view has been rendered,
// but before it has been blitted.
//
void M_Drawer (void)
{
#if !NO_DRAW_MENU
    static short	x;
    static short	y;
    unsigned int	i;
    unsigned int	max;
    char		string[80];
    const char          *name;
    int			start;

#if DOOM_TINY
    int oldinhelpscreens = inhelpscreens;
#endif
    inhelpscreens = 0;
    
    // Horiz. & Vertically center string and print it.
    if (messageToPrint)
    {
	start = 0;
	y = SCREENHEIGHT/2 - (M_StringHeight(messageString) + (messageString2?M_StringHeight(messageString2):0)) / 2;
    const char *current_message = messageString;
	while (current_message[start] != '\0')
	{
	    boolean foundnewline = false;

            for (i = 0; current_message[start + i] != '\0'; i++)
            {
                if (current_message[start + i] == '\n')
                {
                    M_StringCopy(string, current_message + start,
                                 sizeof(string));
                    if (i < sizeof(string))
                    {
                        string[i] = '\0';
                    }

                    foundnewline = true;
                    start += i + 1;
                    break;
                }
            }

            if (!foundnewline)
            {
                M_StringCopy(string, current_message + start, sizeof(string));
                start += strlen(string);
            }

	    x = SCREENWIDTH/2 - M_StringWidth(string) / 2;
	    M_WriteText(x, y, string);
	    y += vpatch_height(resolve_vpatch_handle(vpatch_n(hu_font, 0)));

        // we can move from messageString to messageString2 between lines only
        if (messageString[start] == '\0' && messageString2 && current_message == messageString) {
            current_message = messageString2;
            start = 0;
        }
    }

	return;
    }

    if (opldev)
    {
        M_DrawOPLDev();
    }

    if (!menuactive)
	return;

    if (currentMenu->routine)
	currentMenu->routine();         // call Draw routine
    
    // DRAW MENU
    x = currentMenu->x;
    y = currentMenu->y;
    max = currentMenu->numitems;

    for (i=0;i<max;i++)
    {
#if !USE_WHD
        name = DEH_String(currentMenu->menuitems[i].name);

	if (name[0])
	{
	    V_DrawPatchDirect (x, y, W_CacheLumpName(name, PU_CACHE));
	}
#else
        if (currentMenu->menuitems[i].name) {
            V_DrawPatchDirect (x, y, currentMenu->menuitems[i].name);
        }
#endif
	y += LINEHEIGHT;
    }

    
    // DRAW SKULL
#if DOOM_TINY
    // we don't want to draw skull here immediately as it is split across screens, and whilst we could clip, it is simpler
    // to wait for one more frame and draw it to an overlay
    if (inhelpscreens) {
        if (!oldinhelpscreens) return;
    }
#endif
    if (x >= 0)
        V_DrawPatchDirect(x + SKULLXOFF, currentMenu->y - 5 + itemOn*LINEHEIGHT,
                             VPATCH_HANDLE(skullName[whichSkull]));
    #endif
}


//
// M_ClearMenus
//
void M_ClearMenus (void)
{
    menuactive = 0;
    // if (!netgame && usergame && paused)
    //       sendpause = true;
}




//
// M_SetupNextMenu
//
void M_SetupNextMenu(menu_t *menudef)
{
    currentMenu = menudef;
    itemOn = currentMenu->lastOn;
}


//
// M_Ticker
//
void M_Ticker (void)
{
#if NET_MENU
#if 0
    static boolean launched;
    if (!launched) {
        printf("LOOPo\n");
        launched = true;
        menuactive = 1;
        M_JoinGame(ng_join);
    }
#endif

    if (menuactive && currentMenu == &NetFoyerDef) {
#if USE_PICO_NET
        static uint8_t repeat;
        if (++repeat == 35) {
            piconet_client_check_for_dropped_connection();
            repeat = 0;
        }
#endif
    } else {
#if USE_PICO_NET
        // todo obviously don't stop if we're in game
        if (!netgame) piconet_stop();
#endif
    }
#endif
    if (--skullAnimCounter <= 0)
    {
	whichSkull ^= 1;
	skullAnimCounter = 8;
    }
}


//
// M_Init
//
void M_Init (void)
{
    currentMenu = &MainDef;
    itemOn = currentMenu->lastOn;
    skullAnimCounter = 10;
    screenSize = screenblocks - 3;
    messageLastMenuActive = menuactive;
    quickSaveSlot = -1;
#if !DOOM_TINY // already 0
    menuactive = 0;
    whichSkull = 0;
    messageToPrint = 0;
    messageString = NULL;
    messageString2 = NULL;
#endif
#if NET_MENU
    strcpy(player_name, DEFAULTPLAYERNAME);
#endif

    // Here we could catch other version dependencies,
    //  like HELP1/2, and four episodes.

    // The same hacks were used in the original Doom EXEs.

    if (gameversion >= exe_ultimate)
    {
        MainMenu[readthis].routine = M_ReadThis2;
        ReadDef2.prevMenu = NULL;
    }

    if (gameversion >= exe_final && gameversion <= exe_final2)
    {
        ReadDef2.routine = M_DrawReadThisCommercial;
    }

    if (gamemode == commercial)
    {
        MainMenu[readthis] = MainMenu[quitdoom];
        MainDef.numitems--;
        MainDef.y += 8;
        NewDef.prevMenu = &MainDef;
        ReadDef1.routine = M_DrawReadThisCommercial;
        ReadDef1.x = 330;
        ReadDef1.y = 165;
        ReadMenu1[rdthsempty1].routine = M_FinishReadThis;
    }

    // Versions of doom.exe before the Ultimate Doom release only had
    // three episodes; if we're emulating one of those then don't try
    // to show episode four. If we are, then do show episode four
    // (should crash if missing).
    if (gameversion < exe_ultimate)
    {
        EpiDef.numitems--;
    }
    // chex.exe shows only one episode.
    else if (gameversion_is_chex(gameversion))
    {
        EpiDef.numitems = 1;
    }

#if !DOOM_TINY
    opldev = M_CheckParm("-opldev") > 0;
#endif
}

