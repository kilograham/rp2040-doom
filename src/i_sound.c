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

#if PICO_BUILD
#include "i_picosound.h"
#endif

//#include "SDL_mixer.h"

#include "config.h"
#include "doomtype.h"

#include "gusconf.h"
#include "i_sound.h"
#include "i_video.h"
#include "m_argv.h"
#include "m_config.h"

// Sound sample rate to use for digital output (Hz)
#if !DOOM_TINY

int snd_samplerate = 44100;

// Maximum number of bytes to dedicate to allocated sound effects.
// (Default: 64MB)

int snd_cachesize = 64 * 1024 * 1024;

// Config variable that controls the sound buffer size.
// We default to 28ms (1000 / 35fps = 1 buffer per tic).

int snd_maxslicetime_ms = 28;

// External command to invoke to play back music.

should_be_const constcharstar snd_musiccmd = "";
#endif

// Whether to vary the pitch of sound effects
// Each game will set the default differently

isb_int8_t snd_pitchshift = -1;

#if !DOOM_TINY
int snd_musicdevice = SNDDEVICE_SB;
int snd_sfxdevice = SNDDEVICE_SB;
#endif

// Low-level sound and music modules we are using
static const sound_module_t *sound_module;
static const music_module_t *music_module;

// If true, the music pack module was successfully initialized.
#if !NO_USE_MUSIC_PACKS
static boolean music_packs_active = false;

// This is either equal to music_module or &music_pack_module,
// depending on whether the current track is substituted.
static const music_module_t *active_music_module;
#else
#define active_music_module music_module
#endif

// Sound modules

#if !NO_USE_TIMIDITY
extern void I_InitTimidityConfig(void);
#endif
extern sound_module_t sound_sdl_module;
extern sound_module_t sound_pcsound_module;
extern music_module_t music_sdl_module;
extern const music_module_t music_opl_module;
extern music_module_t music_pack_module;
#if PICO_BUILD
extern sound_module_t sound_pico_module;
#endif

// For OPL module:
#if !DOOM_SMALL
extern opl_driver_ver_t opl_drv_ver;
extern int opl_io_port;
#endif

// For native music module:

#if !NO_USE_MUSIC_PACKS
extern constcharstar music_pack_path;
#endif
#if !NO_USE_TIMIDITY
extern constcharstar timidity_cfg_path;
#endif

// DOS-specific options: These are unused but should be maintained
// so that the config file can be shared between chocolate
// doom and doom.exe

#if !DOOM_SMALL
static int snd_sbport = 0;
static int snd_sbirq = 0;
static int snd_sbdma = 0;
static int snd_mport = 0;
#endif

// Compiled-in sound modules:

static sound_module_t *sound_modules[] =
{
#if PICO_BUILD
    &sound_pico_module,
#else
    &sound_sdl_module,
    &sound_pcsound_module,
#endif
    NULL,
};

// Compiled-in music modules:

static const music_module_t *music_modules[] =
{
#if !PICO_BUILD
    &music_sdl_module,
#endif
    &music_opl_module,
    NULL,
};

// Check if a sound device is in the given list of devices

static boolean SndDeviceInList(snddevice_t device, const snddevice_t *list,
                               int len)
{
    int i;

    for (i=0; i<len; ++i)
    {
        if (device == list[i])
        {
            return true;
        }
    }

    return false;
}

// Find and initialize a sound_module_t appropriate for the setting
// in snd_sfxdevice.

static void InitSfxModule(boolean use_sfx_prefix)
{
#if !DOOM_TINY
    int i;

    sound_module = NULL;

    for (i=0; sound_modules[i] != NULL; ++i)
    {
        // Is the sfx device in the list of devices supported by
        // this module?

        if (SndDeviceInList(snd_sfxdevice, 
                            sound_modules[i]->sound_devices,
                            sound_modules[i]->num_sound_devices))
        {
            // Initialize the module

            if (sound_modules[i]->Init(use_sfx_prefix))
            {
                sound_module = sound_modules[i];
                return;
            }
        }
    }
#else
    if (sound_modules[0] && sound_modules[0]->Init(use_sfx_prefix)) {
        sound_module = sound_modules[0];
    }
#endif
}

// Initialize music according to snd_musicdevice.

