#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <assert.h>
typedef unsigned int uint;

#pragma once

#define TH_IDX_CEILING 0
#define TH_IDX_OFFSET 1

typedef const uint16_t *th_decoder;

// tiny_huff_decoder
// 0000: symbol_offset (0 means no symbols)
// 0001: <length 1> ceiling
// 0002: <length 1> offset
// symbol_offset: symbol bytes

// slightly faster it seems
#if !IS_WHD_GEN
#define TH_USE_ACCUM 1
#endif

typedef struct {
    const uint8_t *cur;
#ifndef NDEBUG
    const uint8_t *end;
#endif
#if TH_USE_ACCUM
    uint32_t accum;
    uint8_t bits;
#else
    uint8_t bit;
#endif
} th_bit_input;

typedef struct {
    const uint8_t *cur;
    uint32_t accum;
    uint8_t bits;
} th_backwards_bit_input;


static inline void th_bit_input_init(th_bit_input *bi, const uint8_t *data) {
    bi->cur = data;
#ifndef NDEBUG
    bi->end = 0;
#endif
#if TH_USE_ACCUM
    bi->accum = 0;
    bi->bits = 0;
#else
    bi->bit = 0;
#endif
}

static inline void th_sized_bit_input_init(th_bit_input *bi, const uint8_t *data, uint size) {
    bi->cur = data;
#ifndef NDEBUG
    bi->end = data + size;
#endif
#if TH_USE_ACCUM
    bi->accum = 0;
    bi->bits = 0;
#else
    bi->bit = 0;
#endif
}

static inline void th_bit_input_init_bit_offset(th_bit_input *bi, const uint8_t *data, uint bit_offset) {
    bi->cur = data + bit_offset / 8;
#ifndef NDEBUG
    bi->end = 0;
#endif
#if TH_USE_ACCUM
    bi->bits = 8 - (bit_offset & 7);
    bi->accum = *bi->cur++ >> (bit_offset & 7);
#else
    bi->bit = 0;
#endif
}

static inline void th_sized_bit_input_init_bit_offset(th_bit_input *bi, const uint8_t *data, uint size, uint bit_offset) {
    bi->cur = data + bit_offset / 8;
#ifndef NDEBUG
    bi->end = data + size - bit_offset / 8;
#endif
#if TH_USE_ACCUM
    bi->bits = 8 - (bit_offset & 7);
    bi->accum = *bi->cur++ >> (bit_offset & 7);
#else
    bi->bit = 0;
#endif
}

static inline void th_backwards_bit_input_init(th_backwards_bit_input *bi, const uint8_t *data) {
    bi->cur = data;
    bi->bits = 0;
    bi->accum = 0;
}

static inline void th_backwards_bit_input_init_bit_offset(th_backwards_bit_input *bi, const uint8_t *data, uint bit_offset) {
    bi->cur = data + bit_offset / 8;
    bi->bits = bit_offset & 7u;
    bi->accum = *bi->cur & ((1u << bi->bits)-1); // if we start on a non byte boundary, then the bits are in the wrong place
}

void th_bit_overrun(th_bit_input *bi); // global and user provided

static inline int th_bit(th_bit_input *bi) {
#if TH_USE_ACCUM
    if (!bi->bits) {
#ifndef NDEBUG
        if (bi->cur == bi->end) th_bit_overrun(bi);
#endif
        bi->accum = *bi->cur++;
        bi->bits = 8;
    }
    bi->bits--;
    uint rc = bi->accum & 1u;
    bi->accum >>= 1;
    return rc;
#else
#ifndef NDEBUG
    if (bi->cur == bi->end) th_bit_overrun(bi);
#endif
    uint rc = *bi->cur & (1u << bi->bit++);
    bi->cur += bi->bit >> 3u;
    bi->bit &= 7u;
    return rc != 0;
#endif
}
//static inline uint th_bit(th_bit_input *bi) {
//}

#if TH_USE_ACCUM
static inline void th_fill_byte(th_bit_input *bi) {
    assert(bi->bits < 24);
//    if (bi->bits < 8) {
#ifndef NDEBUG
        if (bi->cur == bi->end) th_bit_overrun(bi);
#endif
        bi->accum |= *bi->cur++ << bi->bits;
        bi->bits += 8;
//    }
}
#endif

