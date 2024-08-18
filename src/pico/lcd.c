/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/regs/dreq.h"
#include "pinoutRp2350defcon.h"
#include "dispDefcon.h"
#include "hardware/structs/iobank0.h"
#include "hardware/structs/padsbank0.h"
#include "doom/doomstat.h"

#define PIO_Nth2(x)                            pio ## x ## _hw
#define PIO_Nth(x)                            PIO_Nth2(x)
#define DREQ_PIO_TYPE_IDX2(p, t, i)            DREQ_PIO ## p ## _ ## t ## i
#define DREQ_PIO_TYPE_IDX(p, t, i)            DREQ_PIO_TYPE_IDX2(p, t, i)
#define MY_PIO                                PIO_Nth(DISP_PIO_IDX)


#define DISP_DMA_START_CH                    (DISP_DMA_FIRST + 0)
#define DISP_DMA_XFER_CH                    (DISP_DMA_FIRST + 1)


#define SIDE_SET_HAS_ENABLE_BIT                0

#define SIDE_SET_NUM_BITS                    1
#define DEFINE_PIO_INSTRS

#include "pioAsm.h"

static uint16_t mFb[DISP_WIDTH * DISP_HEIGHT];
static uint32_t mFbStartAddr = (uintptr_t) mFb;
static uint64_t mPerFrameSpace, mNextFrame;
static uint8_t mSm0start;
static bool mDispOn;

void gpiosConfig(bool firstTime)
{
    static struct {
        uint8_t pin;
        uint8_t padCfg;
        uint8_t funcSel			: 5;
        uint8_t oe				: 1;
        uint8_t val				: 1;
        uint8_t skipReconfig	: 1;	//do not reconfigure if not first call to gpiosConfig()
    } const cfgs[] = {
//            {PIN_RAM_NCS, PADS_BANK0_GPIO0_SLEWFAST_BITS, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_XIP_SS_N_1, 1, 1},

            {PIN_LCD_DnC, 0, GPIO_FUNC_SIO, 1, 0, 1},
            {PIN_LCD_SCK, PADS_BANK0_GPIO0_SLEWFAST_BITS, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_PIO0_0, 1, 0, 1},
            {PIN_LCD_DO, PADS_BANK0_GPIO0_SLEWFAST_BITS, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_PIO0_0, 1, 0, 1},
            {PIN_LCD_BL, 0, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_PWM_A_0, 1, 0, 1},
            {PIN_LCD_CS, 0, GPIO_FUNC_SIO, 1, 1, 1},

//            {PIN_TOUCHINT, PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_IE_BITS, GPIO_FUNC_SIO, 0, 0},

//            {PIN_WS2812, 0, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_PIO1_0, 1, 0},

//            {PIN_SD_MISO, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_SPI0_RX, 0, 0},
//            {PIN_SD_NCS, PADS_BANK0_GPIO0_SLEWFAST_BITS, GPIO_FUNC_SIO, 1, 1},
//            {PIN_SD_SCK, PADS_BANK0_GPIO0_SLEWFAST_BITS, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_SPI0_RX, 1, 0},
//            {PIN_SD_MOSI, PADS_BANK0_GPIO0_SLEWFAST_BITS, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_SPI0_RX, 1, 0},

            {PIN_BTN_L, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},
            {PIN_BTN_R, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},
            {PIN_BTN_U, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},
            {PIN_BTN_D, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},
            {PIN_BTN_A, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},
            {PIN_BTN_B, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},
            {PIN_BTN_START, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},
            {PIN_BTN_SEL, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},
            {PIN_BTN_CENTER, PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_PUE_BITS, GPIO_FUNC_SIO, 0, 0},

//            {PIN_IRDA_IN, PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO1_SCHMITT_BITS, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_PIO1_0, 0, 0},
//            {PIN_IRDA_OUT, 0, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_PIO1_0, 1, 0},
//            {PIN_IRDA_SD, 0, GPIO_FUNC_SIO, 1, 1},

            {PIN_SELF_PWR, 0, GPIO_FUNC_SIO, 1, 1},

//            {PIN_SPQR, 0, IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_PWM_A_0, 1, 0},
    };

    uint32_t gpiosConfigged = 0;
    uint_fast8_t i;

    for (i = 0; i < sizeof(cfgs) / sizeof(*cfgs); i++) {

        uint_fast8_t pinNo = cfgs[i].pin;

        gpiosConfigged |= 1 << pinNo;

        if (firstTime || !cfgs[i].skipReconfig) {

            padsbank0_hw->io[pinNo] = (padsbank0_hw->io[pinNo] &~ (PADS_BANK0_GPIO0_ISO_BITS | PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_DRIVE_BITS | PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_PDE_BITS | PADS_BANK0_GPIO0_SCHMITT_BITS | PADS_BANK0_GPIO0_SLEWFAST_BITS)) | cfgs[i].padCfg;
            iobank0_hw->io[pinNo].ctrl = (iobank0_hw->io[pinNo].ctrl &~ IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS) | (((uint32_t)cfgs[i].funcSel) << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB);

            if (cfgs[i].oe) {

                if (cfgs[i].val)
                    sio_hw->gpio_set = 1 << pinNo;
                else
                    sio_hw->gpio_clr = 1 << pinNo;

                sio_hw->gpio_oe_set = 1 << pinNo;
            }
            else
                sio_hw->gpio_oe_clr = 1 << pinNo;
        }
    }

//now the leftovers
    if (0 && firstTime) {
        for (i = 0; i < 30; i++) {

            if (!(gpiosConfigged & (1 << i))) {

                padsbank0_hw->io[i] |= PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_ISO_BITS;
                iobank0_hw->io[i].ctrl |= IO_BANK0_GPIO0_CTRL_FUNCSEL_VALUE_NULL;
            }
        }
    }
}

