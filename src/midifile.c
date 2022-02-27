//
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
//    Reading of MIDI files.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "doomtype.h"
#include "i_swap.h"
#include "i_system.h"
#include "midifile.h"

#if USE_MUSX
#include "musx_decoder.h"
#endif

#define HEADER_CHUNK_ID "MThd"
#define TRACK_CHUNK_ID  "MTrk"
#define MAX_BUFFER_SIZE 0x10000

// haleyjd 09/09/10: packing required
#ifdef _MSC_VER
#pragma pack(push, 1)
#endif

typedef PACKED_STRUCT (
{
    byte chunk_id[4];
    unsigned int chunk_size;
}) chunk_header_t;

typedef PACKED_STRUCT (
{
    chunk_header_t chunk_header;
    unsigned short format_type;
    unsigned short num_tracks;
    unsigned short time_division;
}) midi_header_t;

// haleyjd 09/09/10: packing off.
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#if USE_DIRECT_MIDI_LUMP || USE_MIDI_DUMP_FILE
typedef struct {
                           midi_header_t header;
                           uint16_t pad;
} raw_midi_t;
static_assert(sizeof(raw_midi_t) == 16, "");

typedef struct {
                           // Time between the previous event and this event.
                           uint32_t delta_time;
                           uint8_t event;
                           uint8_t param[3];
} raw_midi_event_t;
static_assert(sizeof(raw_midi_event_t) == 8, "");
#endif

typedef struct
{
#if !USE_DIRECT_MIDI_LUMP
    // Length in bytes:

    unsigned int data_len;

    // Events in this track:

    midi_event_t *events;
#else
#if !USE_MUSX
    raw_midi_event_t *raw_events;
#else
    const byte *buffer;
    uint32_t buffer_size;
#endif
#endif
    int num_events;
} midi_track_t;

struct midi_file_s
{
#if !USE_MUSX
    midi_header_t header;

    // All tracks in this file:
    midi_track_t *tracks;
    unsigned int num_tracks;
#endif
#if !USE_DIRECT_MIDI_LUMP
    // Data buffer used to store data read for SysEx or meta events:
    byte *buffer;
    unsigned int buffer_size;
#endif
#if USE_MUSX
    midi_track_t tracks[1];
#endif
};

struct midi_track_iter_s
{
    midi_track_t *track;
    unsigned int position;
#if USE_MUSX
    midi_event_t events[2];
    int peek_index;
    th_bit_input bit_input; // note we mark end of stream reached by NULLing this out
    musx_decoder decoder;
    uint16_t decoder_space[MUSX_MAX_DECODER_SPACE];
#endif
};


#if !USE_DIRECT_MIDI_LUMP


// Check the header of a chunk:

static boolean CheckChunkHeader(chunk_header_t *chunk,
                                const char *expected_id)
{
    boolean result;
    
    result = (memcmp((char *) chunk->chunk_id, expected_id, 4) == 0);

    if (!result)
    {
        stderr_print( "CheckChunkHeader: Expected '%s' chunk header, "
                        "got '%c%c%c%c'\n",
                        expected_id,
                        chunk->chunk_id[0], chunk->chunk_id[1],
                        chunk->chunk_id[2], chunk->chunk_id[3]);
    }

    return result;
}

// Read a single byte.  Returns false on error.

static boolean ReadByte(byte *result, FILE *stream)
{
    int c;

    c = fgetc(stream);

    if (c == EOF)
    {
        stderr_print( "ReadByte: Unexpected end of file\n");
        return false;
    }
    else
    {
        *result = (byte) c;

        return true;
    }
}

// Read a variable-length value.

static boolean ReadVariableLength(unsigned int *result, FILE *stream)
{
    int i;
    byte b = 0;

    *result = 0;

    for (i=0; i<4; ++i)
    {
        if (!ReadByte(&b, stream))
        {
            stderr_print( "ReadVariableLength: Error while reading "
                            "variable-length value\n");
            return false;
        }

        // Insert the bottom seven bits from this byte.

        *result <<= 7;
        *result |= b & 0x7f;

        // If the top bit is not set, this is the end.

        if ((b & 0x80) == 0)
        {
            return true;
        }
    }

    stderr_print( "ReadVariableLength: Variable-length value too "
                    "long: maximum of four bytes\n");
    return false;
}

