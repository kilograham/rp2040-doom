/**
 * emu8950 v1.1.0
 * https://github.com/digital-sound-antiques/emu8950
 * Copyright (C) 2001-2020 Mitsutaka Okazaki
 * Copyright (C) 2021-2022 Graham Sanderson
 *
 * SPDX-License-Identifier: MIT
 */
#if USE_EMU8950_OPL
#include "emu8950.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define SAMPLE_BUF_SIZE 1024

#ifndef INLINE
#if defined(_MSC_VER)
#define INLINE __inline
#elif defined(__GNUC__)
#define INLINE __inline__
#else
#define INLINE inline
#endif
#endif

#define _PI_ 3.14159265358979323846264338327950288

/* dynamic range of envelope output */
#if !EMU8950_NO_FLOAT
#define EG_STEP 0.1875
#else
#define EG_STEPx16 3
#endif

/* dynamic range of total level */
#define TL_STEP 0.75
#define TL_BITS 6

/* dynamic range of sustine level */
#define SL_STEP 3.0
#define SL_BITS 4

/* damper speed before key-on. key-scale affects. */
#define DAMPER_RATE 12

#define TL2EG(tl) ((tl) << 2)


/* clang-format off */
/* exp_table[255-x] = round((exp2((double)x / 256.0) - 1) * 1024) */
static uint16_t exp_table[256] = {
        1018,  1013,  1007,  1002,   996,   991,   986,   980,   975,   969,   964,   959,   953,   948,   942,   937,
        932,   927,   921,   916,   911,   906,   900,   895,   890,   885,   880,   874,   869,   864,   859,   854,
        849,   844,   839,   834,   829,   824,   819,   814,   809,   804,   799,   794,   789,   784,   779,   774,
        770,   765,   760,   755,   750,   745,   741,   736,   731,   726,   722,   717,   712,   708,   703,   698,
        693,   689,   684,   680,   675,   670,   666,   661,   657,   652,   648,   643,   639,   634,   630,   625,
        621,   616,   612,   607,   603,   599,   594,   590,   585,   581,   577,   572,   568,   564,   560,   555,
        551,   547,   542,   538,   534,   530,   526,   521,   517,   513,   509,   505,   501,   496,   492,   488,
        484,   480,   476,   472,   468,   464,   460,   456,   452,   448,   444,   440,   436,   432,   428,   424,
        420,   416,   412,   409,   405,   401,   397,   393,   389,   385,   382,   378,   374,   370,   367,   363,
        359,   355,   352,   348,   344,   340,   337,   333,   329,   326,   322,   318,   315,   311,   308,   304,
        300,   297,   293,   290,   286,   283,   279,   276,   272,   268,   265,   262,   258,   255,   251,   248,
        244,   241,   237,   234,   231,   227,   224,   220,   217,   214,   210,   207,   204,   200,   197,   194,
        190,   187,   184,   181,   177,   174,   171,   168,   164,   161,   158,   155,   152,   148,   145,   142,
        139,   136,   133,   130,   126,   123,   120,   117,   114,   111,   108,   105,   102,    99,    96,    93,
        90,    87,    84,    81,    78,    75,    72,    69,    66,    63,    60,    57,    54,    51,    48,    45,
        42,    40,    37,    34,    31,    28,    25,    22,    20,    17,    14,    11,     8,     6,     3,     0,
};
/* logsin_table[x] = round(-log2(sin((x + 0.5) * PI / (PG_WIDTH / 4) / 2)) * 256) */

#if !EMU8950_NO_WAVE_TABLE_MAP
#define LOGSIN_TABLE_SIZE PG_WIDTH / 4
#else
#define LOGSIN_TABLE_SIZE PG_WIDTH / 2
#endif
static uint16_t logsin_table[LOGSIN_TABLE_SIZE] = {
        2137, 1731, 1543, 1419, 1326, 1252, 1190, 1137, 1091, 1050, 1013, 979, 949, 920, 894, 869,
        846, 825, 804, 785, 767, 749, 732, 717, 701, 687, 672, 659, 646, 633, 621, 609,
        598, 587, 576, 566, 556, 546, 536, 527, 518, 509, 501, 492, 484, 476, 468, 461,
        453, 446, 439, 432, 425, 418, 411, 405, 399, 392, 386, 380, 375, 369, 363, 358,
        352, 347, 341, 336, 331, 326, 321, 316, 311, 307, 302, 297, 293, 289, 284, 280,
        276, 271, 267, 263, 259, 255, 251, 248, 244, 240, 236, 233, 229, 226, 222, 219,
        215, 212, 209, 205, 202, 199, 196, 193, 190, 187, 184, 181, 178, 175, 172, 169,
        167, 164, 161, 159, 156, 153, 151, 148, 146, 143, 141, 138, 136, 134, 131, 129,
        127, 125, 122, 120, 118, 116, 114, 112, 110, 108, 106, 104, 102, 100, 98, 96,
        94, 92, 91, 89, 87, 85, 83, 82, 80, 78, 77, 75, 74, 72, 70, 69,
        67, 66, 64, 63, 62, 60, 59, 57, 56, 55, 53, 52, 51, 49, 48, 47,
        46, 45, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30,
        29, 28, 27, 26, 25, 24, 23, 23, 22, 21, 20, 20, 19, 18, 17, 17,
        16, 15, 15, 14, 13, 13, 12, 12, 11, 10, 10, 9, 9, 8, 8, 7,
        7, 7, 6, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2,
        2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
#if EMU8950_NO_WAVE_TABLE_MAP
        // double the table size to include second 1/4 cycle
        0,     0,     0,     0,     0,     0,     0,     0,     1,     1,     1,     1,     1,     1,     1,     2,
        2,     2,     2,     3,     3,     3,     4,     4,     4,     5,     5,     5,     6,     6,     7,     7,
        7,     8,     8,     9,     9,    10,    10,    11,    12,    12,    13,    13,    14,    15,    15,    16,
        17,    17,    18,    19,    20,    20,    21,    22,    23,    23,    24,    25,    26,    27,    28,    29,
        30,    31,    32,    33,    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,    45,    46,
        47,    48,    49,    51,    52,    53,    55,    56,    57,    59,    60,    62,    63,    64,    66,    67,
        69,    70,    72,    74,    75,    77,    78,    80,    82,    83,    85,    87,    89,    91,    92,    94,
        96,    98,   100,   102,   104,   106,   108,   110,   112,   114,   116,   118,   120,   122,   125,   127,
        129,   131,   134,   136,   138,   141,   143,   146,   148,   151,   153,   156,   159,   161,   164,   167,
        169,   172,   175,   178,   181,   184,   187,   190,   193,   196,   199,   202,   205,   209,   212,   215,
        219,   222,   226,   229,   233,   236,   240,   244,   248,   251,   255,   259,   263,   267,   271,   276,
        280,   284,   289,   293,   297,   302,   307,   311,   316,   321,   326,   331,   336,   341,   347,   352,
        358,   363,   369,   375,   380,   386,   392,   399,   405,   411,   418,   425,   432,   439,   446,   453,
        461,   468,   476,   484,   492,   501,   509,   518,   527,   536,   546,   556,   566,   576,   587,   598,
        609,   621,   633,   646,   659,   672,   687,   701,   717,   732,   749,   767,   785,   804,   825,   846,
        869,   894,   920,   949,   979,  1013,  1050,  1091,  1137,  1190,  1252,  1326,  1419,  1543,  1731,  2137,
#endif
};
/* clang-format on */

/* amplitude lfo table */
/* The following envelop pattern is verified on real YM2413. */
/* each element repeates 64 cycles */
static uint8_t am_table[210] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,  //
                                2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,  //
                                4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,  //
                                6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,  //
                                8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9,  //
                                10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, //
                                12, 12, 12, 12, 12, 12, 12, 12,                                 //
                                13, 13, 13,                                                     //
                                12, 12, 12, 12, 12, 12, 12, 12,                                 //
                                11, 11, 11, 11, 11, 11, 11, 11, 10, 10, 10, 10, 10, 10, 10, 10, //
                                9, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8,  //
                                7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6,  //
                                5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,  //
                                3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2,  //
                                1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};

#if !EMU8950_SLOT_RENDER
#if !EMU8950_NO_WAVE_TABLE_MAP
static uint16_t wave_table_map[4][PG_WIDTH];
#else
// we start with
//  _  _
// / \/ \ which is abs(sine) wave
//
static uint16_t wav_or_table_lookup[4][4] = {
        {0, 0, 0x8000, 0x8000}, // .. negate second half
        {0, 0, 0x0fff, 0x0fff}, // .. attenuate second half
        {0, 0, 0x0000, 0x0000}, // .. leave second half alone
        {0, 0xfff, 0, 0xfff}, // .. attenuate 1 and 3
};
#endif


