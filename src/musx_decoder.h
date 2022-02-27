/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once
#include "tiny_huff.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "assert.h"

#define MUSX_CHANNEL_COUNT 16
#define MUSX_RELEASE_DIST_COUNT 16
#define MUSX_NOTE_LIMIT 24

// todo this should be dynamic per WAD
#define MUSX_MAX_DECODER_SPACE 384

#define MUSX_INITIAL_CHANNEL_VOLUME 100
#define MUSX_INITIAL_CHANNEL_WHEEL 128
#define MUSX_INITIAL_CHANNEL_VIBRATO 0

#define MUSX_GAP_MAX 255

static_assert(MUSX_NOTE_LIMIT < 128, "");

// MUSX event codes
enum seq_event {
    system_event = 0x0,
    score_end = 0x1,
    release_key = 0x2,
    press_key = 0x3,
    change_controller = 0x4,
    delta_pitch = 0x5,
    delta_volume = 0x6,
    delta_vibrato = 0x7
};

typedef enum {
    vol_last_channel = 128,
    vol_last_global = 129,
} note_volume;


typedef struct {
    th_bit_input *bi;
    uint8_t group_remaining;
    int8_t channel_note_free;
    uint8_t last_volume;
    uint16_t *decoders;
    uint16_t channel_event_idx;
    uint16_t delta_volume_idx;
    uint16_t delta_pitch_idx;
    uint16_t delta_vibrato_idx;
    uint16_t press_note_idx;
    uint16_t press_note9_idx;
    uint16_t press_volume_idx;
    uint16_t group_size_idx;
    uint16_t gap_idx;
    uint16_t release_dist_idx[MUSX_RELEASE_DIST_COUNT];
    uint8_t channel_last_volume[MUSX_CHANNEL_COUNT];
    uint8_t channel_last_press_volume[MUSX_CHANNEL_COUNT]; // distinct because "mus" last refers only to this
    uint8_t channel_last_wheel[MUSX_CHANNEL_COUNT];
    uint8_t channel_last_vibrato[MUSX_CHANNEL_COUNT];
    uint8_t channel_note_count[MUSX_CHANNEL_COUNT];
    int8_t channel_note_head[MUSX_CHANNEL_COUNT];
    int8_t channel_note_tail[MUSX_CHANNEL_COUNT];
    int8_t on_notes[MUSX_NOTE_LIMIT*2];
#ifndef NDEBEUG
    int pos;
#endif
} musx_decoder;

uint musx_decoder_init(musx_decoder *d, th_bit_input *bi, uint16_t *decoder_buffer, uint decoder_buffer_size, uint8_t *tmp_buf, uint tmp_buf_size);

#include <stdio.h>

// only inline as it is only needed once
static inline void musx_record_note_on(musx_decoder *d, uint8_t channel, uint8_t note) {
    assert(d->channel_note_free >= 0);
    int8_t index = d->channel_note_free;
    d->channel_note_free = d->on_notes[index*2];
    if (!d->channel_note_count[channel]++) {
        assert(d->channel_note_head[channel]==-1);
        assert(d->channel_note_tail[channel]==-1);
        d->channel_note_head[channel] = d->channel_note_tail[channel] = index;
    } else {
        d->on_notes[d->channel_note_tail[channel]*2] = index;
        d->channel_note_tail[channel] = index;
    }
#ifndef NDEBUG
    d->on_notes[index*2] = -1;
#endif
    d->on_notes[index*2+1] = (int8_t)note;
#if 0 && !defined(NDEBUG)
    printf("ON %d,%d: ", channel, note);
    int p = (int)d->channel_note_head[channel];
    int last_p = p;
    while (p>=0) {
        printf("%d ", d->on_notes[p*2+1]);
        p = (int)d->on_notes[p*2];
        if (p >= 0) last_p = p;
    }
    assert(last_p == d->channel_note_tail[channel]);
    printf("\n");
#endif
}

static inline uint8_t musx_record_note_off(musx_decoder *d, uint8_t channel, uint8_t dist) {
    assert(dist < d->channel_note_count[channel]);
    d->channel_note_count[channel]--;
    int8_t freed_index;
    if (!dist) {
        freed_index = d->channel_note_head[channel];
        assert(freed_index >= 0);
        d->channel_note_head[channel] = d->on_notes[freed_index*2];
#ifndef NDEBUG
        if (d->channel_note_head[channel] < 0) {
            d->channel_note_tail[channel] = -1;
        }
#endif
    } else {
        assert(d->channel_note_head[channel]>=0);
        assert(d->channel_note_tail[channel]>=0);
        int8_t prev_index = d->channel_note_head[channel];
        for(int i=dist; i>1; i--) {
            prev_index = d->on_notes[prev_index*2];
            assert(prev_index >= 0); // note only set to -1 at all in NDEBUG, but should always be positive
        }
        freed_index = d->on_notes[prev_index*2];
        assert(freed_index >= 0); // note only set to -1 at all in NDEBUG, but should always be positive
        if (freed_index == d->channel_note_tail[channel]) {
            d->channel_note_tail[channel] = prev_index;
        }
        d->on_notes[prev_index*2] = d->on_notes[freed_index*2];
    }
#if 0 && !defined(NDEBUG)
    printf("OFF %d,%d: ", channel, dist);
    int p = (int)d->channel_note_head[channel];
    int last_p = p;
    while (p>=0) {
        printf("%d ", d->on_notes[p*2+1]);
        p = (int)d->on_notes[p*2];
        if (p >= 0) last_p = p;
    }
    assert(last_p == d->channel_note_tail[channel]);
    printf("\n");
#endif
    d->on_notes[freed_index*2] = d->channel_note_free;
    d->channel_note_free = freed_index;
    return d->on_notes[freed_index*2+1];
}

#ifdef __cplusplus
}
#endif