/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once
#include "wad.h"
#include <vector>
#include "statsomizer.h"

extern statsomizer musx_decoder_space;

std::vector<uint8_t> compress_mus(std::pair<const int, lump> &e);

