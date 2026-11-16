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

#include "Common.h"
#include "SynthEngine.h"

class Timbre;
class Voice;
class SynthState;
struct MsfaTimbreState;

/*
 * MSFA engine adapter.
 * Owns the MSFA program/patch and timbre state; the low-level DSP stays in
 * MsfaEngine.*.
 */
class MsfaSynthEngine : public SynthEngine {
public:
    MsfaSynthEngine() : timbre_(nullptr), msfaState_(nullptr), timbreNumber_(0) {}

    void init(Timbre *timbre, SynthState *synthState) override;
    void afterNewParamsLoad() override;
    void bindVoice(Voice *voice) override;
    void noteOn(char note, char velocity) override;
    void noteOff(char note) override;
    uint8_t voicesNextBlock() override;
    void prepareBlock() override {}
    void glide() override {}
    void gateFx() override {}

    void setMatrixSource(SourceEnum source, float value) override;
    void setMatrixSourceMPE(uint8_t, SourceEnum, float) override {}
    void resetMatrixDestination(float) override {}
    void setMatrixPolyAfterTouch(uint8_t, float) override {}
    void noteOnMpe(uint8_t, uint8_t note, uint8_t velocity) override {
        noteOn(note, velocity);
    }
    void noteOffMpe(uint8_t, uint8_t note, uint8_t velocity) override {
        noteOff(note);
    }

    void loadMsfaPackedPatch(const uint8_t *packedPatch);
    void loadMsfaProgram(const uint8_t *program);
    const uint8_t* getMsfaProgram() const {
        return program_[timbreNumber_];
    }
    uint8_t* getMsfaProgramMutable() {
        return program_[timbreNumber_];
    }
    void setController(SourceEnum source, float value);

private:
    Timbre *timbre_;
    MsfaTimbreState *msfaState_;
    int timbreNumber_;
    static uint8_t packedPatch_[NUMBER_OF_TIMBRES][MSFA_PACKED_PATCH_SIZE] __attribute__((section(".ram_d1")));
    static uint8_t program_[NUMBER_OF_TIMBRES][MSFA_PROGRAM_SIZE] __attribute__((section(".ram_d1")));
};

#endif
