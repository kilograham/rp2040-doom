/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <functional>
#include "compress_mus.h"
#include "mus2seq.h"
#include "config.h"
#include "huffman.h"
#include "tiny_huff.h"
#include <cstring>
#include "musx_decoder.h"
#include "huff_sink.h"

#if 1
#define printf(x, ...) ((void)0)
#endif
statsomizer musx_decoder_space("MUSX Decoder Space");

std::vector<uint8_t> decode_musx(std::vector<uint8_t> &data);

const char *seq_event_name(seq_event event) {
    switch (event) {
        case seq_event::change_controller:
            return "change controller";
        case seq_event::delta_volume:
            return "delta volume";
        case seq_event::delta_pitch:
            return "delta pitch";
        case seq_event::delta_vibrato:
            return "delta vibrato";
        case seq_event::press_key:
            return "press key";
        case seq_event::release_key:
            return "release key";
        case seq_event::system_event:
            return "system event";
        case seq_event::score_end:
            return "score end";
        default:
            assert(false);
            return "unknown";
    }
}

std::ostream &operator<<(std::ostream &out, const seq_event &event) {
    out << seq_event_name(event);
    return out;
}

template<typename T, typename S>
std::ostream &operator<<(std::ostream &os, const std::pair<T, S> &v) {
    os << "(" << v.first << ", " << v.second << ")";
    return os;
}

template<typename T>
std::ostream &operator<<(std::ostream &os, const std::pair<T, uint8_t> &v) {
    os << "(" << v.first << ", " << (int) v.second << ")";
    return os;
}

typedef std::pair<uint16_t, seq_event> channel_event;

template<typename BO=std::shared_ptr<byte_vector_bit_output>>
struct mus_compressor_context {
    mus_compressor_context() :
            event_channel_sink("Event/Channel"),
            delta_volume_sink("Delta Volume"),
            delta_pitch_sink("Delta Pitch"),
            delta_vibrato_sink("Delta Vibrato"),
            press_note_sink("Press Note"),
            press_note9_sink("Press Note 9"),
            press_volume_sink("Press Volume"),
            gap_sink("Gap")
#if MUS_GROUP_SIZE_CODE
            , group_size_sink("Group Size")
#endif
    {
        for (int i = 0; i < MUSX_RELEASE_DIST_COUNT; i++) {
            release_dist_sinks.emplace_back(std::string("Release Dist (") + std::to_string(i) + ")");
        }
        for (int i = 0; i < MUSX_RELEASE_DIST_COUNT; i++) {
            wrappers.wrappers.push_back(release_dist_sinks[i]);
        }
    }

    symbol_sink<huffman_params<channel_event>, BO> event_channel_sink;
    symbol_sink<huffman_params<uint8_t>, BO> delta_volume_sink;
    symbol_sink<huffman_params<uint8_t>, BO> delta_pitch_sink; // note that this seems to be nearly always multiples of 2
    symbol_sink<huffman_params<uint8_t>, BO> delta_vibrato_sink;
    symbol_sink<huffman_params<uint8_t>, BO> press_note_sink;
    symbol_sink<huffman_params<uint8_t>, BO> press_note9_sink;
    symbol_sink<huffman_params<uint8_t>, BO> press_volume_sink;
    symbol_sink<huffman_params<uint16_t>, BO> gap_sink;
    std::vector<symbol_sink<huffman_params<uint8_t>, BO>> release_dist_sinks;
    bit_sink<BO> raw_bits;
#if MUS_GROUP_SIZE_CODE
    symbol_sink<huffman_params<uint8_t>, BO> group_size_sink;
#endif
    sink_wrappers<BO> wrappers{event_channel_sink, delta_volume_sink, delta_pitch_sink, delta_vibrato_sink,
                               press_note_sink, press_note9_sink, press_volume_sink, gap_sink, raw_bits,
#if MUS_GROUP_SIZE_CODE
                               group_size_sink,
#endif
    };
};