// Read a byte sequence into the data buffer.

static void *ReadByteSequence(unsigned int num_bytes, FILE *stream)
{
    unsigned int i;
    byte *result;

    // Allocate a buffer. Allocate one extra byte, as malloc(0) is
    // non-portable.

    result = malloc(num_bytes + 1);

    if (result == NULL)
    {
        stderr_print( "ReadByteSequence: Failed to allocate buffer\n");
        return NULL;
    }

    // Read the data:

    for (i=0; i<num_bytes; ++i)
    {
        if (!ReadByte(&result[i], stream))
        {
            stderr_print( "ReadByteSequence: Error while reading byte %u\n",
                            i);
            free(result);
            return NULL;
        }
    }

    return result;
}

// Read a MIDI channel event.
// two_param indicates that the event type takes two parameters
// (three byte) otherwise it is single parameter (two byte)

static boolean ReadChannelEvent(midi_event_t *event,
                                byte event_type, boolean two_param,
                                FILE *stream)
{
    byte b = 0;

    // Set basics:

    event->event_type = event_type & 0xf0;
    event->data.channel.channel = event_type & 0x0f;

    // Read parameters:

    if (!ReadByte(&b, stream))
    {
        stderr_print( "ReadChannelEvent: Error while reading channel "
                        "event parameters\n");
        return false;
    }

    event->data.channel.param1 = b;

    // Second parameter:

    if (two_param)
    {
        if (!ReadByte(&b, stream))
        {
            stderr_print( "ReadChannelEvent: Error while reading channel "
                            "event parameters\n");
            return false;
        }

        event->data.channel.param2 = b;
    } else {
        event->data.channel.param2 = 0;
    }

    return true;
}

// Read sysex event:

static boolean ReadSysExEvent(midi_event_t *event, int event_type,
                              FILE *stream)
{
    event->event_type = event_type;

    if (!ReadVariableLength(&event->data.sysex.length, stream))
    {
        stderr_print( "ReadSysExEvent: Failed to read length of "
                                        "SysEx block\n");
        return false;
    }

    // Read the byte sequence:

    event->data.sysex.data = ReadByteSequence(event->data.sysex.length, stream);

    if (event->data.sysex.data == NULL)
    {
        stderr_print( "ReadSysExEvent: Failed while reading SysEx event\n");
        return false;
    }

    return true;
}

// Read meta event:

static boolean ReadMetaEvent(midi_event_t *event, FILE *stream)
{
    byte b = 0;

    event->event_type = MIDI_EVENT_META;

    // Read meta event type:

    if (!ReadByte(&b, stream))
    {
        stderr_print( "ReadMetaEvent: Failed to read meta event type\n");
        return false;
    }

    event->data.meta.type = b;

    // Read length of meta event data:

    if (!ReadVariableLength(&event->data.meta.length, stream))
    {
        stderr_print( "ReadSysExEvent: Failed to read length of "
                                        "SysEx block\n");
        return false;
    }

    // Read the byte sequence:

    event->data.meta.data = ReadByteSequence(event->data.meta.length, stream);

    if (event->data.meta.data == NULL)
    {
        stderr_print( "ReadSysExEvent: Failed while reading SysEx event\n");
        return false;
    }

    return true;
}

