//
// Copyright (c) 20222 Graham Sanderson
//
// SPDX-License-Identifier: BSD-3-Clause
//

#include "pico/asm_helper.S"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/sio.h"
.syntax unified
.cpu cortex-m0plus
.thumb

#define INTERP_OFFSET0(x) ((x) - SIO_INTERP0_ACCUM0_OFFSET)
#define INTERP_OFFSET1(x) (INTERP_OFFSET0(x) + SIO_INTERP1_ACCUM0_OFFSET - SIO_INTERP0_ACCUM0_OFFSET)

.section .time_critical.blit

.global palette8to16
.type palette8to16,%function
.thumb_func
// palette8to16(dest, src, pixels) // note src, dest, and pixels must be 4 aligned
palette8to16:
    push {r4, r5, lr}
    add r2, r1
    mov ip, r2
    ldr r5, =#SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET
1:
    ldmia r1!, {r2}
    str r2, [r5, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    str r2, [r5, #INTERP_OFFSET0(SIO_INTERP1_ACCUM0_OFFSET)]
    ldr r2, [r5, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldrh r2, [r2, r2]
    ldr r3, [r5, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]
    ldrh r3, [r3, r3]
    lsls r3, #16
    orrs r3, r2
    ldr r2, [r5, #INTERP_OFFSET0(SIO_INTERP1_PEEK_LANE0_OFFSET)]
    ldrh r2, [r2, r2]
    ldr r4, [r5, #INTERP_OFFSET0(SIO_INTERP1_PEEK_LANE1_OFFSET)]
    ldrh r4, [r4, r4]
    lsls r4, #16
    orrs r4, r2
    stmia r0!, {r3, r4}
    cmp r1, ip
    bne 1b
    pop {r4, r5, pc}


.global palette4to16
.type palette4to16,%function
.thumb_func
// palette4to16(dest, src, pixels) // note src, dest, and pixels must be 4 aligned
palette4to16:
    push {r4-r6, lr}
    add r2, r1
    mov ip, r2
    ldr r5, =#SIO_BASE + SIO_INTERP0_ACCUM0_OFFSET
1:
    ldmia r1!, {r6}
    str r6, [r5, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    str r6, [r5, #INTERP_OFFSET0(SIO_INTERP1_ACCUM0_OFFSET)]
    ldr r2, [r5, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldrh r2, [r2, r2]
    ldr r3, [r5, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]
    ldrh r3, [r3, r3]
    lsls r3, #16
    orrs r3, r2
    ldr r2, [r5, #INTERP_OFFSET0(SIO_INTERP1_PEEK_LANE0_OFFSET)]
    ldrh r2, [r2, r2]
    ldr r4, [r5, #INTERP_OFFSET0(SIO_INTERP1_PEEK_LANE1_OFFSET)]
    ldrh r4, [r4, r4]
    lsls r4, #16
    orrs r4, r2
    stmia r0!, {r3, r4}
    lsrs r6, #16
    str r6, [r5, #INTERP_OFFSET0(SIO_INTERP0_ACCUM0_OFFSET)]
    str r6, [r5, #INTERP_OFFSET0(SIO_INTERP1_ACCUM0_OFFSET)]
    ldr r2, [r5, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE0_OFFSET)]
    ldrh r2, [r2, r2]
    ldr r3, [r5, #INTERP_OFFSET0(SIO_INTERP0_PEEK_LANE1_OFFSET)]
    ldrh r3, [r3, r3]
    lsls r3, #16
    orrs r3, r2
    ldr r2, [r5, #INTERP_OFFSET0(SIO_INTERP1_PEEK_LANE0_OFFSET)]
    ldrh r2, [r2, r2]
    ldr r4, [r5, #INTERP_OFFSET0(SIO_INTERP1_PEEK_LANE1_OFFSET)]
    ldrh r4, [r4, r4]
    lsls r4, #16
    orrs r4, r2
    stmia r0!, {r3, r4}

    cmp r1, ip
    bne 1b
    pop {r4-r6, pc}