template<typename BO, typename H>
void output_channel_events(BO &bit_output, huffman_encoding<channel_event, H> &huff) {
    if (huff.empty()) {
        assert(false); // should be handled at a higher level
        return;
    }
    auto stats = huff.get_stats();

    int min_cl = huff.get_min_code_length();
    int max_cl = huff.get_max_code_length();
    assert(max_cl < 16);
    assert(min_cl <= max_cl);
    bit_output->write(bit_sequence(min_cl, 4));
    bit_output->write(bit_sequence(max_cl, 4));
    int bit_count = 32 - __builtin_clz(max_cl - min_cl);

    for (uint ch = 0; ch < MUSX_CHANNEL_COUNT; ch++) {
        uint8_t event_mask = 0;
        for (const auto &e : stats.symbol_counts) {
            if (e.first.first == ch) {
                assert((uint) e.first.second < 8);
                event_mask |= 1u << (uint) e.first.second;
            }
        }
        bit_output->write(bit_sequence(event_mask != 0, 1));
        if (event_mask) {
            for (uint bit = 0; bit < 8; bit++) {
                if (event_mask & (1u << bit)) {
                    bit_output->write(bit_sequence(1, 1));
                    auto ev = static_cast<seq_event>(bit);
                    auto length = huff.get_code_length(std::make_pair(ch, ev));
                    assert(length);
                    if (min_cl == max_cl) {
                        assert(length == min_cl);
                    } else {
                        bit_output->write(bit_sequence(length - min_cl, bit_count));
                    }
                } else {
                    bit_output->write(bit_sequence(0, 1));
                }
            }
        }
    }
}

