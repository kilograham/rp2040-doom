/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once

#include <cstdint>
#include <vector>

template<int N=256> std::vector<int> huff(uint8_t *data, int size, bool dump = false);