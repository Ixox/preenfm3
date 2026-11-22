/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "EditorByos.h"

#include "ByosParams.h"
#include "ScreenEditor.h"
#include "SynthState.h"
#include "Timbre.h"

static const char* byosParamNames[] = {"Wave", "Wave", "Mult", "Mult", "Mix 12", "Attk", "Rel"};

static const char* byosShapeNames[] = {"sin ", "saw ", "squa", "s^2 ", "szer", "spos", "rand",
                                       "off ", "Usr1", "Usr2", "Usr3", "Usr4", "Usr5", "Usr6"};

// clang-format off
static const struct EditorPageState byosOpState = {
    "OP",
    {
        BUTTON_SIMPLE_PARAM(BYOS_PARAM_OP1_WAVE),
        BUTTON_SIMPLE_PARAM(BYOS_PARAM_OP1_MULTIPLIER),
        BUTTON_SIMPLE_PARAM(BYOS_PARAM_MIX),
        BUTTON_SIMPLE_PARAM(BYOS_PARAM_OP2_WAVE),
        BUTTON_SIMPLE_PARAM(BYOS_PARAM_OP2_MULTIPLIER),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState byosEnvState = {
    "ENV",
    {
        BUTTON_SIMPLE_PARAM(BYOS_PARAM_ATTACK),
        BUTTON_SIMPLE_PARAM(BYOS_PARAM_RELEASE),
        BUTTON_EMPTY(),
        BUTTON_EMPTY(),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageButton byosOpButton = {
    "OP",
    BUTTONID_ENGINE_PAGE_1,
    1,
    { &byosOpState }
};

static const struct EditorPageButton byosEnvButton = {
    "ENV",
    BUTTONID_ENGINE_PAGE_2,
    1,
    { &byosEnvState }
};

static const struct EditorMenu byosMenu = {
    "Engine",
    2,
    { &byosOpButton, &byosEnvButton }
};
// clang-format on

static float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static float getByosParamValue(const ByosParams* params, int paramId) {
    switch (paramId) {
    case BYOS_PARAM_OP1_WAVE:
        return params->wave[0];
    case BYOS_PARAM_OP2_WAVE:
        return params->wave[1];
    case BYOS_PARAM_OP1_MULTIPLIER:
        return params->multiplier[0];
    case BYOS_PARAM_OP2_MULTIPLIER:
        return params->multiplier[1];
    case BYOS_PARAM_MIX:
        return params->mix;
    case BYOS_PARAM_ATTACK:
        return params->attack;
    case BYOS_PARAM_RELEASE:
        return params->release;
    default:
        return 0.0f;
    }
}

EditorByos::EditorByos() {
}

void EditorByos::init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) {
    parent_ = parent;
    synthState_ = synthState;
    tft_ = tft;
    parent->insertEditor(this);
}

SynthEngineType EditorByos::getEngineType() const {
    return SYNTH_ENGINE_BYOS;
}

const char* EditorByos::getEngineName() const {
    return "BYOS";
}

const struct EditorMenu* EditorByos::getEngineMenu(int page) const {
    if (page == 0) {
        return &byosMenu;
    }
    return nullptr;
}

int EditorByos::getCommonPage(int page) const {
    if (page == 1) {
        return COMMON_PAGE_ARP_FX;
    }
    return COMMON_PAGE_NONE;
}

int EditorByos::currentTimbre() const {
    return synthState_->getCurrentTimbre();
}

Timbre* EditorByos::timbre() {
    return synthState_->getTimbre(currentTimbre());
}

void EditorByos::drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) {
    drawEditorButtonDefault(button, editMenu, page);
}

void EditorByos::printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                                   const struct EditorPageButton* page) {
    (void)editMenu;
    (void)page;

    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_MEDIUM_GRAY);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);
    if (slot.id != BYOS_PARAM_NONE) {
        tft_->print(byosParamNames[slot.id]);
    } else {
        tft_->print("     ");
    }
}

