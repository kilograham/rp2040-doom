#include "tiny_huff.h"
#include <string.h>
#include <stdio.h>
#if PICO_BUILD
#include "pico.h"
#else
#define __not_in_flash_func(x) x
#endif

#ifndef NDEBUG
//#define DUMP 1
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
uint th_decoder_size(uint count, uint max_code_length) {
    if (!count) {
        return 1;
    } else if (count == 1) {
        return 2;
    } else {
        return 1 + max_code_length * 2 + (count + 1) / 2;
    }
}

uint th_decoder_size_16(uint count, uint max_code_length) {
    if (!count) {
        return 1;
    } else if (count == 1) {
        return 2;
    } else {
        return 1 + max_code_length * 2 + count;
    }
}


uint16_t *th_create_decoder(uint16_t *buffer, const uint8_t *symbols_and_lengths, uint count, uint max_code_length) {
    if (!count) {
        *buffer++ = 0;
    } else if (count == 1) {
        buffer[0] = 1;
        *(uint8_t*)(buffer + 1) = symbols_and_lengths[0];
        buffer += 2;
    } else {
        buffer[0] = 1 + max_code_length * 2;
        uint8_t *symbols = (uint8_t *)(buffer + buffer[0]);
        buffer++;
        memset(buffer, 0, max_code_length * 2 * sizeof(uint16_t));
        // pre-use ceiling slot for num_codes
        #define TH_IDX_NUM_CODES TH_IDX_CEILING
        for(int i=0;i<count;i++) {
            int length = symbols_and_lengths[i*2+1];
            assert(length > 0 && length <= max_code_length);
            length--; // length 1 at index 0
            buffer[length * 2 + TH_IDX_NUM_CODES]++;
        }
        // init offsets
        for(int length = 1; length < max_code_length; length++) {
            buffer[length * 2 + TH_IDX_OFFSET] = buffer[(length-1)*2 + TH_IDX_OFFSET] + buffer[(length-1)*2 + TH_IDX_NUM_CODES];
        }
        // assign symbols
        for(int i=0;i<count;i++) {
            int length = symbols_and_lengths[i*2+1]-1;
            symbols[buffer[length * 2 + TH_IDX_OFFSET]++] = symbols_and_lengths[i*2];
        }
        int code = 0;
        int pos = 0;
        for(int length = 0; length < max_code_length; length++) {
#if DUMP
            printf("Code at length %d: ", length);
            for(int i=length;i>=0;i--) {
                putchar((code & (1u << i)) ? '1' : '0');
            }
            printf("\n");
#endif
            int num_codes = buffer[length * 2 + TH_IDX_NUM_CODES];
            buffer[length * 2 + TH_IDX_OFFSET] = code - pos;
            code += num_codes;
#if DUMP
            printf("Last at length %d: ", length);
            for(int i=length;i>=0;i--) {
                putchar((code & (1u << i)) ? '1' : '0');
            }
            printf("\n");
#endif
            buffer[length * 2 + TH_IDX_CEILING] = code;
            pos += num_codes;
            code <<= 1;
        }
        buffer += max_code_length * 2 + (count + 1) / 2;
    }
    return buffer;
}

uint16_t *th_create_decoder_16(uint16_t *buffer, const uint8_t *symbols_and_lengths, uint count, uint max_code_length) {
    if (!count) {
        *buffer++ = 0;
    } else if (count == 1) {
        buffer[0] = 1;
        buffer[1] = symbols_and_lengths[0] + (symbols_and_lengths[1] << 8);
        buffer += 2;
    } else {
        buffer[0] = 1 + max_code_length * 2;
        uint16_t *symbols = (uint16_t *)(buffer + buffer[0]);
        buffer++;
        memset(buffer, 0, max_code_length * 2 * sizeof(uint16_t));
        // pre-use ceiling slot for num_codes
        #define TH_IDX_NUM_CODES TH_IDX_CEILING
        for(int i=0;i<count;i++) {
            int length = symbols_and_lengths[i*3+2];
            assert(length > 0 && length <= max_code_length);
            length--; // length 1 at index 0
            buffer[length * 2 + TH_IDX_NUM_CODES]++;
        }
        // init offsets
        for(int length = 1; length < max_code_length; length++) {
            buffer[length * 2 + TH_IDX_OFFSET] = buffer[(length-1)*2 + TH_IDX_OFFSET] + buffer[(length-1)*2 + TH_IDX_NUM_CODES];
        }
        // assign symbols
        for(int i=0;i<count;i++) {
            int length = symbols_and_lengths[i*3+2]-1;
            symbols[buffer[length * 2 + TH_IDX_OFFSET]++] = symbols_and_lengths[i*3] + (symbols_and_lengths[i*3+1] << 8);
        }
        int code = 0;
        int pos = 0;
        for(int length = 0; length < max_code_length; length++) {
#if DUMP
            printf("Code at length %d: ", length);
            for(int i=length;i>=0;i--) {
                putchar((code & (1u << i)) ? '1' : '0');
            }
            printf("\n");
#endif
            int num_codes = buffer[length * 2 + TH_IDX_NUM_CODES];
            buffer[length * 2 + TH_IDX_OFFSET] = code - pos;
            code += num_codes;
#if DUMP
            printf("Last at length %d: ", length);
            for(int i=length;i>=0;i--) {
                putchar((code & (1u << i)) ? '1' : '0');
            }
            printf("\n");
#endif
            buffer[length * 2 + TH_IDX_CEILING] = code;
            pos += num_codes;
            code <<= 1;
        }
        buffer += max_code_length * 2 + count;
    }
    return buffer;
}

