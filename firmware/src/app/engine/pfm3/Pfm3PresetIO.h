/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_PRESET_IO_H_
#define PFM3_PRESET_IO_H_

#include "Pfm3Preset.h"
#include "TimbreParams.h"

/*
 * PFM3 preset mirror conversion.
 *
 * Pure field copies between the runtime owners (Pfm3TimbreParams +
 * TimbreSharedParams) and the flat Pfm3Preset. The engine keeps the mirror
 * pointer and shared-parameter propagation; these functions only move data.
 */
void pfm3SerializePreset(const Pfm3TimbreParams& params, const TimbreSharedParams& shared, Pfm3Preset& out);
void pfm3DeserializePreset(const Pfm3Preset& in, Pfm3TimbreParams& params, TimbreSharedParams& shared);

#endif /* PFM3_PRESET_IO_H_ */
