/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PREENFM3_ENGINE_H_
#define PREENFM3_ENGINE_H_

#include "Common.h"
#include "TimbreParams.h"
#include "Lfo.h"
#include "Osc.h"
#include "Env.h"
#include "SynthEngine.h"

class Timbre;
class Voice;

/*
 * Per-timbre values that only depend on the preset, not on the voice.
 * Owned and refreshed by the engine, copied into a voice when it is bound.
 */
struct Pfm3TimbreContext {
    struct MatrixRowParams *matrixRows;
    struct LfoParams *lfoOscParams[NUMBER_OF_LFO_OSC];
    float *lfoPhases[NUMBER_OF_LFO_OSC];
    struct EnvelopeLfoParams *lfoEnv;
    struct Envelope2LfoParams *lfoEnv2;
    struct StepSequencerParams *lfoStepSeqParams[NUMBER_OF_LFO_STEP];
    struct StepSequencerSteps *lfoStepSeqSteps[NUMBER_OF_LFO_STEP];
    float panLeft[6];
    float panRight[6];
    float mixerGain;
};

/*
 * PFM3 engine adapter.
 * For now it forwards to the existing Timbre pfm3 and preen methods; the code
 * moves here in a later step.
 */
class Preenfm3Engine : public SynthEngine {
public:
    Preenfm3Engine() : timbre_(nullptr) {}

    void init(Timbre *timbre, SynthState *synthState) override;
    void afterNewParamsLoad() override;
    void bindVoice(Voice *voice) override;
    void noteOn(char note, char velocity) override;
    void noteOff(char note) override;
    uint8_t voicesNextBlock() override;
    void prepareBlock() override;
    void glide() override;
    void gateFx() override;

    void setMatrixSource(SourceEnum source, float value) override;
    void setMatrixSourceMPE(uint8_t channel, SourceEnum source, float value) override;
    void resetMatrixDestination(float oldValue) override;
    void setMatrixPolyAfterTouch(uint8_t note, float value) override;
    void noteOnMpe(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void noteOffMpe(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void newParamValue(int row, int encoder) override;
    void setParamValue(int index, float value) override;
    void prepareFxMatrix(Voice *voice) override;
    void updateVoiceMatrix(Voice *voice, int note, int velocity) override;

    void lfoValueChange(int row, int encoder, float value) override;
    void updateMidiNoteScale(int scale) override;
    void verifyLfoUsed(int encoder, float oldValue, float newValue) override;
    bool isLfoUsed(int lfo) override;
    void midiClockContinue(int songPosition) override;
    void midiClockStart() override;
    void midiClockSongPositionStep(int songPosition) override;
    float getMatrixDestination(int destination) override;
    uint32_t getStorageMagic() const override;
    int getStorageNameOffset(uint32_t magic) const override;
    const char* getStorageDefaultName() const override;
    uint32_t getStorageBlob(uint8_t *out, uint32_t maxSize, const TimbreSharedParams &shared) override;
    bool loadStorageBlob(const uint8_t *in, uint32_t size, uint32_t magic) override;
    bool supportsDeferredSteal() const override {
        return true;
    }

    Osc& osc(int index) {
        return osc_[index];
    }
    Env& env(int index) {
        return env_[index];
    }

    Pfm3TimbreParams& pfm3Params() {
        return pfm3Params_[timbreNumber_];
    }
    const Pfm3TimbreParams& pfm3Params() const {
        return pfm3Params_[timbreNumber_];
    }

    void refreshContext();

private:
    void initContext();
    void initOscEnv(SynthState *synthState);
    void preenNoteOnUpdateMatrix(int voiceToUse, int note, int velocity);

    Timbre *timbre_;
    Pfm3TimbreContext *context_;
    int timbreNumber_;
    static Pfm3TimbreParams pfm3Params_[NUMBER_OF_TIMBRES];
    Osc osc_[6];
    Env env_[6];
    int lfoUSed_[NUMBER_OF_LFO];
    bool recomputeNext_;
    float currentGate_;
    static const float unisonPhase[MAX_UNISON_VOICES];
};

#endif
