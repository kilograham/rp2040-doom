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

#include <stdio.h>
#include <stdlib.h>

#include "i_sound.h"
#include "i_system.h"

#include "deh_str.h"

#include "doomstat.h"
#include "doomtype.h"

#include "sounds.h"
#include "s_sound.h"

#include "m_misc.h"
#include "m_random.h"
#include "m_argv.h"

#include "p_local.h"
#include "w_wad.h"
#include "z_zone.h"

#if INCLUDE_SOUND_C_IN_S_SOUND
#undef INCLUDE_SOUND_C_IN_S_SOUND
#include "sounds.c"
#endif

// when to clip out sounds
// Does not fit the large outdoor areas.

#define S_CLIPPING_DIST (1200 * FRACUNIT)

// Distance tp origin when sounds should be maxed out.
// This should relate to movement clipping resolution
// (see BLOCKMAP handling).
// In the source code release: (160*FRACUNIT).  Changed back to the
// Vanilla value of 200 (why was this changed?)

#define S_CLOSE_DIST (200 * FRACUNIT)

// The range over which sound attenuates

#define S_ATTENUATOR ((S_CLIPPING_DIST - S_CLOSE_DIST) >> FRACBITS)

// Stereo separation

#define S_STEREO_SWING (96 * FRACUNIT)

#define NORM_PRIORITY 64
#define NORM_SEP 128

// todo graham this could be half the size or so, there are 8 of them
typedef struct
{
    // sound information (if null, channel avail.)
    should_be_const sfxinfo_t *sfxinfo;

    // origin of sound
    xy_positioned_t *origin;

    // handle of the sound being played
    int handle;

    int pitch;

} channel_t;

// The set of channels available

static channel_t *channels;

// Maximum volume of a sound effect.
// Internal default is max out of 0-15.

int sfxVolume = 8;

// Maximum volume of music.

int musicVolume = 8;

// Internal volume level, ranging from 0-127

static int snd_SfxVolume;

// Whether songs are mus_paused

static boolean mus_paused;

// Music currently being played

static musicinfo_t *mus_playing = NULL;

// Number of channels to use

int snd_channels = 8;

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void S_Init(int sfxVolume, int musicVolume)
{
    int i;

    if (gameversion == exe_doom_1_666)
    {
        if (logical_gamemission == doom)
        {
            I_SetOPLDriverVer(opl_doom1_1_666);
        }
        else
        {
            I_SetOPLDriverVer(opl_doom2_1_666);
        }
    }
    else
    {
        I_SetOPLDriverVer(opl_doom_1_9);
    }

    I_PrecacheSounds(S_sfx, NUMSFX);

    S_SetSfxVolume(sfxVolume);
    S_SetMusicVolume(musicVolume);

    // Allocating the internal channels for mixing
    // (the maximum numer of sounds rendered
    // simultaneously) within zone memory.
    channels = Z_Malloc(snd_channels*sizeof(channel_t), PU_STATIC, 0);

    // Free all channels for use
    for (i=0 ; i<snd_channels ; i++)
    {
        channels[i].sfxinfo = 0;
    }

    // no sounds are playing, and they are not mus_paused
    mus_paused = 0;

    // Note that sounds have not been cached (yet).
    for (i=1 ; i<NUMSFX ; i++)
    {
#if !USE_CONST_SFX
        S_sfx[i].lumpnum = S_sfx[i].usefulness = -1;
#else
        S_sfx_mut[i].lumpnum = S_sfx_mut[i].usefulness = -1;
#endif
    }

    // Doom defaults to pitch-shifting off.
    if (snd_pitchshift == -1)
    {
        snd_pitchshift = 0;
    }

#if !NO_USE_EXIT
    I_AtExit(S_Shutdown, true);
#endif
}

#if !NO_USE_EXIT
void S_Shutdown(void)
{
    I_ShutdownSound();
    I_ShutdownMusic();
}
#endif

static void S_StopChannel(int cnum)
{
    int i;
    channel_t *c;

    c = &channels[cnum];

    if (c->sfxinfo)
    {
        // stop the sound playing

        if (I_SoundIsPlaying(c->handle))
        {
            I_StopSound(c->handle);
        }

        // check to see if other channels are playing the sound

        for (i=0; i<snd_channels; i++)
        {
            if (cnum != i && c->sfxinfo == channels[i].sfxinfo)
            {
                break;
            }
        }

        // degrade usefulness of sound data

        sfx_mut(c->sfxinfo)->usefulness--;
        c->sfxinfo = NULL;
        c->origin = NULL;
    }
}

