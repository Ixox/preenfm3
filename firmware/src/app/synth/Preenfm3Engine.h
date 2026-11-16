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
#include "Lfo.h"
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

    void init(Timbre *timbre) override;
    void afterNewParamsLoad() override;
    void bindVoice(Voice *voice) override;
    void noteOn(char note, char velocity) override;
    void noteOff(char note) override;
    uint8_t voicesNextBlock() override;
    void prepareBlock() override;
    void glide() override;
    void gateFx() override;

    void refreshContext();

private:
    void initContext();

    Timbre *timbre_;
    Pfm3TimbreContext *context_;
};

#endif
