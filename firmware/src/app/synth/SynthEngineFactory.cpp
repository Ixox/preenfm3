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
MsfaSynthEngine SynthEngineFactory::msfaEngines_[NUMBER_OF_TIMBRES];

namespace {
struct StorageMagicEntry {
    uint32_t magic;
    SynthEngineType type;
};

const StorageMagicEntry storageMagics[] = {
    { PRESET_VERSION1, SYNTH_ENGINE_PFM3 },
    { PRESET_VERSION2, SYNTH_ENGINE_PFM3 },
    { PRESET_MSFA, SYNTH_ENGINE_MSFA },
};
}

SynthEngineType SynthEngineFactory::engineTypeForStorageMagic(uint32_t magic) {
    for (const StorageMagicEntry &entry : storageMagics) {
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
    MsfaSynthEngine::initGlobal();
}

const char* SynthEngineFactory::getEngineDisplayName(SynthEngineType type) {
    switch (type) {
    case SYNTH_ENGINE_PFM3:
        return "PFM3";
    case SYNTH_ENGINE_MSFA:
        return "MSFA";
    default:
        return "";
    }
}

void SynthEngineFactory::init(Timbre *timbre, SynthState *synthState) {
    int timbreNumber = timbre->getTimbreNumber();
    pfm3Engines_[timbreNumber].init(timbre, synthState);
    msfaEngines_[timbreNumber].init(timbre, synthState);
}

SynthEngine* SynthEngineFactory::get(SynthEngineType type, int timbreNumber) {
    if (type == SYNTH_ENGINE_MSFA) {
        return static_cast<SynthEngine*>(&msfaEngines_[timbreNumber]);
    }
    return static_cast<SynthEngine*>(&pfm3Engines_[timbreNumber]);
}
