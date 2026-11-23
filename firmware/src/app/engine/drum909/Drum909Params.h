/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DRUM909_PARAMS_H_
#define DRUM909_PARAMS_H_

#include <stdint.h>

// Keep in sync with DR_NUM / DR_MAX_PARAMS in drum909.h.
#define DRUM909_DRUM_COUNT 11
#define DRUM909_MAX_PARAMS 11

struct Drum909Params {
    uint8_t pots[DRUM909_DRUM_COUNT + 1][DRUM909_MAX_PARAMS];
};

#endif
