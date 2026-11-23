/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_FLASH_FORMAT_H_
#define PFM3_FLASH_FORMAT_H_

#include "Common.h"
#include "Pfm3Preset.h"

// Storage mapping of the PFM3 parameters (legacy on-flash layout).
struct FlashSynthParams {
    struct Engine1Params engine1;
    struct FlashEngineIm1 flashEngineIm1;
    struct FlashEngineIm2 flashEngineIm2;
    struct EngineMix1 engineMix1;
    struct EngineMix2 engineMix2;
    struct EngineMix3 engineMix3;
    struct OscillatorParams osc1;
    struct OscillatorParams osc2;
    struct OscillatorParams osc3;
    struct OscillatorParams osc4;
    struct OscillatorParams osc5;
    struct OscillatorParams osc6;
    struct EnvelopeParamsA env1a;
    struct EnvelopeParamsB env1b;
    struct EnvelopeParamsA env2a;
    struct EnvelopeParamsB env2b;
    struct EnvelopeParamsA env3a;
    struct EnvelopeParamsB env3b;
    struct EnvelopeParamsA env4a;
    struct EnvelopeParamsB env4b;
    struct EnvelopeParamsA env5a;
    struct EnvelopeParamsB env5b;
    struct EnvelopeParamsA env6a;
    struct EnvelopeParamsB env6b;
    struct MatrixRowParams matrixRowState1;
    struct MatrixRowParams matrixRowState2;
    struct MatrixRowParams matrixRowState3;
    struct MatrixRowParams matrixRowState4;
    struct MatrixRowParams matrixRowState5;
    struct MatrixRowParams matrixRowState6;
    struct MatrixRowParams matrixRowState7;
    struct MatrixRowParams matrixRowState8;
    struct MatrixRowParams matrixRowState9;
    struct MatrixRowParams matrixRowState10;
    struct MatrixRowParams matrixRowState11;
    struct MatrixRowParams matrixRowState12;
    struct LfoParams lfoOsc1;
    struct LfoParams lfoOsc2;
    struct LfoParams lfoOsc3;
    struct EnvelopeLfoParams lfoEnv1;
    struct Envelope2LfoParams lfoEnv2;
    struct StepSequencerParams lfoSeq1;
    struct StepSequencerParams lfoSeq2;
    struct StepSequencerSteps lfoSteps1;
    struct StepSequencerSteps lfoSteps2;
    char presetName[13];
    struct EngineArp1 engineArp1;
    struct EngineArp2 engineArp2;
    struct FlashEngineVeloIm1 flashEngineVeloIm1;
    struct FlashEngineVeloIm2 flashEngineVeloIm2;
    struct EffectRowParams effect;
    struct EngineArpUserPatterns engineArpUserPatterns;
    struct LfoPhaseRowParams lfoPhases;
    struct MidiNoteCurveRowParams midiNote1Curve;
    struct MidiNoteCurveRowParams midiNote2Curve;
    struct Engine2Params engine2;
    struct EnvelopeCurveFlashMemory1 envCurves1To4;
    struct EnvelopeCurveFlashMemory2 envCurves5To6;
    struct EffectRowParams effect2;
    struct ArpAdvancedParams arpAdvanced;
};

void pfm3ParamsToFlash(const struct Pfm3Preset* params, struct FlashSynthParams* flashMemory, bool saveArp);
void pfm3FlashToParams(const struct FlashSynthParams* flashMemory, struct Pfm3Preset* params, bool loadArp);

#endif
