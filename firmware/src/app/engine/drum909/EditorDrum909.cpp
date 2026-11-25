/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "EditorDrum909.h"

#include <string.h>

#include "Drum909Logo.h"
#include "Drum909Params.h"
#include "Drum909TitleLogo.h"
#include "ScreenEditor.h"
#include "SynthState.h"
#include "Timbre.h"

extern "C" {
#include "drum909.h"
}

static void printParamName(FirmwareTftDisplay* tft, const char* name, int width) {
    int len = (int)strlen(name);
    if (len > width) {
        len = width;
    }
    for (int i = 0; i < len; i++) {
        tft->print(name[i]);
    }
    for (int i = len; i < width; i++) {
        tft->print(' ');
    }
}

static void printValue(FirmwareTftDisplay* tft, int value) {
    tft->print(value);
    int digits = 1;
    int v = value;
    while (v >= 10) {
        v /= 10;
        digits++;
    }
    for (int i = digits; i < 5; i++) {
        tft->print(' ');
    }
}

static void printName(FirmwareTftDisplay* tft, const char* name) {
    tft->print(name);
    const int len = (int)strlen(name);
    for (int i = len; i < 5; i++) {
        tft->print(' ');
    }
}

// clang-format off
static const struct EditorPageState bd0State = {
    "Bass Drum 1/2",
    { BUTTON_SIMPLE_PARAM(0, DR_BD), BUTTON_SIMPLE_PARAM(1, DR_BD), BUTTON_SIMPLE_PARAM(2, DR_BD),
      BUTTON_SIMPLE_PARAM(3, DR_BD), BUTTON_SIMPLE_PARAM(4, DR_BD), BUTTON_SIMPLE_PARAM(5, DR_BD) }
};
static const struct EditorPageState bd1State = {
    "Bass Drum 2/2",
    { BUTTON_SIMPLE_PARAM(6, DR_BD), BUTTON_SIMPLE_PARAM(7, DR_BD), BUTTON_EMPTY(),
      BUTTON_EMPTY(), BUTTON_EMPTY(), BUTTON_EMPTY() }
};

static const struct EditorPageState sd0State = {
    "Snare Drum 1/2",
    { BUTTON_SIMPLE_PARAM(0, DR_SD), BUTTON_SIMPLE_PARAM(1, DR_SD), BUTTON_SIMPLE_PARAM(2, DR_SD),
      BUTTON_SIMPLE_PARAM(3, DR_SD), BUTTON_SIMPLE_PARAM(4, DR_SD), BUTTON_SIMPLE_PARAM(5, DR_SD) }
};
static const struct EditorPageState sd1State = {
    "Snare Drum 2/2",
    { BUTTON_SIMPLE_PARAM(6, DR_SD), BUTTON_EMPTY(), BUTTON_EMPTY(),
      BUTTON_EMPTY(), BUTTON_EMPTY(), BUTTON_EMPTY() }
};

static const struct EditorPageState lt0State = {
    "Low Tom",
    { BUTTON_SIMPLE_PARAM(0, DR_LT), BUTTON_SIMPLE_PARAM(1, DR_LT), BUTTON_SIMPLE_PARAM(2, DR_LT),
      BUTTON_SIMPLE_PARAM(3, DR_LT), BUTTON_SIMPLE_PARAM(4, DR_LT), BUTTON_SIMPLE_PARAM(5, DR_LT) }
};

static const struct EditorPageState mt0State = {
    "Mid Tom",
    { BUTTON_SIMPLE_PARAM(0, DR_MT), BUTTON_SIMPLE_PARAM(1, DR_MT), BUTTON_SIMPLE_PARAM(2, DR_MT),
      BUTTON_SIMPLE_PARAM(3, DR_MT), BUTTON_SIMPLE_PARAM(4, DR_MT), BUTTON_SIMPLE_PARAM(5, DR_MT) }
};

static const struct EditorPageState ht0State = {
    "High Tom",
    { BUTTON_SIMPLE_PARAM(0, DR_HT), BUTTON_SIMPLE_PARAM(1, DR_HT), BUTTON_SIMPLE_PARAM(2, DR_HT),
      BUTTON_SIMPLE_PARAM(3, DR_HT), BUTTON_SIMPLE_PARAM(4, DR_HT), BUTTON_SIMPLE_PARAM(5, DR_HT) }
};

static const struct EditorPageState rs0State = {
    "Rimshot",
    { BUTTON_SIMPLE_PARAM(0, DR_RS), BUTTON_SIMPLE_PARAM(1, DR_RS), BUTTON_SIMPLE_PARAM(2, DR_RS),
      BUTTON_SIMPLE_PARAM(3, DR_RS), BUTTON_EMPTY(), BUTTON_EMPTY() }
};