uint16_t *th_read_simple_decoder(th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size) {
    int non_empty = th_bit(bi);
    if (!non_empty) {
#if DUMP
        printf("  empty\n");
#endif
        // inlined from create_decoder
        *buffer++ = 0;
        return buffer;
    } else {
        int group8 = th_bit(bi);
        uint8_t min = th_read_bits(bi, 8);
        uint8_t max = th_read_bits(bi, 8);
        if (min == max) {
            // inlined from create_decoder
            buffer[0] = 1;
            *(uint8_t*)(buffer + 1) = min;
            buffer += 2;
            return buffer;
        }
#if DUMP
        printf("  Min/Max %d/%d\n", min, max);
#endif
        uint min_cl = th_read_bits(bi, 4);
        uint max_cl = th_read_bits(bi, 4);
#if DUMP
        printf("  Code length %d->%d\n", min_cl, max_cl);
#endif
        uint count = 0;
        if (min_cl == max_cl) {
            for(uint val=min;val<=max;val++) {
                if (th_bit(bi)) {
#if DUMP
                    printf("  %d: %d bits\n", val, min_cl);
#endif
                    assert(count * 2 + 1 < tmp_buf_size);
                    tmp_buf[count*2] = val;
                    tmp_buf[count*2+1] = min_cl;
                    count++;
                }
            }
        } else {
            uint bit_count = 32 - __builtin_clz(max_cl - min_cl);
            if (group8) {
                for (int base_val = min; base_val <= max; base_val += 8) {
                    int all_same = th_bit(bi);
                    if (all_same) {
                        if (th_bit(bi)) {
                            for(uint i=0;i<=MIN(7, max - base_val); i++) {
                                uint code_length = min_cl + th_read_bits(bi, bit_count);
                                assert(count * 2 + 1 < tmp_buf_size);
                                tmp_buf[count*2] = base_val + i;
                                tmp_buf[count*2+1] = code_length;
                                count++;
#if DUMP
                                printf("  %d: %d bits\n", base_val + i, code_length);
#endif
                            }
                        }
                    } else {
                        for(int i=0;i<=MIN(7, max - base_val); i++) {
                            if (th_bit(bi)) {
                                uint code_length = min_cl + th_read_bits(bi, bit_count);
                                assert(count * 2 + 1 < tmp_buf_size);
                                tmp_buf[count*2] = base_val + i;
                                tmp_buf[count*2+1] = code_length;
                                count++;
#if DUMP
                                printf("  %d: %d bits\n", base_val + i, code_length);
#endif
                            }
                        }
                    }
                }
            } else {
                for (int val = min; val <= max; val++) {
                    if (th_bit(bi)) {
                        uint code_length = min_cl + th_read_bits(bi, bit_count);
                        assert(count * 2 + 1 < tmp_buf_size);
                        tmp_buf[count*2] = val;
                        tmp_buf[count*2+1] = code_length;
                        count++;
#if DUMP
                        printf("  %d: %d bits\n", val, code_length);
#endif
                    }
                }
            }
        }
        assert(buffer_size >= th_decoder_size(count, max_cl));
        return th_create_decoder(buffer, tmp_buf, count, max_cl);
    }
}

const uint8_t reverse8[256] = {
        0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
        0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
        0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
        0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
        0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
        0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
        0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
        0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
        0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
        0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
        0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
        0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
        0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
        0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
        0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
        0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

#pragma GCC push_options
#if PICO_ON_DEVICE
#pragma GCC optimize("O3")
#endif

int __not_in_flash_func(th_make_prefix_length_table)(th_decoder decoder, uint8_t *prefix_lengths) {
    int max_length = decoder[0] / 2;
    if (max_length > 8) max_length = 8;
    decoder++;
    int code = 0;
    memset(prefix_lengths, 0, 256);
    for(int length=1; length<=max_length;length++) {
        for(;code < decoder[TH_IDX_CEILING];code++) {
            for(int i=0;i < (1u << (8-length)); i++) {
                prefix_lengths[reverse8[i | (code << (8-length))]] = length;
            }
        }
        code <<= 1;
        decoder += 2;
    }
    return max_length;
}
#pragma GCC pop_options