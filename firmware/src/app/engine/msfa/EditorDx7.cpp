/*
 * Copyright 2026 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "EditorDx7.h"
#include "EditorDx7Algo.h"
#include "ScreenEditor.h"

#include "Dx7Engine.h"
#include "Dx7Params.h"
#include "SynthState.h"
#include "Timbre.h"

/*
 * DX7 156-byte program layout (see Dx7Note::init):
 *   operator op (block index 0..5) at op * 21:
 *     [0..3]   EG rates R1..R4
 *     [4..7]   EG levels L1..L4
 *     [8]      break point
 *     [9]      left depth
 *     [10]     right depth
 *     [11]     left curve
 *     [12]     right curve
 *     [13]     rate scaling
 *     [14]     amp mod sensitivity
 *     [15]     key velocity sensitivity
 *     [16]     output level
 *     [17]     oscillator mode
 *     [18]     coarse
 *     [19]     fine
 *     [20]     detune
 *   [126..129] pitch EG rates
 *   [130..133] pitch EG levels
 *   [134]      algorithm
 *   [135]      feedback
 *   [136]      oscillator key sync
 *   [137]      LFO speed
 *   [138]      LFO delay
 *   [139]      LFO pitch mod depth
 *   [140]      LFO amp mod depth
 *   [141]      LFO key sync
 *   [142]      LFO waveform
 *   [143]      LFO pitch mod sensitivity
 *   [144..154] name
 *   [155]      operator on/off bitmask (Dexed extension)
 *
 * DX7 operator 1 (operatorNumber 0) lives in block 5, operator 6 in block 0.
 */