void EditorByos::displayParamValue(int encoder, TFT_COLOR color) {
    const EditorSlot slot = slotForEncoder(encoder);

    uint8_t x = getEncoderX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);

    if (slot.id == BYOS_PARAM_NONE) {
        tft_->print("     ");
        return;
    }

    ByosParams* params = (ByosParams*)timbre()->getPatchMemory();
    switch (slot.id) {
    case BYOS_PARAM_OP1_WAVE:
    case BYOS_PARAM_OP2_WAVE: {
        int shape = (int)params->wave[slot.id == BYOS_PARAM_OP2_WAVE ? 1 : 0];
        if (shape < 0) {
            shape = 0;
        } else if (shape >= OSC_SHAPE_LAST) {
            shape = OSC_SHAPE_LAST - 1;
        }
        tft_->print(byosShapeNames[shape]);
        break;
    }
    case BYOS_PARAM_OP1_MULTIPLIER:
    case BYOS_PARAM_OP2_MULTIPLIER:
        tft_->printFloatWithSpace(params->multiplier[slot.id == BYOS_PARAM_OP2_MULTIPLIER ? 1 : 0]);
        break;
    case BYOS_PARAM_MIX:
        tft_->printFloatWithSpace(params->mix);
        break;
    case BYOS_PARAM_ATTACK:
        tft_->printFloatWithSpace(params->attack);
        break;
    case BYOS_PARAM_RELEASE:
        tft_->printFloatWithSpace(params->release);
        break;
    default:
        tft_->print("     ");
        break;
    }
}

void EditorByos::encoderTurned(int encoder, int ticks) {
    const EditorSlot slot = slotForEncoder(encoder);
    if (slot.id == BYOS_PARAM_NONE) {
        return;
    }

    ByosParams* params = (ByosParams*)timbre()->getPatchMemory();
    float oldValue = getByosParamValue(params, slot.id);
    float step = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;

    switch (slot.id) {
    case BYOS_PARAM_OP1_WAVE:
    case BYOS_PARAM_OP2_WAVE:
        step = 1.0f;
        maxValue = (float)(OSC_SHAPE_LAST - 1);
        break;
    case BYOS_PARAM_OP1_MULTIPLIER:
    case BYOS_PARAM_OP2_MULTIPLIER:
        step = 0.25f;
        minValue = 0.25f;
        maxValue = 4.0f;
        break;
    case BYOS_PARAM_MIX:
        step = 0.05f;
        maxValue = 1.0f;
        break;
    case BYOS_PARAM_ATTACK:
    case BYOS_PARAM_RELEASE:
        step = 0.01f;
        maxValue = 5.0f;
        break;
    default:
        return;
    }

    float newValue = clampf(oldValue + (float)ticks * step, minValue, maxValue);
    if (newValue != oldValue) {
        synthState_->propagateNewEngineParamValue(currentTimbre(), slot.id, 0, oldValue, newValue);
    }
}

void EditorByos::newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) {
    (void)refreshStatus;
    (void)timbre;
    (void)context;
    (void)oldValue;
    (void)newValue;

    struct FullState* fullState = &synthState_->fullState_;
    const struct EditorPageButton* page = currentPage();
    if (page == nullptr) {
        return;
    }

    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        const EditorSlot slot = page->states[fullState->buttonState[page->buttonId]]->slots[e];
        if (slot.id == paramId) {
            displayParamValue(e, COLOR_WHITE);
        }
    }
}

void EditorByos::buttonPressed(int button) {
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
                fullState->buttonState[page->buttonId] = (fullState->buttonState[page->buttonId] + 1) % page->stateCount;
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

void EditorByos::drawAlgorithm() {
    // Same 80x100 box as the PFM3/DX7 algorithm (TFT_ALGO_X/Y in TftDisplay.cpp).
    static const int algoX = 160;
    static const int algoY = 160;
    static const int algoWidth = 80;
    static const int algoHeight = 100;
    static const char* lines[] = {"Your", "Synth", "Here"};
    static const int lineLength[] = {4, 5, 4};

    int y = algoY + (algoHeight - 3 * TFT_BIG_CHAR_HEIGHT) / 2;
    for (int i = 0; i < 3; i++) {
        int x = algoX + (algoWidth - lineLength[i] * TFT_BIG_CHAR_WIDTH) / 2;
        tft_->setCursorInPixel(x, y);
        tft_->print(lines[i], COLOR_CYAN, COLOR_BLACK);
        y += TFT_BIG_CHAR_HEIGHT;
    }
}

void EditorByos::slowTick() {
}

void EditorByos::refreshOscilloBG() {
    tft_->oscilloBgActionClear();
}