std::vector<uint8_t> compress_seq(std::vector<seq_group> groups) {
    mus_compressor_context<> ctx;
    auto bitoutput = std::make_shared<byte_vector_bit_output>();
    for (int pass = 0; pass < 2; pass++) {
        bool done = false;
        uint pressed_note_count = 0;
        for (const auto &g : groups) {
#if MUS_GROUP_SIZE_CODE
            ctx.group_size_sink.output(g.items.size());
#endif
            for (size_t i = 0; i < g.items.size(); i++) {
                const auto &e = g.items[i];
                printf("%d ", e.channel);
                if (e.channel >= MUSX_CHANNEL_COUNT) {
                    fail("MUSX_NUM_CHANNELS(%d) exceeded", MUSX_CHANNEL_COUNT);
                }
                ctx.event_channel_sink.output(std::make_pair(e.channel, e.event));
                assert(!done);
                switch (e.event) {
                    case seq_event::change_controller:
                        printf("change controller %d %d\n", e.p1, e.p2);
                        ctx.raw_bits.output(bit_sequence(e.p1, 4));
                        ctx.raw_bits.output(bit_sequence::for_byte(e.p2));
                        break;
                    case seq_event::delta_volume:
                        printf("delta volume %d\n", (int8_t) e.p1);
                        ctx.delta_volume_sink.output(to_zig((int8_t) e.p1));
                        break;
                    case seq_event::delta_pitch:
                        printf("delta pitch %d\n",
                               (int8_t) e.p1); // for some songs this is always even; don't think we care that much
                        ctx.delta_pitch_sink.output(to_zig((int8_t) e.p1));
                        break;
                    case seq_event::delta_vibrato:
                        printf("delta vibrato %d\n", (int8_t) e.p1);
                        ctx.delta_vibrato_sink.output(to_zig((int8_t) e.p1));
                        break;
                    case seq_event::press_key:
                        printf("press key %d vol %d\n", e.p1, e.p2);
                        pressed_note_count++;
                        if (pressed_note_count > MUSX_NOTE_LIMIT) {
                            fail("too many simultaneous notes");
                        }
                        if (e.channel == 9) {
                            ctx.press_note9_sink.output(e.p1);
                        } else {
                            ctx.press_note_sink.output(e.p1);
                        }
                        ctx.press_volume_sink.output((int8_t) e.p2);
                        break;
                    case seq_event::release_key:
                        pressed_note_count--;
                        if (pressed_note_count < 0) {
                            fail("too many released notes");
                        }
                        printf("release key dist %d\n", e.p1);
                        if (e.p2 > 1) {
                            if (e.p2 >= MUSX_RELEASE_DIST_COUNT) {
                                fail("Release distance MUSX_RELEASE_DIST_COUNT(%d) exceeded, consider increasing", MUSX_RELEASE_DIST_COUNT);
                            }
                            ctx.release_dist_sinks[e.p2].output(e.p1);
                        }
                        break;
                    case seq_event::system_event:
                        printf("system event %d\n", e.p1);
                        ctx.raw_bits.output(bit_sequence(e.p1 - 10, 3));
                        break;
                    case seq_event::score_end:
                        printf("score end\n");
                        done = true;
                        break;
                    default:
                        assert(false);
                }
#if MUS_PER_EVENT_GAP && !MUS_GROUP_SIZE_CODE
                // will be better to have groups therefore
                if (i != g.items.size()-1) {
                    ctx.gap_sink.output(0);
                }
#endif
            }
#if !MUS_PER_EVENT_GAP && !MUS_GROUP_SIZE_CODE
            ctx.event_channel_sink.output(std::make_pair(1, seq_event::score_end)); // use this for gap right now
#endif
            int gap = g.gap;
            if (!gap) {
                assert(done);
            } else {
                while (gap >= MUSX_GAP_MAX) {
                    ctx.gap_sink.output(MUSX_GAP_MAX);
                    gap -= MUSX_GAP_MAX;
                }
                // todo omit
                ctx.gap_sink.output(gap);
                printf("<- %d ->\n", g.gap);
            }
        }
        if (!pass) {
            ctx.wrappers.begin_output(bitoutput);
            auto bitoutput2 = std::make_shared<byte_vector_bit_output>();
            printf("Bit size was %d (%d)\n", (int) bitoutput->bit_size(), (int) bitoutput->bit_size() / 8);
            output_channel_events(bitoutput2, ctx.event_channel_sink.huff);
            printf("Bit size now %d (%d)\n", (int) bitoutput2->bit_size(), (int) bitoutput2->bit_size() / 8);
            output_min_max_best(bitoutput2, ctx.delta_volume_sink.huff);
            output_min_max_best(bitoutput2, ctx.delta_pitch_sink.huff);
            output_min_max_best(bitoutput2, ctx.delta_vibrato_sink.huff);
            output_min_max_best(bitoutput2, ctx.press_note_sink.huff);
            output_min_max_best(bitoutput2, ctx.press_note9_sink.huff);
            output_min_max_best(bitoutput2, ctx.press_volume_sink.huff);
#if MUS_GROUP_SIZE_CODE
            output_min_max_best(bitoutput2, ctx.group_size_sink.huff);
#endif
            int last_ne = MUSX_RELEASE_DIST_COUNT - 1;
            assert(ctx.release_dist_sinks[0].huff.empty());
            assert(ctx.release_dist_sinks[1].huff.empty());
            for (; last_ne >= 3 && ctx.release_dist_sinks[last_ne].huff.empty(); last_ne--);
            bitoutput2->write(bit_sequence(last_ne, 4));
            for (int i = 2; i <= last_ne; i++) {
                output_min_max_best(bitoutput2, ctx.release_dist_sinks[i].huff);
            }
            output_min_max_best(bitoutput2, ctx.gap_sink.huff);
            printf("Bit size now %d (%d)\n", (int) bitoutput2->bit_size(), (int) bitoutput2->bit_size() / 8);
            bitoutput2->write_to(bitoutput);
        }
    }
    return bitoutput->get_output();
}

std::vector<uint8_t> compress_mus(std::pair<const int, lump> &e) {
    std::vector<seq_group> seq_groups;
    printf("MUS %s\n", e.second.name.c_str());
    printf("AAAAAAAA\n");
    if (mus2seq(e.second.data, seq_groups)) {
        fail("Error converting MUS track %s\n", e.second.name.c_str());
    }
    auto musx = compress_seq(seq_groups);
    auto raw = decode_musx(musx);
    std::vector<uint8_t> mus;
    mus.insert(mus.end(), e.second.data.begin(), e.second.data.begin() + 14); // copy header
    // force offset of data
    mus[6] = 14;
    mus[7] = 0;
    mus.insert(mus.end(), raw.begin(), raw.end());
    std::vector<seq_group> seq_groups2;
    printf("BBBBBBBB\n");
    mus2seq(mus, seq_groups2);
    return musx;
}