static boolean ReadEvent(midi_event_t *event, unsigned int *last_event_type,
                         FILE *stream)
{
    byte event_type = 0;

    if (!ReadVariableLength(&event->delta_time, stream))
    {
        stderr_print( "ReadEvent: Failed to read event timestamp\n");
        return false;
    }

    if (!ReadByte(&event_type, stream))
    {
        stderr_print( "ReadEvent: Failed to read event type\n");
        return false;
    }

    // All event types have their top bit set.  Therefore, if 
    // the top bit is not set, it is because we are using the "same
    // as previous event type" shortcut to save a byte.  Skip back
    // a byte so that we read this byte again.

    if ((event_type & 0x80) == 0)
    {
        event_type = *last_event_type;

        if (fseek(stream, -1, SEEK_CUR) < 0)
        {
            stderr_print( "ReadEvent: Unable to seek in stream\n");
            return false;
        }
    }
    else
    {
        *last_event_type = event_type;
    }

    // Check event type:

    switch (event_type & 0xf0)
    {
        // Two parameter channel events:

        case MIDI_EVENT_NOTE_OFF:
        case MIDI_EVENT_NOTE_ON:
        case MIDI_EVENT_AFTERTOUCH:
        case MIDI_EVENT_CONTROLLER:
        case MIDI_EVENT_PITCH_BEND:
            return ReadChannelEvent(event, event_type, true, stream);

        // Single parameter channel events:

        case MIDI_EVENT_PROGRAM_CHANGE:
        case MIDI_EVENT_CHAN_AFTERTOUCH:
            return ReadChannelEvent(event, event_type, false, stream);

        default:
            break;
    }

    // Specific value?

    switch (event_type)
    {
        case MIDI_EVENT_SYSEX:
        case MIDI_EVENT_SYSEX_SPLIT:
            return ReadSysExEvent(event, event_type, stream);

        case MIDI_EVENT_META:
            return ReadMetaEvent(event, stream);

        default:
            break;
    }

    stderr_print( "ReadEvent: Unknown MIDI event type: 0x%x\n", event_type);
    return false;
}

// Free an event:

static void FreeEvent(midi_event_t *event)
{
    // Some event types have dynamically allocated buffers assigned
    // to them that must be freed.

    switch (event->event_type)
    {
        case MIDI_EVENT_SYSEX:
        case MIDI_EVENT_SYSEX_SPLIT:
            free(event->data.sysex.data);
            break;

        case MIDI_EVENT_META:
            free(event->data.meta.data);
            break;

        default:
            // Nothing to do.
            break;
    }
}

// Read and check the track chunk header

static boolean ReadTrackHeader(midi_track_t *track, FILE *stream)
{
    size_t records_read;
    chunk_header_t chunk_header;

    records_read = fread(&chunk_header, sizeof(chunk_header_t), 1, stream);

    if (records_read < 1)
    {
        return false;
    }

    if (!CheckChunkHeader(&chunk_header, TRACK_CHUNK_ID))
    {
        return false;
    }

    track->data_len = SDL_SwapBE32(chunk_header.chunk_size);

    return true;
}

static boolean ReadTrack(midi_track_t *track, FILE *stream)
{
    midi_event_t *new_events;
    midi_event_t *event;
    unsigned int last_event_type;

    track->num_events = 0;
    track->events = NULL;

    // Read the header:

    if (!ReadTrackHeader(track, stream))
    {
        return false;
    }

    // Then the events:

    last_event_type = 0;

    for (;;)
    {
        // Resize the track slightly larger to hold another event:

        new_events = I_Realloc(track->events, 
                             sizeof(midi_event_t) * (track->num_events + 1));
        track->events = new_events;

        // Read the next event:

        event = &track->events[track->num_events];
        if (!ReadEvent(event, &last_event_type, stream))
        {
            return false;
        }

        ++track->num_events;

        // End of track?

        if (event->event_type == MIDI_EVENT_META
         && event->data.meta.type == MIDI_META_END_OF_TRACK)
        {
            break;
        }
    }

    return true;
}

// Free a track:

static void FreeTrack(midi_track_t *track)
{
    unsigned int i;

    for (i=0; i<track->num_events; ++i)
    {
        FreeEvent(&track->events[i]);
    }

    free(track->events);
}