// clang-format off
static const struct EditorOneButtonState dx7EngineState = {
    "Engine",
    {
        { DX7_ALGORITHM, 0 },
        { DX7_FEEDBACK, 0 },
        { DX7_OSC_KEY_SYNC, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButtonState dx7LfoModState = {
    "Modulation",
    {
        { DX7_LFO_AMP_MOD_DEPTH, 0 },
        { DX7_LFO_PITCH_MOD_DEPTH, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButtonState dx7LfoState = {
    "LFO",
    {
        { DX7_LFO_SPEED, 0 },
        { DX7_LFO_DELAY, 0 },
        { DX7_LFO_WAVEFORM, 0 },
        { DX7_LFO_KEY_SYNC, 0 },
        { DX7_LFO_PITCH_MOD_SENSITIVITY, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButtonState dx7PitchRateState = {
    "Pitch Rate",
    {
        { DX7_PITCH_EG_RATE_1, 0 },
        { DX7_PITCH_EG_RATE_2, 0 },
        { DX7_PITCH_EG_RATE_3, 0 },
        { DX7_PITCH_EG_RATE_4, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButtonState dx7PitchLevelState = {
    "Pitch Level",
    {
        { DX7_PITCH_EG_LEVEL_1, 0 },
        { DX7_PITCH_EG_LEVEL_2, 0 },
        { DX7_PITCH_EG_LEVEL_3, 0 },
        { DX7_PITCH_EG_LEVEL_4, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButtonState dx7AmpState = {
    "Amp",
    {
        { DX7_OPERATOR_ENABLED, 0 },
        { DX7_OPERATOR_OUTPUT_LEVEL, 0 },
        { DX7_KEY_VELOCITY_SENSITIVITY, 0 },
        { DX7_AMPLITUDE_MOD_SENSITIVITY, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButtonState dx7ScalState = {
    "Scaling",
    {
        { DX7_LEVEL_SCALING_BREAK_POINT, 0 },
        { DX7_LEVEL_SCALING_LEFT_DEPTH, 0 },
        { DX7_LEVEL_SCALING_RIGHT_DEPTH, 0 },
        { DX7_LEVEL_SCALING_LEFT_CURVE, 0 },
        { DX7_LEVEL_SCALING_RIGHT_CURVE, 0 },
        { DX7_RATE_SCALING, 0 }
    }
};

static const struct EditorOneButtonState dx7FreqState = {
    "Frequency",
    {
        { DX7_OSCILLATOR_MODE, 0 },
        { DX7_COARSE_FREQUENCY, 0 },
        { DX7_FINE_FREQUENCY, 0 },
        { DX7_DETUNE, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButtonState dx7EgRateState = {
    "Rate",
    {
        { DX7_EG_RATE_1, 0 },
        { DX7_EG_RATE_2, 0 },
        { DX7_EG_RATE_3, 0 },
        { DX7_EG_RATE_4, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButtonState dx7EgLevelState = {
    "Level",
    {
        { DX7_EG_LEVEL_1, 0 },
        { DX7_EG_LEVEL_2, 0 },
        { DX7_EG_LEVEL_3, 0 },
        { DX7_EG_LEVEL_4, 0 },
        { DX7_NONE, 0 },
        { DX7_NONE, 0 }
    }
};

static const struct EditorOneButton dx7ButtonEngine = {
    "Engine",
    BUTTONID_ENGINE_PAGE_1,
    1,
    { &dx7EngineState }
};

static const struct EditorOneButton dx7ButtonLfo = {
    "LFO",
    BUTTONID_ENGINE_PAGE_2,
    2,
    { &dx7LfoModState, &dx7LfoState }
};

static const struct EditorOneButton dx7ButtonPitchEnv = {
    "Pitch",
    BUTTONID_ENGINE_PAGE_3,
    2,
    { &dx7PitchRateState, &dx7PitchLevelState }
};

static const struct EditorOneButton dx7ButtonLevel = {
    "Level",
    BUTTONID_ENGINE_PAGE_4,
    2,
    { &dx7AmpState, &dx7ScalState }
};

static const struct EditorOneButton dx7ButtonOsc = {
    "Osc",
    BUTTONID_ENGINE_PAGE_6,
    1,
    { &dx7FreqState }
};

static const struct EditorOneButton dx7ButtonEnv = {
    "Env",
    BUTTONID_ENGINE_PAGE_7,
    2,
    { &dx7EgRateState, &dx7EgLevelState }
};

static const struct EditorEditMenu dx7GlobalMenu = {
    "Global",
    3,
    { &dx7ButtonEngine, &dx7ButtonLfo, &dx7ButtonPitchEnv }
};

static const struct EditorEditMenu dx7OperMenu = {
    "Oper",
    3,
    { &dx7ButtonLevel, &dx7ButtonOsc, &dx7ButtonEnv }
};
// clang-format on

EditorDx7::EditorDx7() {
    copyOperatorValid_ = false;
    feedbackChangedCounter_ = 0;
    feedbackSource_ = 0;
    feedbackDest_ = 0;
    for (int i = 0; i < 6; i++) {
        valueChangedCounter_[i] = 0;
        operatorChangedCounter_[i] = 0;
        highlightedOperator_[i] = 0;
    }
}

void EditorDx7::init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) {
    parent_ = parent;
    synthState_ = synthState;
    tft_ = tft;
    parent->insertEditor(this);
}

SynthEngineType EditorDx7::getEngineType() const {
    return SYNTH_ENGINE_DX7;
}

const char* EditorDx7::getEngineName() const {
    return "DX7";
}

const struct EditorEditMenu* EditorDx7::getEngineMenu(int page) const {
    if (page == 0) {
        return &dx7GlobalMenu;
    }
    if (page == 1) {
        return &dx7OperMenu;
    }
    return nullptr;
}

int EditorDx7::getCommonPage(int page) const {
    if (page == 2) {
        return COMMON_PAGE_ARP_FX;
    }
    return COMMON_PAGE_NONE;
}

void EditorDx7::drawPageHeader() {
    if (synthState_->fullState_.mainPage == 1) {
        tft_->print("Op ");
        tft_->print(synthState_->fullState_.operatorNumber + 1);
        tft_->print(" ");
    }
}

int EditorDx7::currentTimbre() const {
    return synthState_->getCurrentTimbre();
}

Timbre* EditorDx7::timbre() {
    return synthState_->getTimbre(currentTimbre());
}

void EditorDx7::displayValue(uint8_t paramId, uint8_t value) {
    const struct Dx7ParamDesc* d = &dx7Params[paramId];

    switch (d->displayType) {
    case DX7_DISPLAY_STRINGS:
        tft_->print(d->valueNames[value]);
        break;
    case DX7_DISPLAY_ALGO:
        tft_->printValueWithSpace(value + 1);
        break;
    case DX7_DISPLAY_DETUNE:
        tft_->printValueWithSpace((int)value - 7);
        break;
    case DX7_DISPLAY_INT:
    default:
        tft_->printValueWithSpace(value);
        break;
    }
}

void EditorDx7::displayParamValue(int encoder, TFT_COLOR color) {
    struct FullState* fullState = &synthState_->fullState_;
    const struct EditorOneButton* page = currentPage();
    if (page == nullptr) {
        return;
    }
    const struct RowEncoder rowEncoder = page->states[fullState->buttonState[page->buttonId]]->rowEncoder[encoder];

    uint8_t x = getEncoderX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);

    if (rowEncoder.row != DX7_NONE) {
        displayValue(rowEncoder.row, getDx7ParamValue(timbre()->getPatchMemory(), rowEncoder.row, synthState_->fullState_.operatorNumber));
    } else {
        tft_->print("     ");
    }
}

void EditorDx7::drawAlgorithm() {
    tft_->drawAlgoTable(EditorDx7Algo::table(timbre()->getPatchMemory()[134]));
}

void EditorDx7::drawEditorButton(int button, const struct EditorEditMenu* editMenu, const struct EditorOneButton* page) {
    struct FullState* fullState = &synthState_->fullState_;
    if (fullState->mainPage == 1 && button >= 3 && button <= 5) {
        tft_->drawButtonBiState(270, 29, button, fullState->operatorNumber, 5, fullState->buttonState[BUTTONID_OPERATOR_1_2 + button - 3],
                                (button - 3) * 2);
    } else if (button < editMenu->numberOfButtons) {
        const struct EditorOneButton* buttonPage = editMenu->pages[button];
        tft_->drawButton(buttonPage->buttonLabel, 270, 29, button, buttonPage->numberOfStates,
                         fullState->editPage == button ? fullState->buttonState[page->buttonId] + 1 : 0);
    } else {
        tft_->drawButton("", 270, 29, button, 0, 0);
    }
}

void EditorDx7::drawParameterName(int button, const struct RowEncoder& rowEncoder, const struct EditorEditMenu* editMenu,
                                  const struct EditorOneButton* page) {
    (void)editMenu;
    (void)page;
    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_MEDIUM_GRAY);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);
    if (rowEncoder.row != DX7_NONE) {
        tft_->print(dx7Params[rowEncoder.row].name);
    } else {
        tft_->print("     ");
    }
}

void EditorDx7::encoderTurned(int encoder6, int ticks) {
    struct FullState* fullState = &synthState_->fullState_;

    if (fullState->mainPage < 0) {
        return;
    }

    const struct EditorOneButton* page = currentPage();
    if (page == nullptr) {
        return;
    }
    const struct RowEncoder rowEncoder = page->states[fullState->buttonState[page->buttonId]]->rowEncoder[encoder6];

    if (rowEncoder.row == DX7_NONE) {
        return;
    }

    const struct Dx7ParamDesc* d = &dx7Params[rowEncoder.row];
    const int operatorNumber = fullState->operatorNumber;
    uint8_t oldValue = getDx7ParamValue(timbre()->getPatchMemory(), rowEncoder.row, operatorNumber);
    int newValue = oldValue;

    if (d->displayType == DX7_DISPLAY_STRINGS) {
        ticks = ticks > 0 ? 1 : -1;
    }

    newValue += ticks;
    if (newValue > d->maxValue) {
        newValue = d->maxValue;
    }
    if (newValue < d->minValue) {
        newValue = d->minValue;
    }
    if (newValue == oldValue) {
        return;
    }

    synthState_->propagateNewEngineParamValue(currentTimbre(), rowEncoder.row, operatorNumber, oldValue, newValue);
}

void EditorDx7::newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) {
    (void)timbre;
    (void)oldValue;

    struct FullState* fullState = &synthState_->fullState_;
    if (fullState->mainPage < 0 || fullState->mainPage > 1 || paramId <= DX7_NONE || paramId >= DX7_PARAM_COUNT) {
        return;
    }
    const struct EditorOneButton* page = currentPage();
    if (page == nullptr) {
        return;
    }

    int encoder = -1;
    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        const struct RowEncoder re = page->states[fullState->buttonState[page->buttonId]]->rowEncoder[e];
        if (re.row == paramId) {
            encoder = e;
            break;
        }
    }

    if (paramId == DX7_FEEDBACK && encoder >= 0) {
        uint8_t source = 0;
        uint8_t dest = 0;
        if (EditorDx7Algo::getFeedback(this->timbre()->getPatchMemory()[134], source, dest)) {
            tft_->highlightFeedback(encoder, source, dest);
            feedbackChangedCounter_ = 4;
            feedbackSource_ = source;
            feedbackDest_ = dest;
        }
    } else if ((dx7Params[paramId].kind == DX7_KIND_OPERATOR || dx7Params[paramId].kind == DX7_KIND_OP_ENABLED) && encoder >= 0) {
        tft_->highlightOperator(context);
        operatorChangedCounter_[encoder] = 4;
        highlightedOperator_[encoder] = context;
    }

    if (paramId >= DX7_EG_RATE_1 && paramId <= DX7_EG_LEVEL_4) {
        refreshOscillatorOperatorEnvelope();
    }
    if (paramId == DX7_ALGORITHM) {
        tft_->drawAlgoTable(EditorDx7Algo::table((int)newValue));
    }

    if (encoder >= 0) {
        valueChangedCounter_[encoder] = 4;
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(COLOR_YELLOW);
        displayParamValue(encoder, COLOR_YELLOW);
    }
}

void EditorDx7::encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button) {
    // DX7 has no multiple-operator edition yet.
}

void EditorDx7::buttonPressed(int button) {
    struct FullState* fullState = &synthState_->fullState_;
    int mainPage = fullState->mainPage;
    const struct EditorEditMenu* editMenu = currentEditMenu();
    const struct EditorOneButton* page = currentPage();
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
        if (mainPage == 1 && button >= BUTTON_PFM3_4) {
            // Operator pages: buttons 4..6 select OP1/2, OP3/4, OP5/6.
            uint8_t buttonSelected = button - BUTTON_PFM3_4;
            uint8_t currentButton = fullState->operatorNumber >> 1;
            int buttonId = BUTTONID_OPERATOR_1_2 + buttonSelected;
            uint8_t newButtonState = fullState->buttonState[buttonId];
            if (buttonSelected == currentButton) {
                newButtonState = (newButtonState + 1) & 0x1;
            }
            fullState->operatorNumber = buttonSelected * 2 + newButtonState;
            fullState->buttonState[buttonId] = newButtonState;
        } else if (fullState->editPage == button) {
            if (page->numberOfStates > 0) {
                fullState->buttonState[page->buttonId] = (fullState->buttonState[page->buttonId] + 1) % page->numberOfStates;
            }
        } else if (button < editMenu->numberOfButtons) {
            if (editMenu->pages[button]->numberOfStates > 0) {
                parent_->setEditPageSelected(mainPage, button);
                fullState->editPage = button;
            }
        }
        break;
    }

    synthState_->propagateNewScreenPage();
}

void EditorDx7::buttonLongPressed(int instrument, int button) {
    int mainPage = synthState_->fullState_.mainPage;

    if (mainPage != 1) {
        return;
    }

    uint8_t* program = timbre()->getPatchMemory();
    int block = (5 - synthState_->fullState_.operatorNumber) * 21;

    if (button == BUTTON_PREVIOUS_INSTRUMENT) {
        for (int i = 0; i < 21; i++) {
            copyOperator_[i] = program[block + i];
        }
        copyOperatorValid_ = true;
        parent_->displayPopup(COLOR_GREEN, "Copy Op", 7);
    } else if (button == BUTTON_NEXT_INSTRUMENT) {
        if (copyOperatorValid_) {
            for (int i = 0; i < 21; i++) {
                program[block + i] = copyOperator_[i];
            }
            applyProgramChange();
            parent_->setPresetModified(currentTimbre(), true);
            parent_->displayPopup(COLOR_GREEN, "Paste Op", 8);
        } else {
            parent_->displayPopup(COLOR_RED, "Cannot", 6);
        }
    }
}

void EditorDx7::refreshOscilloBG() {
    if (synthState_->fullState_.mainPage == 1) {
        refreshOscillatorOperatorEnvelope();
    } else {
        tft_->oscilloBgActionClear();
    }
}

void EditorDx7::refreshOscillatorOperatorEnvelope() {
    int op = synthState_->fullState_.operatorNumber;
    Dx7EnvelopeDisplay env;
    getDx7EnvelopeDisplay(timbre()->getPatchMemory(), op, &env);

    tft_->oscilloBgSetEnvelope(env.time[0], env.time[1], env.time[2], env.time[3], env.level[0], env.level[1], env.level[2], env.level[3],
                               CURVE_TYPE_LIN, CURVE_TYPE_LIN, CURVE_TYPE_LIN, CURVE_TYPE_LIN);
    tft_->oscilloBgActionEnvelope();
}

void EditorDx7::tempoClick() {
    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        if (valueChangedCounter_[e] > 0) {
            valueChangedCounter_[e]--;
            if (valueChangedCounter_[e] == 0 && synthState_->fullState_.mainPage >= 0) {
                tft_->setCharBackgroundColor(COLOR_BLACK);
                displayParamValue(e, COLOR_WHITE);
            }
        }
        if (operatorChangedCounter_[e] > 0) {
            operatorChangedCounter_[e]--;
            if (operatorChangedCounter_[e] == 0) {
                tft_->eraseHighlightOperator(highlightedOperator_[e]);
            }
        }
    }
    if (feedbackChangedCounter_ > 0) {
        feedbackChangedCounter_--;
        if (feedbackChangedCounter_ == 0) {
            tft_->eraseHighlightFeedback(0, feedbackSource_, feedbackDest_);
        }
    }
}

void EditorDx7::applyProgramChange() {
    timbre()->patchMemoryChanged();
}
