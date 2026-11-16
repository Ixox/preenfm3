/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SYNTH_ENGINE_H_
#define SYNTH_ENGINE_H_

#include <stdint.h>

class Timbre;
class Voice;

/*
 * Per-timbre sound engine interface.
 * Calls are per note or per block, never per sample.
 */
class SynthEngine {
public:
    virtual ~SynthEngine() {}

    virtual void init(Timbre *timbre) = 0;
    virtual void afterNewParamsLoad() = 0;
    virtual void bindVoice(Voice *voice) = 0;

    virtual void noteOn(char note, char velocity) = 0;
    virtual void noteOff(char note) = 0;

    virtual uint8_t voicesNextBlock() = 0;

    virtual void prepareBlock() = 0;
    virtual void glide() = 0;
    virtual void gateFx() = 0;
};

#endif
