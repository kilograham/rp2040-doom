/**
 * Copyright (C) 2001-2020 Mitsutaka Okazaki
 * Copyright (C) 2021-2022 Graham Sanderson
 */
#include "slot_render.h"
#include <cstdio>
#include <cstring>

#if PICO_ON_DEVICE
#include "hardware/interp.h"
#define SLOT_RENDER_DATA __scratch_y("slot_render_cpp")
#else
#define SLOT_RENDER_DATA
#endif

static_assert(PM_DPHASE > 0, "");

#define unlikely(x)     __builtin_expect((x),0)

#if EMU8950_SLOT_RENDER
#include <algorithm>

#define INLINE inline
// todo combine all these tablles into 1
static uint8_t eg_step_tables_fast[4][8] = {
        {1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1, 1, 2, 1, 1, 1, 2},
        {1, 2, 1, 2, 1, 2, 1, 2},
        {1, 2, 2, 2, 1, 2, 2, 2},
};
static uint8_t eg_step_tables_fast2[4][8] = {
        {2, 2, 2, 2, 2, 2, 2, 2},
        {2, 2, 2, 4, 2, 2, 2, 4},
        {2, 4, 2, 4, 2, 4, 2, 4},
        {2, 4, 4, 4, 2, 4, 4, 4},
};
static uint8_t eg_step_tables[4][8] = {
        {0, 1, 0, 1, 0, 1, 0, 1},
        {0, 1, 0, 1, 1, 1, 0, 1},
        {0, 1, 1, 1, 0, 1, 1, 1},
        {0, 1, 1, 1, 1, 1, 1, 1},
};
static uint8_t eg_step_table4[8] = {
        4,4,4,4,4,4,4,4
};
// too can find this elesewhere
static uint8_t eg_step_table0[8] = {
        0,0,0,0,0,0,0,0
};

// todo combine all these tablles into 1 - seems slower for now probably not in asm tho
//static uint8_t eg_step[14][8] = {
//#define ST_NORMAL 0
//        {0, 1, 0, 1, 0, 1, 0, 1},
//        {0, 1, 0, 1, 1, 1, 0, 1},
//        {0, 1, 1, 1, 0, 1, 1, 1},
//        {0, 1, 1, 1, 1, 1, 1, 1},
//#define ST_FAST 4
//        {1, 1, 1, 1, 1, 1, 1, 1},
//        {1, 1, 1, 2, 1, 1, 1, 2},
//        {1, 2, 1, 2, 1, 2, 1, 2},
//        {1, 2, 2, 2, 1, 2, 2, 2},
//#define ST_FAST2 8
//        {2, 2, 2, 2, 2, 2, 2, 2},
//        {2, 2, 2, 4, 2, 2, 2, 4},
//        {2, 4, 2, 4, 2, 4, 2, 4},
//        {2, 4, 4, 4, 2, 4, 4, 4},
//#define ST_FOUR 12
//        {4, 4, 4, 4, 4, 4, 4, 4},
//#define ST_ZERO 13
//        {0, 0, 0, 0, 0, 0, 0, 0},
//};

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
// todo we are probably fine with pm_table_half[x] = pm_table[x/2] >> 1 as an approcimation, but keeping separate for diff for now
static int8_t pm_table_half[8][PM_PG_WIDTH] = {
        {0 >> 1, 0 >> 1, 0 >> 1, 0 >> 1, 0 >> 1, 0 >> 1,  0 >> 1,  0 >> 1},    // fnum = 000xxxxx
        {0 >> 1, 0 >> 1, 1 >> 1, 0 >> 1, 0 >> 1, 0 >> 1,  -1 >> 1, 0 >> 1},   // fnum = 001xxxxx
        {0 >> 1, 1 >> 1, 2 >> 1, 1 >> 1, 0 >> 1, -1 >> 1, -2 >> 1, -1 >> 1}, // fnum = 010xxxxx
        {0 >> 1, 1 >> 1, 3 >> 1, 1 >> 1, 0 >> 1, -1 >> 1, -3 >> 1, -1 >> 1}, // fnum = 011xxxxx
        {0 >> 1, 2 >> 1, 4 >> 1, 2 >> 1, 0 >> 1, -2 >> 1, -4 >> 1, -2 >> 1}, // fnum = 100xxxxx
        {0 >> 1, 2 >> 1, 5 >> 1, 2 >> 1, 0 >> 1, -2 >> 1, -5 >> 1, -2 >> 1}, // fnum = 101xxxxx
        {0 >> 1, 3 >> 1, 6 >> 1, 3 >> 1, 0 >> 1, -3 >> 1, -6 >> 1, -3 >> 1}, // fnum = 110xxxxx
        {0 >> 1, 3 >> 1, 7 >> 1, 3 >> 1, 0 >> 1, -3 >> 1, -7 >> 1, -3 >> 1}, // fnum = 111xxxxx
};

