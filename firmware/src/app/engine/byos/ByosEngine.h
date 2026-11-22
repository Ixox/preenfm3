/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef BYOS_ENGINE_H_
#define BYOS_ENGINE_H_

#include "ByosParams.h"
#include "Common.h"
#include "engine/pfm3/Osc.h"
#include "engine/pfm3/Pfm3Params.h"
#include "SynthEngine.h"

enum ByosEnvStage { BYOS_ENV_IDLE = 0, BYOS_ENV_ATTACK, BYOS_ENV_SUSTAIN, BYOS_ENV_RELEASE };

struct ByosVoiceState {
    OscState osc[2];
    float envLevel;
    float envInc;
    float velocityGain;
    uint8_t envStage;
};

/*
 * Minimal two-operator engine used as a BYOS (bring your own synth) example.
 * Two PFM3 oscillators, additive mix, one linear AR envelope per voice.
 */
class ByosEngine : public SynthEngine {
public:
    ByosEngine();

    void init(Timbre* timbre, SynthState* synthState) override;
    void afterNewParamsLoad() override;
    void bindVoice(Voice* voice) override;
    void noteOn(char note, char velocity) override;
    void noteOff(char note) override;
    void killVoice(Voice* voice) override;
    void noteOffVoice(Voice* voice) override;
    void noteOffVoiceQuick(Voice* voice) override;
    uint8_t voicesNextBlock() override;
    void prepareBlock() override {
    }
    void glide() override {
    }
    void gateFx() override {
    }
    void setMatrixSource(SourceEnum source, float value) override {
    }
    void setMatrixSourceMPE(uint8_t channel, SourceEnum source, float value) override {
    }
    void resetMatrixDestination(float oldValue) override {
    }
    void setMatrixPolyAfterTouch(uint8_t note, float value) override {
    }
    void noteOnMpe(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) override;

    void engineParamChanged(int paramId, int context, float oldValue, float newValue) override;
    uint8_t* getPatchMemory() override;
    void patchMemoryChanged() override;

    bool midiData(int timbre, const struct MidiData& data) override;
    bool midiParamFeedback(int paramId, int context, float newValue, struct MidiParamMessage& out) const override;
    bool supportsStepSequencer() const override {
        return false;
    }

    uint32_t getStorageMagic() const override;
    int getStorageNameOffset(uint32_t magic) const override;
    const char* getStorageDefaultName() const override;
    uint32_t getStorageBlob(uint8_t* out, uint32_t maxSize, const TimbreSharedParams& shared) override;
    bool loadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic) override;

private:
    void applyParams();
    float getParamValue(int paramId, int context) const;
    void startVoice(Voice* voice, int note, int velocity, uint32_t index, float noteFrequency);
    void releaseVoice(Voice* voice);
    void resetVoiceState(Voice* voice);
    void renderVoice(Voice* voice);

    Timbre* timbre_;
    SynthState* synthState_;
    int timbreNumber_;
    OscillatorParams oscParams_[2];
    Osc osc_[2];
    ByosParams params_;
    static ByosVoiceState voiceStates_[MAX_NUMBER_OF_VOICES];
};

#endif