static inline uint th_read_bits(th_bit_input *bi, int n) {
#if TH_USE_ACCUM
    assert(n<=32);
    while (bi->bits < n) {
        th_fill_byte(bi);
    }
    bi->bits -= (int8_t)n;
    uint tmp = bi->accum;
    bi->accum >>= n;
    return tmp ^ (bi->accum << n);
#else
    assert(n<=32);
#ifndef NDEBUG
    if (bi->cur == bi->end) th_bit_overrun(bi);
#endif
    uint accum = *bi->cur >> bi->bit;
    uint pos = 8 - bi->bit;
    while ((int)pos < n) {
        bi->cur++;
#ifndef NDEBUG
        if (bi->cur == bi->end) th_bit_overrun(bi);
#endif
        accum |= *bi->cur << pos;
        pos += 8;
    }
    bi->bit = (bi->bit + n) & 7u;
    if (!bi->bit) {
        bi->cur++;
    }
    return accum & ((1u << n) - 1u);
#endif
}

static inline uint th_read32(th_bit_input *bi) {
    return th_read_bits(bi, 16) | (th_read_bits(bi, 16) << 16);
}

static inline uint th_read_backwards_bits(th_backwards_bit_input *bi, uint n) {
    assert(n<=32);
    bi->bits -= n;
    while (bi->bits > 32) { // really negative
        bi->accum = (bi->accum << 8) | *--bi->cur;
        bi->bits += 8;
    }
    uint tmp = bi->accum >> bi->bits;
    bi->accum ^= tmp << bi->bits;
    return tmp;
}

static inline uint8_t th_decode(th_decoder decoder, th_bit_input *bi) {
    assert(decoder[0]);
    const uint8_t *symbols = (uint8_t *)(decoder + decoder[0]);
    if (decoder[0] == 1) {
        return symbols[0];
    }
//    printf("DECODE AT %d %p.%d ", xarn ++, bi->cur, bi->bit);
    uint code = 0;
#ifndef NDEBUG
    uint max_code_length = (decoder[0] - 1)/2;
    uint length = 0;
#endif
    decoder++;
    do {
        code = (code << 1u) | th_bit(bi);
        if (code < decoder[TH_IDX_CEILING]) {
//            printf(" code %04x len %d symbol %02x\n", code, length + 1, symbols[code - decoder[TH_IDX_OFFSET]]);
            return symbols[code - decoder[TH_IDX_OFFSET]];
        }
        decoder += 2;
#ifndef NDEBUG
        length++;
        assert(length < max_code_length);
#endif
    } while (1);
}

extern const uint8_t reverse8[256];
int th_make_prefix_length_table(th_decoder decoder, uint8_t *prefix_lengths);
static inline uint8_t th_decode_table_special(th_decoder decoder, const uint8_t *prefix_lengths, th_bit_input *bi) {
    assert(decoder[0] > 1); // we should be called for the empty decoder case
#if TH_USE_ACCUM
    if (bi->bits < 8) th_fill_byte(bi);
    uint code = bi->accum;
#else
    uint code = *bi->cur >> bi->bit;
//    printf("DECODE AT %d %p.%d ", xarn++, bi->cur, bi->bit);
    if (bi->bit) {
        code |= ((bi->cur[1] << 8) >> bi->bit);
    }
#endif
    code &= 0xff;
    uint8_t length = prefix_lengths[code];
    const uint8_t *symbols = (uint8_t *)(decoder + decoder[0]);
    if (length) {
        assert(length <= 8);
        code = reverse8[(uint8_t)(code << (8 - length))];
        assert(code < decoder[TH_IDX_CEILING + 2*length-1]);
#if TH_USE_ACCUM
        bi->bits -= length;
        assert(bi->bits >= 0);
        bi->accum >>= length;
#else
        bi->bit += length;
        bi->cur += bi->bit >> 3;
        bi->bit &= 7;
#endif
//        printf(" code %04x len %d symbol %02x\n", code, length, symbols[code - decoder[TH_IDX_OFFSET + 2*length-1]]);
        return symbols[code - decoder[TH_IDX_OFFSET + 2*length-1]];
    }
//    printf("otherwise\n");
    // we have advanced exactly one byte
#if !TH_USE_ACCUM
    bi->cur++;
#else
    bi->accum >>= 8;
    bi->bits -= 8;
    assert(bi->bits >= 0);
#endif
#ifndef NDEBUG
    uint max_code_length = (decoder[0] - 1)/2;
    length = 8;
#endif
    code = reverse8[code];
    decoder += 17;
    do {
        code = (code << 1u) | th_bit(bi);
        if (code < decoder[TH_IDX_CEILING]) {
            return symbols[code - decoder[TH_IDX_OFFSET]];
        }
        decoder += 2;
#ifndef NDEBUG
        length++;
        assert(length < max_code_length);
#endif
    } while (1);
}