/* offset to fnum, rough approximation of 14 cents depth. */
static int8_t pm_table[8][PM_PG_WIDTH] = {
        {0, 0, 0, 0, 0, 0,  0,  0},    // fnum = 000xxxxx
        {0, 0, 1, 0, 0, 0,  -1, 0},   // fnum = 001xxxxx
        {0, 1, 2, 1, 0, -1, -2, -1}, // fnum = 010xxxxx
        {0, 1, 3, 1, 0, -1, -3, -1}, // fnum = 011xxxxx
        {0, 2, 4, 2, 0, -2, -4, -2}, // fnum = 100xxxxx
        {0, 2, 5, 2, 0, -2, -5, -2}, // fnum = 101xxxxx
        {0, 3, 6, 3, 0, -3, -6, -3}, // fnum = 110xxxxx
        {0, 3, 7, 3, 0, -3, -7, -3}, // fnum = 111xxxxx
};


/* envelope decay increment step table */
static uint8_t eg_step_tables[4][8] = {
        {0, 1, 0, 1, 0, 1, 0, 1},
        {0, 1, 0, 1, 1, 1, 0, 1},
        {0, 1, 1, 1, 0, 1, 1, 1},
        {0, 1, 1, 1, 1, 1, 1, 1},
};
static uint8_t eg_step_tables_fast[4][8] = {
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 2, 1, 1, 1, 2},
        {1, 2, 1, 2, 1, 2, 1, 2},
        {1, 2, 2, 2, 1, 2, 2, 2},
};

static uint32_t ml_table[16] = {1, 1 * 2, 2 * 2, 3 * 2, 4 * 2, 5 * 2, 6 * 2, 7 * 2,
                                8 * 2, 9 * 2, 10 * 2, 10 * 2, 12 * 2, 12 * 2, 15 * 2, 15 * 2};

#endif

#if !EMU8950_NO_TLL
#if !EMU8950_NO_FLOAT
#define dB2(x) ((x)*2)
static double kl_table[16] = {dB2(0.000),  dB2(9.000),  dB2(12.000), dB2(13.875), dB2(15.000), dB2(16.125),
                              dB2(16.875), dB2(17.625), dB2(18.000), dB2(18.750), dB2(19.125), dB2(19.500),
                              dB2(19.875), dB2(20.250), dB2(20.625), dB2(21.000)};
#else
#define dB2x16(x) ((uint16_t)((x)*32))
static int16_t kl_tablex16[16] = {dB2x16(0.000), dB2x16(9.000), dB2x16(12.000), dB2x16(13.875), dB2x16(15.000),
                                  dB2x16(16.125),
                                  dB2x16(16.875), dB2x16(17.625), dB2x16(18.000), dB2x16(18.750), dB2x16(19.125),
                                  dB2x16(19.500),
                                  dB2x16(19.875), dB2x16(20.250), dB2x16(20.625), dB2x16(21.000)};
#endif
#endif

#if !EMU8950_NO_TLL
static uint32_t tll_table[8 * 16][1 << TL_BITS][4];
#endif
static int32_t rks_table[2][32][2];

#define min(i, j) (((i) < (j)) ? (i) : (j))
#define max(i, j) (((i) > (j)) ? (i) : (j))

/***************************************************

           Internal Sample Rate Converter

****************************************************/
/* Note: to disable internal rate converter, set clock/72 to output sampling rate. */

/*
 * LW is truncate length of sinc(x) calculation.
 * Lower LW is faster, higher LW results better quality.
 * LW must be a non-zero positive even number, no upper limit.
 * LW=16 or greater is recommended when upsampling.
 * LW=8 is practically okay for downsampling.
 */
#define LW 16

#if !EMU8950_NO_RATECONV
/* resolution of sinc(x) table. sinc(x) where 0.0<=x<1.0 corresponds to sinc_table[0...SINC_RESO-1] */
#define SINC_RESO 256
#define SINC_AMP_BITS 12

// double hamming(double x) { return 0.54 - 0.46 * cos(2 * PI * x); }
static double blackman(double x) { return 0.42 - 0.5 * cos(2 * _PI_ * x) + 0.08 * cos(4 * _PI_ * x); }

static double sinc(double x) { return (x == 0.0 ? 1.0 : sin(_PI_ * x) / (_PI_ * x)); }

static double windowed_sinc(double x) { return blackman(0.5 + 0.5 * x / (LW / 2)) * sinc(x); }

/* f_inp: input frequency. f_out: output frequencey, ch: number of channels */
OPL_RateConv *OPL_RateConv_new(double f_inp, double f_out, int ch) {
    OPL_RateConv *conv = malloc(sizeof(OPL_RateConv));
    int i;

    conv->ch = ch;
    conv->f_ratio = f_inp / f_out;
    conv->buf = malloc(sizeof(void *) * ch);
    for (i = 0; i < ch; i++) {
        conv->buf[i] = malloc(sizeof(conv->buf[0][0]) * LW);
    }

    /* create sinc_table for positive 0 <= x < LW/2 */
    conv->sinc_table = malloc(sizeof(conv->sinc_table[0]) * SINC_RESO * LW / 2);
    for (i = 0; i < SINC_RESO * LW / 2; i++) {
        const double x = (double) i / SINC_RESO;
        if (f_out < f_inp) {
            /* for downsampling */
            conv->sinc_table[i] = (int16_t) ((1 << SINC_AMP_BITS) * windowed_sinc(x / conv->f_ratio) / conv->f_ratio);
        } else {
            /* for upsampling */
            conv->sinc_table[i] = (int16_t) ((1 << SINC_AMP_BITS) * windowed_sinc(x));
        }
    }

    return conv;
}

static INLINE int16_t lookup_sinc_table(int16_t *table, double x) {
    int16_t index = (int16_t) (x * SINC_RESO);
    if (index < 0)
        index = -index;
    return table[min(SINC_RESO * LW / 2 - 1, index)];
}

void OPL_RateConv_reset(OPL_RateConv *conv) {
    int i;
    conv->timer = 0;
    for (i = 0; i < conv->ch; i++) {
        memset(conv->buf[i], 0, sizeof(conv->buf[i][0]) * LW);
    }
}

/* put original data to this converter at f_inp. */
void OPL_RateConv_putData(OPL_RateConv *conv, int ch, int16_t data) {
    int16_t *buf = conv->buf[ch];
    int i;
    for (i = 0; i < LW - 1; i++) {
        buf[i] = buf[i + 1];
    }
    buf[LW - 1] = data;
}

/* get resampled data from this converter at f_out. */
/* this function must be called f_out / f_inp times per one putData call. */
int16_t OPL_RateConv_getData(OPL_RateConv *conv, int ch) {
    int16_t *buf = conv->buf[ch];
    int32_t sum = 0;
    int k;
    double dn;
    conv->timer += conv->f_ratio;
    dn = conv->timer - floor(conv->timer);
    conv->timer = dn;

    for (k = 0; k < LW; k++) {
        double x = ((double) k - (LW / 2 - 1)) - dn;
        sum += buf[k] * lookup_sinc_table(conv->sinc_table, x);
    }
    return sum >> SINC_AMP_BITS;
}

void OPL_RateConv_delete(OPL_RateConv *conv) {
    int i;
    for (i = 0; i < conv->ch; i++) {
        free(conv->buf[i]);
    }
    free(conv->buf);
    free(conv->sinc_table);
    free(conv);
}

#endif

/***************************************************

                  Create tables

****************************************************/
static void makeSinTable(void) {
#if !EMU8950_NO_WAVE_TABLE_MAP
    int x;

    for (x = 0; x < PG_WIDTH; x++) {
        if (x < PG_WIDTH / 4) {
            wave_table_map[0][x] = logsin_table[x];
        } else if (x < PG_WIDTH / 2) {
            wave_table_map[0][x] = logsin_table[PG_WIDTH / 2 - x - 1];
        } else {
            wave_table_map[0][x] = 0x8000 | wave_table_map[0][PG_WIDTH - x - 1];
        }
    }

    for (x = 0; x < PG_WIDTH; x++) {
        if (x < PG_WIDTH / 2) {
            wave_table_map[1][x] = wave_table_map[0][x];
        } else {
            wave_table_map[1][x] = 0xfff;
        }
    }

    for (x = 0; x < PG_WIDTH; x++) {
        if (x < PG_WIDTH / 2) {
            wave_table_map[2][x] = wave_table_map[0][x];
        } else {
            wave_table_map[2][x] = wave_table_map[0][x - PG_WIDTH / 2];
        }
    }

    for (x = 0; x < PG_WIDTH; x++) {
        if (x < PG_WIDTH / 4) {
            wave_table_map[3][x] = wave_table_map[0][x];
        } else if (x < PG_WIDTH / 2) {
            wave_table_map[3][x] = 0xfff;
        } else if (x < PG_WIDTH * 3 / 4) {
            wave_table_map[3][x] = wave_table_map[0][x - PG_WIDTH / 2];
        } else {
            wave_table_map[3][x] = 0xfff;
        }
    }
#endif
}

