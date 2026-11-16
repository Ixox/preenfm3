/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "SynthEngineFactory.h"
#include "Timbre.h"

Preenfm3Engine SynthEngineFactory::pfm3Engines_[NUMBER_OF_TIMBRES];
MsfaSynthEngine SynthEngineFactory::msfaEngines_[NUMBER_OF_TIMBRES];

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
