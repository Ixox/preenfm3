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

#ifndef SCREEN_TYPES_H
#define SCREEN_TYPES_H

#include "Common.h"
#include "engine/pfm3/Pfm3Param.h"
#include "engine/pfm3/Pfm3Params.h"

extern const char* polyMonoNames[];

#define MIN(X, Y)        (((X) < (Y)) ? (X) : (Y))
#define CUSTOM_CHAR_NOTE (char)7

extern arp_pattern_t lut_res_arpeggiator_patterns[ARPEGGIATOR_PRESET_PATTERN_COUNT];
extern const uint16_t lut_res_arpeggiator_ties[ARPEGGIATOR_PRESET_PATTERN_COUNT];

extern int getLength(const char* str);

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

enum class EditorSlotKind : uint8_t { Param, Action, Custom, Region, Empty };

struct EditorParamRef {
    uint16_t paramId;
    uint8_t context;
};

struct EditorSlot {
    EditorSlotKind kind;
    uint16_t id;
    uint8_t context;
};

// EditorPageState slot initializers. Context defaults to 0 (engine-owned).
#define BUTTON_SIMPLE_PARAM(...) {EditorSlotKind::Param, __VA_ARGS__}
#define BUTTON_EMPTY()           {EditorSlotKind::Empty, 0, 0}

struct EditorPageState {
    const char* stateLabel;
    EditorSlot slots[NUMBER_OF_ENCODERS];
};

struct EditorPageButton {
    const char* buttonLabel;
    uint8_t buttonId;
    uint8_t stateCount;
    const EditorPageState* states[];
};

struct EditorMenu {
    const char* mainMenuName;
    uint8_t buttonCount;
    const EditorPageButton* buttons[];
};

struct Rect {
    int16_t x, y, w, h;
};

struct EditorMainMenu {
    const EditorMenu* editMenu[6];
};

#endif
