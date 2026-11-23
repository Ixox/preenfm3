/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_STORAGE_BLOB_H_
#define PFM3_STORAGE_BLOB_H_

#include <stdint.h>

struct Pfm3Preset;
struct Pfm3TimbreParams;
struct TimbreSharedParams;

/*
 * PFM3 on-slot payload (without the 1024-byte slot trailer).
 *
 * Current format: magic PRESET_VERSION1 (0), FlashSynthParams (976 bytes).
 * Legacy format:  magic PRESET_VERSION2, raw 1008-byte Pfm3Preset prefix.
 */
uint32_t pfm3BuildStorageBlob(const Pfm3TimbreParams& params, const TimbreSharedParams& shared, uint8_t* out, uint32_t maxSize);
bool pfm3LoadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic, Pfm3Preset& out);

#endif /* PFM3_STORAGE_BLOB_H_ */
