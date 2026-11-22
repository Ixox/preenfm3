/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "Pfm3Param.h"
#include "Pfm3ParamUiData.h"

#include <math.h>
#include <stddef.h>

#include "Common.h"
#include "MidiControlChange.h"
#include "Pfm3ControlChange.h"
#include "Pfm3Params.h"
#include "Pfm3Preset.h"
#include "SynthState.h"
#include "TimbreParams.h"

/*
 * Clean parameter model.
 *
 * One ParamDesc per field. The block carries the per-element base/stride for
 * the live store, the flat Pfm3Preset defaults and the legacy NRPN numbering.
 * This keeps per-element NRPN/default metadata without duplicating a full
 * descriptor for every operator.
 *
 * MIDI input uses these tables; the editor is not wired yet.
 */

/*
 * Editor name tables. They live here so the full parameter table is compiled
 * by the host tests too.
 */
const char* algoNames[] = {"alg1", "alg2", "alg3", "alg4", "alg5", "alg6", "alg7", "alg8", "alg9", "al10", "al11",
                           "al12", "al13", "al14", "al15", "al16", "al17", "al18", "al19", "al20", "al21", "al22",
                           "al23", "al24", "al25", "al26", "al27", "al28", "al29", "al30", "al31", "al32"};

const char* polyMonoNames[] = {"Mono", "Poly", "Unis"};

const char* glideTypeNames[] = {
    "Off    ",
    "Overlap",
    "Always ",
};

const char* clockName[] = {"Off ", "Int ", "Ext "};
const char* dirName[] = {"Up  ", "Down", "U&D ", "Play", "Rand", "Chrd", "RtUp", "RtDn", "RtUD", "ShUp", "ShDn", "ShUD"};

const char* divNames[] = {"2/1 ", "3/2 ", "1/1 ", "3/4 ", "2/3 ", "1/2 ", "3/8 ", "1/3 ", "1/4 ",
                          "1/6 ", "1/8 ", "1/12", "1/16", "1/24", "1/32", "1/48", "1/96"};
const char* activeName[] = {"Off ", "On  "};

const char* patternName[] = {"1   ", "2   ", "3   ", "4   ", "5   ", "6   ", "7   ",  "8   ", "9   ", "10   ", "11  ", "12  ", "13  ",
                             "14  ", "15  ", "16  ", "17  ", "18  ", "19  ", "20   ", "21  ", "22  ", "Usr1",  "Usr2", "Usr3", "Usr4"};

const char* envCurveNames[] = {"Exp ", "Lin ", "Log ", "Usr1", "Usr2", "Usr3", "Usr4"};

const unsigned char filtersOrder[] = {0,  1,  2,  3,  4,  5,  6,  38, 39, 40, 46, 47, 22, 7,  8,  10, 11, 12, 13, 14,
                                      15, 16, 17, 18, 19, 20, 21, 23, 24, 25, 26, 32, 33, 34, 35, 36, 37, 41, 42, 9,
                                      43, 44, 45, 27, 28, 29, 30, 31, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58};

const unsigned char filtersPosition[] = {0,  1,  2,  3,  4,  5,  6,  13, 14, 39, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                                         25, 26, 12, 27, 28, 29, 30, 43, 44, 45, 46, 47, 31, 32, 33, 34, 35, 36, 7,  8,
                                         9,  37, 38, 40, 41, 42, 10, 11, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58};

const char* fxName[] = {
    "Off ", /*  0   */
    "Mix ", /*  1   */
    "LP  ", /*  2   */
    "HP  ", /*  3   */
    "Bass", /*  4   */
    "BP  ", /*  5   */
    "Crsh", /*  6   */
    "LP2 ", /*  7   */
    "HP2 ", /*  8   */
    "BP2 ", /*  9   */
    "Lp3 ", /*  10  */
    "Hp3 ", /*  11  */
    "Bp3 ", /*  12  */
    "Peak", /*  13  */
    "Notc", /*  14  */
    "Bell", /*  15  */
    "LowS", /*  16  */
    "HigS", /*  17  */
    "LpHp", /*  18  */
    "BpDs", /*  19  */
    "LPws", /*  20  */
    "Tilt", /*  21  */
    "Pann", /*  22  */
    "Sat ", /*  23  */
    "Sigm", /*  24  */
    "Fold", /*  25  */
    "Wrap", /*  26  */
    "Xor ", /*  27  */
    "Txr1", /*  28  */
    "Txr2", /*  29  */
    "LPx1", /*  30  */
    "LPx2", /*  31  */
    "LpSn", /*  32  */
    "HpSn", /*  33  */
    "Not4", /*  34  */
    "Ap4 ", /*  35  */
    "Ap4b", /*  36  */
    "Ap4D", /*  37  */
    "Oryx", /*  38  */
    "Orx2", /*  39  */
    "Orx3", /*  40  */
    "18db", /*  41  */
    "La+d", /*  42  */
    "Lad+", /*  43  */
    "Diod", /*  44  */
    "L+d+", /*  45  */
    "h3o+", /*  46  */
    "Svh3", /*  47  */
    "Alkx", /*  48  */
};

const char* fx2Name[] = {
    "Off ", /* 0 */
    "Flng", /* 1  */
    "Dim ", /* 2  */
    "Chor", /* 3  */
    "Wide", /* 4  */
    "Dblr", /* 5  */
    "3Voi", /* 6  */
    "Bode", /* 7  */
    "Delc", /* 8  */
    "Ping", /* 9  */
    "Diff", /* 10 */
    "Gra1", /* 11 */
    "Gra2", /* 12 */
    "StBp", /* 13 */
};

const char* oscShapeNames[] = {"sin ", "saw ", "squa", "s^2 ", "szer", "spos", "rand",
                               "off ", "Usr1", "Usr2", "Usr3", "Usr4", "Usr5", "Usr6"};
const unsigned char oscShapeNamesOrder[] = {7, 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 12, 13};
const unsigned char oscShapeNamesPosition[] = {1, 2, 3, 4, 5, 6, 7, 0, 8, 9, 10, 11, 12, 13};

const char* oscTypeNames[] = {"keyb", "fixe", "kehz"};
const unsigned char oscTypeNamesOrder[] = {0, 2, 1};
const unsigned char oscTypeNamesPosition[] = {0, 2, 1};