//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//

void S_Start(void)
{
    int cnum;
    int mnum;

    // kill all playing sounds at start of level
    //  (trust me - a good idea)
    for (cnum=0 ; cnum<snd_channels ; cnum++)
    {
        if (channels[cnum].sfxinfo)
        {
            S_StopChannel(cnum);
        }
    }

    // start new music for the level
    mus_paused = 0;

    if (gamemode == commercial)
    {
        mnum = mus_runnin + gamemap - 1;
    }
    else
    {
        int spmus[]=
        {
            // Song - Who? - Where?

            mus_e3m4,        // American     e4m1
            mus_e3m2,        // Romero       e4m2
            mus_e3m3,        // Shawn        e4m3
            mus_e1m5,        // American     e4m4
            mus_e2m7,        // Tim          e4m5
            mus_e2m4,        // Romero       e4m6
            mus_e2m6,        // J.Anderson   e4m7 CHIRON.WAD
            mus_e2m5,        // Shawn        e4m8
            mus_e1m9,        // Tim          e4m9
        };

        if (gameepisode < 4)
        {
            mnum = mus_e1m1 + (gameepisode-1)*9 + gamemap-1;
        }
        else
        {
            mnum = spmus[gamemap-1];
        }
    }

    S_ChangeMusic(mnum, true);
}

void S_StopSound(xy_positioned_t  *origin)
{
    int cnum;

    for (cnum=0 ; cnum<snd_channels ; cnum++)
    {
        if (channels[cnum].sfxinfo && channels[cnum].origin == origin)
        {
            S_StopChannel(cnum);
            break;
        }
    }
}

//
// S_GetChannel :
//   If none available, return -1.  Otherwise channel #.
//

static int S_GetChannel(xy_positioned_t *origin, should_be_const sfxinfo_t *sfxinfo)
{
    // channel number to use
    int                cnum;

    channel_t*        c;

    // Find an open channel
    for (cnum=0 ; cnum<snd_channels ; cnum++)
    {
        if (!channels[cnum].sfxinfo)
        {
            break;
        }
        else if (origin && channels[cnum].origin == origin)
        {
            S_StopChannel(cnum);
            break;
        }
    }

    // None available
    if (cnum == snd_channels)
    {
        // Look for lower priority
        for (cnum=0 ; cnum<snd_channels ; cnum++)
        {
            if (channels[cnum].sfxinfo->priority >= sfxinfo->priority)
            {
                break;
            }
        }

        if (cnum == snd_channels)
        {
            // FUCK!  No lower priority.  Sorry, Charlie.
            return -1;
        }
        else
        {
            // Otherwise, kick out lower priority.
            S_StopChannel(cnum);
        }
    }

    c = &channels[cnum];

    // channel is decided to be cnum.
    c->sfxinfo = sfxinfo;
    c->origin = origin;

    return cnum;
}

//
// Changes volume and stereo-separation variables
//  from the norm of a sound effect to be played.
// If the sound is not audible, returns a 0.
// Otherwise, modifies parameters and returns 1.
//

