/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#if PICO_BUILD
#include "pico.h"
#else
#define __not_in_flash_func(x) x
#endif
#include "image_decoder.h"
#pragma GCC push_options
#if PICO_ON_DEVICE
#pragma GCC optimize("O3")
#endif

uint16_t *__not_in_flash_func(read_raw_pixels_decoder)(th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size) {
    uint min_cl = th_read_bits(bi, 4);
    uint max_cl = th_read_bits(bi, 4);
    uint bit_count = 32 - __builtin_clz(max_cl - min_cl);
    uint count = 0;
    for (uint p = 0; p < 32; p++) {
        if (th_bit(bi)) {
            for (uint bit = 0; bit < 8; bit++) {
                if (th_bit(bi)) {
                    uint length;
                    if (min_cl == max_cl) {
                        length = min_cl;
                    } else {
                        assert(min_cl < max_cl);
                        length = min_cl + th_read_bits(bi, bit_count);
                    }
                    assert(count * 2 + 1 < tmp_buf_size);
                    tmp_buf[count * 2] = (p << 3) | bit;
                    tmp_buf[count * 2 + 1] = length;
                    count++;
                }
            }
        }
    }
    assert(buffer_size >= th_decoder_size(count, max_cl));
    return th_create_decoder(buffer, tmp_buf, count, max_cl);
}

uint16_t *__not_in_flash_func(read_raw_pixels_decoder_c3)(th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size) {
    uint min_cl = th_read_bits(bi, 4);
    uint max_cl = th_read_bits(bi, 4);
//    printf("  Code length %d->%d\n", min_cl, max_cl);
    uint bit_count = 32 - __builtin_clz(max_cl - min_cl);
    uint count = 0;
    for (uint p = 0; p < 32; p++) {
        if (th_bit(bi)) {
            for (uint bit = 0; bit < 8; bit++) {
                if (th_bit(bi)) {
                    uint length;
                    if (min_cl == max_cl) {
                        length = min_cl;
                    } else {
                        length = min_cl + th_read_bits(bi, bit_count);
                    }
                    assert(count * 3 + 2 < tmp_buf_size);
                    tmp_buf[count * 3] = (p << 3) | bit;
//                    printf("0,%d = %d\n", (p << 3) | bit, length);
                    tmp_buf[count * 3 + 1] = 0;
                    tmp_buf[count * 3 + 2] = length;
                    count++;
                }
            }
        }
    }
    bit_count = 32 - __builtin_clz(1 + max_cl - min_cl);
    for(uint8_t i=0;i<7;i++) {
        uint length = th_read_bits(bi, bit_count);
        if (length != (1u << bit_count)-1) {
            length += min_cl;
            assert(count * 3 + 2 < tmp_buf_size);
            tmp_buf[count * 3] = i;
//            printf("1,%d = %d\n", i, length);
            tmp_buf[count * 3 + 1] = 1;
            tmp_buf[count * 3 + 2] = length;
            count++;
        }
    }

    assert(buffer_size >= th_decoder_size(count, max_cl));
    return th_create_decoder_16(buffer, tmp_buf, count, max_cl);
}

#pragma GCC pop_options

void decode_data(uint8_t *dest, uint len, th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size) {
    uint16_t *pos;
    if (th_bit(bi)) {
        pos = th_read_simple_decoder(bi, buffer, buffer_size, tmp_buf, tmp_buf_size);
    } else {
        pos = read_raw_pixels_decoder(bi, buffer, buffer_size, tmp_buf, tmp_buf_size);
    }
    th_make_prefix_length_table(buffer, tmp_buf);
    assert(pos < buffer + buffer_size);
    for(int i=0;i<len;i++) {
        dest[i] = th_decode_table_special(buffer, tmp_buf, bi);
    }
}