#ifndef count_of
#define count_of(a) (sizeof(a)/(sizeof((a)[0])))
#endif

std::vector<uint8_t> decode_musx(std::vector<uint8_t> &data) {
    std::vector<uint8_t> mus;
    typedef enum
    {
        mus_releasekey = 0x00,
        mus_presskey = 0x10,
        mus_pitchwheel = 0x20,
        mus_systemevent = 0x30,
        mus_changecontroller = 0x40,
        mus_scoreend = 0x60
    } musevent;


    byte_vector_bit_input vector_bi(data);
    th_bit_input *bi = create_bip(vector_bi);
    musx_decoder d;
    uint16_t decoder_buffer[512];
    uint8_t tmp_buffer[512];
    musx_decoder_space.record(musx_decoder_init(&d, bi, decoder_buffer, count_of(decoder_buffer), tmp_buffer, count_of(tmp_buffer)));
    bool done = false;
    std::vector<uint8_t> cmd;
    printf("CCCCCCCC\n");
    do {
        if (!d.group_remaining) {
            d.group_remaining = th_decode(d.decoders + d.group_size_idx, bi);
        }
        uint8_t ec = th_decode(d.decoders + d.channel_event_idx, bi);
        uint channel = ec >> 4;
        printf("%d ", channel);
        auto ev = static_cast<seq_event>(ec & 0xfu);
        cmd.clear();
        switch (ev) {
            case seq_event::change_controller: {
                uint8_t p1 = th_read_bits(bi, 4);
                uint8_t p2 = th_read_bits(bi, 8);
                printf("change controller %d %d\n", p1, p2);
                cmd.push_back(mus_changecontroller);
                cmd.push_back(p1);
                cmd.push_back(p2);
                break;
            }
            case seq_event::delta_volume: {
                auto delta = from_zig(th_decode(d.decoders + d.delta_volume_idx, bi));
                d.channel_last_volume[channel] += delta;
                cmd.push_back(mus_changecontroller);
                cmd.push_back(3);
                cmd.push_back(d.channel_last_volume[channel]);
                break;
            }
            case seq_event::delta_pitch: {
                auto delta = from_zig(th_decode(d.decoders + d.delta_pitch_idx, bi));
                d.channel_last_wheel[channel] += delta;
                printf("delta pitch %d, so %d\n", delta, d.channel_last_wheel[channel]);
                cmd.push_back(mus_pitchwheel);
                cmd.push_back(d.channel_last_wheel[channel]);
                break;
            }
            case seq_event::delta_vibrato: {
                auto delta = from_zig(th_decode(d.decoders + d.delta_vibrato_idx, bi));
                d.channel_last_vibrato[channel] += delta;
                printf("delta vibrato %d, so %d\n", delta, d.channel_last_vibrato[channel]);
                cmd.push_back(mus_changecontroller);
                cmd.push_back(2);
                cmd.push_back(d.channel_last_vibrato[channel]);
                break;
            }
            case seq_event::press_key: {
                auto note = th_decode(d.decoders + (channel == 9 ? d.press_note9_idx : d.press_note_idx), bi);
                auto vol = th_decode(d.decoders + d.press_volume_idx, bi);
                printf("press key %d vol %d", note, vol);
                cmd.push_back(mus_presskey);
                if (vol == vol_last_global) {
                    d.channel_last_volume[channel] = d.channel_last_press_volume[channel] = vol = d.last_volume;
                    cmd.push_back(note | 0x80);
                    cmd.push_back(vol);
                } else if (vol == vol_last_channel) {
                    cmd.push_back(note);
                    vol = d.channel_last_press_volume[channel];
                } else {
                    d.last_volume = d.channel_last_volume[channel] = d.channel_last_press_volume[channel] = vol;
                    cmd.push_back(note | 0x80);
                    cmd.push_back(vol);
                }
                printf(" so %d\n", vol);
                musx_record_note_on(&d, channel, note);
                break;
            }
            case seq_event::release_key: {
                assert(d.channel_note_count[channel]);
                uint8_t dist = 0;
                if (d.channel_note_count[channel] > 1) {
                    dist = th_decode(d.decoders + d.release_dist_idx[d.channel_note_count[channel]], bi);
                }
                uint8_t note = musx_record_note_off(&d, channel, dist);
                printf("release key dist %d note %d\n", dist, note);
                cmd.push_back(mus_releasekey);
                cmd.push_back(note);
                break;
            }
            case seq_event::system_event: {
                auto controller = th_read_bits(bi, 3) + 10;
                printf("system event %d\n", controller);
                cmd.push_back(mus_systemevent);
                cmd.push_back(controller);
                break;
            }
            case seq_event::score_end:
                printf("score end\n");
                cmd.push_back(mus_scoreend);
                done = true;
                break;
            default:
                assert(false);
        }
        int gap = 0;
        if (!--d.group_remaining) {
            if (!done) {
                int lgap = 0;
                do {
                    lgap = th_decode(d.decoders + d.gap_idx, bi);
                    gap += lgap;
                } while (lgap == MUSX_GAP_MAX);
            }
            if (gap) {
                printf("<- %d ->\n", gap);
            }
        }
        cmd[0] |= channel == 9 ? 15 : channel;
        if (gap) {
            cmd[0] |= 0x80;
        }
        mus.insert(mus.end(), cmd.begin(), cmd.end());
        if (gap) {
            uint marker = 0;
            for(int bit=21; bit>=0; bit-=7) {
                if (marker || gap <= (int)(0x7fu << bit)) {
                    marker = bit ? 0x80 : 0;
                    mus.push_back(marker | ((gap >> bit)&0x7fu));
                }
            }
        }
    } while (!done);
    uncreate_bip(vector_bi, bi);
    return mus;
}
//struct delta_comparator {
//                delta_comparator() = default;
//
//            public:
//                bool operator()(int8_t a, int8_t b) const {
//                    int aa = (a > 0) ? a * 2 - 1 : -a * 2;
//                    int bb = (b > 0) ? b * 2 - 1 : -b * 2;
//                    return aa < bb;
//                }
//            };
//

