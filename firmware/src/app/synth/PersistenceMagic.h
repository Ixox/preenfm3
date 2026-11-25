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

#ifdef __cplusplus
#include "Common.h"
#endif

// Magic numbers stored at the end of each 1024-byte preset/mixer slot.
// New engine magics must be printable ASCII 4CCs: the hex digits spell the
// code (0x42594F53 = "BYOS"), even though the value is stored little-endian.
enum PRESET_VERSION {
    PRESET_VERSION1 = 0,
    PRESET_VERSION2 = 292928062, // random 32 bits number for the first preenfm3
    PRESET_DX7 = 1297303090,     // random 32 bits number for DX7 presets
    PRESET_BYOS = 0x42594F53,    // "BYOS"
    PRESET_DRUM909 = 0x44393039  // "D909"
};

#define PRESET_CURRENT_VERSION PRESET_VERSION1

#ifdef __cplusplus
struct PresetMagicInfo {
    uint32_t magic;
    SynthEngineType engine;
    const char* engineName;
};

inline int presetMagicCount() {
    return 5;
}

inline const PresetMagicInfo& presetMagicAt(int index) {
    static const PresetMagicInfo infos[] = {
        {PRESET_VERSION1, SYNTH_ENGINE_PFM3, "preenfm3"},
        {PRESET_VERSION2, SYNTH_ENGINE_PFM3, "preenfm3"},
        {PRESET_DX7, SYNTH_ENGINE_DX7, "DX7"},
        {PRESET_BYOS, SYNTH_ENGINE_BYOS, "BYOS"},
        {PRESET_DRUM909, SYNTH_ENGINE_DRUM909, "909"},
    };
    if (index < 0) {
        index = 0;
    } else if (index >= presetMagicCount()) {
        index = presetMagicCount() - 1;
    }
    return infos[index];
}

inline const PresetMagicInfo* presetMagicInfo(uint32_t magic) {
    for (int i = 0; i < presetMagicCount(); ++i) {
        if (presetMagicAt(i).magic == magic) {
            return &presetMagicAt(i);
        }
    }
    return nullptr;
}

inline const char* presetEngineName(uint32_t magic) {
    const PresetMagicInfo* info = presetMagicInfo(magic);
    return info != nullptr ? info->engineName : nullptr;
}
#endif

#endif /* PERSISTENCE_MAGIC_H_ */