static boolean ReadAllTracks(midi_file_t *file, FILE *stream)
{
    unsigned int i;

    // Allocate list of tracks and read each track:

    file->tracks = malloc(sizeof(midi_track_t) * file->num_tracks);

    if (file->tracks == NULL)
    {
        return false;
    }

    memset(file->tracks, 0, sizeof(midi_track_t) * file->num_tracks);

    // Read each track:

    for (i=0; i<file->num_tracks; ++i)
    {
        if (!ReadTrack(&file->tracks[i], stream))
        {
            return false;
        }
    }

    return true;
}

// Read and check the header chunk.

static boolean ReadFileHeader(midi_file_t *file, FILE *stream)
{
    size_t records_read;
    unsigned int format_type;

    records_read = fread(&file->header, sizeof(midi_header_t), 1, stream);

    if (records_read < 1)
    {
        return false;
    }

    if (!CheckChunkHeader(&file->header.chunk_header, HEADER_CHUNK_ID)
     || SDL_SwapBE32(file->header.chunk_header.chunk_size) != 6)
    {
        stderr_print( "ReadFileHeader: Invalid MIDI chunk header! "
                        "chunk_size=%i\n",
                        SDL_SwapBE32(file->header.chunk_header.chunk_size));
        return false;
    }

    format_type = SDL_SwapBE16(file->header.format_type);
    file->num_tracks = SDL_SwapBE16(file->header.num_tracks);

    if ((format_type != 0 && format_type != 1)
     || file->num_tracks < 1)
    {
        stderr_print( "ReadFileHeader: Only type 0/1 "
                                         "MIDI files supported!\n");
        return false;
    }

    return true;
}
#endif

void MIDI_FreeFile(midi_file_t *file)
{

#if !USE_DIRECT_MIDI_LUMP
    if (file->tracks != NULL)
    {
        int i;
        for (i=0; i<file->num_tracks; ++i)
        {
            FreeTrack(&file->tracks[i]);
        }
        free(file->tracks);
    }
#endif

    free(file);
}

#if !USE_DIRECT_MIDI_LUMP
midi_file_t *MIDI_LoadFile(char *filename)
{
    midi_file_t *file;
    FILE *stream;

    file = malloc(sizeof(midi_file_t));

    if (file == NULL)
    {
        return NULL;
    }

    file->tracks = NULL;
    file->num_tracks = 0;
    file->buffer = NULL;
    file->buffer_size = 0;

    // Open file

    stream = fopen(filename, "rb");

    if (stream == NULL)
    {
        stderr_print( "MIDI_LoadFile: Failed to open '%s'\n", filename);
        MIDI_FreeFile(file);
        return NULL;
    }

    // Read MIDI file header

    if (!ReadFileHeader(file, stream))
    {
        fclose(stream);
        MIDI_FreeFile(file);
        return NULL;
    }

    // Read all tracks:

    if (!ReadAllTracks(file, stream))
    {
        fclose(stream);
        MIDI_FreeFile(file);
        return NULL;
    }

    fclose(stream);

    return file;
}
#endif

// Get the number of tracks in a MIDI file.

unsigned int MIDI_NumTracks(midi_file_t *file)
{
    return midifile_numtracks(file);
}

// Start iterating over the events in a track.

#ifdef TEST
static void PrintTrack(midi_track_t *track);
#endif
midi_track_iter_t *MIDI_IterateTrack(midi_file_t *file, unsigned int track)
{
    midi_track_iter_t *iter;

    assert(track < midifile_numtracks(file));

//    printf("Begin iterating track %d\n", track);
//    PrintTrack(&file->tracks[track]);
    iter = malloc(sizeof(*iter));
    iter->track = &file->tracks[track];
    MIDI_RestartIterator(iter);
    return iter;
}

void MIDI_FreeIterator(midi_track_iter_t *iter)
{
    free(iter);
}

// Get the time until the next MIDI event in a track.

unsigned int MIDI_GetDeltaTime(midi_track_iter_t *iter)
{
#if USE_MUSX
    return iter->events[iter->peek_index^1].delta_time;
#else
    if (iter->position < iter->track->num_events)
    {
#if USE_DIRECT_MIDI_LUMP
        raw_midi_event_t *next_event;
        next_event = &iter->track->raw_events[iter->position];
#else
        midi_event_t *next_event;
        next_event = &iter->track->events[iter->position];
        return next_event->delta_time;
#endif
        return next_event->delta_time;
    }
    else
    {
        return 0;
    }
#endif
}