//byte_vector_bit_input bi(bitoutput->get_output());
//std::vector<int> channel_note_count(16);
//
//auto channel_event_decode = huffman_decoder<channel_event>(decode_channel_events(bi));
//auto delta_volume_decode = huffman_decoder<uint8_t>(decode_min_max8(bi));
////    auto delta_pitch_decode = huffman_decoder<uint8_t>(decode_min_max8(bi));
//static uint8_t foo[512];
//static uint16_t foo16[512];
//auto bip = create_bip(bi);
//auto delta_pitch_decode = foo16;
//th_read_simple_decoder(bip, foo16, foo, sizeof(foo));
//uncreate_bip(bi, bip);
//auto delta_vibrato_decode = huffman_decoder<uint8_t>(decode_min_max8(bi));
//auto press_note_decode = huffman_decoder<uint8_t>(decode_min_max8(bi));
//auto press_note9_decode = huffman_decoder<uint8_t>(decode_min_max8(bi));
//auto volume_decode = huffman_decoder<uint8_t>(decode_min_max8(bi));
//#if MUS_GROUP_SIZE_CODE
//auto group_size_decode = huffman_decoder<uint8_t>(decode_min_max8(bi));
//#endif
//int last_ne = bi.read(4);
//std::vector<huffman_decoder<uint8_t>> release_dist_decode(last_ne + 1);
//for (int i = 2; i <= last_ne; i++) {
//    release_dist_decode[i] = huffman_decoder<uint8_t>(decode_min_max8(bi));
//}
//auto gap_decode = huffman_decoder<uint8_t>(decode_min_max8(bi));
//
//bool done = false;
//do {
//    int count;
//#if MUS_GROUP_SIZE_CODE
//    count = group_size_decode.decode(bi);
//#else
//    count = 1;
//#endif
//    for (; count > 0; count--) {
//        auto ec = channel_event_decode.decode(bi);
//        uint channel = ec.first;
//        printf("%d ", channel);
//        switch (ec.second) {
//            case seq_event::change_controller: {
//                uint8_t p1 = bi.read(4);
//                uint8_t p2 = bi.read(8);
//                printf("change controller %d %d\n", p1, p2);
//                break;
//            }
//            case seq_event::delta_volume: {
//                auto delta = from_zig(delta_volume_decode.decode(bi));
//                printf("delta volume %d\n", delta);
//                break;
//            }
//            case seq_event::delta_pitch: {
//                //                    auto delta = from_zig(delta_pitch_decode.decode(bi));
//                auto bip = create_bip(bi);
//                auto delta = from_zig(th_decode(delta_pitch_decode, bip));
//                uncreate_bip(bi, bip);
//                printf("delta pitch %d\n", delta);
//                break;
//            }
//            case seq_event::delta_vibrato: {
//                auto delta = from_zig(delta_vibrato_decode.decode(bi));
//                printf("delta vibrato %d\n", delta);
//                break;
//            }
//            case seq_event::press_key: {
//                auto note = channel == 9 ? press_note9_decode.decode(bi) : press_note_decode.decode(bi);
//                auto vol = ctx.press_volume_sink.decode(bi);
//                channel_note_count[channel]++;
//                printf("press key %d vol %d\n", note, vol);
//                break;
//            }
//            case seq_event::release_key: {
//                assert(channel_note_count[channel]);
//                uint8_t dist = 0;
//                if (channel_note_count[channel] > 1) {
//                    dist = release_dist_decode[channel_note_count[channel]].decode(bi);
//                }
//                channel_note_count[channel]--;
//                printf("release key dist %d\n", dist);
//                break;
//            }
//            case seq_event::system_event: {
//                auto controller = bi.read(3) + 10;
//                printf("system event %d\n", controller);
//                break;
//            }
//            case seq_event::score_end:
//#if !MUS_PER_EVENT_GAP && !MUS_GROUP_SIZE_CODE
//    if (channel == 1) {
//        int gap = ctx.gap_sink.decode(bi);
//        if (gap) {
//            printf("<- %d ->\n", gap);
//        }
//        break;
//    }
//#endif
//printf("score end\n");
//    done = true;
//    break;
//    default:
//        assert(false);
//        }
//    }
//#if MUS_PER_EVENT_GAP || MUS_GROUP_SIZE_CODE
//int gap = 0;
//    if (!done) {
//        int lgap = 0;
//        do {
//            lgap = gap_decode.decode(bi);
//            gap += lgap;
//        } while (lgap == GAP_MAX);
//    }
//    if (gap) {
//        printf("<- %d ->\n", gap);
//    }
//#endif
//} while (!done);

