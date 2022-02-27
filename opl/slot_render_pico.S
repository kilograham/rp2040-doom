//
// Copyright (C) 2001-2020 Mitsutaka Okazaki
// Copyright (C) 2021-2022 Graham Sanderson
//
 .syntax unified

#define ATTACK 0
#define DECAY 1
#define SUSTAIN 2
#define RELEASE 3

//.section .data.slot_render_asm
.section .scratch_y.slot_render_S, "a"
exp_table:
.hword        1024+1018,  1024+1013,  1024+1007,  1024+1002,   1024+996,   1024+991,   1024+986,   1024+980,   1024+975,   1024+969,   1024+964,   1024+959,   1024+953,   1024+948,   1024+942,   1024+937
.hword        1024+932,   1024+927,   1024+921,   1024+916,   1024+911,   1024+906,   1024+900,   1024+895,   1024+890,   1024+885,   1024+880,   1024+874,   1024+869,   1024+864,   1024+859,   1024+854
.hword        1024+849,   1024+844,   1024+839,   1024+834,   1024+829,   1024+824,   1024+819,   1024+814,   1024+809,   1024+804,   1024+799,   1024+794,   1024+789,   1024+784,   1024+779,   1024+774
.hword        1024+770,   1024+765,   1024+760,   1024+755,   1024+750,   1024+745,   1024+741,   1024+736,   1024+731,   1024+726,   1024+722,   1024+717,   1024+712,   1024+708,   1024+703,   1024+698
.hword        1024+693,   1024+689,   1024+684,   1024+680,   1024+675,   1024+670,   1024+666,   1024+661,   1024+657,   1024+652,   1024+648,   1024+643,   1024+639,   1024+634,   1024+630,   1024+625
.hword        1024+621,   1024+616,   1024+612,   1024+607,   1024+603,   1024+599,   1024+594,   1024+590,   1024+585,   1024+581,   1024+577,   1024+572,   1024+568,   1024+564,   1024+560,   1024+555
.hword        1024+551,   1024+547,   1024+542,   1024+538,   1024+534,   1024+530,   1024+526,   1024+521,   1024+517,   1024+513,   1024+509,   1024+505,   1024+501,   1024+496,   1024+492,   1024+488
.hword        1024+484,   1024+480,   1024+476,   1024+472,   1024+468,   1024+464,   1024+460,   1024+456,   1024+452,   1024+448,   1024+444,   1024+440,   1024+436,   1024+432,   1024+428,   1024+424
.hword        1024+420,   1024+416,   1024+412,   1024+409,   1024+405,   1024+401,   1024+397,   1024+393,   1024+389,   1024+385,   1024+382,   1024+378,   1024+374,   1024+370,   1024+367,   1024+363
.hword        1024+359,   1024+355,   1024+352,   1024+348,   1024+344,   1024+340,   1024+337,   1024+333,   1024+329,   1024+326,   1024+322,   1024+318,   1024+315,   1024+311,   1024+308,   1024+304
.hword        1024+300,   1024+297,   1024+293,   1024+290,   1024+286,   1024+283,   1024+279,   1024+276,   1024+272,   1024+268,   1024+265,   1024+262,   1024+258,   1024+255,   1024+251,   1024+248
.hword        1024+244,   1024+241,   1024+237,   1024+234,   1024+231,   1024+227,   1024+224,   1024+220,   1024+217,   1024+214,   1024+210,   1024+207,   1024+204,   1024+200,   1024+197,   1024+194
.hword        1024+190,   1024+187,   1024+184,   1024+181,   1024+177,   1024+174,   1024+171,   1024+168,   1024+164,   1024+161,   1024+158,   1024+155,   1024+152,   1024+148,   1024+145,   1024+142
.hword        1024+139,   1024+136,   1024+133,   1024+130,   1024+126,   1024+123,   1024+120,   1024+117,   1024+114,   1024+111,   1024+108,   1024+105,   1024+102,    1024+99,    1024+96,    1024+93
.hword        1024+90,    1024+87,    1024+84,    1024+81,    1024+78,    1024+75,    1024+72,    1024+69,    1024+66,    1024+63,    1024+60,    1024+57,    1024+54,    1024+51,    1024+48,    1024+45
.hword        1024+42,    1024+40,    1024+37,    1024+34,    1024+31,    1024+28,    1024+25,    1024+22,    1024+20,    1024+17,    1024+14,    1024+11,     1024+8,     1024+6,     1024+3,     1024+0