static void __not_in_flash_func(lcdByte)(uint_fast8_t val) {
    *(volatile uint8_t *) &MY_PIO->txf[DISP_PIO_SM] = val;
    MY_PIO->fdebug = (1 << PIO_FDEBUG_TXSTALL_LSB) << DISP_PIO_SM;
    while (!(MY_PIO->fdebug & ((1 << PIO_FDEBUG_TXSTALL_LSB) << DISP_PIO_SM)));
}

static void __not_in_flash_func(lcdCmd)(uint_fast8_t cmd, bool done, ...) {
    va_list vl;
    int v;

    va_start(vl, done);

    sio_hw->gpio_clr = 1 << PIN_LCD_CS;
    asm volatile("dsb sy\ndsb sy\ndsb sy\n");
    sio_hw->gpio_clr = 1 << PIN_LCD_DnC;
    asm volatile("dsb sy\ndsb sy\ndsb sy\n");
    lcdByte(cmd);
    asm volatile("dsb sy\ndsb sy\ndsb sy\n");
    sio_hw->gpio_set = 1 << PIN_LCD_DnC;
    asm volatile("dsb sy\ndsb sy\ndsb sy\n");
    while ((v = va_arg(vl, int)) >= 0)
        lcdByte(v);
    if (done) {
        asm volatile("dsb sy\ndsb sy\ndsb sy\n");
        sio_hw->gpio_set = 1 << PIN_LCD_CS;
    }

    va_end(vl);
}

static void __not_in_flash_func(lcdSetRegion)(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t xo, uint32_t yo) {
    lcdCmd(0x2a, true, (x + xo) >> 8, (x + xo) & 0xff, (x + xo + w - 1) >> 8, (x + xo + w - 1) & 0xff, -1);
    lcdCmd(0x2b, true, (y + yo) >> 8, (y + yo) & 0xff, (y + yo + h - 1) >> 8, (y + yo + h - 1) & 0xff, -1);
}

#define TICKS_PER_SECOND 1000000
#define getTime() time_us_64()

static void lcdDelayMs(uint32_t msec) {
    busy_wait_ms(msec);
//    uint64_t till = getTime() + (uint64_t) msec * (TICKS_PER_SECOND / 1000);
//
//    while (getTime() < till);
}

static void __not_in_flash_func(dispPrvSetPioWidth)(uint_fast8_t bitsPerXfer) {
    MY_PIO->ctrl &= ~((1 << PIO_CTRL_SM_ENABLE_LSB) << DISP_PIO_SM);
    MY_PIO->ctrl |= (1 << PIO_CTRL_SM_RESTART_LSB) << DISP_PIO_SM;
    MY_PIO->sm[DISP_PIO_SM].shiftctrl = (MY_PIO->sm[DISP_PIO_SM].shiftctrl & ~PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS ) | (
            bitsPerXfer << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);
    MY_PIO->sm[DISP_PIO_SM].instr = I_JMP(0, 0, JMP_ALWAYS, mSm0start);
    MY_PIO->ctrl |= (1 << PIO_CTRL_SM_ENABLE_LSB) << DISP_PIO_SM;
}