static const struct EditorPageState cp0State = {
    "Clap",
    { BUTTON_SIMPLE_PARAM(0, DR_CP), BUTTON_SIMPLE_PARAM(1, DR_CP), BUTTON_SIMPLE_PARAM(2, DR_CP),
      BUTTON_SIMPLE_PARAM(3, DR_CP), BUTTON_SIMPLE_PARAM(4, DR_CP), BUTTON_EMPTY() }
};

static const struct EditorPageState ch0State = {
    "Closed Hat",
    { BUTTON_SIMPLE_PARAM(0, DR_CH), BUTTON_SIMPLE_PARAM(1, DR_CH), BUTTON_SIMPLE_PARAM(2, DR_CH),
      BUTTON_SIMPLE_PARAM(3, DR_CH), BUTTON_SIMPLE_PARAM(4, DR_CH), BUTTON_EMPTY() }
};

static const struct EditorPageState oh0State = {
    "Open Hat",
    { BUTTON_SIMPLE_PARAM(0, DR_OH), BUTTON_SIMPLE_PARAM(1, DR_OH), BUTTON_SIMPLE_PARAM(2, DR_OH),
      BUTTON_SIMPLE_PARAM(3, DR_OH), BUTTON_SIMPLE_PARAM(4, DR_OH), BUTTON_EMPTY() }
};

static const struct EditorPageState cr0State = {
    "Crash",
    { BUTTON_SIMPLE_PARAM(0, DR_CR), BUTTON_SIMPLE_PARAM(1, DR_CR), BUTTON_SIMPLE_PARAM(2, DR_CR),
      BUTTON_SIMPLE_PARAM(3, DR_CR), BUTTON_SIMPLE_PARAM(4, DR_CR), BUTTON_EMPTY() }
};

static const struct EditorPageState rd0State = {
    "Ride",
    { BUTTON_SIMPLE_PARAM(0, DR_RD), BUTTON_SIMPLE_PARAM(1, DR_RD), BUTTON_SIMPLE_PARAM(2, DR_RD),
      BUTTON_SIMPLE_PARAM(3, DR_RD), BUTTON_SIMPLE_PARAM(4, DR_RD), BUTTON_EMPTY() }
};

static const struct EditorPageState kitState = {
    "Kit properties",
    { BUTTON_SIMPLE_PARAM(0, DR_KIT), BUTTON_SIMPLE_PARAM(1, DR_KIT), BUTTON_EMPTY(),
      BUTTON_EMPTY(), BUTTON_EMPTY(), BUTTON_EMPTY() }
};

static const struct EditorPageButton bdButton = {
    "BD", BUTTONID_ENGINE_PAGE_1, 2, { &bd0State, &bd1State }
};
static const struct EditorPageButton sdButton = {
    "Snare", BUTTONID_ENGINE_PAGE_2, 2, { &sd0State, &sd1State }
};
static const struct EditorPageButton tomButton = {
    "Tom", BUTTONID_ENGINE_PAGE_3, 3, { &lt0State, &mt0State, &ht0State }
};
static const struct EditorPageButton percButton = {
    "Perc", BUTTONID_ENGINE_PAGE_4, 2, { &rs0State, &cp0State }
};
static const struct EditorPageButton hatButton = {
    "Hat", BUTTONID_ENGINE_PAGE_5, 2, { &ch0State, &oh0State }
};
static const struct EditorPageButton kitButton = {
    "Kit", BUTTONID_ENGINE_PAGE_6, 1, { &kitState }
};
static const struct EditorPageButton rideButton = {
    "Ride", BUTTONID_ENGINE_PAGE_7, 2, { &cr0State, &rd0State }
};

static const struct EditorMenu drum909Menu = {
    "Parts",
    6,
    { &bdButton, &sdButton, &tomButton, &percButton, &hatButton, &rideButton }
};
static const struct EditorMenu drum909KitMenu = {
    "Kit",
    1,
    { &kitButton }
};
// clang-format on

EditorDrum909::EditorDrum909() {
    theme_.headerBackground = COLOR_ORANGE;
    theme_.headerInstrumentColor = COLOR_BLACK;
    theme_.headerPresetName = COLOR_BLACK;

    theme_.pageLabelColor = COLOR_ORANGE;
    theme_.buttonBackground = COLOR_ORANGE;
    theme_.buttonText = COLOR_BLACK;
    theme_.buttonTextInactive = COLOR_BLACK;
    theme_.buttonIndicatorActive = COLOR_WHITE;
    theme_.buttonIndicatorInactive = COLOR_BLACK;
}

void EditorDrum909::init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) {
    parent_ = parent;
    synthState_ = synthState;
    tft_ = tft;
    parent->insertEditor(this);
}

SynthEngineType EditorDrum909::getEngineType() const {
    return SYNTH_ENGINE_DRUM909;
}

const char* EditorDrum909::getEngineName() const {
    return "909";
}

const struct EditorMenu* EditorDrum909::getEngineMenu(int page) const {
    if (page == 0) {
        return &drum909Menu;
    }
    if (page == 1) {
        return &drum909KitMenu;
    }
    return nullptr;
}

