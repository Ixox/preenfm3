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

static const char *byosParamNames[] = {
    "Wave",
    "Mult",
    "Mix 12",
    "Attk",
    "Rel"
};

static const char *byosShapeNames[] = {
    "sin ",
    "saw ",
    "squa",
    "s^2 ",
    "szer",
    "spos",
    "rand",
    "off ",
    "Usr1",
    "Usr2",
    "Usr3",
    "Usr4",
    "Usr5",
    "Usr6"
};

static const struct EditorOneButtonState byosOpState = {
    "OP",
    {
        { BYOS_PARAM_WAVE, 0 },
        { BYOS_PARAM_MULTIPLIER, 0 },
        { BYOS_PARAM_MIX, 0 },
        { BYOS_PARAM_WAVE, 1 },
        { BYOS_PARAM_MULTIPLIER, 1 },
        { BYOS_PARAM_NONE, 0 }
    }
};

static const struct EditorOneButtonState byosEnvState = {
    "ENV",
    {
        { BYOS_PARAM_ATTACK, 0 },
        { BYOS_PARAM_RELEASE, 0 },
        { BYOS_PARAM_NONE, 0 },
        { BYOS_PARAM_NONE, 0 },
        { BYOS_PARAM_NONE, 0 },
        { BYOS_PARAM_NONE, 0 }
    }
};

static const struct EditorOneButton byosOpButton = {
    "OP",
    BUTTONID_ENGINE_PAGE_1,
    1,
    { &byosOpState }
};

static const struct EditorOneButton byosEnvButton = {
    "ENV",
    BUTTONID_ENGINE_PAGE_2,
    1,
    { &byosEnvState }
};

static const struct EditorEditMenu byosMenu = {
    "Engine",
    2,
    { &byosOpButton, &byosEnvButton }
};

static float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