static int S_AdjustSoundParams(mobj_t *listener, xy_positioned_t *source,
                               int *vol, int *sep)
{
    fixed_t        approx_dist;
    fixed_t        adx;
    fixed_t        ady;
    angle_t        angle;

    // calculate the distance to sound origin
    //  and clip it if necessary
    adx = abs(listener->xy.x - source->x);
    ady = abs(listener->xy.y - source->y);

    // From _GG1_ p.428. Appox. eucledian distance fast.
    approx_dist = adx + ady - ((adx < ady ? adx : ady)>>1);

    if (gamemap != 8 && approx_dist > S_CLIPPING_DIST)
    {
        return 0;
    }

    // angle of source to listener
    angle = R_PointToAngle2(listener->xy.x,
                            listener->xy.y,
                            source->x,
                            source->y);

    if (angle > mobj_full(listener)->angle)
    {
        angle = angle - mobj_full(listener)->angle;
    }
    else
    {
        angle = angle + (0xffffffff - mobj_full(listener)->angle);
    }

    angle >>= ANGLETOFINESHIFT;

    // stereo separation
    *sep = 128 - (FixedMul(S_STEREO_SWING, finesine(angle)) >> FRACBITS);

    // volume calculation
    if (approx_dist < S_CLOSE_DIST)
    {
        *vol = snd_SfxVolume;
    }
    else if (gamemap == 8)
    {
        if (approx_dist > S_CLIPPING_DIST)
        {
            approx_dist = S_CLIPPING_DIST;
        }

        *vol = 15+ ((snd_SfxVolume-15)
                    *((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
            / S_ATTENUATOR;
    }
    else
    {
        // distance effect
        *vol = (snd_SfxVolume
                * ((S_CLIPPING_DIST - approx_dist)>>FRACBITS))
            / S_ATTENUATOR;
    }

    return (*vol > 0);
}

// clamp supplied integer to the range 0 <= x <= 255.

static int Clamp(int x)
{
    if (x < 0)
    {
        return 0;
    }
    else if (x > 255)
    {
        return 255;
    }
    return x;
}

void S_StartObjSound(mobj_t *obj, int sfx_id) {
    S_StartSound(obj ? &obj->xy : NULL, sfx_id);
}

void S_StopObjSound(mobj_t *obj) {
    S_StopSound(obj ? &obj->xy : NULL);
}

void S_StartUnpositionedSound(int sfx_id) {
    S_StartSound(NULL, sfx_id);
}

void S_StartSound(xy_positioned_t *origin, int sfx_id)
{
    should_be_const sfxinfo_t *sfx;
    int rc;
    int sep;
    int pitch;
    int cnum;
    int volume;

    volume = snd_SfxVolume;

    // check for bogus sound #
    if (sfx_id < 1 || sfx_id > NUMSFX)
    {
        I_Error("Bad sfx #: %d", sfx_id);
    }

    sfx = &S_sfx[sfx_id];
//    printf("Start sound %d %s\n", sfx_id, DEH_String(sfx->name));

    // Initialize sound parameters
    pitch = NORM_PITCH;
    if (sfx->link)
    {
        volume += sfx->volume;
        pitch = sfx->pitch;

        if (volume < 1)
        {
            return;
        }

        if (volume > snd_SfxVolume)
        {
            volume = snd_SfxVolume;
        }
    }


    // Check to see if it is audible,
    //  and if not, modify the params
    if (origin && origin != &players[consoleplayer].mo->xy)
    {
        rc = S_AdjustSoundParams(players[consoleplayer].mo,
                                 origin,
                                 &volume,
                                 &sep);

        if (origin->x == players[consoleplayer].mo->xy.x
         && origin->y == players[consoleplayer].mo->xy.y)
        {
            sep = NORM_SEP;
        }

        if (!rc)
        {
            return;
        }
    }
    else
    {
        sep = NORM_SEP;
    }

    // hacks to vary the sfx pitches
    if (sfx_id >= sfx_sawup && sfx_id <= sfx_sawhit)
    {
        pitch += 8 - (M_Random()&15);
    }
    else if (sfx_id != sfx_itemup && sfx_id != sfx_tink)
    {
        pitch += 16 - (M_Random()&31);
    }
    pitch = Clamp(pitch);

    // kill old sound
    S_StopSound(origin);

    // try to find a channel
    cnum = S_GetChannel(origin, sfx);

    if (cnum < 0)
    {
        return;
    }

    // increase the usefulness
    if (sfx_mut(sfx)->usefulness++ < 0)
    {
        sfx_mut(sfx)->usefulness = 1;
    }

    if (sfx_mut(sfx)->lumpnum < 0)
    {
        sfx_mut(sfx)->lumpnum = I_GetSfxLumpNum(sfx);
    }

    channels[cnum].pitch = pitch;
    channels[cnum].handle = I_StartSound(sfx, cnum, volume, sep, channels[cnum].pitch);
}

//
// Stop and resume music, during game PAUSE.
//

void S_PauseSound(void)
{
    if (mus_playing && !mus_paused)
    {
        I_PauseSong();
        mus_paused = true;
    }
}

void S_ResumeSound(void)
{
    if (mus_playing && mus_paused)
    {
        I_ResumeSong();
        mus_paused = false;
    }
}

//
// Updates music & sounds
//

void S_UpdateSounds(mobj_t *listener)
{
    int                audible;
    int                cnum;
    int                volume;
    int                sep;
    should_be_const sfxinfo_t*        sfx;
    channel_t*        c;

    I_UpdateSound();

    for (cnum=0; cnum<snd_channels; cnum++)
    {
        c = &channels[cnum];
        sfx = c->sfxinfo;

        if (c->sfxinfo)
        {
            if (I_SoundIsPlaying(c->handle))
            {
                // initialize parameters
                volume = snd_SfxVolume;
                sep = NORM_SEP;

                if (sfx->link)
                {
                    volume += sfx->volume;
                    if (volume < 1)
                    {
                        S_StopChannel(cnum);
                        continue;
                    }
                    else if (volume > snd_SfxVolume)
                    {
                        volume = snd_SfxVolume;
                    }
                }

                // check non-local sounds for distance clipping
                //  or modify their params
                if (c->origin && &listener->xy != c->origin)
                {
                    audible = S_AdjustSoundParams(listener,
                                                  c->origin,
                                                  &volume,
                                                  &sep);

                    if (!audible)
                    {
                        S_StopChannel(cnum);
                    }
                    else
                    {
                        I_UpdateSoundParams(c->handle, volume, sep);
                    }
                }
            }
            else
            {
                // if channel is allocated but sound has stopped,
                //  free it
                S_StopChannel(cnum);
            }
        }
    }
}

void S_SetMusicVolume(int volume)
{
    if (volume < 0 || volume > 127)
    {
        I_Error("Attempt to set music volume at %d",
                volume);
    }

    I_SetMusicVolume(volume);
}

void S_SetSfxVolume(int volume)
{
    if (volume < 0 || volume > 127)
    {
        I_Error("Attempt to set sfx volume at %d", volume);
    }

    snd_SfxVolume = volume;
}

//
// Starts some music with the music id found in sounds.h.
//

void S_StartMusic(int m_id)
{
    S_ChangeMusic(m_id, false);
}

void S_ChangeMusic(int musicnum, int looping)
{
    musicinfo_t *music = NULL;
    char namebuf[9];
    void *handle;

    // The Doom IWAD file has two versions of the intro music: d_intro
    // and d_introa.  The latter is used for OPL playback.

    if (musicnum == mus_intro && (snd_musicdevice == SNDDEVICE_ADLIB
                               || snd_musicdevice == SNDDEVICE_SB))
    {
        musicnum = mus_introa;
    }

    if (musicnum <= mus_None || musicnum >= NUMMUSIC)
    {
        I_Error("Bad music number %d", musicnum);
    }
    else
    {
        music = &S_music[musicnum];
    }

    if (mus_playing == music)
    {
        return;
    }

    // shutdown old music
    S_StopMusic();

    // get lumpnum if neccessary
    if (!music->lumpnum)
    {
        M_snprintf(namebuf, sizeof(namebuf), "d_%s", DEH_String(music->name));
        music->lumpnum = W_GetNumForName(namebuf);
    }

#if !DOOM_SMALL
    music->data = W_CacheLumpNum(music->lumpnum, PU_STATIC);
    handle = I_RegisterSong(music->data, W_LumpLength(music->lumpnum));
#else
    handle = I_RegisterSong(W_CacheLumpNum(music->lumpnum, PU_STATIC), W_LumpLength(music->lumpnum));
#endif
    music->handle = handle;
    I_PlaySong(handle, looping);

    mus_playing = music;
}

boolean S_MusicPlaying(void)
{
    return I_MusicIsPlaying();
}

void S_StopMusic(void)
{
    if (mus_playing)
    {
        if (mus_paused)
        {
            I_ResumeSong();
        }

        I_StopSong();
        I_UnRegisterSong(mus_playing->handle);
        W_ReleaseLumpNum(mus_playing->lumpnum);
#if !DOOM_SMALL
        mus_playing->data = NULL;
#endif
        mus_playing = NULL;
    }
}

#if 1
void test_next_sound() {
    static int snd_idx = 0;
    do {
        char buf[10];
        snd_idx++;
        if (snd_idx == NUMSFX) snd_idx = 1;
        should_be_const sfxinfo_t *sfx = &S_sfx[snd_idx];
        // Linked sfx lumps? Get the lump number for the sound linked to.
        if (sfx->link != NULL)
        {
            sfx = sfx->link;
        }
        M_snprintf(buf, sizeof(buf), "ds%s", DEH_String(sfx->name));
        if (W_CheckNumForName(buf) > 0) {
            break;
        }
    } while (true);
    S_StartUnpositionedSound(snd_idx);
}
#endif

