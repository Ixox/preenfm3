/*
 * Copyright 2026 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PFM3_PRESET_H_
#define PFM3_PRESET_H_

#include <stddef.h>

#include "Pfm3Params.h"

/*
 * On-flash PFM3 preset layout and editor/storage mirror.
 * This is PFM3-specific.
 */
struct Pfm3Preset {
    struct Engine1Params engine1;
    struct EngineIm1 engineIm1;
    struct EngineIm2 engineIm2;
    struct EngineIm3 engineIm3;
    struct EngineMix1 engineMix1;
    struct EngineMix2 engineMix2;
    struct EngineMix3 engineMix3;
    struct EngineArp1 engineArp1;
    struct EngineArp2 engineArp2;
    struct EngineArpUserPatterns engineArpUserPatterns;
    struct EffectRowParams effect1;
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
    struct EffectRowParams effect2;
    char presetName[13];
};

/*
 * Float offsets in the flat Pfm3Preset layout.
 * Shared: [sharedBlockStart, sharedBlockEnd) (arp + effect1) and
 *         [effect2Start, presetNameStart) (effect2).
 * PFM3:   [0, sharedBlockStart) and [pfm3BlockBStart, effect2Start).
 */
namespace Pfm3PresetLayout {
    static const int sharedBlockStart = offsetof(Pfm3Preset, engineArp1) / sizeof(float);
    static const int sharedBlockEnd = offsetof(Pfm3Preset, osc1) / sizeof(float);
    static const int pfm3BlockBStart = offsetof(Pfm3Preset, osc1) / sizeof(float);
    static const int effect2Start = offsetof(Pfm3Preset, effect2) / sizeof(float);
    static const int presetNameStart = offsetof(Pfm3Preset, presetName) / sizeof(float);
    static const int sharedGap = sharedBlockEnd - sharedBlockStart;
}

extern const struct Pfm3Preset preenMainPreset;
extern const struct Pfm3Preset defaultPreset;
extern const struct Pfm3Preset newPresetParams;
extern struct Pfm3Preset pfm3PresetTmp;

#endif
