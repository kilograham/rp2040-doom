//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2006 Ben Ryves 2006
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
// mus2mid.c - Ben Ryves 2006 - http://benryves.com - benryves@benryves.com
// Use to convert a MUS file into a single track, type 0 MIDI file.

#include "mus2seq.h"
#include <cstring>
#include <algorithm>
#include <cassert>
#include <cstdio>

#if 1
#define printf(x, ...) ((void)0)
#endif

typedef uint8_t byte;

#define MIDI_PERCUSSION_CHAN 9
#define MUS_PERCUSSION_CHAN 15

//#define DUMP 1

// MUS event codes
typedef enum
{
    mus_releasekey = 0x00,
    mus_presskey = 0x10,
    mus_pitchwheel = 0x20,
    mus_systemevent = 0x30,
    mus_changecontroller = 0x40,
    mus_scoreend = 0x60
} musevent;

struct input {
    explicit input(const std::vector<uint8_t>& in) : in(in), pos(0) {}
    const std::vector<uint8_t> &in;
    size_t pos;

    template<typename T> bool read(T *out) {
        if (pos + sizeof(T) > in.size()) {
            return false;
        }
        memcpy((void *)out, (const void *)(in.data() + pos), sizeof(T));
        pos += sizeof(T);
        return true;
    }
};

// Structure to hold MUS file header
typedef struct
        {
            uint8_t id[4];
            unsigned short scorelength;
            unsigned short scorestart;
            unsigned short primarychannels;
            unsigned short secondarychannels;
            unsigned short instrumentcount;
        } musheader;

static_assert(sizeof(musheader)==14,"");
static int channel_map[SEQ_MAX_CHANNEL_COUNT];

// Allocate a free MIDI channel.

static int AllocateMIDIChannel(void) {
    int result;
    int max;
    int i;

    // Find the current highest-allocated channel.

    max = -1;

    for (i = 0; i < SEQ_MAX_CHANNEL_COUNT; ++i) {
        if (channel_map[i] > max) {
            max = channel_map[i];
        }
    }

    // max is now equal to the highest-allocated MIDI channel.  We can
    // now allocate the next available channel.  This also works if
    // no channels are currently allocated (max=-1)

    result = max + 1;

    // Don't allocate the MIDI percussion channel!

    if (result == MIDI_PERCUSSION_CHAN) {
        ++result;
    }

    return result;
}

// Given a MUS channel number, get the MIDI channel number to use
// in the outputted file.

static int GetMIDIChannel(int mus_channel, std::vector<seq_item>& items) {
    // Find the MIDI channel to use for this MUS channel.
    // MUS channel 15 is the percusssion channel.

    if (mus_channel == MUS_PERCUSSION_CHAN) {
        return MIDI_PERCUSSION_CHAN;
    } else {
        // If a MIDI channel hasn't been allocated for this MUS channel
        // yet, allocate the next free MIDI channel.

        if (channel_map[mus_channel] == -1) {
            channel_map[mus_channel] = AllocateMIDIChannel();

            // First time using the channel, send an "all notes off"
            // event. This fixes "The D_DDTBLU disease" described here:
            // https://www.doomworld.com/vb/source-ports/66802-the
#if DUMP
            printf("%d SYSTEM %d\n", mus_channel, 0xb);
#endif

            items.push_back(seq_item::all_notes_off(channel_map[mus_channel]));
        }
        return channel_map[mus_channel];
    }
}