static float getByosParamValue(const ByosParams *params, int paramId, int context) {
    switch (paramId) {
    case BYOS_PARAM_WAVE:
        return (context >= 0 && context < 2) ? params->wave[context] : 0.0f;
    case BYOS_PARAM_MULTIPLIER:
        return (context >= 0 && context < 2) ? params->multiplier[context] : 0.0f;
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

void EditorByos::init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) {
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

const struct EditorEditMenu* EditorByos::getEngineMenu(int page) const {
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

void EditorByos::drawEditorButton(int button, const struct EditorEditMenu *editMenu,
        const struct EditorOneButton *page) {
    struct FullState *fullState = &synthState_->fullState_;
    if (button < editMenu->numberOfButtons) {
        const struct EditorOneButton *buttonPage = editMenu->pages[button];
        tft_->drawButton(buttonPage->buttonLabel, 270, 29, button, buttonPage->numberOfStates,
            fullState->editPage == button ? fullState->buttonState[page->buttonId] + 1 : 0);
    } else {
        tft_->drawButton("", 270, 29, button, 0, 0);
    }
}

void EditorByos::drawParameterName(int button, const struct RowEncoder &rowEncoder,
        const struct EditorEditMenu *editMenu, const struct EditorOneButton *page) {
    (void) editMenu;
    (void) page;

    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_MEDIUM_GRAY);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);
    if (rowEncoder.row != BYOS_PARAM_NONE) {
        tft_->print(byosParamNames[rowEncoder.row]);
    } else {
        tft_->print("     ");
    }
}

void EditorByos::displayParamValue(int encoder, TFT_COLOR color) {
    const struct RowEncoder rowEncoder = rowEncoderFor(encoder);

    uint8_t x = getEncoderX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);

    if (rowEncoder.row == BYOS_PARAM_NONE) {
        tft_->print("     ");
        return;
    }

    ByosParams *params = (ByosParams*) timbre()->getPatchMemory();
    switch (rowEncoder.row) {
    case BYOS_PARAM_WAVE: {
        int shape = (int) params->wave[rowEncoder.encoder];
        if (shape < 0) {
            shape = 0;
        } else if (shape >= OSC_SHAPE_LAST) {
            shape = OSC_SHAPE_LAST - 1;
        }
        tft_->print(byosShapeNames[shape]);
        break;
    }
    case BYOS_PARAM_MULTIPLIER:
        tft_->printFloatWithSpace(params->multiplier[rowEncoder.encoder]);
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

void EditorByos::encoderTurned(int encoder6, int ticks) {
    const struct RowEncoder rowEncoder = rowEncoderFor(encoder6);
    if (rowEncoder.row == BYOS_PARAM_NONE) {
        return;
    }

    ByosParams *params = (ByosParams*) timbre()->getPatchMemory();
    float oldValue = getByosParamValue(params, rowEncoder.row, rowEncoder.encoder);
    float step = 0.0f;
    float minValue = 0.0f;
    float maxValue = 0.0f;

    switch (rowEncoder.row) {
    case BYOS_PARAM_WAVE:
        step = 1.0f;
        maxValue = (float) (OSC_SHAPE_LAST - 1);
        break;
    case BYOS_PARAM_MULTIPLIER:
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

    float newValue = clampf(oldValue + (float) ticks * step, minValue, maxValue);
    if (newValue != oldValue) {
        synthState_->propagateNewEngineParamValue(currentTimbre(), rowEncoder.row, rowEncoder.encoder,
            oldValue, newValue);
    }
}

void EditorByos::newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) {
    (void) timbre;
    (void) oldValue;
    (void) newValue;

    struct FullState *fullState = &synthState_->fullState_;
    const struct EditorOneButton *page = currentPage();
    if (page == nullptr) {
        return;
    }

    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        const struct RowEncoder re = page->states[fullState->buttonState[page->buttonId]]->rowEncoder[e];
        if (re.row == paramId && re.encoder == context) {
            displayParamValue(e, COLOR_WHITE);
        }
    }
}

void EditorByos::buttonPressed(int button) {
    struct FullState *fullState = &synthState_->fullState_;
    int mainPage = fullState->mainPage;
    const struct EditorEditMenu *editMenu = currentEditMenu();
    const struct EditorOneButton *page = currentPage();
    if (editMenu == nullptr || page == nullptr) {
        return;
    }

    switch (button) {
    case BUTTON_PFM3_EDIT:
    case BUTTON_PFM3_MENU:
        fullState->mainPage = -1;
        break;
    case BUTTON_PFM3_1:
    case BUTTON_PFM3_2:
    case BUTTON_PFM3_3:
    case BUTTON_PFM3_4:
    case BUTTON_PFM3_5:
    case BUTTON_PFM3_6:
        if (fullState->editPage == button) {
            if (page->numberOfStates > 0) {
                fullState->buttonState[page->buttonId] =
                    (fullState->buttonState[page->buttonId] + 1) % page->numberOfStates;
            }
        } else if (button < editMenu->numberOfButtons) {
            if (editMenu->pages[button]->numberOfStates > 0) {
                parent_->setEditPageSelected(mainPage, button);
                fullState->editPage = button;
            }
        }
        break;
    }

    synthState_->propagateNewPfm3Page();
}

void EditorByos::drawAlgorithm() {
    // Same 80x100 box as the PFM3/DX7 algorithm (TFT_ALGO_X/Y in TftDisplay.cpp).
    static const int algoX = 160;
    static const int algoY = 160;
    static const int algoWidth = 80;
    static const int algoHeight = 100;
    static const char *lines[] = { "Your", "Synth", "Here" };
    static const int lineLength[] = { 4, 5, 4 };

    int y = algoY + (algoHeight - 3 * TFT_BIG_CHAR_HEIGHT) / 2;
    for (int i = 0; i < 3; i++) {
        int x = algoX + (algoWidth - lineLength[i] * TFT_BIG_CHAR_WIDTH) / 2;
        tft_->setCursorInPixel(x, y);
        tft_->print(lines[i], COLOR_CYAN, COLOR_BLACK);
        y += TFT_BIG_CHAR_HEIGHT;
    }
}

void EditorByos::tempoClick() {
}

void EditorByos::refreshOscilloBG() {
    tft_->oscilloBgActionClear();
}