const char* lofEnv2Loop[] = {"No  ", "Sile", "Attk"};

const char* matrixSourceNames[MATRIX_SOURCE_MAX] = {/* 0 */ "none  ",  "Lfo 1 ", "Lfo 2 ", "Lfo 3 ", "Env 1 ", "Env 2 ", "Seq 1 ",
                                                    "Seq 2 ",
                                                    /* 8 */ "ModWel",  "PitBen", "AftTou", "Veloc ", "Note1 ", "Perf1 ", "Perf2 ",
                                                    /* 15 */ "Perf3 ", "Perf4 ", "Note2 ", "Breath", "74 MPE", "RndKey", "PolyAT",
                                                    /* 22 */ "UsrCC1", "UsrCC2", "UsrCC3", "UsrCC4",
                                                    /* 26 */ "PB MPE", "AT MPE"};

const unsigned char matrixSourceOrder[MATRIX_SOURCE_MAX] = {0,  1,  2,  3,  4,  5,  6,  7,  12, 17, 8,  9,  10, 21,
                                                            11, 18, 13, 14, 15, 16, 20, 26, 27, 19, 22, 23, 24, 25};
unsigned char matrixSourcePosition[MATRIX_SOURCE_MAX];

const char* matrixDestNames[DESTINATION_MAX] = {
    "None ", "Gate ", "IM 1 ", "IM 2 ", "IM 3 ", "IM 4 ", "IMAll", "Mix 1", "Pan 1", "Mix 2", "Pan 2", "Mix 3", "Pan 3", "Mix 4",
    "Pan 4", "Mix *", "Pan *", "o1 Fq", "o2 Fq", "o3 Fq", "o4 Fq", "o5 Fq", "o6 Fq", "o* Fq", "Atk 1", "Atk 2", "Atk 3", "Atk 4",
    "Atk 5", "Atk 6", "Atk C", "Rel C", "mtx 1", "mtx 2", "mtx 3", "mtx 4", "lfo1F", "lfo2F", "lfo3F", "env2S", "stp1G", "stp2G",
    "Fx1 1", "o* Fh", "Dec C", "Atk M", "Dec M", "Rel M", "FdBck", "Fx1 2", "fx1Am", "Fx2 1", "Fx2 2", "Fx2Am",
};
const unsigned char matrixTargetOrder[DESTINATION_MAX] = {0,  1,  2,  3,  4,  5,  48, 6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                                          17, 18, 19, 20, 21, 22, 23, 43, 24, 25, 26, 27, 28, 29, 30, 45, 44, 46,
                                                          31, 47, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 49, 50, 51, 52, 53};

unsigned char matrixTargetPosition[DESTINATION_MAX];

const char* lfoShapeNames[] = {"Sin ", "Saw ", "Tria", "Squa", "Rand", "Brwn", "Wndr", "Flow"};

const char* midiNoteCurves[] = {"Flat", "+Lin", "+Ln8", "+Exp", "-Lin", "-Ln8", "-Exp"};

/*
 * One entry per field: engine/storage (desc), editor (ui) and MIDI (midi).
 * An "element" is one repeated instance of a block: one operator for
 * Osc/Env/IM/Mix, one matrix row, one LFO, one sequencer. A block has
 * `count` elements and `fieldCount` fields per element; the block carries
 * the per-element base/stride for live, flat and NRPN storage.
 *
 * `off` is the field offset within one element. The descriptor address is
 * liveBase + element*liveStride + off.
 *
 * The order must match engineBlocks: each block's descBase points here and
 * fieldCount says how many descriptors it owns.
 */
#define PARAM_FLOAT(n, mn, mx, st, off, dt, nv) \
    {{ParamAccess::Float, (uint16_t)(off), mn, mx, st}, {n, dt, nv, nullptr, nullptr, nullptr}}
#define PARAM_ENUM(n, mx, off, nv, names) \
    {{ParamAccess::Float, (uint16_t)(off), 0, mx, 1.0f}, {n, DISPLAY_TYPE_STRINGS, nv, names, nullptr, nullptr}}
#define PARAM_ENUM_ORDERED(n, mx, off, nv, names, ord, pos) \
    {{ParamAccess::Float, (uint16_t)(off), 0, mx, 1.0f}, {n, DISPLAY_TYPE_STRINGS, nv, names, ord, pos}}
#define PARAM_INTEGER(n, mn, mx, st, off, nv) \
    {{ParamAccess::Float, (uint16_t)(off), mn, mx, st}, {n, DISPLAY_TYPE_INT, nv, nullptr, nullptr, nullptr}}
#define PARAM_STEP_BPM(n, mn, mx, st, off, nv) \
    {{ParamAccess::Float, (uint16_t)(off), mn, mx, st}, {n, DISPLAY_TYPE_STEP_SEQ_BPM, nv, nullptr, nullptr, nullptr}}
#define PARAM_ARP_PATTERN(n, off) \
    {{ParamAccess::Bitfield, (uint16_t)(off), 0, 0xFFFF, 0.0f}, {n, DISPLAY_TYPE_ARP_PATTERN, 0, nullptr, nullptr, nullptr}}
#define PARAM_CHAR(n, off) \
    {{ParamAccess::Char, (uint16_t)(off), 0, 127, 0.0f}, {n, DISPLAY_TYPE_NONE, 0, nullptr, nullptr, nullptr}}
#define PARAM_STEP(n, off) \
    {{ParamAccess::U8, (uint16_t)(off), 0, 15, 0.0f}, {n, DISPLAY_TYPE_NONE, 0, nullptr, nullptr, nullptr}}

/*
 * =============================================================
 * Main parameter table: one entry per field, in block order.
 * =============================================================
 */