bool mus2seq(const std::vector<uint8_t> &musinput, std::vector<seq_group> &seq_groups) {
    input in(musinput);
    // Header for the MUS file
    musheader musfileheader;

    // Descriptor for the current MUS event
    byte eventdescriptor;
    int channel; // Channel number

    // Flag for when the score end marker is hit.
    bool hitscoreend = false;

    // Initialise channel map to mark all channels as unused.

    for (channel = 0; channel < SEQ_MAX_CHANNEL_COUNT; ++channel) {
        channel_map[channel] = -1;
    }

    // Grab the header

    if (!in.read(&musfileheader)) {
        return true;
    }

    // Seek to where the data is held
    in.pos = musfileheader.scorestart;

    // So, we can assume the MUS file is faintly legit. Let's start
    // writing MIDI data...

    auto seq_items = std::vector<seq_item>();
    std::vector<std::vector<uint8_t>> channel_notes(SEQ_MAX_CHANNEL_COUNT);
    uint8_t last_volume = 0;
    std::vector<uint8_t> last_volumes(SEQ_MAX_CHANNEL_COUNT);
    std::vector<uint8_t> last_press_volumes(SEQ_MAX_CHANNEL_COUNT);
    std::vector<uint8_t> last_vibratos(SEQ_MAX_CHANNEL_COUNT);
    std::vector<uint8_t> last_wheel(SEQ_MAX_CHANNEL_COUNT);
    // vibratos initialized to 0
    for(auto &e : last_volumes) e = MUSX_INITIAL_CHANNEL_VOLUME;
    for(auto &e : last_press_volumes) e = MUSX_INITIAL_CHANNEL_VOLUME;
    for(auto &e : last_wheel) e = MUSX_INITIAL_CHANNEL_WHEEL;

#if DUMP
    int ecount = 0;
#endif
    // Now, process the MUS file:
    while (!hitscoreend) {
        // Handle a block of events:

        while (!hitscoreend) {
            // Fetch channel number and event code:

            if (!in.read(&eventdescriptor)) {
                return true;
            }

#if DUMP
            printf("%d: ", ecount++);
#endif
            channel = GetMIDIChannel(eventdescriptor & 0x0F, seq_items);
            switch ((musevent) (eventdescriptor & 0x70)) {
                case mus_releasekey: {
                    uint8_t key;
                    if (!in.read(&key)) {
                        return true;
                    }
#if DUMP
                    printf("%d RELEASEKEY %d\n", channel, key);
#endif
                    auto it = std::find(channel_notes[channel].begin(), channel_notes[channel].end(), key);
                    if (it == channel_notes[channel].end()) {
                        printf("RELEASE OF NOTE NOT ON\n");
                        return true;
                    }
                    uint dist = it - channel_notes[channel].begin();
                    uint max_dist = channel_notes[channel].size();
                    channel_notes[channel].erase(it);
                    seq_items.push_back(seq_item::release_key(channel, dist, max_dist));
                    break;
                }
                case mus_presskey: {
                    uint8_t key;
                    if (!in.read(&key)) {
                        return true;
                    }
                    uint8_t vol = vol_last_channel;
                    // the net here is somewhat arbitrary; i.e. we only update the global last volume
                    // if there was a volume change in the input MUS... todo compare this with updating every time
                    if (key & 0x80) {
                        if (!in.read(&vol)) {
                            return true;
                        }
#if DUMP
                        printf("%d PRESSKEYVOL %d %d\n",  channel, key&0x7f, vol);
#endif
                        last_volumes[channel] = vol;
                        last_press_volumes[channel] = vol;
                        if (vol == last_volume) {
                            vol = vol_last_global;
                        } else {
                            last_volume = vol;
                        }
                        key &= 0x7fu;
                    } else {
#if DUMP
                        printf("%d PRESSKEY %d %d\n", channel, key&0x7f, last_press_volumes[channel]);
#endif
                    }
                    seq_items.push_back(seq_item::press_key(channel, key, vol));
                    channel_notes[channel].push_back(key);
                    break;
                }
                case mus_pitchwheel: {
                    uint8_t wheel;
                    if (!in.read(&wheel)) {
                        return true;
                    }
#if DUMP
                    printf("%d PITCHWHEEL %d\n", channel, wheel);
#endif
                    seq_items.push_back(seq_item::delta_pitch(channel, wheel - last_wheel[channel]));
                    last_wheel[channel] = wheel;
                    break;
                }
                case mus_systemevent:
                {
                    uint8_t controllernumber;
                    if (!in.read(&controllernumber)) {
                        return true;
                    }
                    if (controllernumber < 10 || controllernumber > 14) {
                        return true;
                    }
#if DUMP
                    printf("%d SYSTEM %d\n", channel, controllernumber);
#endif
                    seq_items.push_back(seq_item::system_event(channel, controllernumber));
                    break;
                }
                case mus_changecontroller:
                {
                    uint8_t controllernumber;
                    uint8_t controllervalue;
                    if (!in.read(&controllernumber)) {
                        return true;
                    }
                    if (!in.read(&controllervalue)) {
                        return true;

                    }
                    if (controllernumber > 9) {
                        return true;
                    }
                    if (controllernumber == 2) {
                        seq_items.push_back(seq_item::delta_vibrato(channel, controllervalue - last_vibratos[channel]));
                        last_vibratos[channel] = controllervalue;
                    } else if (controllernumber == 3) {
                        seq_items.push_back(seq_item::delta_volume(channel, controllervalue - last_volumes[channel]));
                        last_volumes[channel] = controllervalue;
                    } else {
                        seq_items.push_back(seq_item::change_controller(channel, controllernumber, controllervalue));
                    }
#if DUMP
                    printf("%d CONTROLLER %d %d\n", channel, controllernumber, controllervalue);
#endif
                    break;
                }
                case mus_scoreend:
                    seq_items.push_back(seq_item::score_end());
#if DUMP
                    printf("%d SCOREEND\n", channel);
#endif
                    hitscoreend = 1;
                    break;

                default:
                    return true;
            }

            if (eventdescriptor & 0x80) {
                break;
            }
        }
        // Now we need to read the time code:
        if (!hitscoreend) {
            uint32_t gap = 0;
            uint8_t b;
            do {
                if (!in.read(&b)) {
                    return true;
                }
                gap = (gap<<7u) + (b & 0x7fu);
            } while (b & 0x80u);
#if DUMP
            if (gap) {
                printf("<- %d ->\n", gap);
            }
#endif
            seq_groups.push_back({seq_items, gap});
            seq_items.clear();
        } else {
            seq_groups.push_back({seq_items, 0});
        }
    }
    return false;
}