static void makeTllTable(void) {
#if !EMU8950_NO_TLL
    int32_t tmp;
    int32_t fnum, block, TL, KL, kx;

    for (fnum = 0; fnum < 16; fnum++) {
      for (block = 0; block < 8; block++) {
        for (TL = 0; TL < 64; TL++) {
          for (KL = 0; KL < 4; KL++) {
            kx = ((KL & 1) << 1) | ((KL >> 1) & 1);
            if (KL == 0) {
              tll_table[(block << 4) | fnum][TL][KL] = TL2EG(TL);
            } else {
#if !EMU8950_NO_FLOAT
              tmp = (int32_t)(kl_table[fnum] - dB2(3.000) * (7 - block));
              if (tmp <= 0)
                tll_table[(block << 4) | fnum][TL][KL] = TL2EG(TL);
              else
                tll_table[(block << 4) | fnum][TL][KL] = (uint32_t)((tmp >> (3 - kx)) / EG_STEP) + TL2EG(TL);
#else
              tmp = (int32_t)(kl_tablex16[fnum] - dB2x16(3.000) * (7 - block));
              if (tmp <= 0)
                tll_table[(block << 4) | fnum][TL][KL] = TL2EG(TL);
              else
                tll_table[(block << 4) | fnum][TL][KL] = (uint32_t)((tmp >> (3 - kx)) / EG_STEPx16) + TL2EG(TL);
#endif
          }
        }
      }
    }
    }
#endif
}

static void makeRksTable(void) {
    int fnum8, fnum9, blk;
    int blk_fnum98;
    for (fnum8 = 0; fnum8 < 2; fnum8++)
        for (fnum9 = 0; fnum9 < 2; fnum9++)
            for (blk = 0; blk < 8; blk++) {
                blk_fnum98 = (blk << 2) | (fnum9 << 1) | fnum8;
                rks_table[0][blk_fnum98][1] = (blk << 1) + fnum9;
                rks_table[0][blk_fnum98][0] = blk >> 1;
                rks_table[1][blk_fnum98][1] = (blk << 1) + (fnum9 & fnum8);
                rks_table[1][blk_fnum98][0] = blk >> 1;
            }
}

static uint8_t table_initialized = 0;

static void initializeTables() {
    makeTllTable();
    makeRksTable();
    makeSinTable();
    table_initialized = 1;
}

/*********************************************************

                      Synthesizing

*********************************************************/
#define SLOT_BD1 12
#define SLOT_BD2 13
#define SLOT_HH 14
#define SLOT_SD 15
#define SLOT_TOM 16
#define SLOT_CYM 17

/* utility macros */
#define MOD(o, x) (&(o)->slot[(x) << 1])
#define CAR(o, x) (&(o)->slot[((x) << 1) | 1])
#define BIT(s, b) (((s) >> (b)) & 1)

#if OPL_DEBUG
static void _debug_print_patch(OPL_SLOT *slot) {
  OPL_PATCH *p = slot->patch;
  printf("[slot#%d am:%d pm:%d eg:%d kr:%d ml:%d kl:%d tl:%d ws:%d fb:%d A:%d D:%d S:%d R:%d]\n", slot->number, //
         p->AM, p->PM, p->EG, p->KR, p->ML,                                                                     //
         p->KL, p->TL, p->WS, p->FB,                                                                            //
         p->AR, p->DR, p->SL, p->RR);
}

static char *_debug_eg_state_name(OPL_SLOT *slot) {
  switch (slot->eg_state) {
  case ATTACK:
    return "attack";
  case DECAY:
    return "decay";
  case SUSTAIN:
    return "sustain";
  case RELEASE:
    return "release";
  case DAMP:
    return "damp";
  default:
    return "unknown";
  }
}

static INLINE void _debug_print_slot_info(OPL_SLOT *slot) {
  char *name = _debug_eg_state_name(slot);
  _debug_print_patch(slot);
  printf("[slot#%d state:%s fnum:%03x rate:%d-%d]\n", slot->number, name, slot->blk_fnum, slot->eg_rate_h,
         slot->eg_rate_l);
  fflush(stdout);
}
#endif

enum SLOT_UPDATE_FLAG
{
    UPDATE_WS = 1,
    UPDATE_TLL = 2,
    UPDATE_RKS = 4,
    UPDATE_EG = 8,
    UPDATE_ALL = 255,
};

static INLINE void request_update(OPL_SLOT *slot, int flag) {
    slot->update_requests |= flag;
}

static INLINE int get_parameter_rate(OPL_SLOT *slot) {
    switch (slot->eg_state) {
        case ATTACK:
            return slot->patch->AR;
        case DECAY:
            return slot->patch->DR;
        case SUSTAIN:
            return slot->patch->EG ? 0 : slot->patch->RR;
        case RELEASE:
            return slot->patch->RR;
        default:
            return 0;
    }
}

