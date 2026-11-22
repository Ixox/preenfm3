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
#include "Env.h"
#include "Lfo.h"
#include "Osc.h"
#include "Pfm3Preset.h"
#include "Pfm3VoiceState.h"
#include "SynthEngine.h"
#include "TimbreParams.h"

class Hexter;
class Timbre;
class Voice;

/*
 * Per-timbre values that only depend on the preset, not on the voice.
 * Owned and refreshed by the engine, copied into a voice when it is bound.
 */
struct Pfm3TimbreContext {
    struct MatrixRowParams* matrixRows;
    struct LfoParams* lfoOscParams[NUMBER_OF_LFO_OSC];
    float* lfoPhases[NUMBER_OF_LFO_OSC];
    struct EnvelopeLfoParams* lfoEnv;
    struct Envelope2LfoParams* lfoEnv2;
    struct StepSequencerParams* lfoStepSeqParams[NUMBER_OF_LFO_STEP];
    struct StepSequencerSteps* lfoStepSeqSteps[NUMBER_OF_LFO_STEP];
    float panLeft[6];
    float panRight[6];
    float mixerGain;
    Pfm3TimbreParams* params;
};

/*
 * PFM3 engine adapter.
 * For now it forwards to the existing Timbre pfm3 and preen methods; the code
 * moves here in a later step.
 */
class Preenfm3Engine : public SynthEngine {
public:
    Preenfm3Engine() : timbre_(nullptr), synthState_(nullptr), hexter_(nullptr), pfm3Preset_(nullptr) {
    }

    struct Pfm3Preset* getPfm3Preset() const {
        return pfm3Preset_;
    }

    void init(Timbre* timbre, SynthState* synthState) override;
    void afterNewParamsLoad() override;
    void bindVoice(Voice* voice) override;
    void noteOn(char note, char velocity) override;
    void noteOff(char note) override;
    void killVoice(Voice* voice) override;
    void noteOffVoice(Voice* voice) override;
    void noteOffVoiceQuick(Voice* voice) override;
    void startVoiceAfterTransfer(Voice* voice, int note, float noteFrequency, uint8_t velocity, float phase, bool alreadyOff) override;
    void completeVoiceTransfer(Voice* voice, int poolIndex, VoicePool* pool) override;
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
    void paramChanged(int row, int encoder, float oldValue, float newValue) override;
    void setParamValue(int index, float value) override;
    bool isUnisonMode() const override;
    void setStepSequencerSteps(int which, const struct StepSequencerSteps& steps) override;
    void paramRowChanged(int row) override;
    void serializePfm3Preset(struct Pfm3Preset& out) const override;
    void deserializePfm3Preset(const struct Pfm3Preset& in) override;
    void applyPfm3Preset(const struct Pfm3Preset& flat);
    void syncSharedParams(const struct TimbreSharedParams& shared);
    void syncStepSequencerSteps(int which);
    int getSeqStepValue(int which, int step) const;
    void setSeqStepValue(int which, int step, int value);
    void loadHexterPatch(const uint8_t* packedPatch) override;
    uint8_t getPlayMode() const override {
        return pfm3Params().engine1.playMode;
    }
    void loadNewPreset() override;
    void randomizePreset(int operatorRandom, int envelopeTypeRandom, int imRandom, int modulationRandom) override;
    int getStepSelection(int which) const override;
    void setStepSelection(int which, int step) override;
    void prepareFxMatrix(Voice* voice) override;
    void updateVoiceMatrix(Voice* voice, int note, int velocity) override;

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
    uint32_t getStorageBlob(uint8_t* out, uint32_t maxSize, const TimbreSharedParams& shared) override;
    bool loadStorageBlob(const uint8_t* in, uint32_t size, uint32_t magic) override;
    bool supportsDeferredSteal() const override {
        return true;
    }
    bool reusesSameNoteVoice() const override {
        return true;
    }
    bool midiData(int timbre, const struct MidiData& data) override;
    bool midiPfm3ParamFeedback(int row, int encoder, float newValue, struct MidiParamMessage& out) const override;
    bool midiPatchParam(int timbre, int index, struct MidiParamMessage& out) const override;

    Osc& osc(int index) {
        return osc_[index];
    }
    Pfm3Env& env(int index) {
        return env_[index];
    }

    Pfm3TimbreParams& pfm3Params() {
        return pfm3Params_[timbreNumber_];
    }
    const Pfm3TimbreParams& pfm3Params() const {
        return pfm3Params_[timbreNumber_];
    }

    static Pfm3VoiceState& voiceState(int slot);
    static void initVoiceState(int slot);

    void refreshContext();

private:
    void initContext();
    void initOscEnv(SynthState* synthState);
    void preenNoteOnUpdateMatrix(int voiceToUse, int note, int velocity);
    void updateAllModulationIndexes(Voice* voice);
    void prepareMatrixForNewBlock(Voice* voice);
    void updateAllMixOscsAndPans(Voice* voice);
    Pfm3VoiceState& pfm3State(Voice* voice);
    void bindVoiceState(Voice* voice);
    void resetPfm3VoiceState(Voice* voice);
    void glideToNote(Voice* voice, short newNote, float newNoteFrequency);
    void glideFirstNoteOff(Voice* voice);
    void glideVoice(Voice* voice);
    float getNoteRealFrequencyEstimation(Voice* voice, float newNoteFrequency);
    void noteOnVoice(Voice* voice, short note, float newNoteFrequency, short velocity, uint32_t index, float phase = 0.25f);
    void noteOnVoiceWithoutPop(Voice* voice, short note, float newNoteFrequency, short velocity, uint32_t index, float phase = 0.25f);
    void endNoteOrBeginNextOne(Voice* voice);
    void lfoNoteOn(Voice* voice);
    void lfoNoteOff(Voice* voice);
    void afterParamsLoadVoice(Voice* voice);
    void setPfm3ValueFromMidi(int row, int encoder, float newValue);
    void setStepValueFromMidi(int whichStepSeq, int step, int newValue);
    bool midiCcData(uint8_t cc, uint8_t value);
    bool midiNrpnData(uint16_t param, uint16_t value);
    void resetMatrix(Voice* voice);
    void lfoValueChangeOnVoice(Voice* voice, int currentRow, int encoder, float newValue);
    void midiClockSongPositionStepOnVoice(Voice* voice, int songPosition, bool recomputeNext);
    void midiClockContinueOnVoice(Voice* voice, int songPosition);
    void midiClockStartOnVoice(Voice* voice);
    void renderVoice(Voice* voice);

    Timbre* timbre_;
    SynthState* synthState_;
    Hexter* hexter_;
    Pfm3Preset* pfm3Preset_;
    Pfm3TimbreContext* context_;
    int timbreNumber_;
    int stepSelect_[2];
    static Pfm3TimbreParams pfm3Params_[NUMBER_OF_TIMBRES];
    static Pfm3VoiceState pfm3VoiceStates_[MAX_NUMBER_OF_VOICES];
    static float glidePhaseInc[13];
    static float mpeBitchBend[6];
    Osc osc_[6];
    Pfm3Env env_[6];
    int lfoUSed_[NUMBER_OF_LFO];
    bool recomputeNext_;
    float currentGate_;
    static const float unisonPhase[MAX_UNISON_VOICES];
};

#endif