void dispWaitLine(void) {
}

void __not_in_flash_func(dispRenderLine)(uint y, uint16_t *buf, uint32_t width) {
    while (dma_channel_is_busy(DISP_DMA_XFER_CH));
    busy_wait_at_least_cycles(3200);
    dispPrvSetPioWidth(8);
    lcdSetRegion(239-y, 0, 1, width, 0, 0);
    lcdCmd(0x2c, false, -1); //write data command
    dispPrvSetPioWidth(16);
    dma_channel_transfer_from_buffer_now(DISP_DMA_XFER_CH, buf, 320);
}
static bool dispPrvTurnOn(bool firstTime) {

    printf("turning display on\n");

    dispPrvSetPioWidth(8);
    lcdCmd(0x2c, false, -1);    //write data command

    dispPrvSetPioWidth(16);
    //dma_hw->ch[DISP_DMA_START_CH].ctrl_trig |= 0;
    //printf("dma is on\n");

    pwm_hw->slice[BACKLITE_PWM_INDEX].csr &= ~PWM_CH0_CSR_EN_BITS;
    while (pwm_hw->slice[BACKLITE_PWM_INDEX].csr & PWM_CH0_CSR_EN_BITS);
    pwm_hw->slice[BACKLITE_PWM_INDEX].top = (pwm_hw->slice[BACKLITE_PWM_INDEX].top & ~PWM_CH0_TOP_BITS) | (1022
            << PWM_CH1_TOP_LSB);        //this is correct. values valid for "cc" become 0..1024 to span the range of 0..100% duty
    pwm_hw->slice[BACKLITE_PWM_INDEX].ctr = 0;
    *(volatile uint16_t *) &pwm_hw->slice[BACKLITE_PWM_INDEX].cc = 0;
    pwm_hw->slice[BACKLITE_PWM_INDEX].csr = (pwm_hw->slice[BACKLITE_PWM_INDEX].csr &
                                             ~(PWM_CH0_CSR_PH_ADV_BITS | PWM_CH0_CSR_PH_RET_BITS |
                                               PWM_CH0_CSR_DIVMODE_BITS | PWM_CH0_CSR_B_INV_BITS |
                                               PWM_CH0_CSR_A_INV_BITS)) |
                                            (PWM_CH0_CSR_DIVMODE_VALUE_DIV << PWM_CH0_CSR_DIVMODE_LSB);
    pwm_hw->slice[BACKLITE_PWM_INDEX].div =
            (pwm_hw->slice[BACKLITE_PWM_INDEX].div & ~(PWM_CH0_DIV_INT_BITS | PWM_CH0_DIV_FRAC_BITS)) |
            (1 << PWM_CH0_DIV_INT_LSB);
    pwm_hw->slice[BACKLITE_PWM_INDEX].csr |= PWM_CH0_CSR_EN_BITS;
    dispSetBrightness((uint8_t)brightnessLevel);

    mDispOn = true;

    return true;
}

static bool dispPrvTurnOff(void) {
    uint_fast8_t i;

    printf("turning display off\n");

    pwm_hw->slice[BACKLITE_PWM_INDEX].csr = 0;    //backlight duty = 0

    for (i = 0; i < 2; i++)
        dma_hw->ch[i].al1_ctrl = 0;

    dma_hw->abort = 3 << 0;

    for (i = 0; i < 2; i++)
        while (dma_hw->ch[i].al1_ctrl & DMA_CH0_CTRL_TRIG_BUSY_BITS)

            sio_hw->gpio_set = 1 << PIN_LCD_CS;

    //verify backlight went off before disabling pwm (pwm can be slow)
    while (sio_hw->gpio_in & (1 << PIN_LCD_BL));
    pwm_hw->slice[BACKLITE_PWM_INDEX].csr &= ~PWM_CH0_CSR_EN_BITS;

    mDispOn = false;

    return true;
}

extern void fill_scanlines(void);
void __not_in_flash_func(line_done)(void) {
    dma_irqn_acknowledge_channel(2, DISP_DMA_XFER_CH);
    fill_scanlines();
}