//
//template<typename BI>
//std::vector<std::pair<channel_event, int>> decode_channel_events(BI &bi) {
//    int min_cl = bi.read(4);
//    int max_cl = bi.read(4);
//    printf("  Code length %d->%d\n", min_cl, max_cl);
//    std::vector<std::pair<channel_event, int>> symbol_lengths;
//    int bit_count = 32 - __builtin_clz(max_cl - min_cl);
//    for (uint ch = 0; ch < NUM_CHANNELS; ch++) {
//        if (bi.bit()) {
//            for (uint bit = 0; bit < 8; bit++) {
//                if (bi.bit()) {
//                    auto ev = static_cast<seq_event>(bit);
//                    int length;
//                    if (min_cl == max_cl) {
//                        length = min_cl;
//                    } else {
//                        length = min_cl + bi.read(bit_count);
//                    }
//                    symbol_lengths.template emplace_back(std::make_pair(ch, ev), length);
//                }
//            }
//        }
//    }
//    return symbol_lengths;
//}

// old stuff
//th_decoder create_decoder(const std::vector<std::pair<uint8_t, int>> &symbol_lengths) {
//#if 1
//    std::vector<uint16_t> code_ceiling;
//    std::vector<uint16_t> offset;
//    std::vector<uint8_t> symbols;
//    if (symbol_lengths.size() > 1) {
//        // could have passed this, but fine for now
//        int min_code_length = std::numeric_limits<int>::max();
//        int max_code_length = std::numeric_limits<int>::min();
//        for (const auto &e : symbol_lengths) {
//            min_code_length = std::min(min_code_length, e.second);
//            max_code_length = std::max(max_code_length, e.second);
//        }
//        code_ceiling.resize(max_code_length);
//        offset.resize(max_code_length);
//        std::vector<int> num_codes(max_code_length);
//        for (const auto &e : symbol_lengths) {
//            num_codes[e.second - 1]++;
//        }
//        for (int i = 1; i < max_code_length; i++) {
//            offset[i] = offset[i - 1] + num_codes[i - 1];
//        }
//        symbols.resize(symbol_lengths.size());
//        for (const auto &e : symbol_lengths) {
//            const auto length = e.second;
//            symbols[offset[length - 1]++] = e.first;
//        }
//        int code = 0;
//        int pos = 0;
//        for (int length = 0; length < max_code_length; length++) {
//            offset[length] = code - pos;
//            code += num_codes[length];
//            code_ceiling[length] = code;
//            pos += num_codes[length];
//            code <<= 1;
//        }
//        for (const auto &s : symbols) {
//            std::cout << static_cast<typename symbol_adapter<uint8_t>::ostream_type>(s) << "\n";
//        }
//    } else if (symbol_lengths.size() == 1) {
//        symbols.push_back(symbol_lengths[0].first);
//    }
//    auto decoder = new uint16_t[1 + offset.size() * 2 + (symbols.size() + 1) / 2];
//    if (symbols.empty()) {
//        decoder[0] = 0;
//    } else {
//        decoder[0] = 1 + offset.size() * 2;
//        for (uint i = 0; i < offset.size(); i++) {
//            decoder[1 + i * 2] = code_ceiling[i];
//            decoder[2 + i * 2] = offset[i];
//        }
//        memcpy(decoder + decoder[0], symbols.data(), symbols.size());
//    }
//
//    uint16_t *buf;
//    uint16_t *end;
//    uint size;
//    if (symbol_lengths.empty()) {
//        size = th_decoder_size(symbol_lengths.size(), 0);
//        buf = new uint16_t[size];
//        end = th_create_decoder(buf, nullptr, 0, 0);
//    } else {
//        int min_code_length = std::numeric_limits<int>::max();
//        int max_code_length = std::numeric_limits<int>::min();
//        for (const auto &e : symbol_lengths) {
//            min_code_length = std::min(min_code_length, e.second);
//            max_code_length = std::max(max_code_length, e.second);
//        }
//        size = th_decoder_size(symbol_lengths.size(), max_code_length);
//        buf = new uint16_t[size];
//        uint8_t *sal = new uint8_t[symbol_lengths.size() * 2];
//        for (int i = 0; i < symbol_lengths.size(); i++) {
//            sal[i * 2] = symbol_lengths[i].first;
//            sal[i * 2 + 1] = symbol_lengths[i].second;
//        }
//        end = th_create_decoder(buf, sal, symbol_lengths.size(), max_code_length);
//    }
//    assert(end == buf + size);
//    for (int i = 0; buf + i < end; i++) {
//        if (decoder[i] != buf[i]) {
//            printf("WAH %d %d %d\n", i, decoder[i], buf[i]);
//        }
//    }
//    return buf;
//#endif
//
//}