/* clang-format off */
/* exp_table[255-x] = round((exp2((double)x / 256.0) - 1) * 1024) */
static uint16_t exp_table[256] = {
        1024+1018,  1024+1013,  1024+1007,  1024+1002,   1024+996,   1024+991,   1024+986,   1024+980,   1024+975,   1024+969,   1024+964,   1024+959,   1024+953,   1024+948,   1024+942,   1024+937,
        1024+932,   1024+927,   1024+921,   1024+916,   1024+911,   1024+906,   1024+900,   1024+895,   1024+890,   1024+885,   1024+880,   1024+874,   1024+869,   1024+864,   1024+859,   1024+854,
        1024+849,   1024+844,   1024+839,   1024+834,   1024+829,   1024+824,   1024+819,   1024+814,   1024+809,   1024+804,   1024+799,   1024+794,   1024+789,   1024+784,   1024+779,   1024+774,
        1024+770,   1024+765,   1024+760,   1024+755,   1024+750,   1024+745,   1024+741,   1024+736,   1024+731,   1024+726,   1024+722,   1024+717,   1024+712,   1024+708,   1024+703,   1024+698,
        1024+693,   1024+689,   1024+684,   1024+680,   1024+675,   1024+670,   1024+666,   1024+661,   1024+657,   1024+652,   1024+648,   1024+643,   1024+639,   1024+634,   1024+630,   1024+625,
        1024+621,   1024+616,   1024+612,   1024+607,   1024+603,   1024+599,   1024+594,   1024+590,   1024+585,   1024+581,   1024+577,   1024+572,   1024+568,   1024+564,   1024+560,   1024+555,
        1024+551,   1024+547,   1024+542,   1024+538,   1024+534,   1024+530,   1024+526,   1024+521,   1024+517,   1024+513,   1024+509,   1024+505,   1024+501,   1024+496,   1024+492,   1024+488,
        1024+484,   1024+480,   1024+476,   1024+472,   1024+468,   1024+464,   1024+460,   1024+456,   1024+452,   1024+448,   1024+444,   1024+440,   1024+436,   1024+432,   1024+428,   1024+424,
        1024+420,   1024+416,   1024+412,   1024+409,   1024+405,   1024+401,   1024+397,   1024+393,   1024+389,   1024+385,   1024+382,   1024+378,   1024+374,   1024+370,   1024+367,   1024+363,
        1024+359,   1024+355,   1024+352,   1024+348,   1024+344,   1024+340,   1024+337,   1024+333,   1024+329,   1024+326,   1024+322,   1024+318,   1024+315,   1024+311,   1024+308,   1024+304,
        1024+300,   1024+297,   1024+293,   1024+290,   1024+286,   1024+283,   1024+279,   1024+276,   1024+272,   1024+268,   1024+265,   1024+262,   1024+258,   1024+255,   1024+251,   1024+248,
        1024+244,   1024+241,   1024+237,   1024+234,   1024+231,   1024+227,   1024+224,   1024+220,   1024+217,   1024+214,   1024+210,   1024+207,   1024+204,   1024+200,   1024+197,   1024+194,
        1024+190,   1024+187,   1024+184,   1024+181,   1024+177,   1024+174,   1024+171,   1024+168,   1024+164,   1024+161,   1024+158,   1024+155,   1024+152,   1024+148,   1024+145,   1024+142,
        1024+139,   1024+136,   1024+133,   1024+130,   1024+126,   1024+123,   1024+120,   1024+117,   1024+114,   1024+111,   1024+108,   1024+105,   1024+102,    1024+99,    1024+96,    1024+93,
        1024+90,    1024+87,    1024+84,    1024+81,    1024+78,    1024+75,    1024+72,    1024+69,    1024+66,    1024+63,    1024+60,    1024+57,    1024+54,    1024+51,    1024+48,    1024+45,
        1024+42,    1024+40,    1024+37,    1024+34,    1024+31,    1024+28,    1024+25,    1024+22,    1024+20,    1024+17,    1024+14,    1024+11,     1024+8,     1024+6,     1024+3,     1024+0,
};

/* logsin_table[x] = round(-log2(sin((x + 0.5) * PI / (PG_WIDTH / 4) / 2)) * 256) */

static uint32_t ml_table[16] = {1, 1 * 2, 2 * 2, 3 * 2, 4 * 2, 5 * 2, 6 * 2, 7 * 2,
                                8 * 2, 9 * 2, 10 * 2, 10 * 2, 12 * 2, 12 * 2, 15 * 2, 15 * 2};