static inline uint16_t th_decode_table_special_16(th_decoder decoder, const uint8_t *prefix_lengths, th_bit_input *bi) {
    assert(decoder[0] > 1); // we should be called for the empty decoder case
#if TH_USE_ACCUM
    if (bi->bits < 8) th_fill_byte(bi);
    uint code = bi->accum;
#else
    uint code = *bi->cur >> bi->bit;
//    printf("DECODE AT %d %p.%d ", xarn++, bi->cur, bi->bit);
    if (bi->bit) {
        code |= ((bi->cur[1] << 8) >> bi->bit);
    }
#endif
    code &= 0xff;
    uint8_t length = prefix_lengths[code];
    const uint16_t *symbols = (uint16_t *)(decoder + decoder[0]);
    if (length) {
        assert(length <= 8);
        code = reverse8[(uint8_t)(code << (8 - length))];
        assert(code < decoder[TH_IDX_CEILING + 2*length-1]);
#if TH_USE_ACCUM
        bi->bits -= length;
        assert(bi->bits >= 0);
        bi->accum >>= length;
#else
        bi->bit += length;
        bi->cur += bi->bit >> 3;
        bi->bit &= 7;
#endif
//        printf(" code %04x len %d symbol %02x\n", code, length, symbols[code - decoder[TH_IDX_OFFSET + 2*length-1]]);
        return symbols[code - decoder[TH_IDX_OFFSET + 2*length-1]];
    }
//    printf("otherwise\n");
    // we have advanced exactly one byte
#if !TH_USE_ACCUM
    bi->cur++;
#else
    bi->accum >>= 8;
    bi->bits -= 8;
    assert(bi->bits >= 0);
#endif
#ifndef NDEBUG
    uint max_code_length = (decoder[0] - 1)/2;
    length = 8;
#endif
    code = reverse8[code];
    decoder += 17;
    do {
        code = (code << 1u) | th_bit(bi);
        if (code < decoder[TH_IDX_CEILING]) {
            return symbols[code - decoder[TH_IDX_OFFSET]];
        }
        decoder += 2;
#ifndef NDEBUG
        length++;
        assert(length < max_code_length);
#endif
    } while (1);
}

