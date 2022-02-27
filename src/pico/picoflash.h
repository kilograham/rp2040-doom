/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include "pico.h"

#define FLASH_SECTOR_SIZE (1u << 12)

// erase and write a 4K sector
void picoflash_sector_program(uint32_t flash_offs, const uint8_t *data);
