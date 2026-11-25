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
Drum909Engine SynthEngineFactory::drum909Engines_[NUMBER_OF_TIMBRES];

SynthEngineType SynthEngineFactory::engineTypeForStorageMagic(uint32_t magic) {
    const PresetMagicInfo* info = presetMagicInfo(magic);
    return info != nullptr ? info->engine : SYNTH_ENGINE_COUNT;
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
    case SYNTH_ENGINE_DRUM909:
        return "909";
    default:
        return "";
    }
}

void SynthEngineFactory::init(Timbre* timbre, SynthState* synthState) {
    int timbreNumber = timbre->getTimbreNumber();
    pfm3Engines_[timbreNumber].init(timbre, synthState);
    dx7Engines_[timbreNumber].init(timbre, synthState);
    byosEngines_[timbreNumber].init(timbre, synthState);
    drum909Engines_[timbreNumber].init(timbre, synthState);
}

SynthEngine* SynthEngineFactory::get(SynthEngineType type, int timbreNumber) {
    if (type == SYNTH_ENGINE_DX7) {
        return static_cast<SynthEngine*>(&dx7Engines_[timbreNumber]);
    }
    if (type == SYNTH_ENGINE_BYOS) {
        return static_cast<SynthEngine*>(&byosEngines_[timbreNumber]);
    }
    if (type == SYNTH_ENGINE_DRUM909) {
        return static_cast<SynthEngine*>(&drum909Engines_[timbreNumber]);
    }
    return static_cast<SynthEngine*>(&pfm3Engines_[timbreNumber]);
}

Preenfm3Engine* SynthEngineFactory::getPfm3(int timbreNumber) {
    return &pfm3Engines_[timbreNumber];
}
