/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PERSISTENCE_MAGIC_H_
#define PERSISTENCE_MAGIC_H_

#include <stdint.h>

// Magic numbers stored at the end of each 1024-byte preset/mixer slot.
enum PRESET_VERSION {
    PRESET_VERSION1 = 0,
    PRESET_VERSION2 = 292928062, // random 32 bits number for the first preenfm3
    PRESET_MSFA = 1297303090,    // random 32 bits number for MSFA presets
    PRESET_BYOS = 0x42594F53     // "BYOS"
};

#define PRESET_CURRENT_VERSION PRESET_VERSION1

#endif