//template<typename BI> std::vector<std::pair<uint8_t, int>> decode_min_max8(BI &bi) {
//    bool non_empty = bi.bit();
//    if (!non_empty) {
//        printf("  No values\n");
//        return {};
//    } else {
//        bool group8 = bi.bit();
//        uint8_t min = bi.read(8);
//        uint8_t max = bi.read(8);
//        if (min == max) {
//            printf("  %d: 0 bits\n", min);
//            return {std::make_pair(min, 0)};
//        }
//        printf("  Min/Max %d/%d\n", min, max);
//        int min_cl = bi.read(4);
//        int max_cl = bi.read(4);
//        printf("  Code length %d->%d\n", min_cl, max_cl);
//        std::vector<std::pair<uint8_t, int>> symbol_lengths;
//        if (min_cl == max_cl) {
//            for (uint val = min; val <= max; val++) {
//                if (bi.bit()) {
//                    printf("  %d: %d bits\n", val, min_cl);
//                    symbol_lengths.template emplace_back(val, min_cl);
//                }
//            }
//        } else {
//            int bit_count = 32 - __builtin_clz(max_cl - min_cl);
//            if (group8) {
//                for (int base_val = min; base_val <= max; base_val += 8) {
//                    bool all_same = bi.bit();
//                    if (all_same) {
//                        if (bi.bit()) {
//                            for (uint i = 0; i <= std::min(7, max - base_val); i++) {
//                                int code_length = min_cl + bi.read(bit_count);
//                                symbol_lengths.template emplace_back(base_val + i, code_length);
//                                printf("  %d: %d bits\n", base_val + i, code_length);
//                            }
//                        }
//                    } else {
//                        for (int i = 0; i <= std::min(7, max - base_val); i++) {
//                            if (bi.bit()) {
//                                int code_length = min_cl + bi.read(bit_count);
//                                symbol_lengths.template emplace_back(base_val + i, code_length);
//                                printf("  %d: %d bits\n", base_val + i, code_length);
//                            }
//                        }
//                    }
//                }
//            } else {
//                for (int val = min; val <= max; val++) {
//                    if (bi.bit()) {
//                        int code_length = min_cl + bi.read(bit_count);
//                        symbol_lengths.template emplace_back(val, code_length);
//                        printf("  %d: %d bits\n", val, code_length);
//                    }
//                }
//            }
//        }
//        return symbol_lengths;
//    }
//}


