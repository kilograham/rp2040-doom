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

uint16_t *read_raw_pixels_decoder(th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size);
uint16_t *read_raw_pixels_decoder_c3(th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size);
void decode_data(uint8_t *dest, uint len, th_bit_input *bi, uint16_t *buffer, uint buffer_size, uint8_t *tmp_buf, uint tmp_buf_size);
#ifdef __cplusplus
}
#endif