#if 0
// todo asm-ify // note we need at most 15 bits
static uint8_t th_decode_fast_special(th_decoder decoder, th_bit_input *bi) {
    assert(decoder[0] > 1); // we should be called for the empty decoder case
    const uint8_t *symbols = (uint8_t *)(decoder + decoder[0]);
    uint code = 0;
    int count;
    decoder++;
    do {
#if TH_USE_ACCUM
        if (bi->bits < 8) th_fill_byte(bi);
        uint bits = bi->accum;
        count = bi->bits;
#else
        assert(bi->cur < bi->end);
        assert(bi->bit <= 7);
        uint8_t bits = *bi->cur >> bi->bit;
        count = 7 - bi->bit; // implicit count = 8 - bi->bit; if (count--) {
#endif
        code = (code << 1) | (bits & 1); if (code < decoder[TH_IDX_CEILING]) break;
        bits >>=1; decoder += 2;
        if (count--) {
            code = (code << 1) | (bits & 1); if (code < decoder[TH_IDX_CEILING]) break;
            bits >>=1; decoder += 2;
            if (count--) {
                code = (code << 1) | (bits & 1); if (code < decoder[TH_IDX_CEILING]) break;
                bits >>=1; decoder += 2;
                if (count--) {
                    code = (code << 1) | (bits & 1); if (code < decoder[TH_IDX_CEILING]) break;
                    bits >>=1; decoder += 2;
                    if (count--) {
                        code = (code << 1) | (bits & 1); if (code < decoder[TH_IDX_CEILING]) break;
                        bits >>=1; decoder += 2;
                        if (count--) {
                            code = (code << 1) | (bits & 1); if (code < decoder[TH_IDX_CEILING]) break;
                            bits >>=1; decoder += 2;
                            if (count--) {
                                code = (code << 1) | (bits & 1); if (code < decoder[TH_IDX_CEILING]) break;
                                bits >>=1; decoder += 2;
                                if (count--) {
                                    code = (code << 1) | (bits & 1); if (code < decoder[TH_IDX_CEILING]) break;
                                    decoder += 2;
                                }
                            }
                        }
                    }
                }
            }
        }
#if TH_USE_ACCUM
        bi->bits -= 8;
        assert(bi->bits >= 0);
        bi->accum >>= 8;
#else
        bi->cur++;
        bi->bit = 0;
#endif
    } while (1);
#if TH_USE_ACCUM
    assert(count >= 0 && count < bi->bits);
    bi->accum >>= (bi->bits - count);
    bi->bits = count;
#else
    bi->bit = (8 - count) & 7u;
    if (!bi->bit) bi->cur++;
    assert(bi->cur < bi->end || (!bi->bit && bi->cur == bi->end));
#endif
    return symbols[code - decoder[TH_IDX_OFFSET]];
}
#endif

static inline uint16_t th_decode_16(th_decoder decoder, th_bit_input *bi) {
    assert(decoder[0]);
    const uint16_t *symbols = decoder + decoder[0];
    if (decoder[0] == 1) {
        return symbols[0];
    }
    uint code = 0;
#ifndef NDEBUG
    uint max_code_length = (decoder[0] - 1)/2;
    uint length = 0;
#endif
    decoder++;
    do {
        code = (code << 1u) | th_bit(bi);
        if (code < decoder[TH_IDX_CEILING]) {
            return symbols[code - decoder[TH_IDX_OFFSET]];
        }
        decoder += 2;
#ifndef NDEBUG
        length++;
        assert(length < max_code_length);
#endif
    } while (1);
}

uint th_decoder_size(uint count, uint max_code_length);
uint16_t *th_create_decoder(uint16_t *buffer, const uint8_t *symbols_and_lengths, uint count, uint max_code_length);
uint th_decoder_size_16(uint count, uint max_code_length);
uint16_t *th_create_decoder_16(uint16_t *buffer, const uint8_t *symbols_and_lengths, uint count, uint max_code_length);
uint16_t *th_read_simple_decoder(th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size);

static inline int to_zig(int x) {
    if (x > 0) {
        return x * 2 - 1;
    } else {
        return -x * 2;
    }
}

static inline int from_zig(int x) {
    if (x & 1) {
        return (x + 1) / 2;
    } else {
        return -x / 2;
    }
}

typedef struct {
    uint8_t *cur;
    uint32_t accum;
    uint8_t *end;
    uint8_t bits;
} th_bit_output;

static inline void th_bit_output_init(th_bit_output *bo, uint8_t *buffer, uint size) {
    bo->cur = buffer;
    bo->accum = 0;
    bo->bits= 0;
    bo->end = buffer + size;
}

static inline void th_flush_bytes(th_bit_output *bo) {
    if (bo->cur == bo->end) return;
    while (bo->bits >= 8) {
        assert(bo->cur < bo->end);
        *bo->cur++ = bo->accum;
        bo->accum >>= 8;
        bo->bits -= 8;
    }
}

static inline void th_write_bits(th_bit_output *bo, uint bits, uint n) {
    assert(bo->bits + n <= 32);
    assert(bits < (1u << n));
    bo->accum |= bits << bo->bits;
    bo->bits += n;
    th_flush_bytes(bo);
}

static inline void th_write32(th_bit_output *bo, uint bits) {
    th_write_bits(bo, bits & 0xffffu, 16);
    th_write_bits(bo, bits >> 16, 16);
}

#ifdef __cplusplus
}
#endif