static void commit_slot_update(OPL_SLOT *slot, uint8_t notesel) {

    if (slot->update_requests & UPDATE_WS) {
#if !EMU8950_NO_WAVE_TABLE_MAP
        slot->wave_table = wave_table_map[slot->patch->WS & 3];
#else
#if !EMU8950_SLOT_RENDER
        slot->wav_or_table = wav_or_table_lookup[slot->patch->WS & 3];
#endif
#endif
    }

    if (slot->update_requests & UPDATE_TLL) {
#if !EMU8950_NO_TLL
        if ((slot->type & 1) == 0) {
          slot->tll = tll_table[slot->blk_fnum >> 6][slot->patch->TL][slot->patch->KL];
        } else {
          slot->tll = tll_table[slot->blk_fnum >> 6][slot->patch->TL][slot->patch->KL];
        }
#else
        static const uint8_t kslrom4[16] = {
                0 * 4, 32 * 4, 40 * 4, 45 * 4, 48 * 4, 51 * 4, 53 * 4, 55 * 4, 56 * 4, 58 * 4, 59 * 4, 60 * 4, 61 * 4,
                62 * 4, 63 * 4, 255
        };

        int fnum = (slot->blk_fnum >> 6) & 15;
        int block = (slot->blk_fnum >> 10);
        int16_t ksl = kslrom4[fnum] - ((0x08 - block) << 5);
        if (ksl < 0) {
            slot->tll = slot->patch->TL4;
        } else {
            slot->tll = slot->patch->TL4 + (ksl >> slot->patch->KL_SHIFT);
        }
#endif
    }

    if (slot->update_requests & UPDATE_RKS) {
        slot->rks = rks_table[notesel][slot->blk_fnum >> 8][slot->patch->KR];
    }

    if (slot->update_requests & (UPDATE_RKS | UPDATE_EG)) {
        int p_rate = get_parameter_rate(slot);

        if (p_rate == 0) {
            slot->eg_shift = 0;
            slot->eg_rate_h = 0;
            slot->eg_rate_l = 0;
        } else {
            slot->eg_rate_h = min(15, p_rate + (slot->rks >> 2));
            slot->eg_rate_l = slot->rks & 3;
            if (slot->eg_state == ATTACK) {
                slot->eg_shift = (0 < slot->eg_rate_h && slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
            } else {
                slot->eg_shift = (slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
            }
        }
    }

#if OPL_DEBUG
    if (slot->last_eg_state != slot->eg_state) {
      _debug_print_slot_info(slot);
      slot->last_eg_state = slot->eg_state;
    }
#endif

    slot->update_requests = 0;
}

#if !EMU8950_SLOT_RENDER
static void commit_slot_update_eg_only(OPL_SLOT *slot, uint8_t notesel) {
    assert(slot->update_requests == UPDATE_EG);
    int p_rate = get_parameter_rate(slot);

    if (p_rate == 0) {
        slot->eg_shift = 0;
        slot->eg_rate_h = 0;
        slot->eg_rate_l = 0;
    } else {
        slot->eg_rate_h = min(15, p_rate + (slot->rks >> 2));
        slot->eg_rate_l = slot->rks & 3;
        if (slot->eg_state == ATTACK) {
            slot->eg_shift = (0 < slot->eg_rate_h && slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
        } else {
            slot->eg_shift = (slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
        }
    }
    slot->update_requests = 0;
}
#endif

static void reset_slot(OPL_SLOT *slot, int number) {
    slot->patch = &(slot->__patch);
    memset(slot->patch, 0, sizeof(OPL_PATCH));
    slot->number = number;
#if !EMU8950_NO_PERCUSSION_MODE
    slot->type = number % 2;
#endif
//    slot->pg_keep = 0;
#if !EMU8950_NO_WAVE_TABLE_MAP
    slot->wave_table = wave_table_map[0];
#else
#if !EMU8950_SLOT_RENDER
    slot->wav_or_table = wav_or_table_lookup[0];
#endif
#endif
//    slot->pg_phase = 0;
//    slot->output[0] = 0;
//    slot->output[1] = 0;
    slot->eg_state = RELEASE;
//    slot->eg_shift = 0;
//    slot->rks = 0;
//    slot->tll = 0;
//    slot->blk_fnum = 0;
//    slot->blk = 0;
//    slot->fnum = 0;
//    slot->pg_out = 0;
    slot->eg_out = EG_MUTE;
}

#if !EMU8950_NO_PERCUSSION_MODE
#define slot_pg_keep(slot) slot->pg_keep
#define opl_perc_mode(opl) opl->perc_mode
#else
#define slot_pg_keep(slot) 0
#define opl_perc_mode(opl) 0
#endif
static INLINE void slotOn(OPL *opl, int i) {
    OPL_SLOT *slot = &opl->slot[i];
    if (min(15, slot->patch->AR + (slot->rks >> 2)) == 15) {
        slot->eg_state = DECAY;
        slot->eg_out = 0;
    } else {
        slot->eg_state = ATTACK;
    }
    if (!slot_pg_keep(slot)) {
        slot->pg_phase = 0;
    }
    request_update(slot, UPDATE_EG);
}

static INLINE void slotOff(OPL *opl, int i) {
    OPL_SLOT *slot = &opl->slot[i];
    slot->eg_state = RELEASE;
    request_update(slot, UPDATE_EG);
}

static INLINE void update_key_status(OPL *opl) {
    const uint8_t r14 = opl->reg[0xbd];
    const uint8_t perc_mode = BIT(r14, 5);
    uint32_t new_slot_key_status = 0;
    uint32_t updated_status;
    int ch;

#if !EMU8950_NO_TIMER
    if (opl->csm_mode && opl->csm_key_count) {
        new_slot_key_status = 0x3ffff;
    }
#endif

    for (ch = 0; ch < 9; ch++)
        if (opl->reg[0xB0 + ch] & 0x20)
            new_slot_key_status |= 3 << (ch * 2);

    if (perc_mode) {
        if (r14 & 0x10)
            new_slot_key_status |= 3 << SLOT_BD1;

        if (r14 & 0x01)
            new_slot_key_status |= 1 << SLOT_HH;

        if (r14 & 0x08)
            new_slot_key_status |= 1 << SLOT_SD;

        if (r14 & 0x04)
            new_slot_key_status |= 1 << SLOT_TOM;

        if (r14 & 0x02)
            new_slot_key_status |= 1 << SLOT_CYM;
    }

    updated_status = opl->slot_key_status ^ new_slot_key_status;

    if (updated_status) {
        int i;
        for (i = 0; i < 18; i++)
            if (BIT(updated_status, i)) {
                if (BIT(new_slot_key_status, i)) {
                    slotOn(opl, i);
                } else {
                    slotOff(opl, i);
                }
            }
    }

    opl->slot_key_status = new_slot_key_status;
}

/* set f-Nnmber ( fnum : 10bit ) */
static INLINE void set_fnumber(OPL *opl, int ch, int fnum) {
    OPL_SLOT *car = CAR(opl, ch);
    OPL_SLOT *mod = MOD(opl, ch);
    car->fnum = fnum;
    car->blk_fnum = (car->blk_fnum & 0x1c00) | (fnum & 0x3ff);
    mod->fnum = fnum;
    mod->blk_fnum = (mod->blk_fnum & 0x1c00) | (fnum & 0x3ff);
    request_update(car, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
    request_update(mod, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
}

/* set block data (blk : 3bit ) */
static INLINE void set_block(OPL *opl, int ch, int blk) {
    OPL_SLOT *car = CAR(opl, ch);
    OPL_SLOT *mod = MOD(opl, ch);
    car->blk = blk;
    car->blk_fnum = ((blk & 7) << 10) | (car->blk_fnum & 0x3ff);
    mod->blk = blk;
    mod->blk_fnum = ((blk & 7) << 10) | (mod->blk_fnum & 0x3ff);
    request_update(car, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
    request_update(mod, UPDATE_EG | UPDATE_RKS | UPDATE_TLL);
}

static INLINE void update_perc_mode(OPL *opl) {
#if !EMU8950_NO_PERCUSSION_MODE
    const uint8_t new_perc_mode = (opl->reg[0xbd] >> 5) & 1;

    if (opl->perc_mode != new_perc_mode) {
        if (new_perc_mode) {
            opl->slot[SLOT_HH].type = 3;
            opl->slot[SLOT_HH].pg_keep = 1;
            opl->slot[SLOT_SD].type = 3;
            opl->slot[SLOT_TOM].type = 3;
            opl->slot[SLOT_CYM].type = 3;
            opl->slot[SLOT_CYM].pg_keep = 1;
        } else {
            opl->slot[SLOT_HH].type = 0;
            opl->slot[SLOT_HH].pg_keep = 0;
            opl->slot[SLOT_SD].type = 1;
            opl->slot[SLOT_TOM].type = 0;
            opl->slot[SLOT_CYM].type = 1;
            opl->slot[SLOT_CYM].pg_keep = 0;
        }
    }
    opl->perc_mode = new_perc_mode;
#else
    assert(!((opl->reg[0xbd] >> 5) & 1)); // new_perc_mode should be 0
#endif
}

#if !EMU8950_LINEAR
static INLINE void update_ampm(OPL *opl) {
#if !EMU8950_NO_TEST_FLAG
    const uint32_t pm_inc = (opl_test_flag(opl) & 8) ? opl->pm_dphase << 10 : opl->pm_dphase;
    const uint32_t am_inc = opl_test_flag(opl) ? 64 : 1;
    if (opl_test_flag(opl) & 2) {
        opl->pm_phase = 0;
        opl->am_phase = 0;
    } else {
        opl->pm_phase = (opl->pm_phase + pm_inc) & (PM_DP_WIDTH - 1);
        opl->am_phase += am_inc;
    }
    opl->lfo_am = am_table[(opl->am_phase >> 6) % sizeof(am_table)] >> (opl->am_mode ? 0 : 2);
#else
    opl->pm_phase = (opl->pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);
    opl->am_phase_index++;
    if (opl->am_phase_index == sizeof(am_table)) opl->am_phase_index = 0;
    opl->lfo_am = am_table[opl->am_phase_index] >> (opl->am_mode ? 0 : 2);
#endif
}
static void update_noise(OPL *opl, int cycle) {
#if !EMU8950_SIMPLER_NOISE
    int i;
    for (i = 0; i < cycle; i++) {
        if (opl->noise & 1) {
            opl->noise ^= 0x800200;
        }
        opl->noise >>= 1;
    }
#endif
}

static int noise_bit(OPL *opl) {
#if !EMU8950_SIMPLER_NOISE
    return opl->noise & 1;
#else
    if (opl->noise & 1) {
        opl->noise ^= 0x800200;
        opl->noise >>= 1;
        return 1;
    }
    opl->noise >>= 1;
    return 0;
#endif
}

static void update_short_noise(OPL *opl) {
    const uint32_t pg_hh = opl->slot[SLOT_HH].pg_out;
    const uint32_t pg_cym = opl->slot[SLOT_CYM].pg_out;

    const uint8_t h_bit2 = BIT(pg_hh, PG_BITS - 8);
    const uint8_t h_bit7 = BIT(pg_hh, PG_BITS - 3);
    const uint8_t h_bit3 = BIT(pg_hh, PG_BITS - 7);

    const uint8_t c_bit3 = BIT(pg_cym, PG_BITS - 7);
    const uint8_t c_bit5 = BIT(pg_cym, PG_BITS - 5);

    opl->short_noise = (h_bit2 ^ h_bit7) | (h_bit3 ^ c_bit5) | (c_bit3 ^ c_bit5);
}
#endif

#if !EMU8950_SLOT_RENDER
static INLINE void calc_phase(OPL_SLOT *slot, int32_t pm_phase, uint8_t pm_mode, uint8_t reset) {
    int8_t pm = 0;
    if (slot->patch->PM) {
        pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
        pm >>= (pm_mode ? 0 : 1);
    }

    if (reset) {
        slot->pg_phase = 0;
    }
    slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
    slot->pg_phase &= (DP_WIDTH - 1);
    slot->pg_out = slot->pg_phase >> DP_BASE_BITS;
}

static INLINE uint8_t lookup_attack_step(OPL_SLOT *slot, uint32_t counter) {
    int index = (counter >> slot->eg_shift) & 7;
    switch (slot->eg_rate_h) {
        case 13:
            return eg_step_tables_fast[slot->eg_rate_l][index];
        case 14:
            return eg_step_tables_fast[slot->eg_rate_l][index] << 1;
        case 0:
        case 15:
            return 0;
        default:
            return eg_step_tables[slot->eg_rate_l][index];
    }
}

static INLINE uint8_t lookup_decay_step(OPL_SLOT *slot, uint32_t counter) {
    int index = (counter >> slot->eg_shift) & 7;
    switch (slot->eg_rate_h) {
        case 0:
            return 0;
        case 13:
            return eg_step_tables_fast[slot->eg_rate_l][index];
        case 14:
            return eg_step_tables_fast[slot->eg_rate_l][index] << 1;
        case 15:
            return 4;
        default:
            return eg_step_tables[slot->eg_rate_l][index];
    }
}

static INLINE void calc_envelope(OPL_SLOT *slot, uint16_t eg_counter, uint8_t test) {

    uint16_t mask = (1 << slot->eg_shift) - 1;
    uint8_t step;

    if (slot->eg_state == ATTACK) {
        if (0 < slot->eg_out && slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
            step = lookup_attack_step(slot, eg_counter);
            slot->eg_out += (~slot->eg_out * step) >> 3;
        }
    } else {
        if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
            slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
        }
    }

    switch (slot->eg_state) {
        case ATTACK:
            if (slot->eg_out == 0) {
                slot->eg_state = DECAY;
                request_update(slot, UPDATE_EG);
            }
            break;

        case DECAY:
            if ((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL) {
                slot->eg_state = SUSTAIN;
                request_update(slot, UPDATE_EG);
            }
            break;

        case SUSTAIN:
        case RELEASE:
        default:
            break;
    }

    if (test) {
        slot->eg_out = 0;
    }
}
#endif

#if !EMU8950_LINEAR
static void update_slots(OPL *opl) {
    int i;
    opl->eg_counter++;

    for (i = 0; i < 18; i++) {
        OPL_SLOT *slot = &opl->slot[i];
        if (slot->update_requests) {
            commit_slot_update(slot, opl->notesel);
        }
        calc_envelope(slot, opl->eg_counter, opl_test_flag(opl) & 1);
        calc_phase(slot, opl->pm_phase, opl->pm_mode, opl_test_flag(opl) & 4);
    }
}

#endif
/* input: 0..8191 output: -4095..4095 */
static int16_t lookup_exp_table(int16_t i) {
    /* from andete's expressoin */
    int16_t t = (exp_table[(i & 0xffu)] + 1024);
    int16_t res = t >> ((i & 0x7f00) >> 8);
#if EMU8950_LINEAR_NEG_NOT_NOT
    return ((i & 0x8000) ? -res : res) << 1;
#else
    return ((i & 0x8000) ? ~res : res) << 1;
#endif
}

static INLINE int16_t to_linear(uint16_t h, OPL_SLOT *slot, int16_t am) {
    uint16_t att;
    if (slot->eg_out >= EG_MAX) {
        return 0;
    }

    att = min(EG_MUTE, (slot->eg_out + slot->tll + am)) << 3;
    return lookup_exp_table(h + att);
}

#define LOGSIN_MASK (PG_WIDTH/4 - 1)
#define LOGSIN_MASK2 (PG_WIDTH/2 - 1)

//static INLINE uint16_t get_wave_table(OPL_SLOT *slot, uint32_t index) {
static uint16_t get_wave_table(OPL_SLOT *slot, uint32_t index) {
#if !EMU8950_NO_WAVE_TABLE_MAP
    return slot->wave_table[index];
#else
#if !EMU8950_SLOT_RENDER
    return slot->wav_or_table[(index >> (PG_BITS - 2))&3] | logsin_table[(index & LOGSIN_MASK2)];
#else
    assert(0);
    return 0;
#endif
#if 0
    switch (((index >> (PG_BITS - 4))&0xc) | (slot->patch->WS & 3)) {
        case 0b0000:
        case 0b0001:
        case 0b0010:
        case 0b0011:
        case 0b1010:
        case 0b1011:
            return logsin_table[index & LOGSIN_MASK];
        case 0b0100:
        case 0b0101:
        case 0b0110:
        case 0b1110:
            return logsin_table[LOGSIN_MASK - (index & LOGSIN_MASK)];
        case 0b1000:
            return 0x8000 | logsin_table[index & LOGSIN_MASK];
        case 0b1100:
            return 0x8000 | logsin_table[LOGSIN_MASK - (index & LOGSIN_MASK)];
        default:
            return 0xfff;
    }
#endif
#endif
}

static INLINE uint16_t get_wave_table_wrap(OPL_SLOT *slot, uint32_t index) {
#if !EMU8950_NO_WAVE_TABLE_MAP
    return get_wave_table(slot, index & (PG_WIDTH - 1));
#else
    return get_wave_table(slot, index);
#endif
}

static INLINE int16_t calc_slot_car(OPL *opl, int ch, int16_t fm) {
    OPL_SLOT *slot = CAR(opl, ch);

    uint8_t am = slot->patch->AM ? opl->lfo_am : 0;

    slot->output[1] = slot->output[0];
    slot->output[0] = to_linear(get_wave_table_wrap(slot, slot->pg_out + 2 * (fm >> 1)), slot, am);

    return slot->output[0];
}

static INLINE int16_t calc_slot_mod(OPL *opl, int ch) {
    OPL_SLOT *slot = MOD(opl, ch);

    int16_t fm = slot->patch->FB > 0 ? (slot->output[1] + slot->output[0]) >> (9 - slot->patch->FB) : 0;
    uint8_t am = slot->patch->AM ? opl->lfo_am : 0;

    slot->output[1] = slot->output[0];
    slot->output[0] = to_linear(get_wave_table_wrap(slot, slot->pg_out + fm), slot, am);

    return slot->output[0];
}

/* Specify phase offset directly based on 10-bit (1024-length) sine table */
#define _PD(phase) ((PG_BITS < 10) ? (phase >> (10 - PG_BITS)) : (phase << (PG_BITS - 10)))

#if !EMU8950_NO_PERCUSSION_MODE
static INLINE int16_t calc_slot_tom(OPL *opl) {
    OPL_SLOT *slot = &(opl->slot[SLOT_TOM]);

    return to_linear(get_wave_table(slot, slot->pg_out), slot, 0);
}


static INLINE int16_t calc_slot_snare(OPL *opl) {
    OPL_SLOT *slot = &(opl->slot[SLOT_SD]);

    uint32_t phase;

    if (BIT(opl->slot[SLOT_HH].pg_out, PG_BITS - 2))
        phase = noise_bit(opl) ? _PD(0x300) : _PD(0x200);
    else
        phase = noise_bit(opl) ? _PD(0x0) : _PD(0x100);

    return to_linear(get_wave_table(slot, phase), slot, 0);
}

static INLINE int16_t calc_slot_cym(OPL *opl) {
    OPL_SLOT *slot = &(opl->slot[SLOT_CYM]);

    uint32_t phase = opl->short_noise ? _PD(0x300) : _PD(0x100);

    return to_linear(get_wave_table(slot, phase), slot, 0);
}

static INLINE int16_t calc_slot_hat(OPL *opl) {
    OPL_SLOT *slot = &(opl->slot[SLOT_HH]);

    uint32_t phase;

    if (opl->short_noise)
        phase = noise_bit(opl) ? _PD(0x2d0) : _PD(0x234);
    else
        phase = noise_bit(opl) ? _PD(0x34) : _PD(0xd0);

    return to_linear(get_wave_table(slot, phase), slot, 0);
}
#endif

#define _MO(x) (-(x) >> 1)
#define _RO(x) (x)

static INLINE int16_t calc_fm(OPL *opl, int ch) {
    if (opl->ch_alg[ch]) {
        return calc_slot_car(opl, ch, 0) + calc_slot_mod(opl, ch);
    }
    return calc_slot_car(opl, ch, calc_slot_mod(opl, ch));
}

#if !EMU8950_NO_TIMER
static void latch_timer1(OPL *opl) { opl->timer1_counter = opl->reg[0x02] << 2; }

static void latch_timer2(OPL *opl) { opl->timer2_counter = opl->reg[0x03] << 4; }

static void csm_key_on(OPL *opl) {
    opl->csm_key_count = 1;
    update_key_status(opl);
}

static void csm_key_off(OPL *opl) {
    opl->csm_key_count = 0;
    update_key_status(opl);
}

static void update_timer(OPL *opl) {
    if (opl->csm_mode && 0 < opl->csm_key_count) {
        csm_key_off(opl);
    }

    if (opl->reg[0x04] & 0x01) {
        opl->timer1_counter++;
        if (opl->timer1_counter >> 10) {
            opl->status |= 0x40; // timer1 overflow
            if (opl->csm_mode) {
                csm_key_on(opl);
            }
            if (opl->timer1_func) {
                opl->timer1_func(opl->timer1_user_data);
            }
            latch_timer1(opl);
        }
    }

    if (opl->reg[0x04] & 0x02) {
        opl->timer2_counter++;
        if (opl->timer2_counter >> 12) {
            opl->status |= 0x20; // timer2 overflow
            if (opl->timer2_func) {
                opl->timer2_func(opl->timer2_user_data);
            }
            latch_timer2(opl);
        }
    }
}
#endif

#if !EMU8950_LINEAR
static void update_output(OPL *opl) {
    int16_t *out;
    int i;

#if !EMU8950_NO_TIMER
    update_timer(opl);
#endif
    // generate amplitude modulation same for all channels
    // need am_phase and lfo_am
    update_ampm(opl);
#if EMU8950_SHORT_NOISE_UPDATE_CHECK
    if (opl->mask & (OPL_MASK_CYM | OPL_MASK_HH))
        update_short_noise(opl);
#else
    update_short_noise(opl);
#endif
    update_slots(opl);

    out = opl->ch_out;

    /* CH1-6 */
    for (i = 0; i < 6; i++) {
        if (!(opl->mask & OPL_MASK_CH(i))) {
            out[i] = _MO(calc_fm(opl, i));
        }
    }

    /* CH7 */
    if (!opl_perc_mode(opl)) {
        if (!(opl->mask & OPL_MASK_CH(6))) {
            out[6] = _MO(calc_fm(opl, 6));
        }
    } else {
        if (!(opl->mask & OPL_MASK_BD)) {
            out[9] = _RO(calc_fm(opl, 6));
        }
    }
    update_noise(opl, 14);

    /* CH8 */
    if (!opl_perc_mode(opl)) {
        if (!(opl->mask & OPL_MASK_CH(7))) {
            out[7] = _MO(calc_fm(opl, 7));
        }
    } else {
        if (!(opl->mask & OPL_MASK_HH)) {
            out[10] = _RO(calc_slot_hat(opl));
        }
        if (!(opl->mask & OPL_MASK_SD)) {
            out[11] = _RO(calc_slot_snare(opl));
        }
    }
    update_noise(opl, 2);

    /* CH9 */
    if (!opl_perc_mode(opl)) {
        if (!(opl->mask & OPL_MASK_CH(8))) {
            out[8] = _MO(calc_fm(opl, 8));
        }
    } else {
        if (!(opl->mask & OPL_MASK_TOM)) {
            out[12] = _RO(calc_slot_tom(opl));
        }
        if (!(opl->mask & OPL_MASK_CYM)) {
            out[13] = _RO(calc_slot_cym(opl));
        }
    }
    update_noise(opl, 2);

}

INLINE static void mix_output(OPL *opl) {
    int16_t out = 0;
    int i;
    for (i = 0; i < 15; i++) {
        out += opl->ch_out[i];
    }
#if !EMU8950_NO_RATECONV
    if (opl->conv) {
        OPL_RateConv_putData(opl->conv, 0, out);
    } else {
        opl->mix_out[0] = out;
    }
#else
    opl->mix_out[0] = out;
#endif
}

INLINE static int16_t mix_output_raw(OPL *opl) {
    int32_t out = 0;

#if !EMU8950_NO_PERCUSSION_MODE
    for (int i = 0; i < 15; i++) {
        out += opl->ch_out[i];
    }
#else
    for (int i = 0; i < 9; i++) {
        out += opl->ch_out[i];
    }
#endif

    return out;
}
#endif

/***********************************************************

                   External Interfaces

***********************************************************/

OPL *OPL_new(uint32_t clk, uint32_t rate) {
    OPL *opl;

    if (!table_initialized) {
        initializeTables();
    }

    opl = (OPL *) calloc(sizeof(OPL), 1);
    if (opl == NULL)
        return NULL;

    opl->clk = clk;
    opl->rate = rate;
//    opl->mask = 0;
#if !EMU8950_NO_RATECONV
//    opl->conv = NULL;
#endif
//    opl->mix_out[0] = 0;
//    opl->mix_out[1] = 0;
#if !EMU8950_NO_TIMER
//    opl->timer1_func = NULL;
//    opl->timer1_user_data = NULL;
//    opl->timer2_func = NULL;
//    opl->timer2_user_data = NULL;
#endif

    OPL_reset(opl);

    return opl;
}

void OPL_delete(OPL *opl) {
#if !EMU8950_NO_RATECONV
    if (opl->conv) {
        OPL_RateConv_delete(opl->conv);
        opl->conv = NULL;
    }
#endif
    free(opl);
}

static void reset_rate_conversion_params(OPL *opl) {
#if !EMU8950_NO_RATECONV
    const double f_out = opl->rate;
    const double f_inp = opl->clk / 72;

    opl->out_time = 0;
    opl->out_step = ((uint32_t) f_inp) << 8;
    opl->inp_step = ((uint32_t) f_out) << 8;

    if (opl->conv) {
        OPL_RateConv_delete(opl->conv);
        opl->conv = NULL;
    }

    if (floor(f_inp) != f_out && floor(f_inp + 0.5) != f_out) {
        opl->conv = OPL_RateConv_new(f_inp, f_out, 2);
    }

    if (opl->conv) {
        OPL_RateConv_reset(opl->conv);
    }
#endif
}

void OPL_reset(OPL *opl) {
    int i;

    if (!opl)
        return;

#if EMU8950_NO_RATECONV
    // no useful fields to preserve
    memset(opl, 0, sizeof(*opl));
#else
    // some fields are not reset
    opl->adr = 0;
    opl->notesel = 0;

    opl->status = 0;

    opl->csm_mode = 0;
    opl->csm_key_count = 0;
    opl->timer1_counter = 0;
    opl->timer2_counter = 0;

    opl->pm_phase = 0;
    opl->am_phase = 0;

    opl->mask = 0;

    opl->perc_mode = 0;
    opl->slot_key_status = 0;
    opl->eg_counter = 0;
#endif

#if !EMU8950_NO_PERCUSSION_MODE
    opl->noise = 1;
#endif

    reset_rate_conversion_params(opl);

    for (i = 0; i < 18; i++) {
        reset_slot(&opl->slot[i], i);
    }

//    for (i = 0; i < 9; i++) {
//        opl->ch_alg[i] = 0;
//    }

//    for (i = 0; i < 0x100; i++) {
//        opl->reg[i] = 0;
//    }
    opl->reg[0x04] = 0x18; // MASK_EOS | MASK_BUF_RDY
    opl->pm_dphase = PM_DPHASE;

//    for (i = 0; i < 15; i++) {
//        opl->ch_out[i] = 0;
//    }

}

void OPL_setRate(OPL *opl, uint32_t rate) {
    opl->rate = rate;
    reset_rate_conversion_params(opl);
}

void OPL_setQuality(OPL *opl, uint8_t q) {}

void OPL_setPan(OPL *opl, uint32_t ch, uint8_t pan) { opl->pan[ch & 15] = pan; }

#if !EMU8950_LINEAR
int16_t OPL_calc(OPL *opl) {
    while (opl->out_step > opl->out_time) {
        opl->out_time += opl->inp_step;
        update_output(opl);
        mix_output(opl);
    }
    opl->out_time -= opl->out_step;
#if !EMU8950_NO_RATECONV
    if (opl->conv) {
        opl->mix_out[0] = OPL_RateConv_getData(opl->conv, 0);
    }
#endif
    return opl->mix_out[0];
}

void OPL_calc_buffer(OPL *opl, int16_t *buffer, uint32_t nsamples) {
    assert(opl->out_step == opl->inp_step);
    for (unsigned i = 0; i < nsamples; i++) {
        update_output(opl);
        buffer[i] = mix_output_raw(opl);
    }
}
#endif

#if PICO_ON_DEVICE
#include "hardware/gpio.h"
#endif
#if !LIB_PICO_PLATFORM
#define __not_in_flash_func(x) x
#endif

#if EMU8950_LINEAR

// these return number of samples rendered (can early out due to silence on the slot - not necessarily the channel)
uint32_t slot_mod_linear(OPL *opl, OPL_SLOT *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase);
uint32_t slot_car_linear_alg0(OPL *opl, OPL_SLOT *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase);
uint32_t slot_car_linear_alg1(OPL *opl, OPL_SLOT *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase);

#if DUMPO
int hack_ch;
#endif
#if !EMU8950_SLOT_RENDER
uint32_t __not_in_flash_func(slot_mod_linear)(OPL *opl, OPL_SLOT *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    uint32_t s = 0;
    uint32_t nsamples_bak = nsamples;
    if (slot->eg_state == ATTACK) {
        for (; s < nsamples; s++) {
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);

            uint16_t mask = (1 << slot->eg_shift) - 1;
            if (0 < slot->eg_out && slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                uint8_t step = lookup_attack_step(slot, eg_counter);
                slot->eg_out += (~slot->eg_out * step) >> 3;
            }
            if (slot->eg_out == 0) {
                slot->eg_state = DECAY;
                // todo collapse
                request_update(slot, UPDATE_EG);
                commit_slot_update_eg_only(slot, opl->notesel);
                nsamples = s;
            }

            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            int16_t fm = slot->patch->FB > 0 ? (slot->output[1] + slot->output[0]) >> (9 - slot->patch->FB) : 0;
            slot->output[1] = slot->output[0];

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            opl->mod_buffer[s] = slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out + fm), slot, am);
        }
    }
    if (slot->eg_state == DECAY) {
        nsamples = nsamples_bak;
        // todo if note ends we can stop early
        for (; s < nsamples; s++) {
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);
            uint16_t mask = (1 << slot->eg_shift) - 1;

            if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
                // todo check for decay to zero
            }

            if ((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL) {
                slot->eg_state = SUSTAIN;
                request_update(slot, UPDATE_EG);
                commit_slot_update_eg_only(slot, opl->notesel);
                nsamples = s;
            }

            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            int16_t fm = slot->patch->FB > 0 ? (slot->output[1] + slot->output[0]) >> (9 - slot->patch->FB) : 0;
            slot->output[1] = slot->output[0];

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            opl->mod_buffer[s] = slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out + fm), slot, am);
        }
    }
    if (slot->eg_state == SUSTAIN || slot->eg_state == RELEASE) {
        nsamples = nsamples_bak;
        for (; s < nsamples; s++) {
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);
            uint16_t mask = (1 << slot->eg_shift) - 1;

            if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
            }
            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            int16_t fm = slot->patch->FB > 0 ? (slot->output[1] + slot->output[0]) >> (9 - slot->patch->FB) : 0;
            slot->output[1] = slot->output[0];

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            opl->mod_buffer[s] = slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out + fm), slot, am);
        }
    }
    return nsamples;
}

