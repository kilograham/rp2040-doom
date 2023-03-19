/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_PIMORONI_COSMIC_UNICORN_H
#define _BOARDS_PIMORONI_COSMIC_UNICORN_H

// For board detection
#define PIMORONI_COSMIC_UNICORN

// Audio pins. I2S BCK, LRCK are on the same pins as PWM L/R.
// - When outputting I2S, PWM sees BCK and LRCK, which should sound silent as
//   they are constant duty cycle, and above the filter cutoff
// - When outputting PWM, I2S DIN should be low, so I2S should remain silent.
#define PIMORONI_COSMIC_UNICORN_I2S_DIN_PIN 9
#define PIMORONI_COSMIC_UNICORN_I2S_BCK_PIN 10
#define PIMORONI_COSMIC_UNICORN_I2S_LRCK_PIN 11

#ifndef PICO_AUDIO_I2S_DATA_PIN
#define PICO_AUDIO_I2S_DATA_PIN PIMORONI_COSMIC_UNICORN_I2S_DIN_PIN
#endif
#ifndef PICO_AUDIO_I2S_CLOCK_PIN_BASE
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE PIMORONI_COSMIC_UNICORN_I2S_BCK_PIN
#endif

#define COLUMN_CLOCK             13
#define COLUMN_DATA              14
#define COLUMN_LATCH             15
#define COLUMN_BLANK             16

#define ROW_BIT_0                17
#define ROW_BIT_1                18
#define ROW_BIT_2                19
#define ROW_BIT_3                20

#define MUTE                     22

#define SWITCH_A                  0
#define SWITCH_B                  1
#define SWITCH_C                  3
#define SWITCH_D                  6

#define SWITCH_SLEEP             27

#define SWITCH_VOLUME_UP          7
#define SWITCH_VOLUME_DOWN        8
#define SWITCH_BRIGHTNESS_UP     21
#define SWITCH_BRIGHTNESS_DOWN   26

#define ROW_COUNT   16
#define BCD_FRAME_COUNT   14
#define BCD_FRAME_BYTES   72

#include "boards/pico_w.h"

#endif