// Get a pointer to the next MIDI event.
#if USE_MUSX
void peek_event(midi_track_iter_t *iter);
#endif

#if USE_DIRECT_MIDI_LUMP && !USE_MUSX
static void MIDI_DecodeEvent(raw_midi_event_t *raw_event, midi_event_t *event) {
    event->delta_time = raw_event->delta_time;
    if (raw_event->event == MIDI_EVENT_META) {
        event->event_type = raw_event->event;
        event->data.meta.type = MIDI_META_SET_TEMPO;
        event->data.meta.data = raw_event->param;
        event->data.meta.length = 3;
    } else {
        event->event_type = raw_event->event & 0xf0;
        event->data.channel.channel = raw_event->event & 0x0f;
        event->data.channel.param1 = raw_event->param[0];
        event->data.channel.param2 = raw_event->param[1];
    }
}
#endif

int MIDI_GetNextEvent(midi_track_iter_t *iter, midi_event_t **event)
{
#if USE_MUSX
    *event = &iter->events[iter->peek_index];
    peek_event(iter);
    return 1;
#else
    if (iter->position < iter->track->num_events)
    {
#if USE_DIRECT_MIDI_LUMP
        *event = &iter->track->current_event;
        MIDI_DecodeEvent(&iter->track->raw_events[iter->position], *event);
        ++iter->position;
#else
        *event = &iter->track->events[iter->position];
#endif
        ++iter->position;
        return 1;
    }
    else
    {
        return 0;
    }
#endif
}

unsigned int MIDI_GetFileTimeDivision(midi_file_t *file)
{
    short result = midifile_timedivision(file);

    // Negative time division indicates SMPTE time and must be handled
    // differently.
    if (result < 0)
    {
        return (signed int)(-(result/256))
             * (signed int)(result & 0xFF);
    }
    else
    {
        return result;
    }
}

void MIDI_RestartIterator(midi_track_iter_t *iter)
{
    iter->position = 0;
#if USE_MUSX
    iter->peek_index = 0;
    iter->events[iter->peek_index].delta_time = 0; // time before first event
    uint8_t tmp_buf[512]; // todo get tem[ workspace if stack not big enough
    th_sized_bit_input_init(&iter->bit_input, iter->track->buffer, iter->track->buffer_size);
    musx_decoder_init(&iter->decoder, &iter->bit_input, iter->decoder_space, count_of(iter->decoder_space), tmp_buf, sizeof(tmp_buf));
    peek_event(iter);
#endif
}

//#define TEST
#ifdef TEST

static char *MIDI_EventTypeToString(midi_event_type_t event_type)
{
    switch (event_type)
    {
        case MIDI_EVENT_NOTE_OFF:
            return "MIDI_EVENT_NOTE_OFF";
        case MIDI_EVENT_NOTE_ON:
            return "MIDI_EVENT_NOTE_ON";
        case MIDI_EVENT_AFTERTOUCH:
            return "MIDI_EVENT_AFTERTOUCH";
        case MIDI_EVENT_CONTROLLER:
            return "MIDI_EVENT_CONTROLLER";
        case MIDI_EVENT_PROGRAM_CHANGE:
            return "MIDI_EVENT_PROGRAM_CHANGE";
        case MIDI_EVENT_CHAN_AFTERTOUCH:
            return "MIDI_EVENT_CHAN_AFTERTOUCH";
        case MIDI_EVENT_PITCH_BEND:
            return "MIDI_EVENT_PITCH_BEND";
        case MIDI_EVENT_SYSEX:
            return "MIDI_EVENT_SYSEX";
        case MIDI_EVENT_SYSEX_SPLIT:
            return "MIDI_EVENT_SYSEX_SPLIT";
        case MIDI_EVENT_META:
            return "MIDI_EVENT_META";

        default:
            return "(unknown)";
    }
}

