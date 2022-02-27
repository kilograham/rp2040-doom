/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <vector>
#include <ostream>
#include <cstdint>
#include <cassert>
#include "musx_decoder.h"

#define SEQ_MAX_CHANNEL_COUNT 16

struct seq_item {
    seq_event event;
    uint8_t channel;
    uint8_t p1;
    uint8_t p2;

    static seq_item system_event(uint8_t channel, uint8_t event) {
        return {
                .event = seq_event::system_event,
                .channel = channel,
                .p1 = event,
                .p2 = 0,
        };
    }

    static seq_item change_controller(uint8_t channel, uint8_t controller, uint8_t value) {
        return {
                .event= seq_event::change_controller,
                .channel = channel,
                .p1 = controller,
                .p2 = value,
        };
    }

    static seq_item all_notes_off(uint8_t channel) {
        return system_event(channel, 0xb);
    }

    static seq_item release_key(uint8_t channel, uint8_t dist, uint8_t max_dist) {
        return {
                .event= seq_event::release_key,
                .channel = channel,
                .p1 = dist,
                .p2 = max_dist,
        };
    }

    static seq_item press_key(uint8_t channel, uint8_t note, uint8_t vol) {
        return {
                .event= seq_event::press_key,
                .channel = channel,
                .p1 = note,
                .p2 = vol,
        };
    }

    static seq_item delta_pitch(uint8_t channel, uint8_t delta) {
        return {
                .event= seq_event::delta_pitch,
                .channel = channel,
                .p1 = delta,
                .p2 = 0,
        };
    }

    static seq_item delta_volume(uint8_t channel, uint8_t delta) {
        return {
                .event= seq_event::delta_volume,
                .channel = channel,
                .p1 = delta,
                .p2 = 0,
        };
    }

    static seq_item delta_vibrato(uint8_t channel, uint8_t delta) {
        return {
                .event= seq_event::delta_vibrato,
                .channel = channel,
                .p1 = delta,
                .p2 = 0,
        };
    }

    static seq_item score_end() {
        return {
                .event= seq_event::score_end,
                .channel = 0, // note this is assumed to be the case when huffman encoding later
                .p1 = 0,
                .p2 = 0,
        };
    }
};

struct seq_group {
    std::vector<seq_item> items;
    uint32_t gap;
};

// return true on error
bool mus2seq(const std::vector<uint8_t> &musinput, std::vector<seq_group> &musoutput);