uint32_t __not_in_flash_func(slot_car_linear_alg1)(OPL *opl, OPL_SLOT *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    uint32_t s = 0;
    uint32_t nsamples_bak = nsamples;
    if (slot->eg_state == ATTACK) {
        for (; s < nsamples; s++) {
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);

            uint16_t mask = (1 << slot->eg_shift) - 1;
            if (0 < slot->eg_out && slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                uint8_t step = lookup_attack_step(slot, eg_counter);
                slot->eg_out += (~slot->eg_out * step) >> 3;
            }
            if (slot->eg_out == 0) {
                slot->eg_state = DECAY;
                // todo collapse
                request_update(slot, UPDATE_EG);
                commit_slot_update_eg_only(slot, opl->notesel);
                nsamples = s;
            }

            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            slot->output[1] = slot->output[0];
            slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out), slot, am);
            opl->buffer[s] += slot->output[0] + opl->mod_buffer[s];
        }
    }
    if (slot->eg_state == DECAY) {
        nsamples = nsamples_bak;
        // todo if note ends we can stop early
        for (; s < nsamples; s++) {
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);
            uint16_t mask = (1 << slot->eg_shift) - 1;

            if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
                // todo check for decay to zero
            }

            if ((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL) {
                slot->eg_state = SUSTAIN;
                request_update(slot, UPDATE_EG);
                commit_slot_update_eg_only(slot, opl->notesel);
                nsamples = s;
            }

            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            slot->output[1] = slot->output[0];
            slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out), slot, am);
            opl->buffer[s] += slot->output[0] + opl->mod_buffer[s];
        }
    }
    if (slot->eg_state == SUSTAIN || slot->eg_state == RELEASE) {
        nsamples = nsamples_bak;
        for (; s < nsamples; s++) {
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);
            uint16_t mask = (1 << slot->eg_shift) - 1;

            if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
            }
            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            slot->output[1] = slot->output[0];
            slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out), slot, am);
            opl->buffer[s] += slot->output[0] + opl->mod_buffer[s];
        }
    }
    return nsamples;
}