int EditorDrum909::getCommonPage(int page) const {
    if (page == 2) {
        return COMMON_PAGE_ARP_FX;
    }
    return COMMON_PAGE_NONE;
}

Timbre* EditorDrum909::timbre() {
    return synthState_->getTimbre(currentTimbre());
}

void EditorDrum909::drawAlgorithm() {
    tft_->drawLogo(&drum909Logo);
}

void EditorDrum909::displayEditorTitle() {
    // 240x64 asset centred in the free title band (y=21..159).
    tft_->drawLogoAt(&drum909TitleLogo, 0, 58);
}

void EditorDrum909::drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) {
    drawEditorButtonDefault(button, editMenu, page);
}

void EditorDrum909::printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                                       const struct EditorPageButton* page) {
    (void)editMenu;
    (void)page;

    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(getTheme().backgroundColor);
    tft_->setCharColor(getTheme().paramNameColor);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);

    // The third column starts at pixel 176; only 5 big chars fit before 240.
    const int width = (button % 3 == 2) ? 5 : 7;
    const x0x_param_t* desc = slot.kind == EditorSlotKind::Param ? drum909_param(slot.context, slot.id) : nullptr;
    if (desc != nullptr) {
        printParamName(tft_, desc->name, width);
    } else {
        for (int i = 0; i < width; i++) {
            tft_->print(' ');
        }
    }
}

void EditorDrum909::displayParamValue(int encoder, TFT_COLOR color) {
    if (color == COLOR_WHITE) {
        color = getTheme().paramValueColor;
    } else if (color == COLOR_YELLOW) {
        color = getTheme().paramValueChangedColor;
    }
    const EditorSlot slot = slotForEncoder(encoder);

    uint8_t x = getEncoderX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(getTheme().backgroundColor);
    tft_->setCharColor(color);

    const x0x_param_t* desc = slot.kind == EditorSlotKind::Param ? drum909_param(slot.context, slot.id) : nullptr;
    if (desc == nullptr) {
        tft_->print("     ");
        return;
    }

    const Drum909Params* params = (const Drum909Params*)timbre()->getPatchMemory();
    const int value = params->pots[slot.context][slot.id];
    if (desc->names != nullptr) {
        printName(tft_, desc->names[value]);
    } else {
        printValue(tft_, value);
    }
}

void EditorDrum909::encoderTurned(int encoder, int ticks) {
    const EditorSlot slot = slotForEncoder(encoder);
    if (slot.kind != EditorSlotKind::Param) {
        return;
    }
    const x0x_param_t* desc = drum909_param(slot.context, slot.id);
    if (desc == nullptr) {
        return;
    }

    const Drum909Params* params = (const Drum909Params*)timbre()->getPatchMemory();
    const int oldValue = params->pots[slot.context][slot.id];
    int newValue = oldValue + ticks;
    if (newValue < 0) {
        newValue = 0;
    } else if (newValue > (int)desc->max) {
        newValue = (int)desc->max;
    }
    if (newValue != oldValue) {
        synthState_->propagateNewEngineParamValue(currentTimbre(), slot.id, slot.context, (float)oldValue, (float)newValue);
    }
}

void EditorDrum909::newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) {
    (void)refreshStatus;
    (void)timbre;
    (void)oldValue;
    (void)newValue;

    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        const EditorSlot slot = slotForEncoder(e);
        if (slot.kind == EditorSlotKind::Param && (int)slot.id == paramId && (int)slot.context == context) {
            displayParamValue(e, COLOR_WHITE);
        }
    }
}

void EditorDrum909::buttonPressed(int button) {
    struct FullState* fullState = &synthState_->fullState_;
    int mainPage = fullState->mainPage;
    const struct EditorMenu* editMenu = currentEditMenu();
    const struct EditorPageButton* page = currentPage();
    if (editMenu == nullptr || page == nullptr) {
        return;
    }

    switch (button) {
    case BUTTON_SWITCH_EDIT:
    case BUTTON_SWITCH_MENU:
        fullState->mainPage = -1;
        break;
    case BUTTON_SWITCH_1:
    case BUTTON_SWITCH_2:
    case BUTTON_SWITCH_3:
    case BUTTON_SWITCH_4:
    case BUTTON_SWITCH_5:
    case BUTTON_SWITCH_6:
        if (editPage_ == button) {
            if (page->stateCount > 0) {
                uint8_t& state = buttonStateRef(page->buttonId);
                state = (state + 1) % page->stateCount;
            }
        } else if (button < editMenu->buttonCount) {
            if (editMenu->buttons[button]->stateCount > 0) {
                setEditPageSelected(mainPage, button);
                editPage_ = button;
            }
        }
        break;
    }

    synthState_->propagateNewScreenPage();
}

void EditorDrum909::slowTick() {
}

void EditorDrum909::refreshOscilloBG() {
    tft_->oscilloBgActionClear();
}