#if !EMU8950_NO_WAVE_TABLE_MAP
#define LOGSIN_TABLE_SIZE PG_WIDTH / 4
#else
#define LOGSIN_TABLE_SIZE PG_WIDTH / 2
#endif
static uint16_t SLOT_RENDER_DATA logsin_table[LOGSIN_TABLE_SIZE] = {
        2137,  1731,  1543,  1419,  1326,  1252,  1190,  1137,  1091,  1050,  1013,  979, 949, 920, 894, 869,
        846,  825,  804,  785,  767,  749,  732,  717,  701,  687,  672,  659,  646,  633,  621,  609,
        598,  587,  576,  566,  556,  546,  536,  527,  518,  509,  501,  492,  484,  476,  468,  461,
        453,  446,  439,  432,  425,  418,  411,  405,  399,  392,  386,  380,  375,  369,  363,  358,
        352,  347,  341,  336,  331,  326,  321,  316,  311,  307,  302,  297,  293,  289,  284,  280,
        276,  271,  267,  263,  259,  255,  251,  248,  244,  240,  236,  233,  229,  226,  222,  219,
        215,  212,  209,  205,  202,  199,  196,  193,  190,  187,  184,  181,  178,  175,  172,  169,
        167,  164,  161,  159,  156,  153,  151,  148,  146,  143,  141,  138,  136,  134,  131,  129,
        127,  125,  122,  120,  118,  116,  114,  112,  110,  108,  106,  104,  102,  100,  98,  96,
        94,  92,  91,  89,  87,  85,  83,  82,  80,  78,  77,  75,  74,  72,  70,  69,
        67,  66,  64,  63,  62,  60,  59,  57,  56,  55,  53,  52,  51,  49,  48,  47,
        46,  45,  43,  42,  41,  40,  39,  38,  37,  36,  35,  34,  33,  32,  31,  30,
        29,  28,  27,  26,  25,  24,  23,  23,  22,  21,  20,  20,  19,  18,  17,  17,
        16,  15,  15,  14,  13,  13,  12,  12,  11,  10,  10,  9,  9,  8,  8,  7,
        7,  7,  6,  6,  5,  5,  5,  4,  4,  4,  3,  3,  3,  2,  2,  2,
        2,  1,  1,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,
#if EMU8950_NO_WAVE_TABLE_MAP
        // double the table size to include second 1/4 cycle
        0,      0,      0,      0,      0,      0,      0,      0,      1,      1,      1,      1,      1,      1,      1,      2,
        2,      2,      2,      3,      3,      3,      4,      4,      4,      5,      5,      5,      6,      6,      7,      7,
        7,      8,      8,      9,      9,     10,     10,     11,     12,     12,     13,     13,     14,     15,     15,     16,
        17,     17,     18,     19,     20,     20,     21,     22,     23,     23,     24,     25,     26,     27,     28,     29,
        30,     31,     32,     33,     34,     35,     36,     37,     38,     39,     40,     41,     42,     43,     45,     46,
        47,     48,     49,     51,     52,     53,     55,     56,     57,     59,     60,     62,     63,     64,     66,     67,
        69,     70,     72,     74,     75,     77,     78,     80,     82,     83,     85,     87,     89,     91,     92,     94,
        96,     98,    100,    102,    104,    106,    108,    110,    112,    114,    116,    118,    120,    122,    125,    127,
        129,    131,    134,    136,    138,    141,    143,    146,    148,    151,    153,    156,    159,    161,    164,    167,
        169,    172,    175,    178,    181,    184,    187,    190,    193,    196,    199,    202,    205,    209,    212,    215,
        219,    222,    226,    229,    233,    236,    240,    244,    248,    251,    255,    259,    263,    267,    271,    276,
        280,    284,    289,    293,    297,    302,    307,    311,    316,    321,    326,    331,    336,    341,    347,    352,
        358,    363,    369,    375,    380,    386,    392,    399,    405,    411,    418,    425,    432,    439,    446,    453,
        461,    468,    476,    484,    492,    501,    509,    518,    527,    536,    546,    556,    566,    576,    587,    598,
        609,    621,    633,    646,    659,    672,    687,    701,    717,    732,    749,    767,    785,    804,    825,    846,
        869,    894,    920,    949,    979,   1013,   1050,   1091,   1137,   1190,   1252,   1326,   1419,   1543,   1731,   2137,
#endif
};