uint32_t __not_in_flash_func(slot_car_linear_alg0)(OPL *opl, OPL_SLOT *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    uint32_t s = 0;
    uint32_t nsamples_bak = nsamples;
    if (slot->eg_state == ATTACK) {
        for (; s < nsamples; s++) {
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);

            uint16_t mask = (1 << slot->eg_shift) - 1;
            if (0 < slot->eg_out && slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                uint8_t step = lookup_attack_step(slot, eg_counter);
                slot->eg_out += (~slot->eg_out * step) >> 3;
            }
            if (slot->eg_out == 0) {
                slot->eg_state = DECAY;
                // todo collapse
                request_update(slot, UPDATE_EG);
                commit_slot_update_eg_only(slot, opl->notesel);
                nsamples = s;
            }

            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            slot->output[1] = slot->output[0];

            // todo is this masking realy necessary; i doubt it. .. seems to be always even anyway
//        int32_t fm = 2 * (opl->mod_buffer[s] >> 1);
            int32_t fm = opl->mod_buffer[s];

            slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out + fm), slot, am);
            opl->buffer[s] += slot->output[0];
        }
    }
    if (slot->eg_state == DECAY) {
        nsamples = nsamples_bak;
        // todo if note ends we can stop early
        for (; s < nsamples; s++) {
            if (hack_ch == 0 && s == 12) {
                breako();
            }
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);
            uint16_t mask = (1 << slot->eg_shift) - 1;

            if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
                // todo check for decay to zero
            }

            if ((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL) {
                slot->eg_state = SUSTAIN;
                request_update(slot, UPDATE_EG);
                commit_slot_update_eg_only(slot, opl->notesel);
                nsamples = s;
            }

            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            slot->output[1] = slot->output[0];

            // todo is this masking realy necessary; i doubt it. .. seems to be always even anyway
//        int32_t fm = 2 * (opl->mod_buffer[s] >> 1);
            int32_t fm = opl->mod_buffer[s];

            slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out + fm), slot, am);
            opl->buffer[s] += slot->output[0];
        }
    }
    if (slot->eg_state == SUSTAIN || slot->eg_state == RELEASE) {
        nsamples = nsamples_bak;
        for (; s < nsamples; s++) {
            if (hack_ch == 0 && s == 12) {
                breako();
            }
            eg_counter++;
            pm_phase = (pm_phase + opl->pm_dphase) & (PM_DP_WIDTH - 1);
            uint16_t mask = (1 << slot->eg_shift) - 1;

            if (slot->eg_rate_h > 0 && (eg_counter & mask) == 0) {
                slot->eg_out = min(EG_MUTE, slot->eg_out + lookup_decay_step(slot, eg_counter));
            }
            int8_t pm = 0;
            if (slot->patch->PM) {
                pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
                pm >>= (opl->pm_mode ? 0 : 1);
            }
            slot->pg_phase += (((slot->fnum & 0x3ff) + pm) * ml_table[slot->patch->ML]) << slot->blk >> 1;
            slot->pg_phase &= (DP_WIDTH - 1);
            uint32_t pg_out = slot->pg_phase >> DP_BASE_BITS;

            uint8_t am = slot->patch->AM ? opl->lfo_am_buffer[s] : 0;
            slot->output[1] = slot->output[0];

            // todo is this masking realy necessary; i doubt it. .. seems to be always even anyway
//        int32_t fm = 2 * (opl->mod_buffer[s] >> 1);
            int32_t fm = opl->mod_buffer[s];

            slot->output[0] = to_linear(get_wave_table_wrap(slot, pg_out + fm), slot, am);
            opl->buffer[s] += slot->output[0];
        }
    }
    return nsamples;
}
#endif

