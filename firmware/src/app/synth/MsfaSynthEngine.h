/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MSFA_SYNTH_ENGINE_H_
#define MSFA_SYNTH_ENGINE_H_

#include "SynthEngine.h"

class Timbre;
class Voice;

/*
 * MSFA engine adapter.
 * Wraps the existing MsfaEngine.* free-function API; the low-level MSFA code
 * stays there.
 */
class MsfaSynthEngine : public SynthEngine {
public:
    MsfaSynthEngine() : timbre_(nullptr) {}

    void init(Timbre *timbre) override;
    void afterNewParamsLoad() override;
    void bindVoice(Voice *voice) override;
    void noteOn(char note, char velocity) override;
    void noteOff(char note) override;
    uint8_t voicesNextBlock() override;
    void prepareBlock() override {}
    void glide() override {}
    void gateFx() override {}

private:
    Timbre *timbre_;
};

#endif
