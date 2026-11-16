/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_VOICE_STATE_H_
#define PFM3_VOICE_STATE_H_

#include "Common.h"

#include "Osc.h"
#include "Env.h"
#include "Matrix.h"
#include "LfoOsc.h"
#include "LfoEnv.h"
#include "LfoEnv2.h"
#include "LfoStepSeq.h"

/*
 * Per-voice state owned by the PFM3 engine.
 * One instance per global voice slot, owned by Preenfm3Engine (DTCMRAM).
 */
struct Pfm3VoiceState {
    Matrix matrix;

    LfoOsc lfoOsc[NUMBER_OF_LFO_OSC];
    LfoEnv lfoEnv[NUMBER_OF_LFO_ENV];
    LfoEnv2 lfoEnv2[NUMBER_OF_LFO_ENV2];
    LfoStepSeq lfoStepSeq[NUMBER_OF_LFO_STEP];

    EnvData envState1_;
    EnvData envState2_;
    EnvData envState3_;
    EnvData envState4_;
    EnvData envState5_;
    EnvData envState6_;

    OscState oscState1_;
    OscState oscState2_;
    OscState oscState3_;
    OscState oscState4_;
    OscState oscState5_;
    OscState oscState6_;

    float velIm1, velIm2, velIm3, velIm4, velIm5, velIm6;
    float freqAi, freqAo;
    float freqBi, freqBo;
    float freqHarm, targetFreqHarm;
    float prevPhase;

    float env1ValueMem;
    float env2ValueMem;
    float env3ValueMem;
    float env4ValueMem;
    float env5ValueMem;
    float env6ValueMem;

    float modulationIndex1, modulationIndex2, modulationIndex3, modulationIndex4, modulationIndex5;
    float feedbackModulation, fdbLastValue[3];
    float mix1, mix2, mix3, mix4, mix5, mix6;
    float mix1Previous, mix2Previous, mix3Previous, mix4Previous, mix5Previous, mix6Previous;
    float pan1Left, pan2Left, pan3Left, pan4Left, pan5Left, pan6Left;
    float pan1Right, pan2Right, pan3Right, pan4Right, pan5Right, pan6Right;
};

#endif
