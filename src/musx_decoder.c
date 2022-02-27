/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "musx_decoder.h"
#include <string.h>
#include <stdio.h>

uint16_t *read_channel_event_decoder(th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size) {
    uint min_cl = th_read_bits(bi, 4);
    uint max_cl = th_read_bits(bi, 4);
//    printf("  Code length %d->%d\n", min_cl, max_cl);
    uint bit_count = 32 - __builtin_clz(max_cl - min_cl);
    uint count = 0;
    for (uint ch = 0; ch < MUSX_CHANNEL_COUNT; ch++) {
        if (th_bit(bi)) {
            for (uint bit = 0; bit < 8; bit++) {
                if (th_bit(bi)) {
                    uint length;
                    if (min_cl == max_cl) {
                        length = min_cl;
                    } else {
                        length = min_cl + th_read_bits(bi, bit_count);
                    }
                    assert(count < tmp_buf_size);
                    tmp_buf[count * 2] = (ch << 4) | bit;
                    tmp_buf[count * 2 + 1] = length;
                    count++;
                }
            }
        }
    }
    assert(buffer_size >= th_decoder_size(count, max_cl));
    return th_create_decoder(buffer, tmp_buf, count, max_cl);
}

uint musx_decoder_init(musx_decoder *d, th_bit_input *bi, uint16_t *decoder_buffer, uint decoder_buffer_size, uint8_t *tmp_buf,
                      uint tmp_buf_size) {
    uint16_t idx = 0;
    memset(d, 0, sizeof(*d));
    d->decoders = decoder_buffer;
#define READ_DECODER(name, func) d->name = idx, idx = func(bi, d->decoders + idx, decoder_buffer_size - idx, tmp_buf, tmp_buf_size) - d->decoders
    READ_DECODER(channel_event_idx, read_channel_event_decoder);
    READ_DECODER(delta_volume_idx, th_read_simple_decoder);
    READ_DECODER(delta_pitch_idx, th_read_simple_decoder);
    READ_DECODER(delta_vibrato_idx, th_read_simple_decoder);
    READ_DECODER(press_note_idx, th_read_simple_decoder);
    READ_DECODER(press_note9_idx, th_read_simple_decoder);
    READ_DECODER(press_volume_idx, th_read_simple_decoder);
    READ_DECODER(group_size_idx, th_read_simple_decoder);
    uint last_ne = th_read_bits(bi, 4);
    for (int i = 2; i <= last_ne; i++) {
        READ_DECODER(release_dist_idx[i], th_read_simple_decoder);
    }
    READ_DECODER(gap_idx, th_read_simple_decoder);
    // build free note linked list
    int i;
    for(i=0;i<MUSX_NOTE_LIMIT-1;i++) {
        d->on_notes[i*2] = (int8_t)(i+1);
    }
    d->on_notes[i*2] = -1; // end of list
#if MUSX_INITIAL_CHANNEL_VOLUME != 0
    memset(d->channel_last_volume, MUSX_INITIAL_CHANNEL_VOLUME, sizeof(d->channel_last_volume)); // empty lists
    memset(d->channel_last_press_volume, MUSX_INITIAL_CHANNEL_VOLUME, sizeof(d->channel_last_press_volume)); // empty lists
#endif
#if MUSX_INITIAL_CHANNEL_WHEEL != 0
    memset(d->channel_last_wheel, MUSX_INITIAL_CHANNEL_WHEEL, sizeof(d->channel_last_wheel)); // empty lists
#endif
#if MUSX_INITIAL_CHANNEL_VIBRATO != 0
    memset(d->channel_last_vibrato, MUSX_INITIAL_CHANNEL_VIBRATO, sizeof(d->channel_last_vibrato)); // empty lists
#endif
#ifndef NDEBUG
    memset(d->channel_note_tail, -1, sizeof(d->channel_note_head)); // empty lists
    memset(d->channel_note_head, -1, sizeof(d->channel_note_head)); // empty lists
#endif
    return idx;
}