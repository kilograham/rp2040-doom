/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// this is modified from of the Pico SDK hardware/flash.c but modified to use a user buffer for boot2_copyout and combine to a single sector erase/write function

#include "picoflash.h"
#include "pico/bootrom.h"

#include "hardware/structs/ssi.h"
#include "hardware/structs/ioqspi.h"

#define FLASH_BLOCK_ERASE_CMD 0xd8

//-----------------------------------------------------------------------------
// Infrastructure for reentering XIP mode after exiting for programming (take
// a copy of boot2 before XIP exit). Calling boot2 as a function works because
// it accepts a return vector in LR (and doesn't trash r4-r7). Bootrom passes
// NULL in LR, instructing boot2 to enter flash vector table's reset handler.

#define BOOT2_SIZE_WORDS 64

static void __no_inline_not_in_flash_func(flash_init_boot2_copyout)(uint32_t boot2_copyout[BOOT2_SIZE_WORDS]) {
    for (int i = 0; i < BOOT2_SIZE_WORDS; ++i)
        boot2_copyout[i] = ((uint32_t *)XIP_BASE)[i];
    __compiler_memory_barrier();
}

static void __no_inline_not_in_flash_func(flash_enable_xip_via_boot2)(const uint32_t boot2_copyout[BOOT2_SIZE_WORDS]) {
    ((void (*)(void))boot2_copyout+1)();
}

#define FLASH_BLOCK_SIZE (1u << 16)

void __no_inline_not_in_flash_func(picoflash_sector_program)(uint32_t flash_offs, const uint8_t *data) {
    rom_connect_internal_flash_fn connect_internal_flash = (rom_connect_internal_flash_fn)rom_func_lookup_inline(ROM_FUNC_CONNECT_INTERNAL_FLASH);
    rom_flash_exit_xip_fn flash_exit_xip = (rom_flash_exit_xip_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_EXIT_XIP);
    rom_flash_range_program_fn flash_range_program = (rom_flash_range_program_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_RANGE_PROGRAM);
    rom_flash_flush_cache_fn flash_flush_cache = (rom_flash_flush_cache_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_FLUSH_CACHE);
    rom_flash_range_erase_fn flash_range_erase = (rom_flash_range_erase_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_RANGE_ERASE);
    assert(connect_internal_flash && flash_exit_xip && flash_range_program && flash_flush_cache);
    uint32_t boot2_copyout[BOOT2_SIZE_WORDS];
    flash_init_boot2_copyout(boot2_copyout);

    __compiler_memory_barrier();

    connect_internal_flash();
    flash_exit_xip();
    flash_range_erase(flash_offs, FLASH_SECTOR_SIZE, FLASH_BLOCK_SIZE, FLASH_BLOCK_ERASE_CMD);
    flash_range_program(flash_offs, data, FLASH_SECTOR_SIZE);
    flash_flush_cache(); // Note this is needed to remove CSn IO force as well as cache flushing
    flash_enable_xip_via_boot2(boot2_copyout);
}
