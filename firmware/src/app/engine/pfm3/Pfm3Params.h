/*
 * Copyright 2026 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_PARAMS_H_
#define PFM3_PARAMS_H_

#include "Common.h"

struct Engine1Params {
    float algo;
    float velocity;
    float playMode;
    float glideSpeed;
};

enum { GLIDE_TYPE_OFF = 0, GLIDE_TYPE_OVERLAP, GLIDE_TYPE_ALWAYS };

struct Engine2Params {
    float glideType;
    float unisonSpread;
    float unisonDetune;
    float pfm3Version;
};

//{ "Acti", "Clk ", "BPM ", "Dire" },
//{ "Ptrn", "Divi", "Dura", "Latc" },

struct EngineArp1 {
    float clock;
    float BPM;
    float direction;
    float octave;
};

struct EngineArp2 {
    float pattern;
    float division;
    float duration;
    float latche;
};

/* low 16 bits = pattern mask, high reserved for now */
typedef uint32_t arp_pattern_t;
#define ARP_PATTERN_GETMASK(x) (uint16_t)((x) & 0xffff)
#define ARP_PATTERN_SETMASK(p, m)                                                                                                          \
    do {                                                                                                                                   \
        (p) = ((p) & ~0xffff) | ((m) & 0xffff);                                                                                            \
    } while (0)
#define ARP_PATTERN(x)           ((uint32_t)(x) & 0xffff)
#define ARP_MASK_GETNIBBLE(m, i) (((m) >> (i * 4)) & 0xf)
#define ARP_MASK_SETNIBBLE(m, i, v)                                                                                                        \
    do {                                                                                                                                   \
        mask &= ~(0xf << (i * 4));                                                                                                         \
        mask |= (v << (i * 4));                                                                                                            \
    } while (0)

struct EngineArpUserPatterns {
    arp_pattern_t patterns[4];
};

struct EngineIm1 {
    float modulationIndex1;
    float modulationIndexVelo1;
    float modulationIndex2;
    float modulationIndexVelo2;
};

struct EngineIm2 {
    float modulationIndex3;
    float modulationIndexVelo3;
    float modulationIndex4;
    float modulationIndexVelo4;
};

struct EngineIm3 {
    float modulationIndex5;
    float modulationIndexVelo5;
    float modulationIndex6;
    float modulationIndexVelo6;
};

struct FlashEngineIm1 {
    float modulationIndex1;
    float modulationIndex2;
    float modulationIndex3;
    float modulationIndex4;
};

struct FlashEngineIm2 {
    float modulationIndex5;
    float modulationIndex6;
    float notUsed1;
    float notUsed2;
};

struct FlashEngineVeloIm1 {
    float modulationIndexVelo1;
    float modulationIndexVelo2;
    float modulationIndexVelo3;
    float modulationIndexVelo4;
};

struct FlashEngineVeloIm2 {
    float modulationIndexVelo5;
    float modulationIndexVelo6;
    float notUsed1;
    float notUsed2;
};

struct EngineMix1 {
    float mixOsc1;
    float panOsc1;
    float mixOsc2;
    float panOsc2;
};

struct EngineMix2 {
    float mixOsc3;
    float panOsc3;
    float mixOsc4;
    float panOsc4;
};

struct EngineMix3 {
    float mixOsc5;
    float panOsc5;
    float mixOsc6;
    float panOsc6;
};

struct EnvelopeParamsA {
    float attackTime;
    float attackLevel;
    float decayTime;
    float decayLevel;
};

struct EnvelopeParamsB {
    float sustainTime;
    float sustainLevel;
    float releaseTime;
    float releaseLevel;
};

struct EnvelopeTimeMemory {
    float attackTime;
    float decayTime;
    float sustainTime;
    float releaseTime;
};

struct EnvelopeLevelMemory {
    float attackLevel;
    float decayLevel;
    float sustainLevel;
    float releaseLevel;
};

struct EnvelopeCurveParams {
    float attackCurve;
    float decayCurve;
    float sustainCurve;
    float releaseCurve;
};

struct EnvelopeCurveFlashMemory1 {
    uint8_t curveAttack1;
    uint8_t curveDecay1;
    uint8_t curveSustain1;
    uint8_t curveRelease1;

    uint8_t curveAttack2;
    uint8_t curveDecay2;
    uint8_t curveSustain2;
    uint8_t curveRelease2;

    uint8_t curveAttack3;
    uint8_t curveDecay3;
    uint8_t curveSustain3;
    uint8_t curveRelease3;

    uint8_t curveAttack4;
    uint8_t curveDecay4;
    uint8_t curveSustain4;
    uint8_t curveRelease4;
};

struct EnvelopeCurveFlashMemory2 {
    uint8_t curveAttack5;
    uint8_t curveDecay5;
    uint8_t curveSustain5;
    uint8_t curveRelease5;

    uint8_t curveAttack6;
    uint8_t curveDecay6;
    uint8_t curveSustain6;
    uint8_t curveRelease6;
};

struct EnvelopeLfoParams {
    float attack;
    float decay;
    float sustain;
    float release;
};

struct Envelope2LfoParams {
    float silence;
    float attack;
    float decay;
    float loop;
};

struct OscillatorParams {
    float shape;         // OSC_SHAPE_*
    float frequencyType; // OSC_FT_*
    float frequencyMul;
    float detune;
};

struct MatrixRowParams {
    float source;
    float mul;
    float dest1;
    float dest2;
};

struct LfoParams {
    float shape; // LFO_SHAPE_*
    float freq;  // lfoFreq[]*
    float bias;
    float keybRamp;
};

struct StepSequencerParams {
    float bpm;
    float gate;
    float unused1;
    float unused2;
};

struct StepSequencerSteps {
    char steps[16];
};

struct EffectRowParams {
    float type;
    float param1;
    float param2;
    float param3;
};

struct PerformanceRowParams {
    float perf1;
    float perf2;
    float perf3;
    float perf4;
};

struct LfoPhaseRowParams {
    float phaseLfo1;
    float phaseLfo2;
    float phaseLfo3;
    float unused1;
};

struct MidiNoteCurveRowParams {
    float curveBefore;
    float breakNote;
    float curveAfter;
    float unused1;
};

#endif