void PrintTrack(midi_track_t *track)
{
    midi_event_t *event;
    unsigned int i;

    for (i=0; i<track->num_events; ++i)
    {
#if USE_DIRECT_MIDI_LUMP
        midi_event_t the_event;
        MIDI_DecodeEvent(&track->raw_events[i], &the_event);
        event = &the_event;
#else
        event = &track->events[i];
#endif

        if (event->delta_time > 0)
        {
            printf("Delay: %i ticks\n", event->delta_time);
        }

        printf("Event type: %s (%i)\n",
               MIDI_EventTypeToString(event->event_type),
               event->event_type);

        switch(event->event_type)
        {
            case MIDI_EVENT_NOTE_OFF:
            case MIDI_EVENT_NOTE_ON:
            case MIDI_EVENT_AFTERTOUCH:
            case MIDI_EVENT_CONTROLLER:
            case MIDI_EVENT_PROGRAM_CHANGE:
            case MIDI_EVENT_CHAN_AFTERTOUCH:
            case MIDI_EVENT_PITCH_BEND:
                printf("\tChannel: %i\n", event->data.channel.channel);
                printf("\tParameter 1: %i\n", event->data.channel.param1);
                printf("\tParameter 2: %i\n", event->data.channel.param2);
                break;

            case MIDI_EVENT_SYSEX:
            case MIDI_EVENT_SYSEX_SPLIT:
                printf("\tLength: %i\n", event->data.sysex.length);
                break;

            case MIDI_EVENT_META:
                printf("\tMeta type: %i\n", event->data.meta.type);
                printf("\tLength: %i\n", event->data.meta.length);
                break;
        }
    }
}
#endif

