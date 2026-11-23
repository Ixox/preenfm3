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

#define PFM3_PATCH_VERSION 2.0f

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

/* low 16 bits = pattern mask, high 16 bits = accent mask */
typedef uint32_t arp_pattern_t;
#define ARP_PATTERN_GETMASK(x)   ((uint16_t)(((uint32_t)(x)) & 0xffffu))
#define ARP_PATTERN_GETACCENT(x) ((uint16_t)(((uint32_t)(x)) >> 16))
#define ARP_PATTERN_SETMASK(p, m)                                                                                                          \
    do {                                                                                                                                   \
        (p) = ((p) & ~0xffffu) | ((uint32_t)(m) & 0xffffu);                                                                                \
    } while (0)
#define ARP_PATTERN_SETACCENT(p, a)                                                                                                        \
    do {                                                                                                                                   \
        (p) = ((p) & 0xffffu) | (((uint32_t)(a) & 0xffffu) << 16);                                                                         \
    } while (0)
#define ARP_PATTERN(x)           ((uint32_t)(x) & 0xffffu)
#define ARP_PATTERN_ACCENT(m, a) (((uint32_t)(m) & 0xffffu) | (((uint32_t)(a) & 0xffffu) << 16))
#define ARP_MASK_GETNIBBLE(m, i) (((m) >> (i * 4)) & 0xf)
#define ARP_MASK_SETNIBBLE(m, i, v)                                                                                                        \
    do {                                                                                                                                   \
        mask &= ~(0xf << (i * 4));                                                                                                         \
        mask |= (v << (i * 4));                                                                                                            \
    } while (0)

struct EngineArpUserPatterns {
    arp_pattern_t patterns[4];
};

#define ARP_ADVANCED_MAGIC   0x41525041u // 'ARPA'
#define ARP_ADVANCED_VERSION 1u

struct ArpAdvancedParams {
    uint32_t magic;
    uint32_t version;
    uint32_t stepModes[6];        // 4 patterns x 16 steps x 3 bits
    uint32_t lengthsAndSeed;      // 4 x 4-bit user lengths, 4-bit seed, reserved
    uint32_t swingGrooveHumanize; // 8-bit swing, 8-bit groove, 8-bit humanize
};

#define ARP_ADVANCED_DEFAULT {ARP_ADVANCED_MAGIC, ARP_ADVANCED_VERSION, {0, 0, 0, 0, 0, 0}, 0, 0}

inline void arpAdvancedDefaults(ArpAdvancedParams& p) {
    p.magic = ARP_ADVANCED_MAGIC;
    p.version = ARP_ADVANCED_VERSION;
    for (int i = 0; i < 6; i++) {
        p.stepModes[i] = 0;
    }
    p.lengthsAndSeed = 0;
    p.swingGrooveHumanize = 0;
}

inline bool arpAdvancedIsValid(const ArpAdvancedParams& p) {
    return p.magic == ARP_ADVANCED_MAGIC;
}

inline int arpAdvancedGetStepMode(const ArpAdvancedParams& p, int pattern, int step) {
    const int bit = pattern * 48 + step * 3;
    return (p.stepModes[bit >> 5] >> (bit & 31)) & 0x7;
}

inline void arpAdvancedSetStepMode(ArpAdvancedParams& p, int pattern, int step, int mode) {
    const int bit = pattern * 48 + step * 3;
    const int shift = bit & 31;
    uint32_t& word = p.stepModes[bit >> 5];
    word = (word & ~(0x7u << shift)) | (((uint32_t)mode & 0x7u) << shift);
}

inline int arpAdvancedGetUserLength(const ArpAdvancedParams& p, int index) {
    const int len = (p.lengthsAndSeed >> (index * 4)) & 0xf;
    return len == 0 ? 16 : len;
}

inline void arpAdvancedSetUserLength(ArpAdvancedParams& p, int index, int length) {
    if (length < 1) {
        length = 1;
    } else if (length > 16) {
        length = 16;
    }
    const uint32_t stored = (length == 16) ? 0u : (uint32_t)length;
    const int shift = index * 4;
    p.lengthsAndSeed = (p.lengthsAndSeed & ~(0xfu << shift)) | (stored << shift);
}

inline int arpAdvancedGetEuclideanSeed(const ArpAdvancedParams& p) {
    return (p.lengthsAndSeed >> 16) & 0xf;
}

inline void arpAdvancedSetEuclideanSeed(ArpAdvancedParams& p, int seed) {
    p.lengthsAndSeed = (p.lengthsAndSeed & ~(0xfu << 16)) | (((uint32_t)seed & 0xfu) << 16);
}

inline int arpAdvancedGetSwing(const ArpAdvancedParams& p) {
    return p.swingGrooveHumanize & 0xff;
}

inline void arpAdvancedSetSwing(ArpAdvancedParams& p, int value) {
    if (value < 0) {
        value = 0;
    } else if (value > 50) {
        value = 50;
    }
    p.swingGrooveHumanize = (p.swingGrooveHumanize & ~0xffu) | ((uint32_t)value & 0xffu);
}