#if EMU8950_NO_WAVE_TABLE_MAP
// we start with
//  _  _
// / \/ \ which is abs(sine) wave
//
static uint16_t wav_or_table_lookup[4][4] = {
        {0x0000, 0x0000, 0x8000, 0x8000}, // .. negate second half
        {0x0000, 0x0000, 0x0fff, 0x0fff}, // .. attenuate second half
        {0x0000, 0x0000, 0x0000, 0x0000}, // .. leave second half alone
        {0x0000, 0x0fff, 0x0000, 0x0fff}, // .. attenuate 1 and 3
};
#endif

static INLINE int16_t calc_sample(const SLOT_RENDER *slot, uint32_t index, int16_t am) {

#if !EMU8950_NO_WAVE_TABLE_MAP
    uint16_t h = slot->wave_table[index & (PG_WIDTH - 1)];
#else
#if PICO_ON_DEVICE
    interp0->accum[0] = index << 1;
    uint16_t h = *(uint16_t *)(interp0->peek[0]) | *(uint16_t *)(interp0->peek[1]);
#else
    uint16_t h =  slot->wav_or_table[(index >> (PG_BITS - 2)) & 3] | slot->logsin_table[(index & (PG_WIDTH / 2 - 1))];
#endif
#endif
    uint16_t att = h + slot->eg_out_tll_lsl3 + am;
    int16_t t = exp_table[att&0xff];
    // note we're really just bit clearing the original top bit 15 ..
    // todo presumably the & is ignored on ARM?
    int16_t res = t >> ((att>>8)&127);
    if (!res) return res; // maybe make things more compatible
#if EMU8950_LINEAR_NEG_NOT_NOT
    return ((att & 0x8000) ? -res : res) << 1;
#else
    return ((att & 0x8000) ? ~res : res) << 1;
#endif
}

#if 0
static uint8_t *get_attack_step_table(SLOT_RENDER *slot) {
    int index = slot->eg_rate_l;
    uint32_t hm1 = (slot->eg_rate_h - 1);
    if (hm1 < 12) {
        index += ST_NORMAL;
    } else if (hm1 >= 14) {
        // 0 and 15
        index = ST_ZERO;
    } else if (hm1 == 12) {
        index += ST_FAST;
    } else {
        index += ST_FAST2;
    }
    return eg_step[index];
}

// note this is the same as attack except for 15 => ST_FOUR
static uint8_t *get_decay_step_table(SLOT_RENDER *slot) {
    uint32_t hm1 = (slot->eg_rate_h - 1);
    int index = slot->eg_rate_l;
    if (hm1 < 12) {
        index += ST_NORMAL;
    } else if (hm1 >= 14) {
        index = hm1 == 14 ? ST_FOUR : ST_ZERO;
    } else if (hm1 == 12) {
        index += ST_FAST;
    } else {
        index += ST_FAST2;
    }
    return eg_step[index];
}
#else
static uint8_t *get_attack_step_table(SLOT_RENDER *slot) {
    switch (slot->eg_rate_h) {
        case 13:
            return eg_step_tables_fast[slot->eg_rate_l];
        case 14:
            return eg_step_tables_fast2[slot->eg_rate_l];
        case 0:
        case 15:
            return eg_step_table0;
        default:
            return eg_step_tables[slot->eg_rate_l];
    }
}

static uint8_t *get_decay_step_table(SLOT_RENDER *slot) {
    switch (slot->eg_rate_h) {
        case 0:
            return eg_step_table0;
        case 13:
            return eg_step_tables_fast[slot->eg_rate_l];
        case 14:
            return eg_step_tables_fast2[slot->eg_rate_l];
        case 15:
            return eg_step_table4;
        default:
            return eg_step_tables[slot->eg_rate_l];
    }
}
#endif

