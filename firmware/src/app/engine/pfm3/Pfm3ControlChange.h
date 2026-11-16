/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef PFM3_CONTROL_CHANGE_H_
#define PFM3_CONTROL_CHANGE_H_

/*
 * CCs that map to PFM3 engine parameters. Only Preenfm3Engine handles them.
 */
enum Pfm3ControlChange {
    CC_UNISON_DETUNE = 13,
    CC_UNISON_SPREAD = 14,
    CC_ALGO = 16,
    CC_IM1 = 17,
    CC_IM2 = 18,
    CC_IM3 = 19,
    CC_IM4 = 20,
    CC_IM5 = 21,
    CC_MIX1 = 22,
    CC_PAN1 = 23,
    CC_MIX2 = 24,
    CC_PAN2 = 25,
    CC_MIX3 = 26,
    CC_PAN3 = 27,
    CC_MIX4 = 28,
    CC_PAN4 = 29,
    CC_IM_FEEDBACK = 30,
    CC_MATRIXROW1_MUL = 46,
    CC_MATRIXROW2_MUL = 47,
    CC_MATRIXROW3_MUL = 48,
    CC_MATRIXROW4_MUL = 49,
    CC_OSC1_FREQ = 50,
    CC_OSC2_FREQ = 51,
    CC_OSC3_FREQ = 52,
    CC_OSC4_FREQ = 53,
    CC_OSC5_FREQ = 54,
    CC_OSC6_FREQ = 55,
    CC_LFO1_FREQ = 56,
    CC_LFO2_FREQ = 57,
    CC_LFO3_FREQ = 58,
    CC_LFO_ENV2_SILENCE = 59,
    CC_STEPSEQ5_GATE = 60,
    CC_STEPSEQ6_GATE = 61,
    CC_ENV_ATK_ALL_MODULATOR = 62,
    CC_ENV_REL_ALL_MODULATOR = 63,
    CC_ENV_ATK_OP1 = 65,
    CC_ENV_ATK_OP2 = 75,
    CC_ENV_ATK_OP3 = 76,
    CC_ENV_ATK_OP4 = 77,
    CC_ENV_ATK_OP5 = 78,
    CC_ENV_ATK_OP6 = 79,
    CC_ENV_ATK_ALL_CARRIER = 80,
    CC_ENV_REL_ALL_CARRIER = 81,
    CC_ENV_REL_OP1 = 82,
    CC_ENV_REL_OP2 = 83,
    CC_ENV_REL_OP3 = 84,
    CC_ENV_REL_OP4 = 85,
    CC_ENV_REL_OP5 = 86,
    CC_ENV_REL_OP6 = 87,
    CC_LFO1_PHASE = 88,
    CC_LFO2_PHASE = 89,
    CC_LFO3_PHASE = 90,
    CC_LFO1_BIAS = 91,
    CC_LFO2_BIAS = 92,
    CC_LFO3_BIAS = 93,
    CC_LFO1_SHAPE = 94,
    CC_LFO2_SHAPE = 95,
    CC_LFO3_SHAPE = 96,
    CC_MATRIX_SOURCE_CC1 = 115,
    CC_MATRIX_SOURCE_CC2 = 116,
    CC_MATRIX_SOURCE_CC3 = 117,
    CC_MATRIX_SOURCE_CC4 = 118
};

#endif
