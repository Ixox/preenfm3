/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * DX7 parameter table and value mapping.
 */

#include "Dx7Params.h"

static const char *dx7OffOn[] = { "Off", "On " };
static const char *dx7OscMode[] = { "Ratio", "Fixed" };
static const char *dx7LfoWave[] = { "Tri", "Saw-", "Saw+", "Sqr", "Sin", "S&H" };
static const char *dx7Curve[] = { "-Lin", "-Exp", "+Exp", "+Lin" };


const struct Dx7ParamDesc dx7Params[DX7_PARAM_COUNT] = {
    // DX7_NONE - Empty slot
    { "",        DX7_KIND_GLOBAL,      0,   0,  0,  DX7_DISPLAY_INT,     nullptr },
    // DX7_ALGORITHM - Algorithm (DX7 algorithm number, displayed 1-32)
    { "Algo",    DX7_KIND_GLOBAL,      134, 0,  31, DX7_DISPLAY_ALGO,    nullptr },
    // DX7_FEEDBACK - Feedback amount
    { "Fdbk",    DX7_KIND_GLOBAL,      135, 0,  7,  DX7_DISPLAY_INT,     nullptr },
    // DX7_OSC_KEY_SYNC - Oscillator key sync
    { "OSync",   DX7_KIND_GLOBAL,      136, 0,  1,  DX7_DISPLAY_STRINGS, dx7OffOn },
    // DX7_LFO_SPEED - LFO speed
    { "Speed", DX7_KIND_GLOBAL,      137, 0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_LFO_DELAY - LFO delay
    { "Delay", DX7_KIND_GLOBAL,      138, 0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_LFO_PITCH_MOD_DEPTH - LFO pitch modulation depth
    { "Pitch",   DX7_KIND_GLOBAL,      139, 0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_LFO_AMP_MOD_DEPTH - LFO amplitude modulation depth
    { "Amp",   DX7_KIND_GLOBAL,      140, 0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_LFO_KEY_SYNC - LFO key sync
    { "K Sync",  DX7_KIND_GLOBAL,      141, 0,  1,  DX7_DISPLAY_STRINGS, dx7OffOn },
    // DX7_LFO_WAVEFORM - LFO waveform
    { "Wave",    DX7_KIND_GLOBAL,      142, 0,  5,  DX7_DISPLAY_STRINGS, dx7LfoWave },
    // DX7_LFO_PITCH_MOD_SENSITIVITY - LFO pitch modulation sensitivity
    { "PitchM",   DX7_KIND_GLOBAL,      143, 0,  7,  DX7_DISPLAY_INT,     nullptr },
    // DX7_EG_RATE_1 - Envelope generator rate 1
    { "Rate 1",      DX7_KIND_OPERATOR,    0,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_EG_RATE_2 - Envelope generator rate 2
    { "Rate 2",      DX7_KIND_OPERATOR,    1,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_EG_RATE_3 - Envelope generator rate 3
    { "Rate 3",      DX7_KIND_OPERATOR,    2,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_EG_RATE_4 - Envelope generator rate 4
    { "Rate 4",      DX7_KIND_OPERATOR,    3,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_EG_LEVEL_1 - Envelope generator level 1
    { "Lvl 1",      DX7_KIND_OPERATOR,    4,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_EG_LEVEL_2 - Envelope generator level 2
    { "Lvl 2",      DX7_KIND_OPERATOR,    5,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_EG_LEVEL_3 - Envelope generator level 3
    { "Lvl 3",      DX7_KIND_OPERATOR,    6,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_EG_LEVEL_4 - Envelope generator level 4
    { "Lvl 4",      DX7_KIND_OPERATOR,    7,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_LEVEL_SCALING_BREAK_POINT - Level scaling break point
    { "Break",   DX7_KIND_OPERATOR,    8,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_LEVEL_SCALING_LEFT_DEPTH - Level scaling left depth
    { "Lvl Dp", DX7_KIND_OPERATOR,    9,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_LEVEL_SCALING_RIGHT_DEPTH - Level scaling right depth
    { "Rt Dep", DX7_KIND_OPERATOR,    10,  0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_LEVEL_SCALING_LEFT_CURVE - Level scaling left curve
    { "Lv Curv", DX7_KIND_OPERATOR,    11,  0,  3,  DX7_DISPLAY_STRINGS, dx7Curve },
    // DX7_LEVEL_SCALING_RIGHT_CURVE - Level scaling right curve
    { "Rt Curv", DX7_KIND_OPERATOR,    12,  0,  3,  DX7_DISPLAY_STRINGS, dx7Curve },
    // DX7_RATE_SCALING - Rate scaling
    { "Rt Sc", DX7_KIND_OPERATOR,    13,  0,  7,  DX7_DISPLAY_INT,     nullptr },
    // DX7_AMPLITUDE_MOD_SENSITIVITY - Amplitude modulation sensitivity
    { "A Mod",     DX7_KIND_OPERATOR,    14,  0,  3,  DX7_DISPLAY_INT,     nullptr },
    // DX7_KEY_VELOCITY_SENSITIVITY - Key velocity sensitivity
    { "K Velo",     DX7_KIND_OPERATOR,    15,  0,  7,  DX7_DISPLAY_INT,     nullptr },
    // DX7_OPERATOR_OUTPUT_LEVEL - Operator output level
    { "Out Lvl", DX7_KIND_OPERATOR,    16,  0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_OSCILLATOR_MODE - Oscillator mode (ratio or fixed)
    { "Mode",    DX7_KIND_OPERATOR,    17,  0,  1,  DX7_DISPLAY_STRINGS, dx7OscMode },
    // DX7_COARSE_FREQUENCY - Coarse frequency
    { "Coarse",  DX7_KIND_OPERATOR,    18,  0,  31, DX7_DISPLAY_INT,     nullptr },
    // DX7_FINE_FREQUENCY - Fine frequency
    { "Fine",    DX7_KIND_OPERATOR,    19,  0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_DETUNE - Detune
    { "Detune",  DX7_KIND_OPERATOR,    20,  0,  14, DX7_DISPLAY_DETUNE,  nullptr },
    // DX7_OPERATOR_ENABLED - Operator enabled (on/off)
    { "Op On",   DX7_KIND_OP_ENABLED,  0,   0,  1,  DX7_DISPLAY_STRINGS, dx7OffOn },
    // DX7_PITCH_EG_RATE_1 - Pitch envelope generator rate 1
    { "Rate 1",    DX7_KIND_PITCH_RATE,  0,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_PITCH_EG_RATE_2 - Pitch envelope generator rate 2
    { "Rate 2",    DX7_KIND_PITCH_RATE,  1,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_PITCH_EG_RATE_3 - Pitch envelope generator rate 3
    { "Rate 3",    DX7_KIND_PITCH_RATE,  2,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_PITCH_EG_RATE_4 - Pitch envelope generator rate 4
    { "Rate 4",    DX7_KIND_PITCH_RATE,  3,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_PITCH_EG_LEVEL_1 - Pitch envelope generator level 1
    { "Lvl 1",    DX7_KIND_PITCH_LEVEL, 0,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_PITCH_EG_LEVEL_2 - Pitch envelope generator level 2
    { "Lvl 2",    DX7_KIND_PITCH_LEVEL, 1,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_PITCH_EG_LEVEL_3 - Pitch envelope generator level 3
    { "Lvl 3",    DX7_KIND_PITCH_LEVEL, 2,   0,  99, DX7_DISPLAY_INT,     nullptr },
    // DX7_PITCH_EG_LEVEL_4 - Pitch envelope generator level 4
    { "Lvl 4",    DX7_KIND_PITCH_LEVEL, 3,   0,  99, DX7_DISPLAY_INT,     nullptr }
};


uint8_t getDx7ParamValue(const uint8_t *program, int paramId, int operatorNumber) {
    const struct Dx7ParamDesc *d = &dx7Params[paramId];
    switch (d->kind) {
    case DX7_KIND_GLOBAL:
        return program[d->offset];
    case DX7_KIND_OPERATOR:
        return program[(5 - operatorNumber) * 21 + d->offset];
    case DX7_KIND_PITCH_RATE:
        return program[126 + d->offset];
    case DX7_KIND_PITCH_LEVEL:
        return program[130 + d->offset];
    case DX7_KIND_OP_ENABLED:
        return (program[155] >> (5 - operatorNumber)) & 0x1;
    }
    return 0;
}

void setDx7ParamValue(uint8_t *program, int paramId, int operatorNumber, uint8_t value) {
    const struct Dx7ParamDesc *d = &dx7Params[paramId];
    switch (d->kind) {
    case DX7_KIND_GLOBAL:
        program[d->offset] = value;
        break;
    case DX7_KIND_OPERATOR:
        program[(5 - operatorNumber) * 21 + d->offset] = value;
        break;
    case DX7_KIND_PITCH_RATE:
        program[126 + d->offset] = value;
        break;
    case DX7_KIND_PITCH_LEVEL:
        program[130 + d->offset] = value;
        break;
    case DX7_KIND_OP_ENABLED: {
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