const ParamFullDescription pfm3EngineParams[72] __attribute__((section(".rodata"))) = {
    /* Engine1 */
    PARAM_ENUM("Algo", ALGO_END - 1, offsetof(Engine1Params, algo), ALGO_END, algoNames),
    PARAM_FLOAT("Velo", 0, 16, 1.0f, offsetof(Engine1Params, velocity), DISPLAY_TYPE_INT, 17),
    PARAM_ENUM("Mode", 2, offsetof(Engine1Params, playMode), 3, polyMonoNames),
    PARAM_FLOAT("Speed", 0, 12, 1.0f, offsetof(Engine1Params, glideSpeed), DISPLAY_TYPE_INT, 13),
    /* Engine2 */
    PARAM_ENUM("Glide", 2, offsetof(Engine2Params, glideType), 3, glideTypeNames),
    PARAM_FLOAT("Spread", 0, 1, 0.01f, offsetof(Engine2Params, unisonSpread), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Detune", -1, 1, 0.01f, offsetof(Engine2Params, unisonDetune), DISPLAY_TYPE_FLOAT, 201),
    PARAM_FLOAT("", 0, 1, 0.01f, offsetof(Engine2Params, pfm3Version), DISPLAY_TYPE_FLOAT, 101),
    /* Mod: 6 operators x (IM, Velo) */
    PARAM_FLOAT("IM", 0, 16, 0.01f, offsetof(EngineIm1, modulationIndex1), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_FLOAT("Velo", 0, 16, 0.01f, offsetof(EngineIm1, modulationIndexVelo1), DISPLAY_TYPE_FLOAT, 1601),
    /* OscMix: 6 operators x (Mix, Pan) */
    PARAM_FLOAT("Mix", 0, 1, 0.01f, offsetof(EngineMix1, mixOsc1), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Pan", -1, 1, 0.01f, offsetof(EngineMix1, panOsc1), DISPLAY_TYPE_FLOAT, 201),
    /* Osc: 6 operators x 4 */
    PARAM_ENUM_ORDERED("Shap", OSC_SHAPE_LAST - 1, offsetof(OscillatorParams, shape), OSC_SHAPE_LAST, oscShapeNames, oscShapeNamesOrder,
                       oscShapeNamesPosition),
    PARAM_ENUM_ORDERED("FTyp", 2, offsetof(OscillatorParams, frequencyType), 3, oscTypeNames, oscTypeNamesOrder, oscTypeNamesPosition),
    PARAM_FLOAT("Freq", 0, 16, 1.0f/12.0f, offsetof(OscillatorParams, frequencyMul), DISPLAY_TYPE_FLOAT_OSC_FREQUENCY, 193),
    PARAM_FLOAT("FTun", -9, 9, 0.01f, offsetof(OscillatorParams, detune), DISPLAY_TYPE_FLOAT_OSC_FREQUENCY, 1801),
    /* EnvTime: 6 operators x 4 */
    PARAM_FLOAT("Attk", 0, 16, 0.01f, offsetof(EnvelopeTimeMemory, attackTime), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_FLOAT("Deca", 0, 16, 0.01f, offsetof(EnvelopeTimeMemory, decayTime), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_FLOAT("Sust", 0, 16, 0.01f, offsetof(EnvelopeTimeMemory, sustainTime), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_FLOAT("Rel", 0, 16, 0.01f, offsetof(EnvelopeTimeMemory, releaseTime), DISPLAY_TYPE_FLOAT, 1601),
    /* EnvLevel: 6 operators x 4 */
    PARAM_FLOAT("Attk", 0, 1, 0.01f, offsetof(EnvelopeLevelMemory, attackLevel), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Deca", 0, 1, 0.01f, offsetof(EnvelopeLevelMemory, decayLevel), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Sust", 0, 1, 0.01f, offsetof(EnvelopeLevelMemory, sustainLevel), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Rel", 0, 1, 0.01f, offsetof(EnvelopeLevelMemory, releaseLevel), DISPLAY_TYPE_FLOAT, 101),
    /* EnvCurve: 6 operators x 4 */
    PARAM_ENUM("Attk", CURVE_TYPE_MAX - 1, offsetof(EnvelopeCurveParams, attackCurve), CURVE_TYPE_MAX, envCurveNames),
    PARAM_ENUM("Dec", CURVE_TYPE_MAX - 1, offsetof(EnvelopeCurveParams, decayCurve), CURVE_TYPE_MAX, envCurveNames),
    PARAM_ENUM("Sust", CURVE_TYPE_MAX - 1, offsetof(EnvelopeCurveParams, sustainCurve), CURVE_TYPE_MAX, envCurveNames),
    PARAM_ENUM("Rel", CURVE_TYPE_MAX - 1, offsetof(EnvelopeCurveParams, releaseCurve), CURVE_TYPE_MAX, envCurveNames),
    /* Matrix: 12 rows x 4 */
    PARAM_ENUM_ORDERED("Srce", MATRIX_SOURCE_MAX - 1, offsetof(MatrixRowParams, source), MATRIX_SOURCE_MAX, matrixSourceNames,
                       matrixSourceOrder, matrixSourcePosition),
    PARAM_FLOAT("Mult", -10, 24, 0.01f, offsetof(MatrixRowParams, mul), DISPLAY_TYPE_FLOAT, 3401),
    PARAM_ENUM_ORDERED("Dest1", DESTINATION_MAX - 1, offsetof(MatrixRowParams, dest1), DESTINATION_MAX, matrixDestNames, matrixTargetOrder,
                       matrixTargetPosition),
    PARAM_ENUM_ORDERED("Dest2", DESTINATION_MAX - 1, offsetof(MatrixRowParams, dest2), DESTINATION_MAX, matrixDestNames, matrixTargetOrder,
                       matrixTargetPosition),
    /* Performance */
    PARAM_FLOAT("p1", -1, 1, 0.01f, offsetof(PerformanceRowParams, perf1), DISPLAY_TYPE_FLOAT, 201),
    PARAM_FLOAT("p2", -1, 1, 0.01f, offsetof(PerformanceRowParams, perf2), DISPLAY_TYPE_FLOAT, 201),
    PARAM_FLOAT("p3", -1, 1, 0.01f, offsetof(PerformanceRowParams, perf3), DISPLAY_TYPE_FLOAT, 201),
    PARAM_FLOAT("p4", -1, 1, 0.01f, offsetof(PerformanceRowParams, perf4), DISPLAY_TYPE_FLOAT, 201),
    /* LfoOsc: 3 x 4 */
    PARAM_ENUM("Shap", LFO_TYPE_MAX - 1, offsetof(LfoParams, shape), LFO_TYPE_MAX, lfoShapeNames),
    PARAM_FLOAT("Freq", 0, 99.9f, 0.1f, offsetof(LfoParams, freq), DISPLAY_TYPE_FLOAT_LFO_FREQUENCY, LFO_FREQ_MAX_TIMES_10 + 1 + 9),
    PARAM_FLOAT("Bias", -1, 1, 0.01f, offsetof(LfoParams, bias), DISPLAY_TYPE_FLOAT, 201),
    PARAM_FLOAT("KSyn", -0.01f, 16, 0.01f, offsetof(LfoParams, keybRamp), DISPLAY_TYPE_LFO_KSYN, 1602),
    /* LfoPhase */
    PARAM_FLOAT("Phase", 0, 1, 0.01f, offsetof(LfoPhaseRowParams, phaseLfo1), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Phase", 0, 1, 0.01f, offsetof(LfoPhaseRowParams, phaseLfo2), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Phase", 0, 1, 0.01f, offsetof(LfoPhaseRowParams, phaseLfo3), DISPLAY_TYPE_FLOAT, 101),
    /* LfoEnv1 */
    PARAM_FLOAT("Attk", 0, 16, 0.01f, offsetof(EnvelopeLfoParams, attack), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_FLOAT("Deca", 0, 16, 0.01f, offsetof(EnvelopeLfoParams, decay), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_FLOAT("Sust", 0, 1, 0.01f, offsetof(EnvelopeLfoParams, sustain), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Rele", 0, 16, 0.01f, offsetof(EnvelopeLfoParams, release), DISPLAY_TYPE_FLOAT, 1601),
    /* LfoEnv2 */
    PARAM_FLOAT("Sile", 0, 16, 0.01f, offsetof(Envelope2LfoParams, silence), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_FLOAT("Attk", 0, 16, 0.01f, offsetof(Envelope2LfoParams, attack), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_FLOAT("Deca", 0, 16, 0.01f, offsetof(Envelope2LfoParams, decay), DISPLAY_TYPE_FLOAT, 1601),
    PARAM_ENUM("Loop", LFO_ENV2_LOOP_ATTACK, offsetof(Envelope2LfoParams, loop), 3, lofEnv2Loop),
    /* LfoSeq: 2 x 2 */
    PARAM_STEP_BPM("Bpm", 10, 245, 1.0f, offsetof(StepSequencerParams, bpm), 236),
    PARAM_FLOAT("Gate", 0, 1, 0.01f, offsetof(StepSequencerParams, gate), DISPLAY_TYPE_FLOAT, 101),
    /* MidiCurve: 2 x 3 */
    PARAM_ENUM("Befo", MIDI_NOTE_CURVE_MAX - 1, offsetof(MidiNoteCurveRowParams, curveBefore), MIDI_NOTE_CURVE_MAX, midiNoteCurves),
    PARAM_INTEGER("Brk", 0, 127, 1.0f, offsetof(MidiNoteCurveRowParams, breakNote), 128),
    PARAM_ENUM("Afte", MIDI_NOTE_CURVE_MAX - 1, offsetof(MidiNoteCurveRowParams, curveAfter), MIDI_NOTE_CURVE_MAX, midiNoteCurves),
    /* StepSeq: 2 x 16 */
    PARAM_STEP("Step", 0),
    PARAM_STEP("Step", 1),
    PARAM_STEP("Step", 2),
    PARAM_STEP("Step", 3),
    PARAM_STEP("Step", 4),
    PARAM_STEP("Step", 5),
    PARAM_STEP("Step", 6),
    PARAM_STEP("Step", 7),
    PARAM_STEP("Step", 8),
    PARAM_STEP("Step", 9),
    PARAM_STEP("Step", 10),
    PARAM_STEP("Step", 11),
    PARAM_STEP("Step", 12),
    PARAM_STEP("Step", 13),
    PARAM_STEP("Step", 14),
    PARAM_STEP("Step", 15),
};

const ParamFullDescription pfm3CommonParams[29] __attribute__((section(".rodata"))) = {
    /* Arp1 */
    PARAM_ENUM("Clock", 2, offsetof(EngineArp1, clock), 3, clockName),
    PARAM_INTEGER("BPM", 10, 240, 1.0f, offsetof(EngineArp1, BPM), 231),
    PARAM_ENUM("Dire", 11, offsetof(EngineArp1, direction), 12, dirName),
    PARAM_INTEGER("Octav", 1, 3, 1.0f, offsetof(EngineArp1, octave), 3),
    /* Arp2 */
    PARAM_ENUM("Ptrn", ARPEGGIATOR_PATTERN_COUNT - 1, offsetof(EngineArp2, pattern), ARPEGGIATOR_PATTERN_COUNT, patternName),
    PARAM_ENUM("Divi", 16, offsetof(EngineArp2, division), 17, divNames),
    PARAM_ENUM("Dura", 16, offsetof(EngineArp2, duration), 17, divNames),
    PARAM_ENUM("Latc", 1, offsetof(EngineArp2, latche), 2, activeName),
    /* ArpPattern: 4 x 1 */
    PARAM_ARP_PATTERN("", 0),
    /* Effect1 */
    PARAM_ENUM_ORDERED("Type", FILTER_LAST - 1, offsetof(EffectRowParams, type), FILTER_LAST, fxName, filtersOrder, filtersPosition),
    PARAM_FLOAT("", 0, 1, 0.01f, offsetof(EffectRowParams, param1), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("", 0, 1, 0.01f, offsetof(EffectRowParams, param2), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Gain", 0, 2, 0.01f, offsetof(EffectRowParams, param3), DISPLAY_TYPE_FLOAT, 201),
    /* Effect2 */
    PARAM_ENUM("Type", FILTER2_LAST - 1, offsetof(EffectRowParams, type), FILTER2_LAST, fx2Name),
    PARAM_FLOAT("", 0, 1, 0.01f, offsetof(EffectRowParams, param1), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("", 0, 1, 0.01f, offsetof(EffectRowParams, param2), DISPLAY_TYPE_FLOAT, 101),
    PARAM_FLOAT("Gain", 0, 2, 0.01f, offsetof(EffectRowParams, param3), DISPLAY_TYPE_FLOAT, 201),
    /* PresetName: 12 chars */
    PARAM_CHAR("", 0),
    PARAM_CHAR("", 1),
    PARAM_CHAR("", 2),
    PARAM_CHAR("", 3),
    PARAM_CHAR("", 4),
    PARAM_CHAR("", 5),
    PARAM_CHAR("", 6),
    PARAM_CHAR("", 7),
    PARAM_CHAR("", 8),
    PARAM_CHAR("", 9),
    PARAM_CHAR("", 10),
    PARAM_CHAR("", 11),
};

static const ParamBlock engineBlocks[(int)Pfm3Block::Count] = {
    {0, offsetof(Pfm3TimbreParams, engine1), 0, 1, 4, 0, 0, (uint8_t)ParamStore::Engine},
    {4, offsetof(Pfm3TimbreParams, engine2), 216, 1, 4, 0, 0, (uint8_t)ParamStore::Engine},
    {8, offsetof(Pfm3TimbreParams, engineIm1), 4, 6, 2, 8, 2, (uint8_t)ParamStore::Engine},
    {10, offsetof(Pfm3TimbreParams, engineMix1), 16, 6, 2, 8, 2, (uint8_t)ParamStore::Engine},
    {12, offsetof(Pfm3TimbreParams, osc1), 44, 6, 4, 16, 4, (uint8_t)ParamStore::Engine},
    {16, offsetof(Pfm3TimbreParams, env1Time), 68, 6, 4, 16, 4, (uint8_t)ParamStore::Engine},
    {20, offsetof(Pfm3TimbreParams, env1Level), 92, 6, 4, 16, 4, (uint8_t)ParamStore::Engine},
    {24, offsetof(Pfm3TimbreParams, env1Curve), 220, 6, 4, 16, 4, (uint8_t)ParamStore::Engine},
    {28, offsetof(Pfm3TimbreParams, matrixRowState1), 116, 12, 4, 16, 4, (uint8_t)ParamStore::Engine},
    {32, offsetof(Pfm3TimbreParams, performance1), 164, 1, 4, 0, 0, (uint8_t)ParamStore::Engine},
    {36, offsetof(Pfm3TimbreParams, lfoOsc1), 168, 3, 4, 16, 4, (uint8_t)ParamStore::Engine},
    {40, offsetof(Pfm3TimbreParams, lfoPhases), 180, 1, 3, 0, 0, (uint8_t)ParamStore::Engine},
    {43, offsetof(Pfm3TimbreParams, lfoEnv1), 184, 1, 4, 0, 0, (uint8_t)ParamStore::Engine},
    {47, offsetof(Pfm3TimbreParams, lfoEnv2), 188, 1, 4, 0, 0, (uint8_t)ParamStore::Engine},
    {51, offsetof(Pfm3TimbreParams, lfoSeq1), 192, 2, 2, 16, 4, (uint8_t)ParamStore::Engine},
    {53, offsetof(Pfm3TimbreParams, midiNote1Curve), 200, 2, 3, 16, 4, (uint8_t)ParamStore::Engine},
    {56, offsetof(Pfm3TimbreParams, lfoSteps1), 0xFFFF, 2, 16, 16, 0, (uint8_t)ParamStore::Engine},
};

static const ParamBlock commonBlocks[(int)CommonBlock::Count] = {
    {0, offsetof(TimbreSharedParams, engineArp1), 28, 1, 4, 0, 0, (uint8_t)ParamStore::Shared},
    {4, offsetof(TimbreSharedParams, engineArp2), 32, 1, 4, 0, 0, (uint8_t)ParamStore::Shared},
    {8, offsetof(TimbreSharedParams, engineArpUserPatterns), 0xFFFF, 4, 1, 4, 0, (uint8_t)ParamStore::Shared},
    {9, offsetof(TimbreSharedParams, effect1), 40, 1, 4, 0, 0, (uint8_t)ParamStore::Shared},
    {13, offsetof(TimbreSharedParams, effect2), 244, 1, 4, 0, 0, (uint8_t)ParamStore::Shared},
    {17, offsetof(TimbreSharedParams, presetName), 0xFFFF, 1, 12, 0, 0, (uint8_t)ParamStore::Shared},
};

static const CcEntry ccEntries[] = {
    {CC_GLIDE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Engine1, 0, 3), MidiTransform::GlideSpeed},
    {CC_PLAY_MODE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Engine1, 0, 2), MidiTransform::PlayMode},
    {CC_UNISON_DETUNE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Engine2, 0, 2), MidiTransform::UnisonDetune},
    {CC_UNISON_SPREAD, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Engine2, 0, 1), MidiTransform::Float127},
    {CC_ALGO, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Engine1, 0, 0), MidiTransform::Raw},
    {CC_IM1, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Mod, 0, 0), MidiTransform::ImTimes10},
    {CC_IM2, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Mod, 1, 0), MidiTransform::ImTimes10},
    {CC_IM3, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Mod, 2, 0), MidiTransform::ImTimes10},
    {CC_IM4, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Mod, 3, 0), MidiTransform::ImTimes10},
    {CC_IM5, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Mod, 4, 0), MidiTransform::ImTimes10},
    {CC_IM_FEEDBACK, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Mod, 5, 0), MidiTransform::Float127},
    {CC_MIX1, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 0, 0), MidiTransform::Float127},
    {CC_PAN1, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 0, 1), MidiTransform::Bipolar},
    {CC_MIX2, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 1, 0), MidiTransform::Float127},
    {CC_PAN2, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 1, 1), MidiTransform::Bipolar},
    {CC_MIX3, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 2, 0), MidiTransform::Float127},
    {CC_PAN3, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 2, 1), MidiTransform::Bipolar},
    {CC_MIX4, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 3, 0), MidiTransform::Float127},
    {CC_PAN4, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 3, 1), MidiTransform::Bipolar},
    {CC_MIX5, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 4, 0), MidiTransform::Float127},
    {CC_PAN5, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 4, 1), MidiTransform::Bipolar},
    {CC_MIX6, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 5, 0), MidiTransform::Float127},
    {CC_PAN6, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::OscMix, 5, 1), MidiTransform::Bipolar},
    {CC_MATRIXROW1_MUL, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Matrix, 0, 1), MidiTransform::MatrixMul},
    {CC_MATRIXROW2_MUL, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Matrix, 1, 1), MidiTransform::MatrixMul},
    {CC_MATRIXROW3_MUL, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Matrix, 2, 1), MidiTransform::MatrixMul},
    {CC_MATRIXROW4_MUL, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Matrix, 3, 1), MidiTransform::MatrixMul},
    {CC_OSC1_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Osc, 0, 2), MidiTransform::OscFrequency},
    {CC_OSC2_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Osc, 1, 2), MidiTransform::OscFrequency},
    {CC_OSC3_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Osc, 2, 2), MidiTransform::OscFrequency},
    {CC_OSC4_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Osc, 3, 2), MidiTransform::OscFrequency},
    {CC_OSC5_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Osc, 4, 2), MidiTransform::OscFrequency},
    {CC_OSC6_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Osc, 5, 2), MidiTransform::OscFrequency},
    {CC_LFO1_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 0, 1), MidiTransform::LfoFrequency},
    {CC_LFO2_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 1, 1), MidiTransform::LfoFrequency},
    {CC_LFO3_FREQ, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 2, 1), MidiTransform::LfoFrequency},
    {CC_LFO_ENV2_SILENCE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoEnv2, 0, 0), MidiTransform::Linear8},
    {CC_STEPSEQ5_GATE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoSeq, 0, 1), MidiTransform::Percent},
    {CC_STEPSEQ6_GATE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoSeq, 1, 1), MidiTransform::Percent},
    {CC_ENV_ATK_ALL_MODULATOR, CcKind::EnvAllAttackModulator, PFM3_PARAM_NONE, MidiTransform::EnvAttack},
    {CC_ENV_REL_ALL_MODULATOR, CcKind::EnvAllReleaseModulator, PFM3_PARAM_NONE, MidiTransform::EnvRelease},
    {CC_ENV_ATK_ALL_CARRIER, CcKind::EnvAllAttackCarrier, PFM3_PARAM_NONE, MidiTransform::EnvAttack},
    {CC_ENV_REL_ALL_CARRIER, CcKind::EnvAllReleaseCarrier, PFM3_PARAM_NONE, MidiTransform::EnvRelease},
    {CC_ENV_ATK_OP1, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 0, 0), MidiTransform::EnvAttack},
    {CC_ENV_ATK_OP2, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 1, 0), MidiTransform::EnvAttack},
    {CC_ENV_ATK_OP3, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 2, 0), MidiTransform::EnvAttack},
    {CC_ENV_ATK_OP4, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 3, 0), MidiTransform::EnvAttack},
    {CC_ENV_ATK_OP5, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 4, 0), MidiTransform::EnvAttack},
    {CC_ENV_ATK_OP6, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 5, 0), MidiTransform::EnvAttack},
    {CC_ENV_REL_OP1, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 0, 3), MidiTransform::EnvRelease},
    {CC_ENV_REL_OP2, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 1, 3), MidiTransform::EnvRelease},
    {CC_ENV_REL_OP3, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 2, 3), MidiTransform::EnvRelease},
    {CC_ENV_REL_OP4, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 3, 3), MidiTransform::EnvRelease},
    {CC_ENV_REL_OP5, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 4, 3), MidiTransform::EnvRelease},
    {CC_ENV_REL_OP6, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::EnvTime, 5, 3), MidiTransform::EnvRelease},
    {CC_LFO1_PHASE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoPhase, 0, 0), MidiTransform::Percent},
    {CC_LFO2_PHASE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoPhase, 0, 1), MidiTransform::Percent},
    {CC_LFO3_PHASE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoPhase, 0, 2), MidiTransform::Percent},
    {CC_LFO1_BIAS, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 0, 2), MidiTransform::Percent},
    {CC_LFO2_BIAS, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 1, 2), MidiTransform::Percent},
    {CC_LFO3_BIAS, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 2, 2), MidiTransform::Percent},
    {CC_LFO1_SHAPE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 0, 0), MidiTransform::Raw},
    {CC_LFO2_SHAPE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 1, 0), MidiTransform::Raw},
    {CC_LFO3_SHAPE, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::LfoOsc, 2, 0), MidiTransform::Raw},
    {CC_MATRIX_SOURCE_CC1, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Performance, 0, 0), MidiTransform::Bipolar64},
    {CC_MATRIX_SOURCE_CC2, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Performance, 0, 1), MidiTransform::Bipolar64},
    {CC_MATRIX_SOURCE_CC3, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Performance, 0, 2), MidiTransform::Bipolar64},
    {CC_MATRIX_SOURCE_CC4, CcKind::Engine, (uint16_t)pfm3Param(Pfm3Block::Performance, 0, 3), MidiTransform::Bipolar64},
};