template <int EG_STATE> int get_parameter_rate(SLOT_RENDER *slot) {
    switch (EG_STATE) {
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

template <int EG_STATE> void commit_slot_update_eg_only(SLOT_RENDER *slot) {
    int p_rate = get_parameter_rate<EG_STATE>(slot);

    if (p_rate == 0) {
        slot->eg_shift = 0;
        slot->eg_rate_h = 0;
        slot->eg_rate_l = 0;
    } else {
        slot->eg_rate_h = std::min(15, p_rate + (slot->rks >> 2));
        slot->eg_rate_l = slot->rks & 3;
        if (EG_STATE == ATTACK) {
            slot->eg_shift = (0 < slot->eg_rate_h && slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
        } else {
            slot->eg_shift = (slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
        }
    }
}

template<bool PM> uint32_t advance_phase(SLOT_RENDER *slot, uint32_t &pm_phase) {
    int8_t pm = 0;
    if (PM) {
#if !PICO_ON_DEVICE
        // todo if we do this with interpolator, then we can just skip the if
        // todo PM_DPHASE == 512
        pm_phase = (pm_phase + PM_DPHASE) & (PM_DP_WIDTH - 1);
        pm = slot->efix_pm_table[pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
#else
        interp1->add_raw[1] = 1;
//        printf("%08x %08x\n", interp1->accum[1], interp1->peek[1]);
        pm = *(int8_t *)interp1->peek[1];
#endif
    }
#if !PICO_ON_DEVICE
    slot->pg_phase += slot->efix_pg_pm_x_fnum3ff + pm * slot->efix_pg_phase_multiplier;
#if EMU8950_NIT_PICKS
    slot->pg_phase &= (DP_WIDTH - 1) * 2; // note the clear bottom bit is just for exact compatability for comparison with original code
#else
    slot->pg_phase &= (DP_WIDTH * 2 - 1); // should be quicker on ARM
#endif
    return slot->pg_phase >> (DP_BASE_BITS + 1);
#else
    static_assert(((DP_WIDTH * 2 - 1) >> (DP_BASE_BITS + 1)) == (1 << PG_BITS) -1, "");
    // pg_phase += pm * slot->efix_pg_phase_multiplier
    if (pm) {
        interp1->add_raw[0] = pm * slot->efix_pg_phase_multiplier;
    }
    interp1->add_raw[0] = interp1->base[0];
    // tmp =  slot->pg_phase >> (DP_BASE_BITS + 1) & ((1 << PG_BITS) - 1);
    // slot->pg_phase += slot->efix_pg_pm_x_fnum3ff
    // return tmp;
    return interp1->add_raw[0];
#endif
}

template<bool PM> void mod_am1_fb1_fn(SLOT_RENDER *slot, uint32_t& pm_phase, uint32_t s) {
    int16_t fm = (slot->output[1] + slot->output[0]) >> slot->nine_minus_FB;
    slot->output[1] = slot->output[0];
    uint32_t pg_out = advance_phase<PM>(slot, pm_phase);
    slot->mod_buffer[s] = slot->output[0] = calc_sample(slot, pg_out + fm, slot->lfo_am_buffer_lsl3[s]);
}

template<bool PM> void mod_am1_fb0_fn(SLOT_RENDER *slot, uint32_t& pm_phase, uint32_t s) {
    uint32_t pg_out = advance_phase<PM>(slot, pm_phase);
    slot->mod_buffer[s] = calc_sample(slot, pg_out, slot->lfo_am_buffer_lsl3[s]);
}

template <bool PM> void mod_am0_fb1_fn(SLOT_RENDER *slot, uint32_t& pm_phase, uint32_t s) {
    int16_t fm = (slot->output[1] + slot->output[0]) >> slot->nine_minus_FB;
    slot->output[1] = slot->output[0];
    uint32_t pg_out = advance_phase<PM>(slot, pm_phase);

    slot->mod_buffer[s] = slot->output[0] = calc_sample(slot, pg_out + fm, 0);
}

template <bool PM> void mod_am0_fb0_fn(SLOT_RENDER *slot, uint32_t& pm_phase, uint32_t s) {
    uint32_t pg_out = advance_phase<PM>(slot, pm_phase);
    slot->mod_buffer[s] = calc_sample(slot, pg_out, 0);

}

template <bool PM> void alg0_am1_fn(SLOT_RENDER *slot, uint32_t& pm_phase, uint32_t s) {
    // todo is this masking realy necessary; i doubt it. .. seems to be always even anyway
//        int32_t fm = 2 * (opl->mod_buffer[s] >> 1);
    int32_t fm = slot->mod_buffer[s];
    uint32_t pg_out = advance_phase<PM>(slot, pm_phase);
    int32_t val = calc_sample(slot, pg_out + fm, slot->lfo_am_buffer_lsl3[s]);
    slot->buffer[s] += val;
}

template <bool PM> void alg0_am0_fn(SLOT_RENDER *slot, uint32_t& pm_phase, uint32_t s) {
    // todo could reset these when we start a new note
//    slot->output[1] = slot->output[0];

    // todo is this masking realy necessary; i doubt it. .. seems to be always even anyway
//        int32_t fm = 2 * (opl->mod_buffer[s] >> 1);
    int32_t fm = slot->mod_buffer[s];

    uint32_t pg_out = advance_phase<PM>(slot, pm_phase);
    int32_t val = calc_sample(slot, pg_out + fm, 0);
    slot->buffer[s] += val;
}


template <bool PM> void alg1_am1_fn(SLOT_RENDER *slot, uint32_t& pm_phase, uint32_t s) {
    uint32_t pg_out = advance_phase<PM>(slot, pm_phase);
    int16_t val = calc_sample(slot, pg_out, slot->lfo_am_buffer_lsl3[s]);
    slot->buffer[s] += val + slot->mod_buffer[s];
}

template <bool PM> void alg1_am0_fn(SLOT_RENDER *slot, uint32_t& pm_phase, uint32_t s) {
    uint32_t pg_out = advance_phase<PM>(slot, pm_phase);
    int16_t val = calc_sample(slot, pg_out, 0);
    slot->buffer[s] += val + slot->mod_buffer[s];
}

#if PICO_ON_DEVICE
extern "C" uint32_t test_slot_asm(SLOT_RENDER *slot, uint32_t nsamples, uint32_t eg_counter, uint fn);
#endif

template <int F_NUM, typename F> uint32_t slot_envelope_loop(F&& fn, SLOT_RENDER *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    // factored out as it is constant per call
    slot->efix_pg_phase_multiplier = ml_table[slot->patch->ML] << slot->blk;
    uint32_t efix_pg_pm_x_fnum3ff = (slot->fnum & 0x3ff) * slot->efix_pg_phase_multiplier;
    // pm = pm_table[(slot->fnum >> 7) & 7][pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
    // pm >>= (slot->pm_mode ? 0 : 1);
    int8_t *efix_pm_table = slot->pm_mode ? pm_table[(slot->fnum >> 7) & 7] :
                          pm_table_half[(slot->fnum >> 7) & 7];
#if !PICO_ON_DEVICE
    slot->wav_or_table = wav_or_table_lookup[slot->patch->WS & 3];
    slot->logsin_table = logsin_table;
    slot->efix_pg_pm_x_fnum3ff = efix_pg_pm_x_fnum3ff;
    slot->efix_pm_table = efix_pm_table;
#else
    // note that index is pre-doubled
    // wave_table[(index >> (PG_BITS - 2)) & 3]
    interp_config c = interp_default_config();
    interp_config_set_shift(&c, PG_BITS - 2);
    interp_config_set_mask(&c, 1, 2);
    interp_set_config(interp0, 0, &c);
    interp0->base[0] = (uintptr_t)wav_or_table_lookup[slot->patch->WS & 3];
    c = interp_default_config();
    interp_config_set_cross_input(&c, true);
    interp_config_set_mask(&c, 1, PG_BITS - 1);
    interp_set_config(interp0, 1, &c);
    // logsin_table[(index & (PG_WIDTH / 2 - 1))];
    interp0->base[1] = (uintptr_t)logsin_table;

    // return slot->pg_phase >> (DP_BASE_BITS + 1) & ((1 << PG_BITS) - 1);
    // post increment with
    c = interp_default_config();
    interp_config_set_add_raw(&c, true);
    interp_config_set_shift(&c, DP_BASE_BITS + 1);
    interp_config_set_mask(&c, 0, PG_BITS - 1);
    interp_set_config(interp1, 0, &c);
    slot->efix_pg_pm_x_fnum3ff = efix_pg_pm_x_fnum3ff;
    interp1->base[0] = efix_pg_pm_x_fnum3ff;
    interp1->accum[0] = slot->pg_phase;

    c = interp_default_config();
    static_assert(PM_DPHASE == 1, ""); // better for everyone!
    // pm_phase = (pm_phase + PM_DPHASE) & (PM_DP_WIDTH - 1);
    // pm = slot->efix_pm_table[pm_phase >> (PM_DP_BITS - PM_PG_BITS)];
    interp_config_set_shift(&c, PM_DP_BITS - PM_PG_BITS);
    interp_config_set_mask(&c, 0, PM_PG_BITS - 1);
    interp_set_config(interp1, 1, &c);
    interp1->base[1] = (uintptr_t)efix_pm_table;
    interp1->accum[1] = pm_phase;
    // we lookup in the lfo_am_buffer_lsl3 at inter0->base[2] + sample_ptr_in_mod_buffer / 2 for mod
    //                                  or at inter0->base[2] + sample_ptr_in_buffer / 4
    if (F_NUM <8) {
        interp0->base[2] = (uintptr_t)slot->lfo_am_buffer_lsl3 - ((uintptr_t)slot->mod_buffer) / 2;
    } else {
        interp0->base[2] = (uintptr_t)slot->lfo_am_buffer_lsl3 - ((uintptr_t)slot->buffer) / 4;
    }
    interp1->base[2] = slot->efix_pg_phase_multiplier;
#endif
    uint32_t s = 0;
    uint32_t nsamples_bak = nsamples;
    slot->eg_out_tll_lsl3 = std::min(EG_MAX/*EG_MUTE*/, slot->eg_out + slot->tll) << 3; // note EG_MAX not EG_MUTE to avoid overflow check later

#if PICO_ON_DEVICE && EMU8950_ASM
    // we lookup in mod_buffer at buffer_mod_buffer_offset + sample_ptr_in_buffer / 2
    slot->buffer_mod_buffer_offset = (uintptr_t)slot->mod_buffer - ((uintptr_t)slot->buffer) / 2;
    s = test_slot_asm(slot, nsamples, eg_counter, F_NUM);
    slot->pg_phase = interp1->accum[0];
    return s;
#endif
    // todo eg_rate_h == 0 ...
    // todo prate == 0 ?
    if (slot->eg_state == ATTACK) {
        // note we roll eg_rate_h > 0 check which is (was) always paired with (eg_counter++ && eg_shift_mask) == 0
        // and guarantee that eg_counter is never zero because bit 31-30 area always set to 10 on entry
        uint32_t eg_shift_mask = slot->eg_rate_h > 0 ? (1 << slot->eg_shift) - 1 : 0xffffffff;
        uint8_t *eg_step_table = get_attack_step_table(slot);

        // this original code did something like this every loop
        //
        //    if ((++eg_counter & eg_shift_mask) == 0 && 0 < slot->eg_out)) {
        //        // update eg_out etc.
        //    }
        //    if (!eg_out) {
        //        // enter decay next time
        //    }
        //
        // since eg_out can only change as a result of the if, we do
        //
        // a) before the loop
        //
        //    if (!eg_out) {
        //       eg_counter++;
        //       // enter decay next time
        //    }
        //
        // b) inside the loop
        //
        //    if ((++eg_counter & eg_shift_mask) == 0) {
        //        // update eg_out etc.
        //        if (!eg_out) {
        //            // enter decay next time
        //        }
        //    }
        if (slot->eg_out == 0) {
            // straight into decay
            eg_counter++;
            slot->eg_state = DECAY;
            commit_slot_update_eg_only<DECAY>(slot);
            fn(slot, pm_phase, s++);
        } else {
            for (; s < nsamples; s++) {
#if DUMPO
                if (hack_ch == 17 && s == 12) breako();
#endif
                if (unlikely((++eg_counter & eg_shift_mask) == 0)) {
                    uint8_t step = eg_step_table[(eg_counter >> slot->eg_shift) & 7];
                    slot->eg_out += (~slot->eg_out * step) >> 3;
                    slot->eg_out_tll_lsl3 = std::min(EG_MAX/*EG_MUTE*/, slot->eg_out + slot->tll) << 3; // note EG_MAX not EG_MUTE to avoid overflow check later
                    if (unlikely(slot->eg_out == 0)) {
                        slot->eg_state = DECAY;
                        commit_slot_update_eg_only<DECAY>(slot);
                        nsamples = s;
                    }
                }
                fn(slot, pm_phase, s);
            }
        }
    }
    if (slot->eg_state == DECAY) {
        nsamples = nsamples_bak;
        // todo if note ends we can stop early
        uint32_t eg_shift_mask = slot->eg_rate_h > 0 ? (1 << slot->eg_shift) - 1 : 0xffffffff;
        uint8_t *eg_step_table = get_decay_step_table(slot);
        // the original code checked for sustain every loop, however we have move it inside the once per 1<<eg_shift_mask check
        // so we must check it here outside the sample loop unless the first iteration of the sample loop would check it because of the
        // value of eg_counter
        if (!(((eg_counter+1) & eg_shift_mask) == 0) &&
            ((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL)) {
            if (s < nsamples) {
                // because we have moved the second line of the test above into the per !(++eg_coiunter & eg_shift_maask))
                // test below, we have to half unroll the first loop
                slot->eg_state = SUSTAIN;
                eg_counter++;
                commit_slot_update_eg_only<SUSTAIN>(slot);
                fn(slot, pm_phase, s++);
            }
        } else {
            for (; s < nsamples; s++) {
                if (unlikely((++eg_counter & eg_shift_mask) == 0)) {
                    slot->eg_out = static_cast<int16_t>(std::min(EG_MUTE, slot->eg_out + (int) eg_step_table[
                            (eg_counter >> slot->eg_shift) & 7]));
                    slot->eg_out_tll_lsl3 = std::min(EG_MAX/*EG_MUTE*/, slot->eg_out + slot->tll) << 3; // note EG_MAX not EG_MUTE to avoid overflow check later
                    // todo check for decay to zero
                    if (unlikely((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL)) {
                        slot->eg_state = SUSTAIN;
                        commit_slot_update_eg_only<SUSTAIN>(slot);
                        nsamples = s;
#if EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION
                    } else if (slot->eg_out == EG_MUTE) {
                        // we have decayed to zero, so give up
                        nsamples = s;
#endif
                    }
                }
                fn(slot, pm_phase, s);
            }
        }
    }
    if (slot->eg_state == SUSTAIN || slot->eg_state == RELEASE) {
        nsamples = nsamples_bak;
        // todo if note ends we can stop early
        uint32_t eg_shift_mask = slot->eg_rate_h > 0 ? (1 << slot->eg_shift) - 1 : 0xffffffff;
        uint8_t *eg_step_table = get_decay_step_table(slot);
        for (; s < nsamples; s++) {
            if (unlikely((++eg_counter & eg_shift_mask) == 0)) {
                slot->eg_out = static_cast<int16_t>(std::min(EG_MUTE, slot->eg_out + (int)eg_step_table[(eg_counter >> slot->eg_shift)&7]));
#if EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION
                if (slot->eg_out == EG_MUTE) {
                    // we have decayed to zero, so give up
                    nsamples = s;
                }
#endif
                slot->eg_out_tll_lsl3 = std::min(EG_MAX/*EG_MUTE*/, slot->eg_out + slot->tll) << 3; // note EG_MAX not EG_MUTE to avoid overflow check later
            }
            fn(slot, pm_phase, s);
        }
    }
#if PICO_ON_DEVICE
    slot->pg_phase = interp1->accum[0];
#endif
    // not necessary for correctness
    //#if EMU8950_NIT_PICKS
    //    slot->pg_phase &= (DP_WIDTH - 1) * 2;
    //#endif
    return s;
}

typedef void OPL;

template<bool PM> uint32_t slot_mod_linear(OPL *opl, SLOT_RENDER *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    if (slot->patch->AM) {
        if (slot->patch->FB) {
            slot->nine_minus_FB = 9 - slot->patch->FB;
            return slot_envelope_loop<6+PM>(mod_am1_fb1_fn<PM>, slot, nsamples, eg_counter, pm_phase);
        } else {
            return slot_envelope_loop<2+PM>(mod_am1_fb0_fn<PM>, slot, nsamples, eg_counter, pm_phase);
        }
    } else {
        if (slot->patch->FB) {
            slot->nine_minus_FB = 9 - slot->patch->FB;
            return slot_envelope_loop<4+PM>(mod_am0_fb1_fn<PM>, slot, nsamples, eg_counter, pm_phase);
        } else {
            return slot_envelope_loop<0+PM>(mod_am0_fb0_fn<PM>, slot, nsamples, eg_counter, pm_phase);
        }
    }
}

extern "C" uint32_t slot_mod_linear(OPL *opl, SLOT_RENDER *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    if (slot->patch->PM)
        return slot_mod_linear<true>(opl, slot, nsamples, eg_counter, pm_phase);
    else
        return slot_mod_linear<false>(opl, slot, nsamples, eg_counter, pm_phase);
}

template<bool PM> uint32_t slot_car_linear_alg0(OPL *opl, SLOT_RENDER *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    if (slot->patch->AM) {
        return slot_envelope_loop<10+PM>(alg0_am1_fn<PM>, slot, nsamples, eg_counter, pm_phase);
    } else {
        return slot_envelope_loop<8+PM>(alg0_am0_fn<PM>, slot, nsamples, eg_counter, pm_phase);
    }
}

extern "C" uint32_t slot_car_linear_alg0(OPL *opl, SLOT_RENDER *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    if (slot->patch->PM)
        return slot_car_linear_alg0<true>(opl, slot, nsamples, eg_counter, pm_phase);
    else
        return slot_car_linear_alg0<false>(opl, slot, nsamples, eg_counter, pm_phase);
}

template<bool PM> uint32_t slot_car_linear_alg1(OPL *opl, SLOT_RENDER *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {

    if (slot->patch->AM) {
        return slot_envelope_loop<14+PM>(alg1_am1_fn<PM>, slot, nsamples, eg_counter, pm_phase);
    } else {
        return slot_envelope_loop<12+PM>(alg1_am0_fn<PM>, slot, nsamples, eg_counter, pm_phase);
    }
}


extern "C" uint32_t slot_car_linear_alg1(OPL *opl, SLOT_RENDER *slot, uint32_t nsamples, uint32_t eg_counter, uint32_t pm_phase) {
    if (slot->patch->PM)
        return slot_car_linear_alg1<true>(opl, slot, nsamples, eg_counter, pm_phase);
    else
        return slot_car_linear_alg1<false>(opl, slot, nsamples, eg_counter, pm_phase);
}
#endif