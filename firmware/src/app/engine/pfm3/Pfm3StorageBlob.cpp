/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Pfm3StorageBlob.h"

#include <string.h>

#include "PersistenceMagic.h"
#include "Pfm3FlashFormat.h"
#include "Pfm3Preset.h"
#include "Pfm3PresetIO.h"

uint32_t pfm3BuildStorageBlob(const Pfm3TimbreParams& params, const TimbreSharedParams& shared, uint8_t* out, uint32_t maxSize) {
    if (out == nullptr || maxSize < sizeof(FlashSynthParams)) {
        return 0;
    }
    Pfm3Preset preset{};
    pfm3SerializePreset(params, shared, preset);
    pfm3ParamsToFlash(&preset, (FlashSynthParams*)out, true);
    return (uint32_t)sizeof(FlashSynthParams);
}

bool pfm3LoadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic, Pfm3Preset& out) {
    (void)size;
    if (magic == PRESET_VERSION2) {
        memset(&out, 0, sizeof(Pfm3Preset));
        memcpy(&out, in, PFM3_LEGACY_PRESET_SIZE);
        arpAdvancedDefaults(out.arpAdvanced);
    } else {
        pfm3FlashToParams((const FlashSynthParams*)in, &out, true);
    }
    return true;
}
