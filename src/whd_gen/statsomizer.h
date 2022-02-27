/*
 * Copyright (c) 20222 Graham Sanderson
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#pragma once
#include <string>
#include <algorithm>

struct statsomizer {
    const std::string name;

    explicit statsomizer(std::string name) : name(std::move(name)) { reset(); }

    void record(int value) {
        total += value;
        min = std::min(min, value);
        max = std::max(max, value);
        count++;
    }

    void record_print(int value) {
        record(value);
        printf("%20s: value=%d min=%d max=%d avg=%d\n", name.c_str(), value, min, max,
               count ? ((int) (total / count)) : 0);
    }

    void print_summary() const {
        printf("%20s: min=%d max=%d avg=%d count=%d total=%ld\n", name.c_str(), min, max,
               count ? ((int) (total / count)) : 0, count, total);
    }

    void reset() {
        total = 0;
        count = 0;
        min = std::numeric_limits<int>::max();
        max = std::numeric_limits<int>::min();
    }

    long total;
    int count, min, max;
};
