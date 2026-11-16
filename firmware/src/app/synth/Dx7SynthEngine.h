/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef DX7_SYNTH_ENGINE_H_
#define DX7_SYNTH_ENGINE_H_

#include "Common.h"
#include "SynthEngine.h"

#define DX7_PACKED_PATCH_SIZE 128
#define DX7_PROGRAM_SIZE 156

class Timbre;
class Voice;
class SynthState;
struct Dx7TimbreState;

/*
 * DX7 engine adapter.
 * Owns the DX7 program/patch and timbre state; the low-level DSP stays in
 * Dx7Engine.*.
 */
class Dx7SynthEngine : public SynthEngine {
public:
    Dx7SynthEngine() : timbre_(nullptr), synthState_(nullptr), dx7State_(nullptr), timbreNumber_(0) {}

    static void initGlobal();
    void init(Timbre *timbre, SynthState *synthState) override;
    void afterNewParamsLoad() override;
    void bindVoice(Voice *voice) override;
    static void keyUp(Voice *voice);
    void noteOn(char note, char velocity) override;
    void noteOff(char note) override;
    void killVoice(Voice *voice) override;
    void noteOffVoice(Voice *voice) override;
    void noteOffVoiceQuick(Voice *voice) override;
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

    void loadDx7Patch(const uint8_t *packedPatch) override;
    uint8_t* getPatchMemory() override {
        return program_[timbreNumber_];
    }
    void engineParamChanged(int paramId, int context, float oldValue, float newValue) override;
    bool midiControlChange(int timbre, uint8_t cc, uint8_t value) override;
    bool midiParamFeedback(int paramId, int context, float newValue, struct MidiParamMessage &out) const override;
    bool acceptsFlatParamFromMidi(int row) const override;
    bool supportsStepSequencer() const override {
        return false;
    }
    void patchMemoryChanged() override {
        afterNewParamsLoad();
    }
    void setController(SourceEnum source, float value);

    uint32_t getStorageMagic() const override;
    int getStorageNameOffset(uint32_t magic) const override;
    const char* getStorageDefaultName() const override;
    uint32_t getStorageBlob(uint8_t *out, uint32_t maxSize, const TimbreSharedParams &shared) override;
    bool loadStorageBlob(const uint8_t *in, uint32_t size, uint32_t magic) override;

private:
    void startVoice(Voice *voice, const uint8_t *program, uint8_t note, uint8_t velocity, uint32_t index);
    void updateVoiceProgram(Voice *voice, const uint8_t *program);
    void renderVoice(Voice *voice);

    Timbre *timbre_;
    SynthState *synthState_;
    Dx7TimbreState *dx7State_;
    int timbreNumber_;
    static uint8_t packedPatch_[NUMBER_OF_TIMBRES][DX7_PACKED_PATCH_SIZE] __attribute__((section(".ram_d1")));
    static uint8_t program_[NUMBER_OF_TIMBRES][DX7_PROGRAM_SIZE] __attribute__((section(".ram_d1")));
};

#endif
