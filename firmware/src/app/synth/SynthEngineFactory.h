/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SYNTH_ENGINE_FACTORY_H_
#define SYNTH_ENGINE_FACTORY_H_

#include "Common.h"
#include "Preenfm3Engine.h"
#include "MsfaSynthEngine.h"

class Timbre;
class SynthState;
class SynthEngine;

/*
 * Static engine registry. One PFM3 and one MSFA engine per timbre, created
 * once at startup; Timbre only keeps a SynthEngine pointer.
 */
class SynthEngineFactory {
public:
    // One-time DSP table initialization, before any engine/voice init.
    static void initDsp();
    static void init(Timbre *timbre, SynthState *synthState);
    static SynthEngine* get(SynthEngineType type, int timbreNumber);
    static const char* getEngineDisplayName(SynthEngineType type);
    // Map a preset/mixer storage magic to its engine, or COUNT if unknown.
    static SynthEngineType engineTypeForStorageMagic(uint32_t magic);
    static SynthEngine* getForStorageMagic(uint32_t magic, int timbreNumber);

private:
    static Preenfm3Engine pfm3Engines_[NUMBER_OF_TIMBRES];
    static MsfaSynthEngine msfaEngines_[NUMBER_OF_TIMBRES];
};

#endif
