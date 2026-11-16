/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef TIMBRE_PARAMS_H_
#define TIMBRE_PARAMS_H_

#include <stddef.h>

#include "Common.h"

/*
 * Runtime parameter split (migration step 6).
 *
 * `OneSynthParams` remains the on-flash PFM3 preset layout and must not
 * change. These structs are the runtime owners: shared host parameters stay
 * in `Timbre`, PFM3 DSP parameters belong to `Preenfm3Engine`.
 *
 * Member names match `OneSynthParams` so serialization is a field-by-field
 * copy in both directions.
 */
struct TimbreSharedParams {
    struct EngineArp1 engineArp1;
    struct EngineArp2 engineArp2;
    struct EngineArpUserPatterns engineArpUserPatterns;
    struct EffectRowParams effect1;
    struct EffectRowParams effect2;
    char presetName[13];
};

struct Pfm3TimbreParams {
    struct Engine1Params engine1;
    struct EngineIm1 engineIm1;
    struct EngineIm2 engineIm2;
    struct EngineIm3 engineIm3;
    struct EngineMix1 engineMix1;
    struct EngineMix2 engineMix2;
    struct EngineMix3 engineMix3;
    struct OscillatorParams osc1;
    struct OscillatorParams osc2;
    struct OscillatorParams osc3;
    struct OscillatorParams osc4;
    struct OscillatorParams osc5;
    struct OscillatorParams osc6;
    struct EnvelopeTimeMemory env1Time;
    struct EnvelopeTimeMemory env2Time;
    struct EnvelopeTimeMemory env3Time;
    struct EnvelopeTimeMemory env4Time;
    struct EnvelopeTimeMemory env5Time;
    struct EnvelopeTimeMemory env6Time;
    struct EnvelopeLevelMemory env1Level;
    struct EnvelopeLevelMemory env2Level;
    struct EnvelopeLevelMemory env3Level;
    struct EnvelopeLevelMemory env4Level;
    struct EnvelopeLevelMemory env5Level;
    struct EnvelopeLevelMemory env6Level;
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
    struct PerformanceRowParams performance1;
    struct LfoParams lfoOsc1;
    struct LfoParams lfoOsc2;
    struct LfoParams lfoOsc3;
    struct LfoPhaseRowParams lfoPhases;
    struct EnvelopeLfoParams lfoEnv1;
    struct Envelope2LfoParams lfoEnv2;
    struct StepSequencerParams lfoSeq1;
    struct StepSequencerParams lfoSeq2;
    struct MidiNoteCurveRowParams midiNote1Curve;
    struct MidiNoteCurveRowParams midiNote2Curve;
    struct StepSequencerSteps lfoSteps1;
    struct StepSequencerSteps lfoSteps2;
    struct Engine2Params engine2;
    struct EnvelopeCurveParams env1Curve;
    struct EnvelopeCurveParams env2Curve;
    struct EnvelopeCurveParams env3Curve;
    struct EnvelopeCurveParams env4Curve;
    struct EnvelopeCurveParams env5Curve;
    struct EnvelopeCurveParams env6Curve;
};

/*
 * Float offsets in the flat OneSynthParams layout.
 * Shared: [sharedBlockStart, sharedBlockEnd) (arp + effect1) and
 *         [effect2Start, presetNameStart) (effect2).
 * PFM3:   [0, sharedBlockStart) and [pfm3BlockBStart, effect2Start).
 */
namespace TimbreParamLayout {
    static const int sharedBlockStart = offsetof(OneSynthParams, engineArp1) / sizeof(float);
    static const int sharedBlockEnd = offsetof(OneSynthParams, osc1) / sizeof(float);
    static const int pfm3BlockBStart = offsetof(OneSynthParams, osc1) / sizeof(float);
    static const int effect2Start = offsetof(OneSynthParams, effect2) / sizeof(float);
    static const int presetNameStart = offsetof(OneSynthParams, presetName) / sizeof(float);
    static const int sharedGap = sharedBlockEnd - sharedBlockStart;
}

static_assert(sizeof(float) == 4, "float must be 32-bit");
static_assert(offsetof(Pfm3TimbreParams, osc1) / sizeof(float) == TimbreParamLayout::sharedBlockStart,
    "Pfm3TimbreParams layout changed");
static_assert(offsetof(TimbreSharedParams, effect2) / sizeof(float) == TimbreParamLayout::sharedGap,
    "TimbreSharedParams layout changed");

#endif