static uint8_t ccToEntry[128];

static uint16_t flatToEngineParam[260];
static uint16_t flatToCommonParam[260];

static bool tablesReady = false;

/* Readable at runtime (gdb) to confirm Phase 1 table validation. */
volatile bool gPfm3ParamTablesValid = false;

const ParamBlock& pfm3Block(Pfm3Param id) {
    return engineBlocks[pfm3ParamBlockOf(id)];
}

const ParamFullDescription& pfm3FullDesc(Pfm3Param id) {
    const ParamBlock& b = pfm3Block(id);
    return pfm3EngineParams[b.descBase + pfm3ParamFieldOf(id)];
}

const ParamDesc& pfm3Desc(Pfm3Param id) {
    return pfm3FullDesc(id).desc;
}

uint16_t pfm3DescIndex(Pfm3Param id) {
    const ParamBlock& b = pfm3Block(id);
    return (uint16_t)(b.descBase + pfm3ParamFieldOf(id));
}

uint16_t pfm3Offset(Pfm3Param id) {
    const ParamBlock& b = pfm3Block(id);
    return (uint16_t)(b.liveBase + pfm3ParamElementOf(id) * b.liveStride + pfm3Desc(id).fieldOffset);
}

uint16_t pfm3FlatIndex(Pfm3Param id) {
    const ParamBlock& b = pfm3Block(id);
    if (b.flatBase == 0xFFFF) {
        return 0xFFFF;
    }
    return (uint16_t)(b.flatBase + pfm3ParamElementOf(id) * b.flatStride + pfm3ParamFieldOf(id));
}

