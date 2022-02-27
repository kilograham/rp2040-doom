/**
 * Copyright (C) 2001-2020 Mitsutaka Okazaki
 * Copyright (C) 2021-2022 Graham Sanderson
 */
 #ifndef _SLOT_RENDER_H_
#define _SLOT_RENDER_H_
#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#define EG_BITS 9
#define EG_MUTE ((1 << EG_BITS) - 1)
#define EG_MAX (0x1f0) // 93dB

/* sine table */
#define PG_BITS 10 /* 2^10 = 1024 length sine table */
#define PG_WIDTH (1 << PG_BITS)

/* phase increment counter */
#define DP_BITS 20
#define DP_WIDTH (1 << DP_BITS)
#define DP_BASE_BITS (DP_BITS - PG_BITS)

/* pitch modulator */
#define PM_PG_BITS 3
#define PM_PG_WIDTH (1 << PM_PG_BITS)

// no benefit that I can see to this being 22 - it just makes PM_DPHASE unruly at 512
#if 0
#define PM_DP_BITS 22
#else
#define PM_DP_BITS 13
#endif
#define PM_DP_WIDTH (1 << PM_DP_BITS)
#define PM_DPHASE (PM_DP_WIDTH / (1024 * 8))

/* voice data */
typedef struct __OPL_PATCH {
#if !EMU8950_NO_TLL
    /* 0 */ uint8_t TL;
    /* 1 */ uint8_t KL;
#else
    /* 0 */ uint8_t TL4;
    /* 1 */ uint8_t KL_SHIFT;
#endif
    /* 2 */ uint8_t FB;
    /* 3 */ uint8_t EG;
    /* 4 */ uint8_t ML;
    /* 5 */ uint8_t AR;
    /* 6 */ uint8_t DR;
    /* 7 */ uint8_t SL;
    /* 8 */ uint8_t RR;
    /* 9 */ uint8_t KR;
    /* a */ uint8_t AM;
    /* b */ uint8_t PM;
    uint8_t WS;
} OPL_PATCH;

enum __OPL_EG_STATE
{
    ATTACK, DECAY, SUSTAIN, RELEASE, UNKNOWN
};

struct SLOT_RENDER {
// stutff the assembly cares about is duplicate at the top so we can be sure of its order witout #ifs
#if EMU8950_ASM
    // note there is code that writes zero to all for at once
    /* 0x00 */ uint8_t eg_state;         /* current state */
    /* 0x01 */ uint8_t eg_rate_h;        /* eg speed rate high 4bits */
    /* 0x02 */ uint8_t eg_rate_l;        /* eg speed rate low 2bits */
    /* 0x03 */ uint8_t eg_shift;         /* shift for eg global counter, controls envelope speed */

    /* 0x04 */ uint8_t nine_minus_FB;
    /* 0x05 */ uint8_t rks;              /* key scale offset (rks) for eg speed */
    uint16_t pad2;
    /* 0x08 */ int16_t eg_out;           /* eg output */
    /* 0x0a */ uint16_t tll;             /* total level + key scale level*/
    /* 0x0c */ int32_t eg_out_tll_lsl3;  /* (eg_out + tll) << 3 */
    // only used for mod slot
    /* 0x10 */ int32_t output[2];        /* output value, latest and previous. */
    // these are actually opl local not slot local
    /* 0x18 */ int16_t *mod_buffer;
    /* 0x1c */ int32_t *buffer;
    /* 0x20 */ uintptr_t buffer_mod_buffer_offset;
    /* 0x24 */ OPL_PATCH *patch;
#else
    uint8_t eg_state;         /* current state */
    uint8_t eg_rate_h;        /* eg speed rate high 4bits */
    uint8_t eg_rate_l;        /* eg speed rate low 2bits */
#if EMU8950_SLOT_RENDER
    uint8_t nine_minus_FB;
    int32_t eg_out_tll_lsl3;  /* (eg_out + tll) << 3 */
#endif
    uint8_t rks;              /* key scale offset (rks) for eg speed */
    int16_t eg_out;           /* eg output */
    uint16_t tll;             /* total level + key scale level*/
    // only used for mod slot
    int32_t output[2];        /* output value, latest and previous. */
    // these are actually opl local not slot local
    int16_t *mod_buffer;
    int32_t *buffer;
    uint32_t eg_shift;        /* shift for eg global counter, controls envelope speed */
    OPL_PATCH *patch;
#endif
    uint16_t fnum;            /* f-number (9 bits) */
    uint16_t efix_pg_phase_multiplier; /* ml_table[slot->patch->ML]) << slot->blk >> 1 */
    uint8_t blk;              /* block (3 bits) */
    uint32_t pg_phase;        /* pg phase */ // note this moves twice as fast in slot_render version
#if EMU8950_SLOT_RENDER
//#if !PICO_ON_DEVICE
    uint32_t efix_pg_pm_x_fnum3ff; /* efix_pg_phase_multiplier * (fnum & 0x3ff) */
//#endif
#endif

#if EMU8950_SLOT_RENDER
    uint8_t *lfo_am_buffer_lsl3;
#else
    uint8_t *lfo_am_buffer;
#endif
    uint8_t pm_mode;

#if !EMU8950_NO_WAVE_TABLE_MAP
    uint16_t *wave_table;     /* wave table */
#else
#if EMU8950_SLOT_RENDER
#if !PICO_ON_DEVICE
    uint16_t *logsin_table;
    int8_t *efix_pm_table;
#endif
#endif
#if !PICO_ON_DEVICE
    uint16_t *wav_or_table;
#endif
#endif
};


#ifdef __cplusplus
}
#endif
#endif