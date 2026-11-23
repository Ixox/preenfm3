/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Pfm3PresetIO.h"

#include <string.h>

void pfm3SerializePreset(const Pfm3TimbreParams& pfm3, const TimbreSharedParams& shared, Pfm3Preset& out) {
    out.engine1 = pfm3.engine1;
    out.engineIm1 = pfm3.engineIm1;
    out.engineIm2 = pfm3.engineIm2;
    out.engineIm3 = pfm3.engineIm3;
    out.engineMix1 = pfm3.engineMix1;
    out.engineMix2 = pfm3.engineMix2;
    out.engineMix3 = pfm3.engineMix3;
    out.osc1 = pfm3.osc1;
    out.osc2 = pfm3.osc2;
    out.osc3 = pfm3.osc3;
    out.osc4 = pfm3.osc4;
    out.osc5 = pfm3.osc5;
    out.osc6 = pfm3.osc6;
    out.env1Time = pfm3.env1Time;
    out.env2Time = pfm3.env2Time;
    out.env3Time = pfm3.env3Time;
    out.env4Time = pfm3.env4Time;
    out.env5Time = pfm3.env5Time;
    out.env6Time = pfm3.env6Time;
    out.env1Level = pfm3.env1Level;
    out.env2Level = pfm3.env2Level;
    out.env3Level = pfm3.env3Level;
    out.env4Level = pfm3.env4Level;
    out.env5Level = pfm3.env5Level;
    out.env6Level = pfm3.env6Level;
    out.matrixRowState1 = pfm3.matrixRowState1;
    out.matrixRowState2 = pfm3.matrixRowState2;
    out.matrixRowState3 = pfm3.matrixRowState3;
    out.matrixRowState4 = pfm3.matrixRowState4;
    out.matrixRowState5 = pfm3.matrixRowState5;
    out.matrixRowState6 = pfm3.matrixRowState6;
    out.matrixRowState7 = pfm3.matrixRowState7;
    out.matrixRowState8 = pfm3.matrixRowState8;
    out.matrixRowState9 = pfm3.matrixRowState9;
    out.matrixRowState10 = pfm3.matrixRowState10;
    out.matrixRowState11 = pfm3.matrixRowState11;
    out.matrixRowState12 = pfm3.matrixRowState12;
    out.performance1 = pfm3.performance1;
    out.lfoOsc1 = pfm3.lfoOsc1;
    out.lfoOsc2 = pfm3.lfoOsc2;
    out.lfoOsc3 = pfm3.lfoOsc3;
    out.lfoPhases = pfm3.lfoPhases;
    out.lfoEnv1 = pfm3.lfoEnv1;
    out.lfoEnv2 = pfm3.lfoEnv2;
    out.lfoSeq1 = pfm3.lfoSeq1;
    out.lfoSeq2 = pfm3.lfoSeq2;
    out.midiNote1Curve = pfm3.midiNote1Curve;
    out.midiNote2Curve = pfm3.midiNote2Curve;
    out.lfoSteps1 = pfm3.lfoSteps1;
    out.lfoSteps2 = pfm3.lfoSteps2;
    out.engine2 = pfm3.engine2;
    out.env1Curve = pfm3.env1Curve;
    out.env2Curve = pfm3.env2Curve;
    out.env3Curve = pfm3.env3Curve;
    out.env4Curve = pfm3.env4Curve;
    out.env5Curve = pfm3.env5Curve;
    out.env6Curve = pfm3.env6Curve;

    out.engineArp1 = shared.engineArp1;
    out.engineArp2 = shared.engineArp2;
    out.engineArpUserPatterns = shared.engineArpUserPatterns;
    out.effect1 = shared.effect1;
    out.effect2 = shared.effect2;
    out.arpAdvanced = shared.arpAdvanced;
    memcpy(out.presetName, shared.presetName, sizeof(out.presetName));
}

void pfm3DeserializePreset(const Pfm3Preset& in, Pfm3TimbreParams& pfm3, TimbreSharedParams& shared) {
    pfm3.engine1 = in.engine1;
    pfm3.engineIm1 = in.engineIm1;
    pfm3.engineIm2 = in.engineIm2;
    pfm3.engineIm3 = in.engineIm3;
    pfm3.engineMix1 = in.engineMix1;
    pfm3.engineMix2 = in.engineMix2;
    pfm3.engineMix3 = in.engineMix3;
    pfm3.osc1 = in.osc1;
    pfm3.osc2 = in.osc2;
    pfm3.osc3 = in.osc3;
    pfm3.osc4 = in.osc4;
    pfm3.osc5 = in.osc5;
    pfm3.osc6 = in.osc6;
    pfm3.env1Time = in.env1Time;
    pfm3.env2Time = in.env2Time;
    pfm3.env3Time = in.env3Time;
    pfm3.env4Time = in.env4Time;
    pfm3.env5Time = in.env5Time;
    pfm3.env6Time = in.env6Time;
    pfm3.env1Level = in.env1Level;
    pfm3.env2Level = in.env2Level;
    pfm3.env3Level = in.env3Level;
    pfm3.env4Level = in.env4Level;
    pfm3.env5Level = in.env5Level;
    pfm3.env6Level = in.env6Level;
    pfm3.matrixRowState1 = in.matrixRowState1;
    pfm3.matrixRowState2 = in.matrixRowState2;
    pfm3.matrixRowState3 = in.matrixRowState3;
    pfm3.matrixRowState4 = in.matrixRowState4;
    pfm3.matrixRowState5 = in.matrixRowState5;
    pfm3.matrixRowState6 = in.matrixRowState6;
    pfm3.matrixRowState7 = in.matrixRowState7;
    pfm3.matrixRowState8 = in.matrixRowState8;
    pfm3.matrixRowState9 = in.matrixRowState9;
    pfm3.matrixRowState10 = in.matrixRowState10;
    pfm3.matrixRowState11 = in.matrixRowState11;
    pfm3.matrixRowState12 = in.matrixRowState12;
    pfm3.performance1 = in.performance1;
    pfm3.lfoOsc1 = in.lfoOsc1;
    pfm3.lfoOsc2 = in.lfoOsc2;
    pfm3.lfoOsc3 = in.lfoOsc3;
    pfm3.lfoPhases = in.lfoPhases;
    pfm3.lfoEnv1 = in.lfoEnv1;
    pfm3.lfoEnv2 = in.lfoEnv2;
    pfm3.lfoSeq1 = in.lfoSeq1;
    pfm3.lfoSeq2 = in.lfoSeq2;
    pfm3.midiNote1Curve = in.midiNote1Curve;
    pfm3.midiNote2Curve = in.midiNote2Curve;
    pfm3.lfoSteps1 = in.lfoSteps1;
    pfm3.lfoSteps2 = in.lfoSteps2;
    pfm3.engine2 = in.engine2;
    pfm3.env1Curve = in.env1Curve;
    pfm3.env2Curve = in.env2Curve;
    pfm3.env3Curve = in.env3Curve;
    pfm3.env4Curve = in.env4Curve;
    pfm3.env5Curve = in.env5Curve;
    pfm3.env6Curve = in.env6Curve;

    shared.engineArp1 = in.engineArp1;
    shared.engineArp2 = in.engineArp2;
    shared.engineArpUserPatterns = in.engineArpUserPatterns;
    shared.effect1 = in.effect1;
    shared.effect2 = in.effect2;
    shared.arpAdvanced = in.arpAdvanced;
    memcpy(shared.presetName, in.presetName, sizeof(shared.presetName));
}