eg_step_tables: // note the defines have 0x80 to indicate that there are 4 x 8 entries based for each of eg_rate_l
#define ST_NORMAL ((0 * 8) | 0x80)
.byte 0, 1, 0, 1, 0, 1, 0, 1
.byte 0, 1, 0, 1, 1, 1, 0, 1
.byte 0, 1, 1, 1, 0, 1, 1, 1
.byte 0, 1, 1, 1, 1, 1, 1, 1
#define ST_FAST ((4 * 8) | 0x80)
.byte 1, 1, 1, 1, 1, 1, 1, 1
.byte 1, 1, 1, 2, 1, 1, 1, 2
.byte 1, 2, 1, 2, 1, 2, 1, 2
.byte 1, 2, 2, 2, 1, 2, 2, 2
#define ST_FAST2 ((8 * 8) | 0x80)
.byte 2, 2, 2, 2, 2, 2, 2, 2
.byte 2, 2, 2, 4, 2, 2, 2, 4
.byte 2, 4, 2, 4, 2, 4, 2, 4
.byte 2, 4, 4, 4, 2, 4, 4, 4
#define ST_FOUR (12 * 8)
.byte 4, 4, 4, 4, 4, 4, 4, 4
#define ST_ZERO (13 * 8)
.byte 0, 0, 0, 0, 0, 0, 0, 0

attack_rate_def_table:
    .byte ST_ZERO, ST_NORMAL, ST_NORMAL, ST_NORMAL
    .byte ST_NORMAL, ST_NORMAL, ST_NORMAL, ST_NORMAL
    .byte ST_NORMAL, ST_NORMAL, ST_NORMAL, ST_NORMAL
    .byte ST_NORMAL, ST_FAST, ST_FAST2, ST_ZERO

decay_rate_def_table:
    .byte ST_ZERO, ST_NORMAL, ST_NORMAL, ST_NORMAL
    .byte ST_NORMAL, ST_NORMAL, ST_NORMAL, ST_NORMAL
    .byte ST_NORMAL, ST_NORMAL, ST_NORMAL, ST_NORMAL
    .byte ST_NORMAL, ST_FAST, ST_FAST2, ST_FOUR

.section .text.slot_render_asm
#include "pico/asm_helper.S"
#include "hardware/regs/sio.h"

#define rl_slot                 r0
#define rl_sample_out           r1
#define rl_eg_counter           r2
#define rl_eg_shift_mask   r3
#define rl_tmp0                 r4
#define rl_tmp1                 r5
#define rl_tmp2                 r6
#define rl_interp               r7

#define rh_begin_loop       r8
#define rh_end_loop         r9
#define rh_sample_out_end   r10
#define rh_fn               r11
#define rh_exp_table        r12
// only used for alg0 and alg1; hmm. don't care
#define rh_buffer_mod_buffer_offset lr

#define INTERP0_ACCUM0 (SIO_INTERP0_ACCUM0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP0_PEEK0 (SIO_INTERP0_PEEK_LANE0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP0_PEEK1 (SIO_INTERP0_PEEK_LANE1_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP0_BASE2_AKA_LFO_AM_BUFFER_LSL3_OFFSET (SIO_INTERP0_BASE2_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP1_PEEK0 (SIO_INTERP1_PEEK_LANE0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP1_PEEK1 (SIO_INTERP1_PEEK_LANE1_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP1_POP0 (SIO_INTERP1_POP_LANE0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP1_ADD_RAW0 (SIO_INTERP1_ACCUM0_ADD_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP1_ADD_RAW1 (SIO_INTERP1_ACCUM1_ADD_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP1_BASE0 (SIO_INTERP1_BASE0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP1_BASE2_AKA_PG_PHASE_MULTIPLIER (SIO_INTERP1_BASE2_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)

#define SLOTB_EG_STATE      0x00
#define SLOTB_EG_RATE_H     0x01
#define SLOTB_EG_RATE_L     0x02
#define SLOTB_EG_SHIFT      0x03
#define SLOTB_NINE_MINUS_FB 0x04
#define SLOTB_RKS           0x05
#define SLOTH_EG_OUT        0x08
#define SLOTH_TLL           0x0a
#define SLOTW_EG_OUT_TLL_LSL3 0x0c
#define SLOTW_OUTPUT0       0x10
#define SLOTW_OUTPUT1       0x14
#define SLOTW_MOD_BUFFER    0x18
#define SLOTW_BUFFER        0x1c
#define SLOTW_BUFFER_MOD_BUFFER_OFFSET 0x20
#define SLOTW_PATCH         0x24

#define PATCH_EG 0x3
#define PATCH_DR 0x6
#define PATCH_SL 0x7
#define PATCH_RR 0x8

