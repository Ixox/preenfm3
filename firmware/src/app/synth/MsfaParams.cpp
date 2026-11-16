/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * MSFA parameter table and value mapping.
 */

#include "MsfaParams.h"

static const char *msfaOffOn[] = { "Off", "On " };
static const char *msfaOscMode[] = { "Ratio", "Fixed" };
static const char *msfaLfoWave[] = { "Tri", "Saw-", "Saw+", "Sqr", "Sin", "S&H" };
static const char *msfaCurve[] = { "-Lin", "-Exp", "+Exp", "+Lin" };


const struct MsfaParamDesc msfaParams[MSFA_PARAM_COUNT] = {
    // MSFA_NONE - Empty slot
    { "",        MSFA_KIND_GLOBAL,      0,   0,  0,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_ALGORITHM - Algorithm (DX7 algorithm number, displayed 1-32)
    { "Algo",    MSFA_KIND_GLOBAL,      134, 0,  31, MSFA_DISPLAY_ALGO,    nullptr },
    // MSFA_FEEDBACK - Feedback amount
    { "Fdbk",    MSFA_KIND_GLOBAL,      135, 0,  7,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_OSC_KEY_SYNC - Oscillator key sync
    { "OSync",   MSFA_KIND_GLOBAL,      136, 0,  1,  MSFA_DISPLAY_STRINGS, msfaOffOn },
    // MSFA_LFO_SPEED - LFO speed
    { "Speed", MSFA_KIND_GLOBAL,      137, 0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LFO_DELAY - LFO delay
    { "Delay", MSFA_KIND_GLOBAL,      138, 0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LFO_PITCH_MOD_DEPTH - LFO pitch modulation depth
    { "Pitch",   MSFA_KIND_GLOBAL,      139, 0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LFO_AMP_MOD_DEPTH - LFO amplitude modulation depth
    { "Amp",   MSFA_KIND_GLOBAL,      140, 0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LFO_KEY_SYNC - LFO key sync
    { "K Sync",  MSFA_KIND_GLOBAL,      141, 0,  1,  MSFA_DISPLAY_STRINGS, msfaOffOn },
    // MSFA_LFO_WAVEFORM - LFO waveform
    { "Wave",    MSFA_KIND_GLOBAL,      142, 0,  5,  MSFA_DISPLAY_STRINGS, msfaLfoWave },
    // MSFA_LFO_PITCH_MOD_SENSITIVITY - LFO pitch modulation sensitivity
    { "PitchM",   MSFA_KIND_GLOBAL,      143, 0,  7,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_RATE_1 - Envelope generator rate 1
    { "Rate 1",      MSFA_KIND_OPERATOR,    0,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_RATE_2 - Envelope generator rate 2
    { "Rate 2",      MSFA_KIND_OPERATOR,    1,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_RATE_3 - Envelope generator rate 3
    { "Rate 3",      MSFA_KIND_OPERATOR,    2,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_RATE_4 - Envelope generator rate 4
    { "Rate 4",      MSFA_KIND_OPERATOR,    3,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_LEVEL_1 - Envelope generator level 1
    { "Lvl 1",      MSFA_KIND_OPERATOR,    4,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_LEVEL_2 - Envelope generator level 2
    { "Lvl 2",      MSFA_KIND_OPERATOR,    5,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_LEVEL_3 - Envelope generator level 3
    { "Lvl 3",      MSFA_KIND_OPERATOR,    6,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_LEVEL_4 - Envelope generator level 4
    { "Lvl 4",      MSFA_KIND_OPERATOR,    7,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LEVEL_SCALING_BREAK_POINT - Level scaling break point
    { "Break",   MSFA_KIND_OPERATOR,    8,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LEVEL_SCALING_LEFT_DEPTH - Level scaling left depth
    { "Lvl Dp", MSFA_KIND_OPERATOR,    9,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LEVEL_SCALING_RIGHT_DEPTH - Level scaling right depth
    { "Rt Dep", MSFA_KIND_OPERATOR,    10,  0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LEVEL_SCALING_LEFT_CURVE - Level scaling left curve
    { "Lv Curv", MSFA_KIND_OPERATOR,    11,  0,  3,  MSFA_DISPLAY_STRINGS, msfaCurve },
    // MSFA_LEVEL_SCALING_RIGHT_CURVE - Level scaling right curve
    { "Rt Curv", MSFA_KIND_OPERATOR,    12,  0,  3,  MSFA_DISPLAY_STRINGS, msfaCurve },
    // MSFA_RATE_SCALING - Rate scaling
    { "Rt Sc", MSFA_KIND_OPERATOR,    13,  0,  7,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_AMPLITUDE_MOD_SENSITIVITY - Amplitude modulation sensitivity
    { "A Mod",     MSFA_KIND_OPERATOR,    14,  0,  3,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_KEY_VELOCITY_SENSITIVITY - Key velocity sensitivity
    { "K Velo",     MSFA_KIND_OPERATOR,    15,  0,  7,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_OPERATOR_OUTPUT_LEVEL - Operator output level
    { "Out Lvl", MSFA_KIND_OPERATOR,    16,  0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_OSCILLATOR_MODE - Oscillator mode (ratio or fixed)
    { "Mode",    MSFA_KIND_OPERATOR,    17,  0,  1,  MSFA_DISPLAY_STRINGS, msfaOscMode },
    // MSFA_COARSE_FREQUENCY - Coarse frequency
    { "Coarse",  MSFA_KIND_OPERATOR,    18,  0,  31, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_FINE_FREQUENCY - Fine frequency
    { "Fine",    MSFA_KIND_OPERATOR,    19,  0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_DETUNE - Detune
    { "Detune",  MSFA_KIND_OPERATOR,    20,  0,  14, MSFA_DISPLAY_DETUNE,  nullptr },
    // MSFA_OPERATOR_ENABLED - Operator enabled (on/off)
    { "Op On",   MSFA_KIND_OP_ENABLED,  0,   0,  1,  MSFA_DISPLAY_STRINGS, msfaOffOn },
    // MSFA_PITCH_EG_RATE_1 - Pitch envelope generator rate 1
    { "Rate 1",    MSFA_KIND_PITCH_RATE,  0,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_RATE_2 - Pitch envelope generator rate 2
    { "Rate 2",    MSFA_KIND_PITCH_RATE,  1,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_RATE_3 - Pitch envelope generator rate 3
    { "Rate 3",    MSFA_KIND_PITCH_RATE,  2,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_RATE_4 - Pitch envelope generator rate 4
    { "Rate 4",    MSFA_KIND_PITCH_RATE,  3,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_LEVEL_1 - Pitch envelope generator level 1
    { "Lvl 1",    MSFA_KIND_PITCH_LEVEL, 0,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_LEVEL_2 - Pitch envelope generator level 2
    { "Lvl 2",    MSFA_KIND_PITCH_LEVEL, 1,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_LEVEL_3 - Pitch envelope generator level 3
    { "Lvl 3",    MSFA_KIND_PITCH_LEVEL, 2,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_LEVEL_4 - Pitch envelope generator level 4
    { "Lvl 4",    MSFA_KIND_PITCH_LEVEL, 3,   0,  99, MSFA_DISPLAY_INT,     nullptr }
};


uint8_t getMsfaParamValue(const uint8_t *program, int paramId, int operatorNumber) {
    const struct MsfaParamDesc *d = &msfaParams[paramId];
    switch (d->kind) {
    case MSFA_KIND_GLOBAL:
        return program[d->offset];
    case MSFA_KIND_OPERATOR:
        return program[(5 - operatorNumber) * 21 + d->offset];
    case MSFA_KIND_PITCH_RATE:
        return program[126 + d->offset];
    case MSFA_KIND_PITCH_LEVEL:
        return program[130 + d->offset];
    case MSFA_KIND_OP_ENABLED:
        return (program[155] >> (5 - operatorNumber)) & 0x1;
    }
    return 0;
}

void setMsfaParamValue(uint8_t *program, int paramId, int operatorNumber, uint8_t value) {
    const struct MsfaParamDesc *d = &msfaParams[paramId];
    switch (d->kind) {
    case MSFA_KIND_GLOBAL:
        program[d->offset] = value;
        break;
    case MSFA_KIND_OPERATOR:
        program[(5 - operatorNumber) * 21 + d->offset] = value;
        break;
    case MSFA_KIND_PITCH_RATE:
        program[126 + d->offset] = value;
        break;
    case MSFA_KIND_PITCH_LEVEL:
        program[130 + d->offset] = value;
        break;
    case MSFA_KIND_OP_ENABLED: {
        uint8_t bit = 5 - operatorNumber;
        if (value) {
            program[155] |= (1U << bit);
        } else {
            program[155] &= ~(1U << bit);
        }
        break;
    }
    }
}
