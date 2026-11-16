/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef BYOS_PARAMS_H_
#define BYOS_PARAMS_H_

enum ByosParamId {
    BYOS_PARAM_WAVE = 0,
    BYOS_PARAM_MULTIPLIER = 1,
    BYOS_PARAM_MIX = 2,
    BYOS_PARAM_ATTACK = 3,
    BYOS_PARAM_RELEASE = 4,
    BYOS_PARAM_NONE = 0xFF
};

struct ByosParams {
    float wave[2];
    float multiplier[2];
    float mix;
    float attack;
    float release;
};

enum ByosControlChange {
    BYOS_CC_OP1_WAVE = 46,
    BYOS_CC_OP1_MULTIPLIER = 47,
    BYOS_CC_OP2_WAVE = 48,
    BYOS_CC_OP2_MULTIPLIER = 49,
    BYOS_CC_MIX = 50,
    BYOS_CC_ATTACK = 51,
    BYOS_CC_RELEASE = 52
};

#endif
