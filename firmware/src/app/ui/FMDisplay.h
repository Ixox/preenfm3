/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef FMDISPLAY_H
#define FMDISPLAY_H


#include "Common.h"


extern const char* lfoSeqMidiClock[];
extern const char* lfoOscMidiClock[];

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define CUSTOM_CHAR_NOTE (char)7


extern struct ModulationIndex modulationIndex[ALGO_END][6];
extern arp_pattern_t lut_res_arpeggiator_patterns[ARPEGGIATOR_PRESET_PATTERN_COUNT];

extern int getLength(const char *str);

enum {
    LINE_PRESET_NAME = 0,
    LINE_ROW_NAME = 2,
    LINE_PARAM_NAME1 = 3,
    LINE_PARAM_VALUE1 = 4,
    LINE_PARAM_NAME2 = 5,
    LINE_PARAM_VALUE2 = 6,
    LINE_MAIN_BUTTON1 = 6,
    LINE_BUTTON1 = 8,
    LINE_BUTTON2 = 9,
    LINE_MAIN_BUTTON2 = 9,
};

enum ParameterDisplayType {
    DISPLAY_TYPE_NONE = 0,
    DISPLAY_TYPE_FLOAT,
    DISPLAY_TYPE_FLOAT_OSC_FREQUENCY,
    DISPLAY_TYPE_FLOAT_LFO_FREQUENCY,
    DISPLAY_TYPE_INT,
    DISPLAY_TYPE_STRINGS,
    DISPLAY_TYPE_STEP_SEQ1,
    DISPLAY_TYPE_STEP_SEQ2,
    DISPLAY_TYPE_STEP_SEQ_BPM,
    DISPLAY_TYPE_LFO_KSYN,
    DISPLAY_TYPE_ARP_PATTERN,
    DISPLAY_TYPE_SCALA_SCALE
};

struct ParameterDisplay {
    float minValue;
    float maxValue;
    float numberOfValues;
    ParameterDisplayType displayType;
    const char** valueName;
    const unsigned char * valueNameOrder;
    const unsigned char * valueNameOrderReversed;
    float incValue;
};



#endif