//    ctx.wrappers.dump();
#if 0
for(int event = 0; event< 8; event++) {
    std::map<uint16_t, int> chgroups;
    std::map<uint16_t, int> chgroups_same;
    int event_count = 0;
    std::vector<uint8_t> channels;
    for(const auto &g : groups) {
        // look for channel groupings
        for(int i=0;i<g.items.size();i++) {
            uint16_t mask = 1u << g.items[i].channel;
            if (g.items[i].event != static_cast<seq_event>(event)) continue;
            channels.push_back(g.items[i].channel);
            event_count++;
            for(int j=i+1;j<g.items.size();j++) {
                uint bit = 1u << g.items[j].channel;
                if (bit > mask && g.items[i].event == g.items[j].event) {
                    mask |= bit;
                } else {
                    break;
                }
            }
            assert(mask);
            uint first_bit = __builtin_ctz(mask);
            if ((1u << first_bit) != mask) {
                bool same = true;
                const auto &e1 = g.items[first_bit];
                for(uint j = first_bit + 1; j < 16; j++) {
                    if (mask & (1u << j)) {
                        const auto &e2 = g.items[j];
                        if (e1.p1 != e2.p1 || e1.p2 != e2.p2) {
                            same = false;
                            break;
                        }
                    }
                }
                if (same) {
                    chgroups_same[mask]++;
                }
            }
            chgroups[mask]++;
        }
    }
    printf("CHGROUPS %s\n", seq_event_name((seq_event)event));
    for(const auto& e: chgroups_same) {
        assert(chgroups[e.first]!=0);
    }
    auto single_bit_lengths = huff(channels.data(), channels.size(), true);
    for(const auto& e: chgroups) {
        int separate_length = 0;
        for(int i=0;i<16;i++) {
            if (e.first & (1u << i)) {
                separate_length += single_bit_lengths[i];
            }
        }
        int length = ceil(-log2( e.second / (double)event_count));
        if (length > separate_length && __builtin_popcount(e.first) > 1) continue;
        for(int i=0x8000;i>0;i>>=1) {
            putchar(e.first & i ? '1' : '0');
        }
        putchar(__builtin_popcount(e.first) == 1 ? '*' : ' ');
        printf(" % 6d % 6d %d vs %d", e.second, e.second * __builtin_popcount(e.first), length, separate_length);
        if (chgroups_same[e.first])
            printf(" (%d / %d)\n", chgroups_same[e.first] * __builtin_popcount(e.first), chgroups_same[e.first]);
        else
            printf("\n");
    }
}
#endif