float pfm3GetParam(const Pfm3TimbreParams& params, Pfm3Param id) {
    const ParamDesc& d = pfm3Desc(id);
    const uint8_t* p = (const uint8_t*)&params + pfm3Offset(id);
    switch (d.access) {
    case ParamAccess::Float:
        return *(const float*)p;
    case ParamAccess::U8:
    case ParamAccess::Char:
        return (float)*p;
    case ParamAccess::U16:
        return (float)*(const uint16_t*)p;
    case ParamAccess::Bitfield:
        return (float)(*(const uint32_t*)p & 0xFFFF);
    default:
        return 0.0f;
    }
}

void pfm3SetParam(Pfm3TimbreParams& params, Pfm3Param id, float value) {
    const ParamDesc& d = pfm3Desc(id);
    uint8_t* p = (uint8_t*)&params + pfm3Offset(id);
    switch (d.access) {
    case ParamAccess::Float: {
        if (value < d.min) {
            value = d.min;
        }
        if (value > d.max) {
            value = d.max;
        }
        *(float*)p = value;
        break;
    }
    case ParamAccess::U8:
    case ParamAccess::Char:
        *p = (uint8_t)value;
        break;
    case ParamAccess::U16:
        *(uint16_t*)p = (uint16_t)value;
        break;
    case ParamAccess::Bitfield:
        *(uint32_t*)p = (uint32_t)value;
        break;
    default:
        break;
    }
}

