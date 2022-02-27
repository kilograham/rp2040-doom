#ifndef _PICODOOM_H
#define _PICODOOM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "m_fixed.h"

typedef enum {
    PDCOL_NONE = 0,
    PDCOL_TOP,
    PDCOL_MID,
    PDCOL_BOTTOM,
    PDCOL_SKY,
    PDCOL_MASKED,
    PDCOL_FLOOR,
    PDCOL_CEILING,
} pd_column_type;

extern volatile uint8_t interp_in_use;
void pd_init();
void pd_core1_loop();
void pd_begin_frame();
void pd_add_column(pd_column_type type);
void pd_add_masked_columns(uint8_t *ys, int seg_count);
void pd_add_plane_column(int x, int yl, int yh, fixed_t scale, int floor, int fd_num);
void pd_end_frame(int wipe_start);
uint8_t *pd_get_work_area(uint32_t *size);
#if PICO_ON_DEVICE
void pd_start_save_pause(void);
void pd_end_save_pause(void);
const uint8_t *get_end_of_flash(void);
#endif
extern int pd_flag;
extern fixed_t pd_scale;
#ifdef __cplusplus
}
#endif

#endif