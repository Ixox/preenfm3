/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * DX7 parameter descriptors shared by the editor and the engine.
 */

#ifndef DX7_PARAMS_H_
#define DX7_PARAMS_H_

#include <stdint.h>

enum Dx7ParamId {
    DX7_NONE = 0,
    DX7_ALGORITHM,
    DX7_FEEDBACK,
    DX7_OSC_KEY_SYNC,
    DX7_LFO_SPEED,
    DX7_LFO_DELAY,
    DX7_LFO_PITCH_MOD_DEPTH,
    DX7_LFO_AMP_MOD_DEPTH,
    DX7_LFO_KEY_SYNC,
    DX7_LFO_WAVEFORM,
    DX7_LFO_PITCH_MOD_SENSITIVITY,
    DX7_EG_RATE_1,
    DX7_EG_RATE_2,
    DX7_EG_RATE_3,
    DX7_EG_RATE_4,
    DX7_EG_LEVEL_1,
    DX7_EG_LEVEL_2,
    DX7_EG_LEVEL_3,
    DX7_EG_LEVEL_4,
    DX7_LEVEL_SCALING_BREAK_POINT,
    DX7_LEVEL_SCALING_LEFT_DEPTH,
    DX7_LEVEL_SCALING_RIGHT_DEPTH,
    DX7_LEVEL_SCALING_LEFT_CURVE,
    DX7_LEVEL_SCALING_RIGHT_CURVE,
    DX7_RATE_SCALING,
    DX7_AMPLITUDE_MOD_SENSITIVITY,
    DX7_KEY_VELOCITY_SENSITIVITY,
    DX7_OPERATOR_OUTPUT_LEVEL,
    DX7_OSCILLATOR_MODE,
    DX7_COARSE_FREQUENCY,
    DX7_FINE_FREQUENCY,
    DX7_DETUNE,
    DX7_OPERATOR_ENABLED,
    DX7_PITCH_EG_RATE_1,
    DX7_PITCH_EG_RATE_2,
    DX7_PITCH_EG_RATE_3,
    DX7_PITCH_EG_RATE_4,
    DX7_PITCH_EG_LEVEL_1,
    DX7_PITCH_EG_LEVEL_2,
    DX7_PITCH_EG_LEVEL_3,
    DX7_PITCH_EG_LEVEL_4,
    DX7_PARAM_COUNT
};

enum Dx7ParamKind { DX7_KIND_GLOBAL, DX7_KIND_OPERATOR, DX7_KIND_PITCH_RATE, DX7_KIND_PITCH_LEVEL, DX7_KIND_OP_ENABLED };

enum Dx7DisplayType { DX7_DISPLAY_INT, DX7_DISPLAY_STRINGS, DX7_DISPLAY_ALGO, DX7_DISPLAY_DETUNE };

struct Dx7ParamDesc {
    const char* name;
    uint8_t kind;
    uint8_t offset;
    uint8_t minValue;
    uint8_t maxValue;
    uint8_t displayType;
    const char** valueNames;
};

extern const struct Dx7ParamDesc dx7Params[];

uint8_t getDx7ParamValue(const uint8_t* program, int paramId, int operatorNumber);
void setDx7ParamValue(uint8_t* program, int paramId, int operatorNumber, uint8_t value);

#endif