const ParamBlock& commonBlock(CommonParamId id) {
    return commonBlocks[commonBlockOf(id)];
}

const ParamFullDescription& commonFullDesc(CommonParamId id) {
    const ParamBlock& b = commonBlock(id);
    return pfm3CommonParams[b.descBase + commonFieldOf(id)];
}

const ParamDesc& commonDesc(CommonParamId id) {
    return commonFullDesc(id).desc;
}

uint16_t commonDescIndex(CommonParamId id) {
    const ParamBlock& b = commonBlock(id);
    return (uint16_t)(b.descBase + commonFieldOf(id));
}

uint16_t commonOffset(CommonParamId id) {
    const ParamBlock& b = commonBlock(id);
    return (uint16_t)(b.liveBase + commonElementOf(id) * b.liveStride + commonDesc(id).fieldOffset);
}

uint16_t commonFlatIndex(CommonParamId id) {
    const ParamBlock& b = commonBlock(id);
    if (b.flatBase == 0xFFFF) {
        return 0xFFFF;
    }
    return (uint16_t)(b.flatBase + commonElementOf(id) * b.flatStride + commonFieldOf(id));
}

float commonGetParam(const TimbreSharedParams& shared, CommonParamId id) {
    const ParamDesc& d = commonDesc(id);
    const uint8_t* p = (const uint8_t*)&shared + commonOffset(id);
    switch (d.access) {
    case ParamAccess::Float:
        return *(const float*)p;
    case ParamAccess::U8:
    case ParamAccess::Char:
        return (float)*p;
    case ParamAccess::U16:
        return (float)*(const uint16_t*)p;
    case ParamAccess::Bitfield:
        return (float)(*(const uint32_t*)p & 0xFFFF);
    default:
        return 0.0f;
    }
}