inline int arpAdvancedGetGroove(const ArpAdvancedParams& p) {
    return (p.swingGrooveHumanize >> 8) & 0xff;
}

inline void arpAdvancedSetGroove(ArpAdvancedParams& p, int value) {
    if (value < 0) {
        value = 0;
    } else if (value > 9) {
        value = 9;
    }
    p.swingGrooveHumanize = (p.swingGrooveHumanize & ~(0xffu << 8)) | (((uint32_t)value & 0xffu) << 8);
}

inline int arpAdvancedGetHumanize(const ArpAdvancedParams& p) {
    return (p.swingGrooveHumanize >> 16) & 0xff;
}

inline void arpAdvancedSetHumanize(ArpAdvancedParams& p, int value) {
    if (value < 0) {
        value = 0;
    } else if (value > 100) {
        value = 100;
    }
    p.swingGrooveHumanize = (p.swingGrooveHumanize & ~(0xffu << 16)) | (((uint32_t)value & 0xffu) << 16);
}

enum ArpStepMode {
    ARP_STEP_MODE_NORM = 0,
    ARP_STEP_MODE_TIE,
    ARP_STEP_MODE_OCT_UP,
    ARP_STEP_MODE_RATCHET2,
    ARP_STEP_MODE_CHORD,
    ARP_STEP_MODE_SLAP,
    ARP_STEP_MODE_DYAD,
    ARP_STEP_MODE_OCT_DOWN,
    ARP_STEP_MODE_COUNT
};

#define ARP_SWING_GROOVE_COUNT 10

inline void arpSwingGrooveDef(int grooveIdx, int* len, uint8_t* mask) {
    static const uint8_t grooveLens[ARP_SWING_GROOVE_COUNT] = {8, 8, 8, 8, 8, 3, 6, 5, 7, 4};
    static const uint8_t grooveMasks[ARP_SWING_GROOVE_COUNT] = {
        0xAAu, // 0: Classic 8th/16th
        0x44u, // 1: Backbeat 2&4
        0x88u, // 2: Sparse offbeat
        0x62u, // 3: MPC broken
        0xACu, // 4: Dilla push
        0x06u, // 5: Triplet 3-step
        0x24u, // 6: Waltz 6/8
        0x14u, // 7: 5/8 swing
        0x54u, // 8: 7/8 groove
        0x0Cu  // 9: Sync 4/4
    };
    if (grooveIdx < 0 || grooveIdx >= ARP_SWING_GROOVE_COUNT) {
        grooveIdx = 0;
    }
    *len = (int)grooveLens[grooveIdx];
    *mask = grooveMasks[grooveIdx];
}

inline float arpSwingVelAttenCoeff(int grooveIdx) {
    int len = 8;
    uint8_t mask = 0xAAu;
    arpSwingGrooveDef(grooveIdx, &len, &mask);
    int swung = 0;
    for (int s = 0; s < len; s++) {
        if ((mask >> s) & 0x1u) {
            swung++;
        }
    }
    float ratio = (len > 0) ? ((float)swung / (float)len) : 0.5f;
    float coeff = 1.2f - 1.4f * ratio;
    if (coeff < 0.30f) {
        coeff = 0.30f;
    } else if (coeff > 0.95f) {
        coeff = 0.95f;
    }
    return coeff;
}

inline bool isArpSwingStep(int stepIdx, int grooveIdx) {
    int len = 8;
    uint8_t mask = 0xAAu;
    arpSwingGrooveDef(grooveIdx, &len, &mask);
    int pos = stepIdx % len;
    return ((mask >> pos) & 0x1u) != 0;
}

inline uint16_t arpPickEuclideanSteps(const uint8_t* active, int n, int offset, uint16_t usedMask, int target) {
    uint8_t cand[16];
    int count = 0;
    for (int i = 0; i < n; i++) {
        int s = active[(i + offset) % n];
        uint16_t bit = (uint16_t)(1u << s);
        if (!(usedMask & bit)) {
            cand[count++] = (uint8_t)s;
        }
    }
    if (count == 0) {
        return 0;
    }
    int stepPick = (count >= target * 2) ? (count / target) : 1;
    uint16_t out = 0;
    int picked = 0;
    for (int i = 0; i < count && picked < target; i += stepPick) {
        out |= (uint16_t)(1u << cand[i]);
        picked++;
    }
    return out;
}