static void InitMusicModule(void)
{
#if !DOOM_TINY
    int i;

    music_module = NULL;

    for (i=0; music_modules[i] != NULL; ++i)
    {
        // Is the music device in the list of devices supported
        // by this module?

        if (SndDeviceInList(snd_musicdevice, 
                            music_modules[i]->sound_devices,
                            music_modules[i]->num_sound_devices))
        {
            // Initialize the module

            if (music_modules[i]->Init())
            {
                music_module = music_modules[i];
                return;
            }
        }
    }
#else
    if (music_modules[0] && music_modules[0]->Init())
    {
        music_module = music_modules[0];
    }
#endif
}

//
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//

void I_InitSound(boolean use_sfx_prefix)
{
    boolean nosound, nosfx, nomusic;
#if !NO_USE_MUSIC_PACKS
    boolean nomusicpacks;
#endif

    //!
    // @vanilla
    //
    // Disable all sound output.
    //

    nosound = M_CheckParm("-nosound") > 0;

    //!
    // @vanilla
    //
    // Disable sound effects. 
    //

    nosfx = M_CheckParm("-nosfx") > 0;

    //!
    // @vanilla
    //
    // Disable music.
    //

    nomusic = M_CheckParm("-nomusic") > 0;

#if !NO_USE_MUSIC_PACKS
    //!
    //
    // Disable substitution music packs.
    //

    nomusicpacks = M_ParmExists("-nomusicpacks");

    // Auto configure the music pack directory.
    M_SetMusicPackDir();
#endif

    // Initialize the sound and music subsystems.

    if (!nosound && !screensaver_mode)
    {
        // This is kind of a hack. If native MIDI is enabled, set up
        // the TIMIDITY_CFG environment variable here before SDL_mixer
        // is opened.

#if !NO_USE_TIMIDITY
        if (!nomusic
         && (snd_musicdevice == SNDDEVICE_GENMIDI
          || snd_musicdevice == SNDDEVICE_GUS))
        {
            I_InitTimidityConfig();
        }
#endif

        if (!nosfx)
        {
            InitSfxModule(use_sfx_prefix);
        }

        if (!nomusic)
        {
            InitMusicModule();
            active_music_module = music_module;
        }

#if !NO_USE_MUSIC_PACKS
        // We may also have substitute MIDIs we can load.
        if (!nomusicpacks && music_module != NULL)
        {
            music_packs_active = music_pack_module.Init();
        }
#endif
    }
}

void I_ShutdownSound(void)
{
    if (sound_module != NULL)
    {
        sound_module->Shutdown();
    }

#if !NO_USE_MUSIC_PACKS
    if (music_packs_active)
    {
        music_pack_module.Shutdown();
    }
#endif

    if (music_module != NULL)
    {
        music_module->Shutdown();
    }
}

int I_GetSfxLumpNum(should_be_const sfxinfo_t *sfxinfo)
{
    if (sound_module != NULL)
    {
        return sound_module->GetSfxLumpNum(sfxinfo);
    }
    else
    {
        return 0;
    }
}

void I_UpdateSound(void)
{
    if (sound_module != NULL)
    {
        sound_module->Update();
    }

    if (active_music_module != NULL && active_music_module->Poll != NULL)
    {
        active_music_module->Poll();
    }
}

static void CheckVolumeSeparation(int *vol, int *sep)
{
    if (*sep < 0)
    {
        *sep = 0;
    }
    else if (*sep > 254)
    {
        *sep = 254;
    }

    if (*vol < 0)
    {
        *vol = 0;
    }
    else if (*vol > 127)
    {
        *vol = 127;
    }
}

void I_UpdateSoundParams(int channel, int vol, int sep)
{
    if (sound_module != NULL)
    {
        CheckVolumeSeparation(&vol, &sep);
        sound_module->UpdateSoundParams(channel, vol, sep);
    }
}

int I_StartSound(should_be_const sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch)
{
    if (sound_module != NULL)
    {
        CheckVolumeSeparation(&vol, &sep);
        return sound_module->StartSound(sfxinfo, channel, vol, sep, pitch);
    }
    else
    {
        return 0;
    }
}

void I_StopSound(int channel)
{
    if (sound_module != NULL)
    {
        sound_module->StopSound(channel);
    }
}

boolean I_SoundIsPlaying(int channel)
{
    if (sound_module != NULL)
    {
        return sound_module->SoundIsPlaying(channel);
    }
    else
    {
        return false;
    }
}