void commonSetParam(TimbreSharedParams& shared, CommonParamId id, float value) {
    const ParamDesc& d = commonDesc(id);
    uint8_t* p = (uint8_t*)&shared + commonOffset(id);
    switch (d.access) {
    case ParamAccess::Float: {
        if (value < d.min) {
            value = d.min;
        }
        if (value > d.max) {
            value = d.max;
        }
        *(float*)p = value;
        break;
    }
    case ParamAccess::U8:
    case ParamAccess::Char:
        *p = (uint8_t)value;
        break;
    case ParamAccess::U16:
        *(uint16_t*)p = (uint16_t)value;
        break;
    case ParamAccess::Bitfield:
        *(uint32_t*)p = (uint32_t)value;
        break;
    default:
        break;
    }
}

Pfm3Param pfm3FlatToParam(uint16_t flat) {
    pfm3InitTables();
    if (flat >= 260) {
        return (Pfm3Param)PFM3_PARAM_NONE;
    }
    return (Pfm3Param)flatToEngineParam[flat];
}

CommonParamId commonFlatToParam(uint16_t flat) {
    pfm3InitTables();
    if (flat >= 260) {
        return COMMON_PARAM_NONE;
    }
    return (CommonParamId)flatToCommonParam[flat];
}

const CcEntry* pfm3CcEntry(uint8_t cc) {
    if (cc >= 128 || ccToEntry[cc] == 0xFF) {
        return nullptr;
    }
    return &ccEntries[ccToEntry[cc]];
}

float pfm3ApplyMidiTransform(MidiTransform transform, uint8_t value) {
    switch (transform) {
    case MidiTransform::Float127:
        return (float)value * (1.0f / 127.0f);
    case MidiTransform::Bipolar:
        return (float)value * (2.0f / 127.0f) - 1.0f;
    case MidiTransform::ImTimes10:
        return (float)value * 0.1f;
    case MidiTransform::OscFrequency:
        return (float)value * (1.0f / 12.0f);
    case MidiTransform::MatrixMul:
        return (float)value * 0.2f - 10.0f;
    case MidiTransform::LfoFrequency:
        return (float)value * 0.2f;
    case MidiTransform::EnvAttack:
        return (float)value * (1.0f / 64.0f);
    case MidiTransform::EnvRelease:
        return (float)value * (1.0f / 32.0f);
    case MidiTransform::Percent:
        return (float)value * 0.01f;
    case MidiTransform::Linear8:
        return (float)value * 0.125f;
    case MidiTransform::Bipolar64:
        return ((float)value - 64.0f) * (2.0f / 127.0f);
    case MidiTransform::UnisonDetune:
        return (float)value * (1.0f / 64.0f) - 1.0f;
    case MidiTransform::PlayMode:
        return (value < 3) ? (float)value : (float)((value * 3) / 128);
    case MidiTransform::GlideSpeed:
        return (float)value * (12.0f / 127.0f);
    case MidiTransform::Raw:
    default:
        return (float)value;
    }
}