static_assert(EMU8950_NO_PERCUSSION_MODE, "");
// this produces stereo
void OPL_calc_buffer_linear(OPL *opl, int32_t *buffer, uint32_t nsamples) {
    int i;
#if EMU8950_SLOT_RENDER
    // kind of a nit pick, but so cheap - saves a bug every 24 hours due to an optimization
    // (we require that incrementing eg_counter is never zero during the rendering loop)
    opl->eg_counter = (opl->eg_counter & 0x3fffffffu) | 0x80000000u;
    static uint8_t lfo_am_buffer_lsl3[SAMPLE_BUF_SIZE];
    assert(nsamples <= sizeof(lfo_am_buffer_lsl3));
    opl->lfo_am_buffer_lsl3 = lfo_am_buffer_lsl3;
#else
    static uint8_t lfo_am_buffer[SAMPLE_BUF_SIZE];
    assert(nsamples <= sizeof(lfo_am_buffer));
    opl->lfo_am_buffer = lfo_am_buffer;
#endif
    static int16_t mod_buffer[SAMPLE_BUF_SIZE];

    opl->mod_buffer = mod_buffer;
    opl->buffer = buffer;

    // todo achievable by memcpy
    for(uint32_t s = 0; s<nsamples; s++) {
        // generate amplitude modulation same for all channels
        // need am_phase and lfo_am
        opl->am_phase_index++;
        if (opl->am_phase_index == sizeof(am_table)) opl->am_phase_index = 0;
        // todo this is a candidate for remove simply because it is not super noticeable without

#if EMU8950_SLOT_RENDER
        // note <<3 still fits within 8 bits
        lfo_am_buffer_lsl3[s] = (am_table[opl->am_phase_index] >> (opl->am_mode ? 0 : 2)) << 3;
#else
        lfo_am_buffer[s] = (am_table[opl->am_phase_index] >> (opl->am_mode ? 0 : 2));
#endif
        buffer[s]=0;
    }

    for (i = 0; i < 18; i++) {
        OPL_SLOT *slot = &opl->slot[i];
        int ch = i >> 1;
#if DUMPO
        hack_ch = ch;
        if (ch == 8 && bc == 1) {
            printf("HRMPH\n");
        }
#endif
        if (slot->update_requests) {
            commit_slot_update(slot, opl->notesel);
        }
#if EMU8950_SLOT_RENDER
        slot->lfo_am_buffer_lsl3 = opl->lfo_am_buffer_lsl3;
        slot->pm_mode = opl->pm_mode;
        slot->mod_buffer = opl->mod_buffer;
#endif
        if (!(i & 1)) {
            // ---- MOD SLOT ----
            if ((slot+1)->eg_out >= EG_MUTE && (slot+1)->eg_state != ATTACK && !opl->ch_alg[ch]) {
#if DUMPO
                memset(slot_output[i], 0, nsamples*2);
                memset(slot_output[i+1], 0, nsamples*2);
#endif
                i++;
                continue;
            }

            uint32_t s_mod;
#if EMU8950_LINEAR_SKIP // todo consider disabling as almost unnecessary with EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION
            if (slot->eg_out >= EG_MUTE && slot->eg_state != ATTACK) {
                s_mod = 0;
            } else
#endif
            {
//                printf("MOD %d\n", i);
//                if (bc == 7 && i == 14) {
//                    printf("WAM\n");
//                }
                s_mod = slot_mod_linear(opl, slot, nsamples, opl->eg_counter, opl->pm_phase);
            }
            if (s_mod != nsamples) {
                memset(opl->mod_buffer + s_mod, 0, (nsamples - s_mod) * 2);
            }
#if DUMPO
            memcpy(slot_output[i], opl->mod_buffer, nsamples * 2);
#endif
        } else {
#if EMU8950_LINEAR_SKIP
#if EMU8950_SLOT_RENDER
            slot->buffer = opl->buffer;
#endif
            uint32_t s_alg;
#if EMU8950_LINEAR_SKIP // todo consider disabling as almost unnecessary with EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION
            if (slot->eg_out >= EG_MUTE && slot->eg_state != ATTACK) {
                s_alg = 0;
            } else
#endif
#if DUMPO
            memcpy(opl_buffer_bak, opl->buffer, nsamples * 4);
            memset(opl->buffer, 0, nsamples * 4);
#endif
            {
                if (opl->ch_alg[ch]) {
                    s_alg = slot_car_linear_alg1(opl, slot, nsamples, opl->eg_counter, opl->pm_phase);
                } else {
                    s_alg = slot_car_linear_alg0(opl, slot, nsamples, opl->eg_counter, opl->pm_phase);
                }
            }
            if (s_alg != nsamples) {
                if (opl->ch_alg[ch]) {
                    for (uint32_t s = s_alg; s < nsamples; s++) {
                        opl->buffer[s] += opl->mod_buffer[s];
                    }
                }
            }
#if DUMPO
            for(uint s = 0; s < nsamples; s++) {
                slot_output[i][s] = opl->buffer[s];
                opl->buffer[s] += opl_buffer_bak[s];
            }
#endif
#endif
        }
    }
    opl->pm_phase = (opl->pm_phase + opl->pm_dphase * nsamples) & (PM_DP_WIDTH - 1);
    opl->eg_counter += nsamples;

}
#endif