inline void arpComputePresetMasks(uint16_t stepMask, uint16_t baseAccent, uint16_t baseTie, int seed, uint16_t* outAccent, uint16_t* outTie,
                                  uint16_t* outOctUp, uint16_t* outRatchet2, uint16_t* outOctDown, uint16_t* outDyad, uint16_t* outChord,
                                  uint16_t* outSlap) {
    *outOctUp = 0;
    *outRatchet2 = 0;
    if (outOctDown)
        *outOctDown = 0;
    if (outDyad)
        *outDyad = 0;
    if (outChord)
        *outChord = 0;
    if (outSlap)
        *outSlap = 0;
    if (seed <= 0) {
        *outAccent = 0;
        *outTie = 0;
        return;
    }
    if (seed == 1) {
        *outAccent = baseAccent;
        *outTie = baseTie;
        return;
    }
    int rawSeed = seed;
    seed -= 1;

    uint8_t active[16];
    int n = 0;
    int baseK = 0;
    int baseTies = 0;
    for (int s = 0; s < 16; s++) {
        uint16_t bit = (uint16_t)(1u << s);
        if (stepMask & bit) {
            active[n++] = (uint8_t)s;
            if (baseAccent & bit) {
                baseK++;
            }
            if (baseTie & bit) {
                baseTies++;
            }
        }
    }
    if (n == 0) {
        *outAccent = 0;
        *outTie = 0;
        return;
    }

    if (baseK < 1) {
        baseK = 1;
    }
    int k = baseK + ((seed / 4) % 3) - 1;
    if (k < 1) {
        k = 1;
    }
    if (n > 1 && k >= n) {
        k = n - 1;
    }

    int rot = seed % n;
    uint16_t accMask = 0;
    for (int i = 0; i < n; i++) {
        int pos = (i + n - rot) % n;
        if (((pos * k) % n) < k) {
            accMask |= (uint16_t)(1u << active[i]);
        }
    }

    uint16_t tieMask = 0;
    if (baseTies > 0) {
        uint8_t cand[16];
        int cCount = 0;
        for (int i = 0; i < n; i++) {
            int s = active[(i + rot) % n];
            uint16_t bit = (uint16_t)(1u << s);
            uint16_t nextBit = (uint16_t)(1u << ((s + 1) & 15));
            if ((accMask & bit) && !(stepMask & nextBit)) {
                cand[cCount++] = (uint8_t)s;
            }
        }
        if (cCount == 0) {
            for (int i = 0; i < n; i++) {
                int s = active[(i + rot) % n];
                uint16_t nextBit = (uint16_t)(1u << ((s + 1) & 15));
                if (!(stepMask & nextBit)) {
                    cand[cCount++] = (uint8_t)s;
                }
            }
        }
        if (cCount > 0) {
            int stepPick = cCount / baseTies;
            if (stepPick < 1) {
                stepPick = 1;
            }
            int picked = 0;
            for (int i = 0; i < cCount && picked < baseTies; i += stepPick) {
                tieMask |= (uint16_t)(1u << cand[i]);
                picked++;
            }
        }
    }

    *outAccent = accMask;
    *outTie = tieMask;

    uint16_t usedMask = tieMask;
    uint16_t octMask = 0;
    uint16_t octDownMask = 0;
    uint16_t rtMask = 0;
    uint16_t dyadMask = 0;
    uint16_t slapMask = 0;
    uint16_t chordMask = 0;

    if (rawSeed >= 8 && n > 2) {
        int target = (rawSeed >= 10 && n >= 6) ? 2 : 1;
        octMask = arpPickEuclideanSteps(active, n, (rot + 1) % n, usedMask, target);
        usedMask |= octMask;
    }
    if (rawSeed >= 10 && n > 3) {
        int target = (rawSeed >= 13 && n >= 7) ? 2 : 1;
        octDownMask = arpPickEuclideanSteps(active, n, (rot + 2) % n, usedMask, target);
        usedMask |= octDownMask;
    }
    if (rawSeed >= 12 && n > 3) {
        int target = (rawSeed >= 14 && n >= 8) ? 2 : 1;
        rtMask = arpPickEuclideanSteps(active, n, (rot * 2 + n - 1) % n, usedMask, target);
        usedMask |= rtMask;
    }
    if (rawSeed >= 13 && n > 4) {
        int target = (rawSeed >= 15 && n >= 8) ? 2 : 1;
        dyadMask = arpPickEuclideanSteps(active, n, (rot + 3) % n, usedMask, target);
        usedMask |= dyadMask;
    }
    if (rawSeed >= 14 && n > 4) {
        slapMask = arpPickEuclideanSteps(active, n, (rot * 3 + 1) % n, usedMask, 1);
        usedMask |= slapMask;
    }
    if (rawSeed >= 15 && n > 5) {
        chordMask = arpPickEuclideanSteps(active, n, (rot + n / 2) % n, usedMask, 1);
    }

    *outOctUp = octMask;
    *outRatchet2 = rtMask;
    if (outOctDown)
        *outOctDown = octDownMask;
    if (outDyad)
        *outDyad = dyadMask;
    if (outChord)
        *outChord = chordMask;
    if (outSlap)
        *outSlap = slapMask;
}

inline int arpPresetLength(int seed) {
    static const uint8_t presetLengths[16] = {16, 16, 16, 16, 16, 12, 14, 15, 16, 13, 11, 15, 16, 12, 10, 7};
    if (seed < 0) {
        seed = 0;
    } else if (seed > 15) {
        seed = 15;
    }
    return (int)presetLengths[seed];
}

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