void pfm3InitTables() {
    if (tablesReady) {
        return;
    }
    tablesReady = true;

    for (int i = 0; i < 260; i++) {
        flatToEngineParam[i] = PFM3_PARAM_NONE;
        flatToCommonParam[i] = PFM3_PARAM_NONE;
    }
    for (int b = 0; b < (int)Pfm3Block::Count; b++) {
        const ParamBlock& block = engineBlocks[b];
        for (int element = 0; element < block.count; element++) {
            for (int field = 0; field < block.fieldCount; field++) {
                Pfm3Param id = pfm3Param((Pfm3Block)b, (uint8_t)element, (uint8_t)field);
                uint16_t flat = pfm3FlatIndex(id);
                if (flat != 0xFFFF && flat < 260) {
                    flatToEngineParam[flat] = (uint16_t)id;
                }
            }
        }
    }
    for (int b = 0; b < (int)CommonBlock::Count; b++) {
        const ParamBlock& block = commonBlocks[b];
        for (int element = 0; element < block.count; element++) {
            for (int field = 0; field < block.fieldCount; field++) {
                CommonParamId id = commonParam((CommonBlock)b, (uint8_t)element, (uint8_t)field);
                uint16_t flat = commonFlatIndex(id);
                if (flat != 0xFFFF && flat < 260) {
                    flatToCommonParam[flat] = (uint16_t)id;
                }
            }
        }
    }
    for (int i = 0; i < 128; i++) {
        ccToEntry[i] = 0xFF;
    }
    for (unsigned i = 0; i < sizeof(ccEntries) / sizeof(ccEntries[0]); i++) {
        ccToEntry[ccEntries[i].cc] = (uint8_t)i;
    }

    int position = 0;
    for (int i = 0; i < DESTINATION_MAX; i++) {
        matrixTargetPosition[matrixTargetOrder[i]] = (unsigned char)position++;
    }
    position = 0;
    for (int i = 0; i < MATRIX_SOURCE_MAX; i++) {
        matrixSourcePosition[matrixSourceOrder[i]] = (unsigned char)position++;
    }
}

static bool accessSizeOk(ParamAccess access, uint16_t offset, uint16_t limit) {
    uint16_t size = 4;
    switch (access) {
    case ParamAccess::U8:
    case ParamAccess::Char:
        size = 1;
        break;
    case ParamAccess::U16:
        size = 2;
        break;
    case ParamAccess::Float:
    case ParamAccess::Bitfield:
    case ParamAccess::Custom:
    default:
        size = 4;
        break;
    }
    return (uint32_t)offset + size <= limit;
}

bool pfm3ValidateTables() {
    pfm3InitTables();

    if (sizeof(Pfm3Preset) != 1008) {
        return false;
    }

    const uint16_t engineDescCount = (uint16_t)(sizeof(pfm3EngineParams) / sizeof(pfm3EngineParams[0]));
    const uint16_t commonDescCount = (uint16_t)(sizeof(pfm3CommonParams) / sizeof(pfm3CommonParams[0]));

    uint16_t expected = 0;
    for (int b = 0; b < (int)Pfm3Block::Count; b++) {
        const ParamBlock& block = engineBlocks[b];
        if (block.descBase != expected) {
            return false;
        }
        expected = (uint16_t)(expected + block.fieldCount);
    }
    if (expected != engineDescCount) {
        return false;
    }

    expected = 0;
    for (int b = 0; b < (int)CommonBlock::Count; b++) {
        const ParamBlock& block = commonBlocks[b];
        if (block.descBase != expected) {
            return false;
        }
        expected = (uint16_t)(expected + block.fieldCount);
    }
    if (expected != commonDescCount) {
        return false;
    }

    for (int b = 0; b < (int)Pfm3Block::Count; b++) {
        const ParamBlock& block = engineBlocks[b];
        for (int element = 0; element < block.count; element++) {
            for (int field = 0; field < block.fieldCount; field++) {
                Pfm3Param id = pfm3Param((Pfm3Block)b, (uint8_t)element, (uint8_t)field);
                const ParamDesc& d = pfm3Desc(id);
                if (pfm3DescIndex(id) >= engineDescCount) {
                    return false;
                }
                if (!accessSizeOk(d.access, pfm3Offset(id), sizeof(Pfm3TimbreParams))) {
                    return false;
                }
            }
        }
    }

    for (int b = 0; b < (int)CommonBlock::Count; b++) {
        const ParamBlock& block = commonBlocks[b];
        for (int element = 0; element < block.count; element++) {
            for (int field = 0; field < block.fieldCount; field++) {
                CommonParamId id = commonParam((CommonBlock)b, (uint8_t)element, (uint8_t)field);
                const ParamDesc& d = commonDesc(id);
                if (commonDescIndex(id) >= commonDescCount) {
                    return false;
                }
                if (!accessSizeOk(d.access, commonOffset(id), sizeof(TimbreSharedParams))) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool pfm3ValidateDefaults(const Pfm3TimbreParams& params, const TimbreSharedParams& shared) {
    const float* flat = (const float*)&defaultPreset;

    for (int b = 0; b < (int)Pfm3Block::Count; b++) {
        const ParamBlock& block = engineBlocks[b];
        for (int element = 0; element < block.count; element++) {
            for (int field = 0; field < block.fieldCount; field++) {
                Pfm3Param id = pfm3Param((Pfm3Block)b, (uint8_t)element, (uint8_t)field);
                const ParamDesc& d = pfm3Desc(id);
                if (d.access != ParamAccess::Float) {
                    continue;
                }
                uint16_t flatIndex = pfm3FlatIndex(id);
                if (flatIndex == 0xFFFF) {
                    continue;
                }
                if (fabsf(pfm3GetParam(params, id) - flat[flatIndex]) > 0.0001f) {
                    return false;
                }
            }
        }
    }

    for (int b = 0; b < (int)CommonBlock::Count; b++) {
        const ParamBlock& block = commonBlocks[b];
        for (int element = 0; element < block.count; element++) {
            for (int field = 0; field < block.fieldCount; field++) {
                CommonParamId id = commonParam((CommonBlock)b, (uint8_t)element, (uint8_t)field);
                const ParamDesc& d = commonDesc(id);
                if (d.access != ParamAccess::Float) {
                    continue;
                }
                uint16_t flatIndex = commonFlatIndex(id);
                if (flatIndex == 0xFFFF) {
                    continue;
                }
                if (fabsf(commonGetParam(shared, id) - flat[flatIndex]) > 0.0001f) {
                    return false;
                }
            }
        }
    }

    return true;
}