.macro advance_phase_pm0
    ldr rl_tmp0, [rl_interp, #INTERP1_BASE0]
    str rl_tmp0, [rl_interp, #INTERP1_ADD_RAW0]
    ldr rl_tmp0, [rl_interp, #INTERP1_ADD_RAW0]
.endm

.macro advance_phase_pm1
    movs rl_tmp1, #1
    str rl_tmp1, [rl_interp, #INTERP1_ADD_RAW1]
    ldr rl_tmp0, [rl_interp, #INTERP1_PEEK1]
    ldrb rl_tmp0, [rl_tmp0] // pm
    sxtb rl_tmp0, rl_tmp0
    ldr rl_tmp1, [rl_interp, #INTERP1_BASE2_AKA_PG_PHASE_MULTIPLIER]
    muls rl_tmp0, rl_tmp1
    ldr rl_tmp1, [rl_interp, #INTERP1_BASE0]
    adds rl_tmp0, rl_tmp1
    str rl_tmp0, [rl_interp, #INTERP1_ADD_RAW0]
    ldr rl_tmp0, [rl_interp, #INTERP1_ADD_RAW0]
.endm

.macro calc_sample is_am
    // interp0->accum[0] = index << 1;
    lsls rl_tmp0, #1 // todo can we avoid this
    str rl_tmp0, [rl_interp, #INTERP0_ACCUM0]

    // uint16_t h = *(uint16_t *)(interp0->peek[0]) | *(uint16_t *)(interp0->peek[1]);
    ldr rl_tmp1, [rl_interp, #INTERP0_PEEK0]
    ldr rl_tmp0, [rl_interp, #INTERP0_PEEK1]
    ldrh rl_tmp0, [rl_tmp0]
    ldrh rl_tmp1, [rl_tmp1]
    orrs rl_tmp1, rl_tmp0             // rl_tmp1 = att

    // uint16_t att = h + slot->eg_out_tll_lsl3
    ldr rl_tmp0, [rl_slot, #SLOTW_EG_OUT_TLL_LSL3]
    adds rl_tmp1, rl_tmp0
    // if (am) { h += am }
.if \is_am
    adds rl_tmp1, rl_tmp2
.endif

    // int16_t t = exp_table[att&0xff];
    lsls rl_tmp0, rl_tmp1, #24
    lsrs rl_tmp0, rl_tmp0, #23        // rl_tmp0 = (att&0xff) *2
    add  rl_tmp0, rh_exp_table // or load
    ldrh rl_tmp0, [rl_tmp0]

    // int16_t res = t >> ((att>>8)&127);
    // todo this is currently res : ~res but we can inverted our OR table
    // return ((att & 0x8000) ? ~res : res);
    lsls rl_tmp1,  #17
    sbcs rl_tmp2, rl_tmp2

    mvns rl_tmp2, rl_tmp2 // todo remove this too, but we need to fix up fm

    lsrs rl_tmp1, #25
    lsrs rl_tmp0, rl_tmp1
    // todo: inaudible todo force zeros for now
    beq 9f
    # note this matches the original ~res << 1 code, however
    # we don't have an early cutoff for full attenuation, so get -2 not 0 in some
    # cases, however this was true of the original code too; the annoying thing
    # is that it isn't trivial to fix up the - otherwise I think unimportant - differences for the sake of diff
    eors rl_tmp0, rl_tmp2
    lsls rl_tmp0, #1 // todo remove
9:
.endm

.macro calc_sample_am0
    calc_sample 0
.endm
.macro calc_sample_am1_lsr1
    // must preserve rl_tmp0... set am << 3 in rl_tmp2
    ldr rl_tmp1, [rl_interp, #INTERP0_BASE2_AKA_LFO_AM_BUFFER_LSL3_OFFSET]
    lsrs rl_tmp2, rl_sample_out, #1
    ldrb rl_tmp2, [rl_tmp1, rl_tmp2]
    calc_sample 1
.endm

.macro calc_sample_am1_lsr2
    // must preserve rl_tmp0... set am << 3 in rl_tmp2
    ldr rl_tmp1, [rl_interp, #INTERP0_BASE2_AKA_LFO_AM_BUFFER_LSL3_OFFSET]
    lsrs rl_tmp2, rl_sample_out, #2
    ldrb rl_tmp2, [rl_tmp1, rl_tmp2]
    calc_sample 1
.endm

.macro mod_sample_done
    strh rl_tmp0, [rl_sample_out]
    adds rl_sample_out, #2
    cmp rl_sample_out, rh_sample_out_end
    bge 9f
    bx rh_begin_loop
9:  bx rh_end_loop
.endm

.macro add_fb_fm
    ldr rl_tmp1, [rl_slot, #SLOTW_OUTPUT0]
    ldr rl_tmp2, [rl_slot, #SLOTW_OUTPUT1]
    str rl_tmp1, [rl_slot, #SLOTW_OUTPUT1]
    adds rl_tmp1, rl_tmp2
    ldrb rl_tmp2, [rl_slot, #SLOTB_NINE_MINUS_FB]
    lsrs rl_tmp1, rl_tmp2
    adds rl_tmp0, rl_tmp1
.endm

.thumb_func
mod_am0_fb0_pm0_fn:
    advance_phase_pm0   // -> tmp0 = pg_out
    calc_sample_am0     // tmp0 = pg_out -> tmp0 = sample
    mod_sample_done

.thumb_func
mod_am0_fb0_pm1_fn:
    advance_phase_pm1   // -> tmp0 = pg_out
    calc_sample_am0     // tmp0 = pg_out -> tmp0 = sample
    mod_sample_done

.thumb_func
mod_am1_fb0_pm0_fn:
    advance_phase_pm0   // -> tmp0 = pg_out
    calc_sample_am1_lsr1 // tmp0 = pg_out -> tmp0 = sample
    mod_sample_done

.thumb_func
mod_am1_fb0_pm1_fn:
    advance_phase_pm1    // -> tmp0 = pg_out

    calc_sample_am1_lsr1 // tmp0 = pg_out -> tmp0 = sample
    mod_sample_done

.thumb_func
mod_am0_fb1_pm0_fn:
    advance_phase_pm0   // -> tmp0 = pg_out
    add_fb_fm           // tmp0 = pg_out -> tmp0 = pg_out + self_fm()
    calc_sample_am0     // tmp0 = pg_out + fm -> tmp0 = sample
    str rl_tmp0, [rl_slot, #SLOTW_OUTPUT0]
    mod_sample_done

.thumb_func
mod_am0_fb1_pm1_fn:
    advance_phase_pm1   // -> tmp0 = pg_out
    add_fb_fm           // tmp0 = pg_out -> tmp0 = pg_out + self_fm()
    calc_sample_am0     // tmp0 = pg_out + fm -> tmp0 = sample
    str rl_tmp0, [rl_slot, #SLOTW_OUTPUT0]
    mod_sample_done

.thumb_func
mod_am1_fb1_pm0_fn:
    advance_phase_pm0   // -> tmp0 = pg_out
    add_fb_fm           // tmp0 = pg_out -> tmp0 = pg_out + self_fm()
    calc_sample_am1_lsr1 // tmp0 = pg_out + fm -> tmp0 = sample
    str rl_tmp0, [rl_slot, #SLOTW_OUTPUT0]
    mod_sample_done

.thumb_func
mod_am1_fb1_pm1_fn:
    advance_phase_pm1   // -> tmp0 = pg_out
    add_fb_fm           // tmp0 = pg_out -> tmp0 = pg_out + self_fm()
    calc_sample_am1_lsr1 // tmp0 = pg_out + fm -> tmp0 = sample
    str rl_tmp0, [rl_slot, #SLOTW_OUTPUT0]
    mod_sample_done

.macro alg_sample_done
    ldr rl_tmp1, [rl_sample_out]
    add rl_tmp1, rl_tmp0
    str rl_tmp1, [rl_sample_out]
    adds rl_sample_out, #4
    cmp rl_sample_out, rh_sample_out_end
    bge 9f
    bx rh_begin_loop
9:  bx rh_end_loop
.endm

// ah these two are the same, just called in differnt places
.macro alg0_f_mod
    lsrs rl_tmp1, rl_sample_out, #1
    add  rl_tmp1, rh_buffer_mod_buffer_offset
    ldrh rl_tmp2, [rl_tmp1]
    add  rl_tmp0, rl_tmp2
.endm

.macro alg1_a_mod
    lsrs rl_tmp1, rl_sample_out, #1
    add  rl_tmp1, rh_buffer_mod_buffer_offset
    ldrh rl_tmp2, [rl_tmp1]
    add  rl_tmp0, rl_tmp2
.endm

// alg0 is FM by the mod slot
.thumb_func
alg0_am0_pm0_fn:
    advance_phase_pm0   // -> tmp0 = pg_out
    alg0_f_mod      // tmp0 = pg_out -> tmp0 = pg_out + mod_slot[s]
    calc_sample_am0     // tmp0 = pg_out + mod_fm -> tmp0 = sample
    str rl_tmp0, [rl_slot, #SLOTW_OUTPUT0]
    alg_sample_done

.thumb_func
alg0_am0_pm1_fn:
    advance_phase_pm1   // -> tmp0 = pg_out
    alg0_f_mod      // tmp0 = pg_out -> tmp0 = pg_out + mod_slot[s]
    calc_sample_am0     // tmp0 = pg_out + mod_fm -> tmp0 = sample
    str rl_tmp0, [rl_slot, #SLOTW_OUTPUT0]
    alg_sample_done

.thumb_func
alg0_am1_pm0_fn:
    advance_phase_pm0   // -> tmp0 = pg_out
    alg0_f_mod      // tmp0 = pg_out -> tmp0 = pg_out + mod_slot[s]
    calc_sample_am1_lsr2 // tmp0 = pg_out + mod_fm -> tmp0 = sample
    str rl_tmp0, [rl_slot, #SLOTW_OUTPUT0]
    alg_sample_done

.thumb_func
alg0_am1_pm1_fn:
    advance_phase_pm1   // -> tmp0 = pg_out
    alg0_f_mod      // tmp0 = pg_out -> tmp0 = pg_out + mod_slot[s]
    calc_sample_am1_lsr2 // tmp0 = pg_out + mod_fm -> tmp0 = sample
    str rl_tmp0, [rl_slot, #SLOTW_OUTPUT0]
    alg_sample_done

// alg1 is AM by the mod slot
.thumb_func
alg1_am0_pm0_fn:
    advance_phase_pm0   // -> tmp0 = pg_out
    calc_sample_am0     // tmp0 = pg_out + mod_fm -> tmp0 = sample
    alg1_a_mod      // tmp0 = sample-> tmp- = sample + mod_slot[s]
    alg_sample_done

.thumb_func
alg1_am0_pm1_fn:
    advance_phase_pm1   // -> tmp0 = pg_out
    calc_sample_am0     // tmp0 = pg_out + mod_fm -> tmp0 = sample
    alg1_a_mod      // tmp0 = sample-> tmp- = sample + mod_slot[s]
    alg_sample_done

.thumb_func
alg1_am1_pm0_fn:
    advance_phase_pm0   // -> tmp0 = pg_out
    calc_sample_am1_lsr2 // tmp0 = pg_out + mod_fm -> tmp0 = sample
    alg1_a_mod      // tmp0 = sample-> tmp- = sample + mod_slot[s]
    alg_sample_done

.thumb_func
alg1_am1_pm1_fn:
    advance_phase_pm1   // -> tmp0 = pg_out
    calc_sample_am1_lsr2 // tmp0 = pg_out + mod_fm -> tmp0 = sample
    alg1_a_mod      // tmp0 = sample-> tmp- = sample + mod_slot[s]
    alg_sample_done

.global slot_render_fn_table
.align 4
slot_render_fn_table:
    .word mod_am0_fb0_pm0_fn
    .word mod_am0_fb0_pm1_fn
    .word mod_am1_fb0_pm0_fn
    .word mod_am1_fb0_pm1_fn
    .word mod_am0_fb1_pm0_fn
    .word mod_am0_fb1_pm1_fn
    .word mod_am1_fb1_pm0_fn
    .word mod_am1_fb1_pm1_fn
    .word alg0_am0_pm0_fn
    .word alg0_am0_pm1_fn
    .word alg0_am1_pm0_fn
    .word alg0_am1_pm1_fn
    .word alg1_am0_pm0_fn
    .word alg1_am0_pm1_fn
    .word alg1_am1_pm0_fn
    .word alg1_am1_pm1_fn

.macro set_eg_shift_mask
    ldrb rl_eg_shift_mask, [rl_slot, #SLOTB_EG_RATE_H]
    cmp rl_eg_shift_mask, #0
    beq 9f
    ldrb rl_tmp0, [rl_slot, #SLOTB_EG_SHIFT]
    movs rl_eg_shift_mask, #1
    lsls rl_eg_shift_mask, rl_tmp0
9:
    subs rl_eg_shift_mask, #1
.endm

// rl_tmp0 = eg_out
.macro update_eg_out_tll_lsl3
    //slot->eg_out_tll_lsl3 = std::min(EG_MAX/*EG_MUTE*/, slot->eg_out + slot->tll) << 3; // note EG_MAX not EG_MUTE to avoid overflow check later
    ldrh rl_tmp1, [rl_slot, #SLOTH_TLL]
    adds rl_tmp0, rl_tmp1
    movs rl_tmp2, #0x1f
    lsls rl_tmp2, #4
    cmp rl_tmp0, rl_tmp2
    ble 9f
    movs rl_tmp0, rl_tmp2
9:
    lsls rl_tmp0, #3
    str rl_tmp0, [rl_slot, #SLOTW_EG_OUT_TLL_LSL3]
.endm

#define FRAMEW_EG_STEP_TABLE 0
#define FRAMEW_SAMPLE_OUT_END_BACK 4
#define FRAME_SIZE 8

.macro set_step_table table_def
    // get_attack_step_table
    ldr rl_tmp0, =eg_step_tables
    ldr rl_tmp1, =\table_def
    ldrb rl_tmp2, [rl_slot, SLOTB_EG_RATE_H]
    ldrb rl_tmp2, [rl_tmp1, rl_tmp2]
    lsls rl_tmp1, rl_tmp2, #25
    bcc 1f
    lsrs rl_tmp2, rl_tmp1, #25
    ldrb rl_tmp1, [rl_slot, SLOTB_EG_RATE_L]
    lsls rl_tmp1, #3
    adds rl_tmp2, rl_tmp1
1:
    adds rl_tmp0, rl_tmp2
    str rl_tmp0, [sp, FRAMEW_EG_STEP_TABLE]
.endm

.macro update_eg_vars_for_non_zero_rate
    ldrb rl_tmp1, [rl_slot, #SLOTB_RKS]
    // slot->eg_rate_l = slot->rks & 3;
    lsls rl_tmp2, rl_tmp1, #30
    lsrs rl_tmp2, rl_tmp2, #30
    strb rl_tmp2, [rl_slot, #SLOTB_EG_RATE_L]
    // slot->eg_rate_h = std::min(15, p_rate + (slot->rks >> 2));
    lsrs rl_tmp1, #2
    adds rl_tmp0, rl_tmp1
    cmp rl_tmp0, #15
    ble 9f
    movs rl_tmp0, #15
9:
    strb rl_tmp0, [rl_slot, #SLOTB_EG_RATE_H]
    // slot->eg_shift = (slot->eg_rate_h < 12) ? (12 - slot->eg_rate_h) : 0;
    // note eg_shift was already zeroed above
    movs rl_tmp1, #12
    subs rl_tmp1, rl_tmp0
    bcc 1f
    strb rl_tmp1, [rl_slot, #SLOTB_EG_SHIFT]
.endm

// (slot, nsamples, eg_counter, fn);
.global test_slot_asm
.thumb_func
test_slot_asm:
#if 0
#define rl_slot                 r0 i
#define rl_sample_out           r1 x
#define rl_eg_counter           r2 i
#define rl_eg_shift_mask   r3 x
#define rl_tmp0                 r4 -
#define rl_tmp1                 r5 -
#define rl_tmp2                 r6 -
#define rl_interp               r7
#endif

// rh_begin_loop       r8   x
// rh_end_loop         r9   x
// rh_sample_out_end   r10  x
// rh_fn               r11  x
// rh_exp_table        r12  x
// rh_mod_buffer_offset lr  -

    push {r4-r7, lr}
    mov r4, r8
    mov r5, r9
    mov r6, r10
    mov r7, r11
    push {r4-r7}
    mov r4, r12
    push {r3, r4} // we want to save r3 as well
    sub sp, #FRAME_SIZE

    // note r1 == rl_sample_out
    lsrs r5, r3, #3
    bne 1f
    // is a mod fn
    lsls r4, r1, #1
    ldr rl_sample_out, [rl_slot, #SLOTW_MOD_BUFFER]
    add r4, rl_sample_out
    b 2f
1:
    // is an arg fn
    lsls r4, r1, #2
    ldr rl_sample_out, [rl_slot, #SLOTW_BUFFER]
    add r4, rl_sample_out
2:
    mov rh_sample_out_end, r4
    str r4, [sp, #FRAMEW_SAMPLE_OUT_END_BACK]

    ldr r4, =slot_render_fn_table
    lsls r3, #2
    ldr r4, [r4, r3]
    mov rh_fn, r4

    ldr rl_tmp0, =exp_table
    mov rh_exp_table, rl_tmp0

    ldr rl_tmp0, [rl_slot, #SLOTW_BUFFER_MOD_BUFFER_OFFSET]
    mov rh_buffer_mod_buffer_offset, rl_tmp0

    ldr rl_interp, =0xd0000080

    ldrb rl_tmp0, [rl_slot, #SLOTB_EG_STATE]
    cmp rl_tmp0, #ATTACK
    bne check_decay
attack:
    ldr rl_tmp0, =check_decay
    mov rh_end_loop, rl_tmp0

    set_eg_shift_mask
    set_step_table attack_rate_def_table

    // if (slot->eg_out == 0) {
    ldrh rl_tmp1, [rl_slot, #SLOTH_EG_OUT]
    cmp rl_tmp1, #0
    bne attack_loop_enter

    // early enter decay state
    adds rl_eg_counter, #1
    b enter_decay_state

attack_loop_enter:
    ldr rl_tmp0, =attack_loop
    mov rh_begin_loop, rl_tmp0
.thumb_func
attack_loop:
    adds rl_eg_counter, #1
    tst rl_eg_counter, rl_eg_shift_mask
    beq 1f
    bx rh_fn
1:
    ldrh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]
//    cmp rl_tmp0, #0
//    bhi 1f
//    bx rh_fn
//1:

    // uint8_t step = eg_step_table[(eg_counter >> slot->eg_shift) & 7];

    ldrb rl_tmp1, [rl_slot, #SLOTB_EG_SHIFT]
    movs rl_tmp2, rl_eg_counter
    lsrs rl_tmp2, rl_tmp1
    lsls rl_tmp2, #29
    lsrs rl_tmp2, #29
    ldr rl_tmp1, [sp, #FRAMEW_EG_STEP_TABLE]
    ldrb rl_tmp1, [rl_tmp1, rl_tmp2]

    // slot->eg_out += (~slot->eg_out * step) >> 3;
    mvns rl_tmp2, rl_tmp0
    muls rl_tmp2, rl_tmp1
    asrs rl_tmp2, #3
    adds rl_tmp0, rl_tmp2
    strh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]

    update_eg_out_tll_lsl3

    ldrh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]
    cmp rl_tmp0, #0
    beq enter_decay_state
    bx rh_fn

enter_decay_state:
    // int p_rate = slot->patch->DR;
    ldr rl_tmp0, [rl_slot, #SLOTW_PATCH]
    ldrb rl_tmp0, [rl_tmp0, #PATCH_DR]
    cmp rl_tmp0, #0
    // set all eg_state, shift, rate_h, rate_l all to zero (if rl_tmp0 is zero)
    str rl_tmp0, [rl_slot, #SLOTB_EG_STATE]
    beq 1f
    update_eg_vars_for_non_zero_rate
1:
    movs rl_tmp0, #DECAY
    strb rl_tmp0, [rl_slot, #SLOTB_EG_STATE]
    // do last sample
    mov rh_sample_out_end, rl_sample_out

    ldr rl_tmp0, =check_decay
    mov rh_begin_loop, rl_tmp0
    bx rh_fn

.thumb_func
check_decay:
    ldr rl_tmp0, [sp, #FRAMEW_SAMPLE_OUT_END_BACK]
    cmp rl_sample_out, rl_tmp0
    beq check_sustain_release // done is too far away
    mov rh_sample_out_end, rl_tmp0
    ldrb rl_tmp0, [rl_slot, #SLOTB_EG_STATE]
    cmp rl_tmp0, #DECAY
    bne check_sustain_release
decay:
    ldr rl_tmp0, =check_sustain_release
    mov rh_end_loop, rl_tmp0

    set_eg_shift_mask
    set_step_table decay_rate_def_table

    adds rl_tmp0, rl_eg_counter, #1
    tst rl_tmp0, rl_eg_shift_mask
    beq decay_loop_enter

    // check ((slot->patch->SL != 15) && (slot->eg_out >> 4) == slot->patch->SL))
    ldrh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]
    ldr rl_tmp1, [rl_slot, #SLOTW_PATCH]
    ldrb rl_tmp1, [rl_tmp1, #PATCH_SL]
    cmp rl_tmp1, #15
    beq decay_loop_enter

    lsrs rl_tmp2, rl_tmp0, #4
    cmp rl_tmp2, rl_tmp1
    bne decay_loop_enter
    // early enter sustain state
    mov rl_eg_counter, rl_tmp0
    b enter_sustain_state

decay_loop_enter:
    ldr rl_tmp0, =decay_loop
    mov rh_begin_loop, rl_tmp0
.thumb_func
decay_loop:
    adds rl_eg_counter, #1
    tst rl_eg_counter, rl_eg_shift_mask
    beq 1f
    bx rh_fn
1:
    //slot->eg_out = static_cast<int16_t>(std::min(EG_MUTE, slot->eg_out + (int) eg_step_table[(eg_counter >> slot->eg_shift) & 7]));
    ldr rl_tmp0, [sp, #FRAMEW_EG_STEP_TABLE]
    ldrb rl_tmp1, [rl_slot, #SLOTB_EG_SHIFT]
    movs rl_tmp2, rl_eg_counter
    lsrs rl_tmp2, rl_tmp1
    lsls rl_tmp2, #29
    lsrs rl_tmp2, #29
    ldrb rl_tmp1, [rl_tmp0, rl_tmp2]
    ldrh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]
    adds rl_tmp0, rl_tmp1
    // todo do we actually need this min check
    lsrs rl_tmp1, rl_tmp0, #9
    beq 1f
    ldr rl_tmp0, =0x1ff
1:
    strh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]

    // uses rl_tmp0
    update_eg_out_tll_lsl3

    ldrh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]
    ldr rl_tmp1, [rl_slot, #SLOTW_PATCH]
    ldrb rl_tmp1, [rl_tmp1, #PATCH_SL]
    cmp rl_tmp1, #15
    beq 1f

    lsrs rl_tmp2, rl_tmp0, #4
    cmp rl_tmp2, rl_tmp1
    beq enter_sustain_state

1:
    // check for EG_MUTE (0x1ff)
    adds rl_tmp1, rl_tmp0, #1
    lsrs rl_tmp1, #9
    bne 1f
    bx rh_fn

1:
    // we decayed to zero, but still need to run loop contents once more
    mov rh_sample_out_end, rl_sample_out
    bx rh_fn

enter_sustain_state:
    // int p_rate = slot->patch->EG ? 0 : slot->patch->RR
    ldr rl_tmp2, [rl_slot, #SLOTW_PATCH]
    ldrb rl_tmp1, [rl_tmp2, #PATCH_EG]
    movs rl_tmp0, #0
    cmp rl_tmp1, #0
    bne 1f
    ldrb rl_tmp0, [rl_tmp2, #PATCH_RR]
1:
    // set all eg_state, shift, rate_h, rate_l all to zero (if rl_tmp0 is zero)
    str rl_tmp0, [rl_slot, #SLOTB_EG_STATE]
    cmp rl_tmp0, #0
    beq 1f
    update_eg_vars_for_non_zero_rate
1:
    movs rl_tmp0, #SUSTAIN
    strb rl_tmp0, [rl_slot, #SLOTB_EG_STATE]
    // do last sample
    mov rh_sample_out_end, rl_sample_out

    ldr rl_tmp0, =check_sustain_release
    mov rh_begin_loop, rl_tmp0
    bx rh_fn

.thumb_func
check_sustain_release:
    ldr rl_tmp0, [sp, #FRAMEW_SAMPLE_OUT_END_BACK]
    cmp rl_sample_out, rl_tmp0
    beq done
    mov rh_sample_out_end, rl_tmp0
    ldrb rl_tmp0, [rl_slot, #SLOTB_EG_STATE]
    cmp rl_tmp0, #SUSTAIN
    blt done
    ldr rl_tmp0, =done
    mov rh_end_loop, rl_tmp0
    set_eg_shift_mask
    set_step_table decay_rate_def_table

sustain_release_loop_enter:
    ldr rl_tmp0, =sustain_release_loop
    mov rh_begin_loop, rl_tmp0
.thumb_func
sustain_release_loop:
    adds rl_eg_counter, #1
    tst rl_eg_counter, rl_eg_shift_mask
    beq 1f
    bx rh_fn
1:
    //slot->eg_out = static_cast<int16_t>(std::min(EG_MUTE, slot->eg_out + (int) eg_step_table[(eg_counter >> slot->eg_shift) & 7]));
    ldr rl_tmp0, [sp, #FRAMEW_EG_STEP_TABLE]
    ldrb rl_tmp1, [rl_slot, #SLOTB_EG_SHIFT]
    movs rl_tmp2, rl_eg_counter
    lsrs rl_tmp2, rl_tmp1
    lsls rl_tmp2, #29
    lsrs rl_tmp2, #29
    ldrb rl_tmp1, [rl_tmp0, rl_tmp2]
    ldrh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]
    adds rl_tmp0, rl_tmp1

    strh rl_tmp0, [rl_slot, #SLOTH_EG_OUT]
    adds rl_tmp2, rl_tmp0, #1
    lsrs rl_tmp1, rl_tmp2, #9
    bne 1f
    update_eg_out_tll_lsl3
    bx rh_fn
1:
    // we decayed to zero, but still need to run loop contents
    mov rh_sample_out_end, rl_sample_out
    bx rh_fn
.thumb_func
done:
    add sp, #FRAME_SIZE
    pop {r2-r7}
    mov r12, r3
    mov r8, r4
    mov r9, r5
    mov r10, r6
    mov r11, r7

    // figure out how many samples we did
    lsrs r2, #3
    bne 1f
    // is a mod fn
    ldr r0, [rl_slot, #SLOTW_MOD_BUFFER]
    subs r1, r0
    lsrs r0, r1, #1
    pop {r4-r7, pc}
1:
    // is an arg fn
    ldr r0, [rl_slot, #SLOTW_BUFFER]
    subs r1, r1
    lsrs r1, r0, #2
    pop {r4-r7, pc}


#if 0
    movs rl_tmp0, #1
    lsls rl_tmp0, #12
    subs rl_tmp0, #2
    lsls rl_tmp1, rl_eg_counter, #1
    cmp rl_tmp0, rl_tmp1
    bne 1f
    bkpt #0
1:

#endif