void I_PrecacheSounds(should_be_const sfxinfo_t *sounds, int num_sounds)
{
    if (sound_module != NULL && sound_module->CacheSounds != NULL)
    {
        sound_module->CacheSounds(sounds, num_sounds);
    }
}

void I_InitMusic(void)
{
}

void I_ShutdownMusic(void)
{

}

void I_SetMusicVolume(int volume)
{
    if (active_music_module != NULL)
    {
        active_music_module->SetMusicVolume(volume);
    }
}

void I_PauseSong(void)
{
    if (active_music_module != NULL)
    {
        active_music_module->PauseMusic();
    }
}

void I_ResumeSong(void)
{
    if (active_music_module != NULL)
    {
        active_music_module->ResumeMusic();
    }
}

void *I_RegisterSong(should_be_const void *data, int len)
{
#if !NO_USE_MUSIC_PACKS
    // If the music pack module is active, check to see if there is a
    // valid substitution for this track. If there is, we set the
    // active_music_module pointer to the music pack module for the
    // duration of this particular track.
    if (music_packs_active)
    {
        void *handle;

        handle = music_pack_module.RegisterSong(data, len);
        if (handle != NULL)
        {
            active_music_module = &music_pack_module;
            return handle;
        }
    }

    // No substitution for this track, so use the main module.
    active_music_module = music_module;
#endif
    if (active_music_module != NULL)
    {
        return active_music_module->RegisterSong(data, len);
    }
    else
    {
        return NULL;
    }
}

void I_UnRegisterSong(void *handle)
{
    if (active_music_module != NULL)
    {
        active_music_module->UnRegisterSong(handle);
    }
}

void I_PlaySong(void *handle, boolean looping)
{
    if (active_music_module != NULL)
    {
        active_music_module->PlaySong(handle, looping);
    }
}

void I_StopSong(void)
{
    if (active_music_module != NULL)
    {
        active_music_module->StopSong();
    }
}

boolean I_MusicIsPlaying(void)
{
    if (active_music_module != NULL)
    {
        return active_music_module->MusicIsPlaying();
    }
    else
    {
        return false;
    }
}

void I_BindSoundVariables(void)
{
    extern should_be_const constcharstar snd_dmxoption;
#if !NO_USE_LIBSAMPLERATE
    extern int use_libsamplerate;
    extern float libsamplerate_scale;
#endif

#if !DOOM_TINY
    M_BindIntVariable("snd_musicdevice",         &snd_musicdevice);
    M_BindIntVariable("snd_sfxdevice",           &snd_sfxdevice);
#endif
#if !DOOM_SMALL
    M_BindIntVariable("snd_sbport",              &snd_sbport);
    M_BindIntVariable("snd_sbirq",               &snd_sbirq);
    M_BindIntVariable("snd_sbdma",               &snd_sbdma);
    M_BindIntVariable("snd_mport",               &snd_mport);
#endif
#if !DOOM_TINY
    M_BindIntVariable("snd_maxslicetime_ms",     &snd_maxslicetime_ms);
    M_BindStringVariable("snd_musiccmd",         &snd_musiccmd);
    M_BindStringVariable("snd_dmxoption",        &snd_dmxoption);
    M_BindIntVariable("snd_samplerate",          &snd_samplerate);
    M_BindIntVariable("snd_cachesize",           &snd_cachesize);
#endif
#if !DOOM_SMALL
    M_BindIntVariable("opl_io_port",             &opl_io_port);
#endif
    M_BindIntVariable("snd_pitchshift",          &snd_pitchshift);

#if !NO_USE_MUSIC_PACKS
    M_BindStringVariable("music_pack_path",      &music_pack_path);
#endif
#if !NO_USE_TIMIDITY
    M_BindStringVariable("timidity_cfg_path",    &timidity_cfg_path);
#endif
#if !NO_USE_GUS
    M_BindStringVariable("gus_patch_path",       &gus_patch_path);
    M_BindIntVariable("gus_ram_kb",              &gus_ram_kb);
#endif
#if !NO_USE_LIBSAMPLERATE
    M_BindIntVariable("use_libsamplerate",       &use_libsamplerate);
    M_BindFloatVariable("libsamplerate_scale",   &libsamplerate_scale);
#endif
}