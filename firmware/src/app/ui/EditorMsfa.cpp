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

#include "EditorMsfa.h"
#include "ScreenEditor.h"

#include "SynthState.h"
#include "Timbre.h"
#include "MsfaEngine.h"
#include "MsfaParams.h"

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


static const struct EditorOneButtonState msfaEngineState = {
    "Engine",
    {
        { MSFA_ALGORITHM, 0 },
        { MSFA_FEEDBACK, 0 },
        { MSFA_OSC_KEY_SYNC, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButtonState msfaLfoModState = {
    "Modulation",
    {
        { MSFA_LFO_AMP_MOD_DEPTH, 0 },
        { MSFA_LFO_PITCH_MOD_DEPTH, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButtonState msfaLfoState = {
    "LFO",
    {
        { MSFA_LFO_SPEED, 0 },
        { MSFA_LFO_DELAY, 0 },
        { MSFA_LFO_WAVEFORM, 0 },
        { MSFA_LFO_KEY_SYNC, 0 },
        { MSFA_LFO_PITCH_MOD_SENSITIVITY, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButtonState msfaPitchRateState = {
    "Pitch Rate",
    {
        { MSFA_PITCH_EG_RATE_1, 0 },
        { MSFA_PITCH_EG_RATE_2, 0 },
        { MSFA_PITCH_EG_RATE_3, 0 },
        { MSFA_PITCH_EG_RATE_4, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButtonState msfaPitchLevelState = {
    "Pitch Level",
    {
        { MSFA_PITCH_EG_LEVEL_1, 0 },
        { MSFA_PITCH_EG_LEVEL_2, 0 },
        { MSFA_PITCH_EG_LEVEL_3, 0 },
        { MSFA_PITCH_EG_LEVEL_4, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButtonState msfaAmpState = {
    "Amp",
    {
        { MSFA_OPERATOR_ENABLED, 0 },
        { MSFA_OPERATOR_OUTPUT_LEVEL, 0 },
        { MSFA_KEY_VELOCITY_SENSITIVITY, 0 },
        { MSFA_AMPLITUDE_MOD_SENSITIVITY, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButtonState msfaScalState = {
    "Scaling",
    {
        { MSFA_LEVEL_SCALING_BREAK_POINT, 0 },
        { MSFA_LEVEL_SCALING_LEFT_DEPTH, 0 },
        { MSFA_LEVEL_SCALING_RIGHT_DEPTH, 0 },
        { MSFA_LEVEL_SCALING_LEFT_CURVE, 0 },
        { MSFA_LEVEL_SCALING_RIGHT_CURVE, 0 },
        { MSFA_RATE_SCALING, 0 }
    }
};

static const struct EditorOneButtonState msfaFreqState = {
    "Frequency",
    {
        { MSFA_OSCILLATOR_MODE, 0 },
        { MSFA_COARSE_FREQUENCY, 0 },
        { MSFA_FINE_FREQUENCY, 0 },
        { MSFA_DETUNE, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButtonState msfaEgRateState = {
    "Rate",
    {
        { MSFA_EG_RATE_1, 0 },
        { MSFA_EG_RATE_2, 0 },
        { MSFA_EG_RATE_3, 0 },
        { MSFA_EG_RATE_4, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButtonState msfaEgLevelState = {
    "Level",
    {
        { MSFA_EG_LEVEL_1, 0 },
        { MSFA_EG_LEVEL_2, 0 },
        { MSFA_EG_LEVEL_3, 0 },
        { MSFA_EG_LEVEL_4, 0 },
        { MSFA_NONE, 0 },
        { MSFA_NONE, 0 }
    }
};

static const struct EditorOneButton msfaButtonEngine = {
    "Engine",
    BUTTONID_ENGINE_PAGE_1,
    1,
    { &msfaEngineState }
};

static const struct EditorOneButton msfaButtonLfo = {
    "LFO",
    BUTTONID_ENGINE_PAGE_2,
    2,
    { &msfaLfoModState, &msfaLfoState }
};

static const struct EditorOneButton msfaButtonPitchEnv = {
    "Pitch",
    BUTTONID_ENGINE_PAGE_3,
    2,
    { &msfaPitchRateState, &msfaPitchLevelState }
};

static const struct EditorOneButton msfaButtonLevel = {
    "Level",
    BUTTONID_ENGINE_PAGE_4,
    2,
    { &msfaAmpState, &msfaScalState }
};

static const struct EditorOneButton msfaButtonOsc = {
    "Osc",
    BUTTONID_ENGINE_PAGE_6,
    1,
    { &msfaFreqState }
};

static const struct EditorOneButton msfaButtonEnv = {
    "Env",
    BUTTONID_ENGINE_PAGE_7,
    2,
    { &msfaEgRateState, &msfaEgLevelState }
};

static const struct EditorEditMenu msfaGlobalMenu = {
    "Global",
    3,
    { &msfaButtonEngine, &msfaButtonLfo, &msfaButtonPitchEnv }
};

static const struct EditorEditMenu msfaOperMenu = {
    "Oper",
    3,
    { &msfaButtonLevel, &msfaButtonOsc, &msfaButtonEnv }
};

EditorMsfa::EditorMsfa() {
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

void EditorMsfa::init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) {
    parent_ = parent;
    synthState_ = synthState;
    tft_ = tft;
    parent->insertEditor(this);
}

SynthEngineType EditorMsfa::getEngineType() const {
    return SYNTH_ENGINE_MSFA;
}

const char* EditorMsfa::getEngineName() const {
    return "DX7";
}

const struct EditorEditMenu* EditorMsfa::getEngineMenu(int page) const {
    if (page == 0) {
        return &msfaGlobalMenu;
    }
    if (page == 1) {
        return &msfaOperMenu;
    }
    return nullptr;
}

int EditorMsfa::getCommonPage(int page) const {
    if (page == 2) {
        return COMMON_PAGE_ARP_FX;
    }
    return COMMON_PAGE_NONE;
}

void EditorMsfa::drawPageHeader() {
    if (synthState_->fullState.mainPage == 1) {
        tft_->print("Op ");
        tft_->print(synthState_->fullState.operatorNumber + 1);
        tft_->print(" ");
    }
}

int EditorMsfa::currentTimbre() const {
    return synthState_->getCurrentTimbre();
}

Timbre* EditorMsfa::timbre() {
    return synthState_->getTimbre(currentTimbre());
}



void EditorMsfa::displayValue(uint8_t paramId, uint8_t value) {
    const struct MsfaParamDesc *d = &msfaParams[paramId];

    switch (d->displayType) {
    case MSFA_DISPLAY_STRINGS:
        tft_->print(d->valueNames[value]);
        break;
    case MSFA_DISPLAY_ALGO:
        tft_->printValueWithSpace(value + 1);
        break;
    case MSFA_DISPLAY_DETUNE:
        tft_->printValueWithSpace((int) value - 7);
        break;
    case MSFA_DISPLAY_INT:
    default:
        tft_->printValueWithSpace(value);
        break;
    }
}

void EditorMsfa::displayParamValue(int encoder, TFT_COLOR color) {
    struct FullState *fullState = &synthState_->fullState;
    const struct EditorOneButton *page = currentPage();
    if (page == nullptr) {
        return;
    }
    const struct RowEncoder rowEncoder =
        page->states[fullState->buttonState[page->buttonId]]->rowEncoder[encoder];

    uint8_t x = getEncoderX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);

    if (rowEncoder.row != MSFA_NONE) {
        displayValue(rowEncoder.row, getMsfaParamValue(timbre()->getPatchMemory(), rowEncoder.row, synthState_->fullState.operatorNumber));
    } else {
        tft_->print("     ");
    }
}

void EditorMsfa::drawAlgorithm() {
    tft_->drawDx7Algo(timbre()->getPatchMemory()[134]);
}

void EditorMsfa::drawEditorButton(int button, const struct EditorEditMenu *editMenu,
        const struct EditorOneButton *page) {
    struct FullState *fullState = &synthState_->fullState;
    if (fullState->mainPage == 1 && button >= 3 && button <= 5) {
        tft_->drawButtonBiState(270, 29, button, fullState->operatorNumber, 5,
            fullState->buttonState[BUTTONID_OPERATOR_1_2 + button - 3], (button - 3) * 2);
    } else if (button < editMenu->numberOfButtons) {
        const struct EditorOneButton *buttonPage = editMenu->pages[button];
        tft_->drawButton(buttonPage->buttonLabel, 270, 29, button, buttonPage->numberOfStates,
            fullState->editPage == button ? fullState->buttonState[page->buttonId] + 1 : 0);
    } else {
        tft_->drawButton("", 270, 29, button, 0, 0);
    }
}

void EditorMsfa::drawParameterName(int button, const struct RowEncoder &rowEncoder,
        const struct EditorEditMenu *editMenu, const struct EditorOneButton *page) {
    (void) editMenu;
    (void) page;
    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_MEDIUM_GRAY);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);
    if (rowEncoder.row != MSFA_NONE) {
        tft_->print(msfaParams[rowEncoder.row].name);
    } else {
        tft_->print("     ");
    }
}

void EditorMsfa::encoderTurned(int encoder6, int ticks) {
    struct FullState *fullState = &synthState_->fullState;

    if (fullState->mainPage < 0) {
        return;
    }

    const struct EditorOneButton *page = currentPage();
    if (page == nullptr) {
        return;
    }
    const struct RowEncoder rowEncoder =
        page->states[fullState->buttonState[page->buttonId]]->rowEncoder[encoder6];

    if (rowEncoder.row == MSFA_NONE) {
        return;
    }

    const struct MsfaParamDesc *d = &msfaParams[rowEncoder.row];
    const int operatorNumber = fullState->operatorNumber;
    uint8_t oldValue = getMsfaParamValue(timbre()->getPatchMemory(), rowEncoder.row, operatorNumber);
    int newValue = oldValue;

    if (d->displayType == MSFA_DISPLAY_STRINGS) {
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

void EditorMsfa::newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) {
    (void) timbre; (void) oldValue;

    struct FullState *fullState = &synthState_->fullState;
    if (fullState->mainPage < 0 || fullState->mainPage > 1
        || paramId <= MSFA_NONE || paramId >= MSFA_PARAM_COUNT) {
        return;
    }
    const struct EditorOneButton *page = currentPage();
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

    if (paramId == MSFA_FEEDBACK && encoder >= 0) {
        uint8_t source = 0;
        uint8_t dest = 0;
        if (tft_->getDx7Feedback(this->timbre()->getPatchMemory()[134], source, dest)) {
            tft_->highlightFeedback(encoder, source, dest);
            feedbackChangedCounter_ = 4;
            feedbackSource_ = source;
            feedbackDest_ = dest;
        }
    } else if ((msfaParams[paramId].kind == MSFA_KIND_OPERATOR
            || msfaParams[paramId].kind == MSFA_KIND_OP_ENABLED) && encoder >= 0) {
        tft_->highlightOperator(context);
        operatorChangedCounter_[encoder] = 4;
        highlightedOperator_[encoder] = context;
    }

    if (paramId >= MSFA_EG_RATE_1 && paramId <= MSFA_EG_LEVEL_4) {
        refreshOscillatorOperatorEnvelope();
    }
    if (paramId == MSFA_ALGORITHM) {
        tft_->drawDx7Algo((int) newValue);
    }

    if (encoder >= 0) {
        valueChangedCounter_[encoder] = 4;
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(COLOR_YELLOW);
        displayParamValue(encoder, COLOR_YELLOW);
    }
}

void EditorMsfa::encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button) {
    // MSFA has no multiple-operator edition yet.
}

void EditorMsfa::buttonPressed(int button) {
    struct FullState *fullState = &synthState_->fullState;
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

void EditorMsfa::buttonLongPressed(int instrument, int button) {
    int mainPage = synthState_->fullState.mainPage;

    if (mainPage != 1) {
        return;
    }

    uint8_t *program = timbre()->getPatchMemory();
    int block = (5 - synthState_->fullState.operatorNumber) * 21;

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

void EditorMsfa::refreshOscilloBG() {
    if (synthState_->fullState.mainPage == 1) {
        refreshOscillatorOperatorEnvelope();
    } else {
        tft_->oscilloBgActionClear();
    }
}

void EditorMsfa::refreshOscillatorOperatorEnvelope() {
    int op = synthState_->fullState.operatorNumber;
    MsfaEnvelopeDisplay env;
    getMsfaEnvelopeDisplay(timbre()->getPatchMemory(), op, &env);

    tft_->oscilloBgSetEnvelope(env.time[0], env.time[1], env.time[2], env.time[3],
            env.level[0], env.level[1], env.level[2], env.level[3],
            CURVE_TYPE_LIN, CURVE_TYPE_LIN, CURVE_TYPE_LIN, CURVE_TYPE_LIN);
    tft_->oscilloBgActionEnvelope();
}

void EditorMsfa::tempoClick() {
    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        if (valueChangedCounter_[e] > 0) {
            valueChangedCounter_[e]--;
            if (valueChangedCounter_[e] == 0 && synthState_->fullState.mainPage >= 0) {
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

void EditorMsfa::applyProgramChange() {
    timbre()->patchMemoryChanged();
}
