/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SynthEngineFactory.h"
#include "PersistenceMagic.h"
#include "Timbre.h"

Preenfm3Engine SynthEngineFactory::pfm3Engines_[NUMBER_OF_TIMBRES];
Dx7SynthEngine SynthEngineFactory::dx7Engines_[NUMBER_OF_TIMBRES];
ByosEngine SynthEngineFactory::byosEngines_[NUMBER_OF_TIMBRES];

namespace {
struct StorageMagicEntry {
    uint32_t magic;
    SynthEngineType type;
};

const StorageMagicEntry storageMagics[] = {
    {PRESET_VERSION1, SYNTH_ENGINE_PFM3},
    {PRESET_VERSION2, SYNTH_ENGINE_PFM3},
    {PRESET_DX7, SYNTH_ENGINE_DX7},
    {PRESET_BYOS, SYNTH_ENGINE_BYOS},
};
} // namespace

SynthEngineType SynthEngineFactory::engineTypeForStorageMagic(uint32_t magic) {
    for (const StorageMagicEntry& entry : storageMagics) {
        if (entry.magic == magic) {
            return entry.type;
        }
    }
    return SYNTH_ENGINE_COUNT;
}

SynthEngine* SynthEngineFactory::getForStorageMagic(uint32_t magic, int timbreNumber) {
    SynthEngineType type = engineTypeForStorageMagic(magic);
    if (type == SYNTH_ENGINE_COUNT) {
        return nullptr;
    }
    return get(type, timbreNumber);
}

void SynthEngineFactory::initDsp() {
    Dx7SynthEngine::initGlobal();
}

const char* SynthEngineFactory::getEngineDisplayName(SynthEngineType type) {
    switch (type) {
    case SYNTH_ENGINE_PFM3:
        return "PFM3";
    case SYNTH_ENGINE_DX7:
        return "DX7";
    case SYNTH_ENGINE_BYOS:
        return "BYOS";
    default:
        return "";
    }
}

void SynthEngineFactory::init(Timbre* timbre, SynthState* synthState) {
    int timbreNumber = timbre->getTimbreNumber();
    pfm3Engines_[timbreNumber].init(timbre, synthState);
    dx7Engines_[timbreNumber].init(timbre, synthState);
    byosEngines_[timbreNumber].init(timbre, synthState);
}

SynthEngine* SynthEngineFactory::get(SynthEngineType type, int timbreNumber) {
    if (type == SYNTH_ENGINE_DX7) {
        return static_cast<SynthEngine*>(&dx7Engines_[timbreNumber]);
    }
    if (type == SYNTH_ENGINE_BYOS) {
        return static_cast<SynthEngine*>(&byosEngines_[timbreNumber]);
    }
    return static_cast<SynthEngine*>(&pfm3Engines_[timbreNumber]);
}

Preenfm3Engine* SynthEngineFactory::getPfm3(int timbreNumber) {
    return &pfm3Engines_[timbreNumber];
}