#ifdef TEST
int main(int argc, char *argv[])
{
    midi_file_t *file;
    unsigned int i;

    if (argc < 2)
    {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    file = MIDI_LoadFile(argv[1]);

    if (file == NULL)
    {
        stderr_print( "Failed to open %s\n", argv[1]);
        exit(1);
    }

    for (i=0; i<midifile_numtracks(file); ++i)
    {
        printf("\n== Track %i ==\n\n", i);

        PrintTrack(&file->tracks[i]);
    }

    return 0;
}

#endif

#if USE_DIRECT_MIDI_LUMP
#if !USE_MUSX
midi_file_t *MIDI_LoadRaw(const void *data, int len) {
    const raw_midi_t *raw = (const raw_midi_t *)data;
    if (memcmp(raw->header.chunk_header.chunk_id, "MidX", 4) != 0) {
        stderr_print( "MIDI_LoadRAW Data wasn't RAW MID.\n");
        return NULL;
    }
    midi_file_t *file = malloc(sizeof(midi_file_t));
    if (file == NULL)
    {
        return NULL;
    }
    file->header = raw->header;
    file->num_tracks = SDL_SwapBE16(file->header.num_tracks);
    file->tracks = calloc(file->num_tracks, sizeof(midi_track_t));
    if (!file->tracks) {
        free(file);
        return NULL;
    }
    const void *event_data = data + sizeof(raw_midi_t);
    for(int i=0;i<file->num_tracks;i++) {
        file->tracks[i].num_events = *(int32_t*)event_data;
        event_data += 4;
        file->tracks[i].raw_events = (raw_midi_event_t *)event_data;
        event_data += file->tracks[i].num_events * 8;
    }
    return file;
}
#else

midi_file_t *MUSX_LoadRaw(const void *data, int len) {
#if !MUSX_COMPRESSED
    if (memcmp(data, "MUS2", 4) != 0) {
        stderr_print( "MUSX_LoadRAW Data wasn't uncompressed MUS2.\n");
        return NULL;
    }
#else
    if (memcmp(data, "MUSX", 4) != 0) {
        stderr_print( "MUSX_LoadRAW Data wasn't compressed MUSX.\n");
        return NULL;
    }
#endif
    midi_file_t *file = malloc(sizeof(midi_file_t));
    if (file == NULL)
    {
        return NULL;
    }
    file->tracks[0].buffer_size = *(uint32_t *)(data+4);
    assert(file->tracks[0].buffer_size == len - 8);
    file->tracks[0].buffer = data + 8;
    return file;
}

static const byte controller_map[] = {
        0x00, 0x20, 0x01, 0x07, 0x0A, 0x0B, 0x5B, 0x5D,
        0x40, 0x43, 0x78, 0x7B, 0x7E, 0x7F, 0x79
};

void peek_event(midi_track_iter_t *iter) {
    iter->peek_index ^= 1;
    midi_event_t *me = &iter->events[iter->peek_index];

    musx_decoder *d = &iter->decoder;
    th_bit_input *bi = &iter->bit_input;

    if (bi->cur) {
        if (!d->group_remaining) {
            d->group_remaining = th_decode(d->decoders + d->group_size_idx, bi);
        }
        uint8_t ec = th_decode(d->decoders + d->channel_event_idx, bi);
        uint channel = me->data.channel.channel = ec >> 4;
        me->data.channel.param1 = me->data.channel.param2 = 0; // not sure if necessary
        switch (ec & 0xf) {
            case change_controller: {
                uint8_t controller = th_read_bits(bi, 4);
                uint8_t value = th_read_bits(bi, 8);
                if (controller == 0) {
                    me->event_type = MIDI_EVENT_PROGRAM_CHANGE;
                    me->data.channel.param1 = value;
                } else {
                    assert(controller >= 1 && controller <= 9);
                    me->event_type = MIDI_EVENT_CONTROLLER;
                    me->data.channel.param1 = controller_map[controller];
                    me->data.channel.param2 = value;
                }
                break;
            }
            case delta_volume: {
                int delta = from_zig(th_decode(d->decoders + d->delta_volume_idx, bi));
                d->channel_last_volume[channel] += delta;
                me->event_type = MIDI_EVENT_CONTROLLER;
                me->data.channel.param1 = controller_map[3];
                me->data.channel.param2 = d->channel_last_volume[channel];
//                printf("delta volume %d, so %d\n", delta, d->channel_last_volume[channel]);
                break;
            }
            case delta_pitch: {
                int delta = from_zig(th_decode(d->decoders + d->delta_pitch_idx, bi));
                d->channel_last_wheel[channel] += delta;
                uint wheel = d->channel_last_wheel[channel] * 64;
                me->event_type = MIDI_EVENT_PITCH_BEND;
                me->data.channel.param1 = wheel & 0x7fu;
                me->data.channel.param2 = (wheel >> 7u) & 0x7fu;
                //            printf("delta pitch %d, so %d\n", delta, d->channel_last_wheel[channel]);
                break;
            }
            case delta_vibrato: {
                uint delta = from_zig(th_decode(d->decoders + d->delta_vibrato_idx, bi));
                d->channel_last_vibrato[channel] += delta;
                me->event_type = MIDI_EVENT_CONTROLLER;
                me->data.channel.param1 = controller_map[2];
                me->data.channel.param2 = d->channel_last_vibrato[channel];
//                printf("delta vibrato %d, so %d\n", delta, d->channel_last_vibrato[channel]);
                break;
            }
            case press_key: {
                int note = th_decode(d->decoders + (channel == 9 ? d->press_note9_idx : d->press_note_idx), bi);
                int vol = th_decode(d->decoders + d->press_volume_idx, bi);
                //            printf("press key %d vol %d", note, vol);
                if (vol == vol_last_global) {
                    vol = d->channel_last_volume[channel] = d->channel_last_press_volume[channel] = d->last_volume;
                } else if (vol == vol_last_channel) {
                    vol = d->channel_last_press_volume[channel];
                } else {
                    d->last_volume = d->channel_last_volume[channel] = d->channel_last_press_volume[channel] = vol;
                }
                //            printf(" so %d\n", vol);
                musx_record_note_on(d, channel, note);
                me->event_type = MIDI_EVENT_NOTE_ON;
                me->data.channel.param1 = note;
                me->data.channel.param2 = vol;
                break;
            }
            case release_key: {
                assert(d->channel_note_count[channel]);
                uint8_t dist = 0;
                if (d->channel_note_count[channel] > 1) {
                    dist = th_decode(d->decoders + d->release_dist_idx[d->channel_note_count[channel]], bi);
                }
                uint8_t note = musx_record_note_off(d, channel, dist);
                me->event_type = MIDI_EVENT_NOTE_OFF;
                me->data.channel.param1 = note;
                //            printf("release key dist %d note %d\n", dist, note);
                break;
            }
            case system_event: {
                uint controller = th_read_bits(bi, 3) + 10;
                me->event_type = MIDI_EVENT_CONTROLLER;
                me->data.channel.param1 = controller_map[controller];
                //            printf("system event %d\n", controller);
                break;
            }
            case score_end:
                me->event_type = MIDI_EVENT_META;
                me->data.meta.type = MIDI_META_END_OF_TRACK;
                bi->cur = NULL;
                break;
            default:
                assert(false);
        }
        int gap = 0;
        if (bi->cur) {
            if (!--d->group_remaining) {
                int lgap;
                do {
                    lgap = th_decode(d->decoders + d->gap_idx, bi);
                    gap += lgap;
                } while (lgap == MUSX_GAP_MAX);
            }
        }
        me->delta_time = gap;
//        printf("%d MIDI %02x %d %02x %02x\n", d->pos++, me->event_type, me->data.channel.channel, me->data.channel.param1, me->data.channel.param2);
    } else {
        // already at score end; should we fill in?
    }
}
#endif
#endif

#if USE_MIDI_DUMP_FILE
static int filter_event(const midi_event_t *event) {
    midi_event_type_t type = event->event_type;
    if (type == MIDI_EVENT_SYSEX || type == MIDI_EVENT_SYSEX_SPLIT) {
        return false;
    }
    if (type == MIDI_EVENT_META) {
        if (event->data.meta.type != MIDI_META_SET_TEMPO) {
            printf("Skipping meta 0x%02x\n", event->data.meta.type);
            return false;
        }
        if (event->data.meta.length != 3) {
            printf("EXPECTED LENGTH 3\n");
            return false;
        }
    }
    return true;
}

void MIDI_DumpFile(midi_file_t *file, const char *filename) {
    FILE *out = fopen(filename, "wb");
    if (!out) return;
    raw_midi_t rm;
    rm.header = file->header;
    memcpy(rm.header.chunk_header.chunk_id, "MidX", 4);
    fwrite(&rm, sizeof(rm), 1, out);
    for(int i=0;i<file->num_tracks;i++) {
        const midi_track_t *track = &file->tracks[i];
        int32_t event_count = 0;
        for(int j=0;j<track->num_events;j++) {
            const midi_event_t *event = &track->events[j];
            if (filter_event(event)) event_count++;
        }
        fwrite(&event_count, 4, 1, out);
        int ecount = 0;
        for(int j=0;j<track->num_events;j++) {
            const midi_event_t *event = &track->events[j];
            raw_midi_event_t raw_event;
            raw_event.delta_time = event->delta_time;
            raw_event.event = event->event_type;
            if (filter_event(event)) {
                if (event->event_type == MIDI_EVENT_META) {
                    assert(event->data.meta.length == 3);
                    assert(event->data.meta.type == MIDI_META_SET_TEMPO);
                    memcpy(raw_event.param, event->data.meta.data, 3);
                } else {
                    raw_event.event |= event->data.channel.channel;
                    raw_event.param[0] = event->data.channel.param1;
                    raw_event.param[1] = event->data.channel.param2;
                    raw_event.param[2] = 0;
                }
                ecount++;
                static_assert(sizeof(raw_event) == 8, "");
                fwrite(&raw_event, sizeof(raw_event), 1, out);
            }
        }
        assert(ecount == event_count);
    }
    fclose(out);
}
#endif