bool dispInit(uint32_t desiredFramerate) {
    uint_fast8_t pc = DISP_PIO_FIRST_USED_PC, sm0Start, sm0LoopTo, sm0LoopFrom;
    uint32_t i, j, maxClk = 70000000, divClk;    //in hz

    mPerFrameSpace = clock_get_hz(clk_sys) / desiredFramerate;

    sm0Start = sm0LoopTo = pc;
    MY_PIO->instr_mem[pc++] = I_OUT(0, 0, OUT_DST_PINS, 1);
    MY_PIO->instr_mem[pc++] = I_MOV(0, 1, MOV_DST_X, MOV_OP_COPY, MOV_SRC_X);
    sm0LoopFrom = pc - 1;

    printf("disp: using %u instrs, next free is %u\n", pc - DISP_PIO_FIRST_USED_PC, pc);

    divClk = (clock_get_hz(clk_sys) / 2 + maxClk - 1) / maxClk;    //non-integer divisions do not do well with PIO
//    __breakpoint();
    divClk = 2;
    //printf("DISP: using divisor %u for a clock rate of %luHz\n", divClk, clock_get_hz(clk_sys)  / 2 / divClk);
    //configure sm0
    MY_PIO->sm[DISP_PIO_SM].clkdiv = (divClk << PIO_SM0_CLKDIV_INT_LSB);
    MY_PIO->sm[DISP_PIO_SM].execctrl = (MY_PIO->sm[DISP_PIO_SM].execctrl &
                                        ~(PIO_SM0_EXECCTRL_WRAP_TOP_BITS | PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS |
                                          PIO_SM0_EXECCTRL_SIDE_EN_BITS)) |
                                       (sm0LoopFrom << PIO_SM0_EXECCTRL_WRAP_TOP_LSB) | (sm0LoopTo << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) |
                                       (SIDE_SET_HAS_ENABLE_BIT ? PIO_SM2_EXECCTRL_SIDE_EN_BITS : 0);
    MY_PIO->sm[DISP_PIO_SM].shiftctrl = (MY_PIO->sm[DISP_PIO_SM].shiftctrl & ~(PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS |
                                                                               PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS |
                                                                               PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_BITS |
                                                                               PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_BITS |
                                                                               PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS)) |
                                        PIO_SM0_SHIFTCTRL_AUTOPULL_BITS | (8 << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);

    //pins are outputs (temporarily set pinctrl to enable this for sideset pin)
    MY_PIO->sm[DISP_PIO_SM].pinctrl =
            (PIN_LCD_SCK << PIO_SM0_PINCTRL_SET_BASE_LSB) | (1 << PIO_SM0_PINCTRL_SET_COUNT_LSB);
    MY_PIO->sm[DISP_PIO_SM].instr = I_SET(0, 0, SET_DST_PINDIRS, 1);
    MY_PIO->sm[DISP_PIO_SM].pinctrl =
            (PIN_LCD_DO << PIO_SM0_PINCTRL_SET_BASE_LSB) | (1 << PIO_SM0_PINCTRL_SET_COUNT_LSB);
    MY_PIO->sm[DISP_PIO_SM].instr = I_SET(0, 0, SET_DST_PINDIRS, 1);

    //properly configure pins
    MY_PIO->sm[DISP_PIO_SM].pinctrl =
            (SIDE_SET_BITS_USED << PIO_SM1_PINCTRL_SIDESET_COUNT_LSB) | (1 << PIO_SM1_PINCTRL_OUT_COUNT_LSB) |
            (PIN_LCD_SCK << PIO_SM1_PINCTRL_SIDESET_BASE_LSB) | (PIN_LCD_DO << PIO_SM1_PINCTRL_OUT_BASE_LSB);

    //start it
    mSm0start = sm0Start;
    dispPrvSetPioWidth(8);

//    dma_hw->ch[DISP_DMA_START_CH].read_addr = (uintptr_t) &mFbStartAddr;
//    dma_hw->ch[DISP_DMA_START_CH].write_addr = (uintptr_t) &dma_hw->ch[DISP_DMA_XFER_CH].al3_read_addr_trig;
//    dma_hw->ch[DISP_DMA_START_CH].transfer_count = 1;
//    dma_hw->ch[DISP_DMA_START_CH].al1_ctrl =
//            (DMA_CH0_CTRL_TRIG_TREQ_SEL_VALUE_PERMANENT << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB) |
//            (DISP_DMA_START_CH << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB) |
//            (DMA_CH0_CTRL_TRIG_DATA_SIZE_VALUE_SIZE_WORD << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB) |
//            DMA_CH0_CTRL_TRIG_EN_BITS;

    dma_irqn_set_channel_enabled(2, DISP_DMA_XFER_CH, true);
    irq_set_exclusive_handler(DMA_IRQ_2, line_done);
    irq_set_enabled(DMA_IRQ_2, true);
    dma_hw->ch[DISP_DMA_XFER_CH].write_addr = (uintptr_t) &MY_PIO->txf[DISP_PIO_SM];
    dma_hw->ch[DISP_DMA_XFER_CH].transfer_count = DISP_WIDTH;
    dma_hw->ch[DISP_DMA_XFER_CH].al1_ctrl =
            (DREQ_PIO_TYPE_IDX(DISP_PIO_IDX, TX, DISP_PIO_SM) << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB) |
//            (DISP_DMA_START_CH << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB) |
            (DMA_CH0_CTRL_TRIG_DATA_SIZE_VALUE_SIZE_HALFWORD << DMA_CH0_CTRL_TRIG_DATA_SIZE_LSB) |
            DMA_CH0_CTRL_TRIG_INCR_READ_BITS | DMA_CH0_CTRL_TRIG_HIGH_PRIORITY_BITS | DMA_CH0_CTRL_TRIG_EN_BITS;
//    channel_config_set_dreq(DREQ_PIO_TYPE_IDX(DISP_PIO_IDX, TX, DISP_PIO_SM) << DMA_CH0_CTRL_TRIG_TREQ_SEL_LSB);


    lcdCmd(0x01, true, -1);
    lcdDelayMs(100);

    lcdCmd(0x11, true, -1);
    lcdDelayMs(50);

    lcdCmd(0x3a, true, 0x55, -1);
    lcdDelayMs(10);


    lcdCmd(0x36, true, 0x00, -1);
    lcdSetRegion(0, 0, HARDWARE_WIDTH, HARDWARE_HEIGHT, 0, 0);

    lcdCmd(0x20, true, -1);
    lcdDelayMs(10);
    lcdCmd(0x13, true, -1);
    lcdDelayMs(10);
    lcdCmd(0x29, true, -1);
    lcdDelayMs(10);


    //erase the screen
    lcdCmd(0x2c, false, -1);    //write data command

    //dear reader, i do not know why we need to send an extra frame minus a pixel here. no part of this display has matched spec when running full-res. luckily sub-full-res works well
    for (i = 0; i < HARDWARE_HEIGHT * HARDWARE_WIDTH * 2 - 2; i++)
        lcdByte(0);

    //data needs to be endian flipped AND v-flipped (which is really an h-flip since display is 90 degrees rotated)
    for (i = 0; i < HARDWARE_HEIGHT; i++) {
        for (j = 0; j < HARDWARE_WIDTH; j++) {

            uint32_t idx = i * HARDWARE_WIDTH + (HARDWARE_WIDTH - j - 1);

            lcdByte(0x0);//lcdFrame[idx * 2 + 1]);
            lcdByte(0x0);//lcdFrame[idx * 2 + 0]);
        }
    }
    sio_hw->gpio_set = 1 << PIN_LCD_CS;

    printf("inited display at %u fps\n", (uint)desiredFramerate);

//    lcdSetRegion((HARDWARE_WIDTH - DISP_WIDTH) / 2, (HARDWARE_HEIGHT - DISP_HEIGHT) / 2, DISP_WIDTH, DISP_HEIGHT, 0, 0);
//    lcdSetRegion(40, 0, 1, 320, 0, 0);//(HARDWARE_WIDTH - DISP_WIDTH) / 2, (HARDWARE_HEIGHT - DISP_HEIGHT) / 2, DISP_WIDTH, DISP_HEIGHT, 0, 0);

    return dispPrvTurnOn(true);
}

bool dispOn(void) {
    return mDispOn || dispPrvTurnOn(false);
}

bool dispOff(void) {
    return !mDispOn || dispPrvTurnOff();
}

void *dispGetFb(void) {
    return mFb;
}

void dispPrvFrameCtrReset(void) {
    mNextFrame = getTime() + mPerFrameSpace;
}

void dispPrvFrameCtrWait(void) {
    while (getTime() < mNextFrame);
    mNextFrame += mPerFrameSpace;
}

void dispSetContrast(uint_fast8_t val) {
    //nothing
}

void dispSetBrightness(uint_fast8_t bri) {
    uint32_t pwmVal = bri;

    //useful output range is 100..1023
    //inout is 0..15

    pwmVal *= pwmVal * pwmVal * 2;
    pwmVal /= 9;
    pwmVal += 114;

//    printf("bri %u -> %u\n", bri, pwmVal);
    *(volatile uint16_t *) &pwm_hw->slice[BACKLITE_PWM_INDEX].cc = pwmVal;        //so backlight stays on
}

