/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

/*
 * MSFA parameter descriptors shared by the editor and the engine.
 */

#ifndef MSFA_PARAMS_H_
#define MSFA_PARAMS_H_

#include <stdint.h>

enum MsfaParamId {
    MSFA_NONE = 0,
    MSFA_ALGORITHM,
    MSFA_FEEDBACK,
    MSFA_OSC_KEY_SYNC,
    MSFA_LFO_SPEED,
    MSFA_LFO_DELAY,
    MSFA_LFO_PITCH_MOD_DEPTH,
    MSFA_LFO_AMP_MOD_DEPTH,
    MSFA_LFO_KEY_SYNC,
    MSFA_LFO_WAVEFORM,
    MSFA_LFO_PITCH_MOD_SENSITIVITY,
    MSFA_EG_RATE_1,
    MSFA_EG_RATE_2,
    MSFA_EG_RATE_3,
    MSFA_EG_RATE_4,
    MSFA_EG_LEVEL_1,
    MSFA_EG_LEVEL_2,
    MSFA_EG_LEVEL_3,
    MSFA_EG_LEVEL_4,
    MSFA_LEVEL_SCALING_BREAK_POINT,
    MSFA_LEVEL_SCALING_LEFT_DEPTH,
    MSFA_LEVEL_SCALING_RIGHT_DEPTH,
    MSFA_LEVEL_SCALING_LEFT_CURVE,
    MSFA_LEVEL_SCALING_RIGHT_CURVE,
    MSFA_RATE_SCALING,
    MSFA_AMPLITUDE_MOD_SENSITIVITY,
    MSFA_KEY_VELOCITY_SENSITIVITY,
    MSFA_OPERATOR_OUTPUT_LEVEL,
    MSFA_OSCILLATOR_MODE,
    MSFA_COARSE_FREQUENCY,
    MSFA_FINE_FREQUENCY,
    MSFA_DETUNE,
    MSFA_OPERATOR_ENABLED,
    MSFA_PITCH_EG_RATE_1,
    MSFA_PITCH_EG_RATE_2,
    MSFA_PITCH_EG_RATE_3,
    MSFA_PITCH_EG_RATE_4,
    MSFA_PITCH_EG_LEVEL_1,
    MSFA_PITCH_EG_LEVEL_2,
    MSFA_PITCH_EG_LEVEL_3,
    MSFA_PITCH_EG_LEVEL_4,
    MSFA_PARAM_COUNT
};

enum MsfaParamKind {
    MSFA_KIND_GLOBAL,
    MSFA_KIND_OPERATOR,
    MSFA_KIND_PITCH_RATE,
    MSFA_KIND_PITCH_LEVEL,
    MSFA_KIND_OP_ENABLED
};

enum MsfaDisplayType {
    MSFA_DISPLAY_INT,
    MSFA_DISPLAY_STRINGS,
    MSFA_DISPLAY_ALGO,
    MSFA_DISPLAY_DETUNE
};

struct MsfaParamDesc {
    const char *name;
    uint8_t kind;
    uint8_t offset;
    uint8_t minValue;
    uint8_t maxValue;
    uint8_t displayType;
    const char **valueNames;
};


extern const struct MsfaParamDesc msfaParams[];

uint8_t getMsfaParamValue(const uint8_t *program, int paramId, int operatorNumber);
void setMsfaParamValue(uint8_t *program, int paramId, int operatorNumber, uint8_t value);

#endif