void OPL_calc_buffer_stereo(OPL *opl, int32_t *buffer, uint32_t nsamples) {
    assert(opl->out_step == opl->inp_step);
#if DUMPO
    bc++;
#endif
#if !EMU8950_LINEAR
    for (unsigned i = 0; i < nsamples; i++) {
        update_output(opl);
        uint16_t raw = mix_output_raw(opl);
#if DUMPO
    printf("SND %d %d %08x : ", bc, i, raw);
    for (int i = 0; i < 9; i++) {
        printf("%04x ", (uint16_t) opl->ch_out[i]);
    }
    printf("\n");
#endif

        buffer[i] =  (raw << 16u) | raw;
    }
#else
    OPL_calc_buffer_linear(opl, buffer, nsamples);
//    if (0)
    for (unsigned i = 0; i < nsamples; i++) {
#if DUMPO
        uint16_t raw = _MO(buffer[i]);
        printf("SND %d %d %08x : ", bc, i, raw);
        for (int j = 0; j < 18; j++) {
            printf("%04x ", (uint16_t) slot_output[j][i]);
        }
        printf("\n");
#else
        uint16_t raw = buffer[i] >> 1; // _MO()
#endif
        // todo clamp?
        buffer[i] = (raw << 16u) | raw;
    }
#endif
}

void OPL_writeReg(OPL *opl, uint32_t reg, uint8_t data) {

//    printf("WR %04x %2x\n", reg, data);
    int32_t s, c;

    static int32_t stbl[32] = {0, 2, 4, 1, 3, 5, -1, -1, 6, 8, 10, 7, 9, 11, -1, -1,
                               12, 14, 16, 13, 15, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    reg = reg & 0xff;

    if ((reg == 0x04) && (data & 0x80)) {
        // IRQ RESET
        opl->status = 0;
        opl->reg[0x04] &= 0x7f;
        return;
    }

    opl->reg[reg] = data;

    if (reg == 0x01) {
#if !EMU8950_NO_TEST_FLAG
        opl->test_flag = data;
#endif
    } else if (reg == 0x04) {

        if (data & 0x01) {
#if !EMU8950_NO_TIMER
            latch_timer1(opl);
#else
            printf("WARNING TIMER1 LATCH\n");
#endif
        }
        if (data & 0x02) {
#if !EMU8950_NO_TIMER
            latch_timer2(opl);
#else
            printf("WARNING TIMER2 LATCH\n");
#endif
        }

    } else if (0x07 <= reg && reg <= 0x12) {

        if (reg == 0x08) {
#if !EMU8950_NO_TIMER
            opl->csm_mode = (data >> 7) & 1;
#else
            if ((data >> 7) & 1) {
                printf("WARNING SET CSM MODE\n");
            }
#endif
            opl->notesel = (data >> 6) & 1;
        }

    } else if (0x20 <= reg && reg < 0x40) {

        s = stbl[reg - 0x20];
        if (s >= 0) {
            opl->slot[s].patch->AM = (data >> 7) & 1;
            opl->slot[s].patch->PM = (data >> 6) & 1;
            opl->slot[s].patch->EG = (data >> 5) & 1;
            opl->slot[s].patch->KR = (data >> 4) & 1;
            opl->slot[s].patch->ML = (data) & 15;
            request_update(&(opl->slot[s]), UPDATE_ALL);
        }

    } else if (0x40 <= reg && reg < 0x60) {

        s = stbl[reg - 0x40];
        if (s >= 0) {
#if !EMU8950_NO_TLL
            opl->slot[s].patch->TL = (data)&63;
            opl->slot[s].patch->KL = (data >> 6) & 3;
#else
            opl->slot[s].patch->TL4 = data << 2;
            static const uint8_t kslshift[4] = {
                    8, 1, 2, 0
            };
            opl->slot[s].patch->KL_SHIFT = kslshift[(data >> 6) & 3];
#endif
            request_update(&(opl->slot[s]), UPDATE_ALL);
        }

    } else if (0x60 <= reg && reg < 0x80) {

        s = stbl[reg - 0x60];
        if (s >= 0) {
            opl->slot[s].patch->AR = (data >> 4) & 15;
            opl->slot[s].patch->DR = (data) & 15;
            request_update(&(opl->slot[s]), UPDATE_EG);
        }

    } else if (0x80 <= reg && reg < 0xa0) {

        s = stbl[reg - 0x80];
        if (s >= 0) {
            opl->slot[s].patch->SL = (data >> 4) & 15;
            opl->slot[s].patch->RR = (data) & 15;
            request_update(&(opl->slot[s]), UPDATE_EG);
        }

    } else if (0xa0 <= reg && reg < 0xa9) {

        c = reg - 0xa0;
        set_fnumber(opl, c, data + ((opl->reg[reg + 0x10] & 3) << 8));

    } else if (0xb0 <= reg && reg < 0xb9) {

        c = reg - 0xb0;
        set_fnumber(opl, c, ((data & 3) << 8) + opl->reg[reg - 0x10]);
        set_block(opl, c, (data >> 2) & 7);
        update_key_status(opl);

    } else if (0xc0 <= reg && reg < 0xc9) {

        c = reg - 0xc0;
        opl->slot[c * 2].patch->FB = (data >> 1) & 7;
        opl->ch_alg[c] = data & 1;

    } else if (reg == 0xbd) {

        update_perc_mode(opl);
        update_key_status(opl);
        opl->am_mode = (data >> 7) & 1;
        opl->pm_mode = (data >> 6) & 1;

    } else if (0xe0 <= reg && reg < 0x100) {
        if (opl->reg[0x01] & 0x20) {
            s = stbl[reg - 0xe0];
            if (s >= 0) {
                opl->slot[s].patch->WS = data & 3;
                request_update(&(opl->slot[s]), UPDATE_WS);
            }
        }
    }
}
#endif