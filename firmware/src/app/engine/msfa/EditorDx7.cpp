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

#include "Dx7Logo.h"
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
static const struct EditorPageState dx7EngineState = {
    "Engine",
    {
        BUTTON_SIMPLE_PARAM(DX7_ALGORITHM),
        BUTTON_SIMPLE_PARAM(DX7_FEEDBACK),
        BUTTON_SIMPLE_PARAM(DX7_OSC_KEY_SYNC),
        BUTTON_EMPTY(),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState dx7LfoModState = {
    "Modulation",
    {
        BUTTON_SIMPLE_PARAM(DX7_LFO_AMP_MOD_DEPTH),
        BUTTON_SIMPLE_PARAM(DX7_LFO_PITCH_MOD_DEPTH),
        BUTTON_EMPTY(),
        BUTTON_EMPTY(),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState dx7LfoState = {
    "LFO",
    {
        BUTTON_SIMPLE_PARAM(DX7_LFO_SPEED),
        BUTTON_SIMPLE_PARAM(DX7_LFO_DELAY),
        BUTTON_SIMPLE_PARAM(DX7_LFO_WAVEFORM),
        BUTTON_SIMPLE_PARAM(DX7_LFO_KEY_SYNC),
        BUTTON_SIMPLE_PARAM(DX7_LFO_PITCH_MOD_SENSITIVITY),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState dx7PitchRateState = {
    "Pitch Rate",
    {
        BUTTON_SIMPLE_PARAM(DX7_PITCH_EG_RATE_1),
        BUTTON_SIMPLE_PARAM(DX7_PITCH_EG_RATE_2),
        BUTTON_SIMPLE_PARAM(DX7_PITCH_EG_RATE_3),
        BUTTON_SIMPLE_PARAM(DX7_PITCH_EG_RATE_4),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState dx7PitchLevelState = {
    "Pitch Level",
    {
        BUTTON_SIMPLE_PARAM(DX7_PITCH_EG_LEVEL_1),
        BUTTON_SIMPLE_PARAM(DX7_PITCH_EG_LEVEL_2),
        BUTTON_SIMPLE_PARAM(DX7_PITCH_EG_LEVEL_3),
        BUTTON_SIMPLE_PARAM(DX7_PITCH_EG_LEVEL_4),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState dx7AmpState = {
    "Amp",
    {
        BUTTON_SIMPLE_PARAM(DX7_OPERATOR_ENABLED),
        BUTTON_SIMPLE_PARAM(DX7_OPERATOR_OUTPUT_LEVEL),
        BUTTON_SIMPLE_PARAM(DX7_KEY_VELOCITY_SENSITIVITY),
        BUTTON_SIMPLE_PARAM(DX7_AMPLITUDE_MOD_SENSITIVITY),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState dx7ScalState = {
    "Scaling",
    {
        BUTTON_SIMPLE_PARAM(DX7_LEVEL_SCALING_BREAK_POINT),
        BUTTON_SIMPLE_PARAM(DX7_LEVEL_SCALING_LEFT_DEPTH),
        BUTTON_SIMPLE_PARAM(DX7_LEVEL_SCALING_RIGHT_DEPTH),
        BUTTON_SIMPLE_PARAM(DX7_LEVEL_SCALING_LEFT_CURVE),
        BUTTON_SIMPLE_PARAM(DX7_LEVEL_SCALING_RIGHT_CURVE),
        BUTTON_SIMPLE_PARAM(DX7_RATE_SCALING)
    }
};

static const struct EditorPageState dx7FreqState = {
    "Frequency",
    {
        BUTTON_SIMPLE_PARAM(DX7_OSCILLATOR_MODE),
        BUTTON_SIMPLE_PARAM(DX7_COARSE_FREQUENCY),
        BUTTON_SIMPLE_PARAM(DX7_FINE_FREQUENCY),
        BUTTON_SIMPLE_PARAM(DX7_DETUNE),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState dx7EgRateState = {
    "Rate",
    {
        BUTTON_SIMPLE_PARAM(DX7_EG_RATE_1),
        BUTTON_SIMPLE_PARAM(DX7_EG_RATE_2),
        BUTTON_SIMPLE_PARAM(DX7_EG_RATE_3),
        BUTTON_SIMPLE_PARAM(DX7_EG_RATE_4),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageState dx7EgLevelState = {
    "Level",
    {
        BUTTON_SIMPLE_PARAM(DX7_EG_LEVEL_1),
        BUTTON_SIMPLE_PARAM(DX7_EG_LEVEL_2),
        BUTTON_SIMPLE_PARAM(DX7_EG_LEVEL_3),
        BUTTON_SIMPLE_PARAM(DX7_EG_LEVEL_4),
        BUTTON_EMPTY(),
        BUTTON_EMPTY()
    }
};

static const struct EditorPageButton dx7ButtonEngine = {
    "Engine",
    BUTTONID_ENGINE_PAGE_1,
    1,
    { &dx7EngineState }
};

static const struct EditorPageButton dx7ButtonLfo = {
    "LFO",
    BUTTONID_ENGINE_PAGE_2,
    2,
    { &dx7LfoModState, &dx7LfoState }
};

static const struct EditorPageButton dx7ButtonPitchEnv = {
    "Pitch",
    BUTTONID_ENGINE_PAGE_3,
    2,
    { &dx7PitchRateState, &dx7PitchLevelState }
};

static const struct EditorPageButton dx7ButtonLevel = {
    "Level",
    BUTTONID_ENGINE_PAGE_4,
    2,
    { &dx7AmpState, &dx7ScalState }
};

static const struct EditorPageButton dx7ButtonOsc = {
    "Osc",
    BUTTONID_ENGINE_PAGE_6,
    1,
    { &dx7FreqState }
};

static const struct EditorPageButton dx7ButtonEnv = {
    "Env",
    BUTTONID_ENGINE_PAGE_7,
    2,
    { &dx7EgRateState, &dx7EgLevelState }
};

static const struct EditorMenu dx7GlobalMenu = {
    "Global",
    3,
    { &dx7ButtonEngine, &dx7ButtonLfo, &dx7ButtonPitchEnv }
};

static const struct EditorMenu dx7OperMenu = {
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
    }

    theme_.headerBackground = COLOR_GRAY_DARK;
    theme_.headerInstrumentColor = COLOR_WHITE;
    theme_.headerPresetName = COLOR_WHITE;

    theme_.pageLabelBackground = COLOR_GRAY_LIGHT;
    theme_.pageLabelColor = COLOR_BLACK;

    theme_.buttonBackground = COLOR_GRAY_LIGHT;
    theme_.buttonText = COLOR_BLACK;
    theme_.buttonTextInactive = COLOR_GRAY_DARK;
    theme_.buttonIndicatorActive = COLOR_WHITE;
    theme_.buttonIndicatorInactive = COLOR_GRAY_MEDIUM;

    theme_.paramNameColor = COLOR_GRAY_LIGHT;
    theme_.paramValueColor = COLOR_WHITE;
    theme_.paramValueChangedColor = COLOR_RED;

    theme_.buttonSelectorFilled = false;
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

const struct EditorMenu* EditorDx7::getEngineMenu(int page) const {
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

void EditorDx7::printPageHeader() {
    if (synthState_->fullState_.mainPage == 1) {
        tft_->print("Op ");
        tft_->print(operatorNumber_ + 1);
        tft_->print(" ");
    }
}

void EditorDx7::preparePage() {
    if (synthState_->fullState_.mainPage == 1) {
        tft_->clearAlgoFG();
        tft_->highlightOperator(operatorNumber_);
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
    if (color == COLOR_WHITE) {
        color = getTheme().paramValueColor;
    } else if (color == COLOR_YELLOW) {
        color = getTheme().paramValueChangedColor;
    }
    struct FullState* fullState = &synthState_->fullState_;
    const struct EditorPageButton* page = currentPage();
    if (page == nullptr) {
        return;
    }
    const EditorSlot slot = page->states[getButtonState(page)]->slots[encoder];

    uint8_t x = getEncoderX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(getTheme().backgroundColor);
    tft_->setCharColor(color);

    if (slot.id != DX7_NONE) {
        displayValue(slot.id, getDx7ParamValue(timbre()->getPatchMemory(), slot.id, operatorNumber_));
    } else {
        tft_->print("     ");
    }
}

void EditorDx7::drawAlgorithm() {
    tft_->drawAlgoTable(EditorDx7Algo::table(timbre()->getPatchMemory()[134]));
}

void EditorDx7::displayEditorTitle() {
    // 240x52 asset centred in the free title band (y=21..159).
    tft_->drawLogoAt(&dx7Logo, 0, 64);
}

EditorSelectorKind EditorDx7::selectorKind(int button) const {
    if (synthState_->fullState_.mainPage == 1 && button >= 3 && button <= 5) {
        return EditorSelectorKind::Operator;
    }
    return EditorSelectorKind::None;
}

void EditorDx7::drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) {
    if (selectorKind(button) == EditorSelectorKind::Operator) {
        drawOperatorSelector(button, 5);
        return;
    }
    drawEditorButtonDefault(button, editMenu, page);
}

void EditorDx7::printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                                   const struct EditorPageButton* page) {
    (void)editMenu;
    (void)page;
    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(getTheme().backgroundColor);
    tft_->setCharColor(getTheme().paramNameColor);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);
    if (slot.id != DX7_NONE) {
        tft_->print(dx7Params[slot.id].name);
    } else {
        tft_->print("     ");
    }
}

void EditorDx7::encoderTurned(int encoder, int ticks) {
    struct FullState* fullState = &synthState_->fullState_;

    if (fullState->mainPage < 0) {
        return;
    }

    const struct EditorPageButton* page = currentPage();
    if (page == nullptr) {
        return;
    }
    const EditorSlot slot = page->states[getButtonState(page)]->slots[encoder];

    if (slot.id == DX7_NONE) {
        return;
    }

    const struct Dx7ParamDesc* d = &dx7Params[slot.id];
    const int operatorNumber = operatorNumber_;
    uint8_t oldValue = getDx7ParamValue(timbre()->getPatchMemory(), slot.id, operatorNumber);
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

    synthState_->propagateNewEngineParamValue(currentTimbre(), slot.id, operatorNumber, oldValue, newValue);
}

void EditorDx7::newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) {
    (void)refreshStatus;
    (void)timbre;
    (void)context;
    (void)oldValue;

    struct FullState* fullState = &synthState_->fullState_;
    if (fullState->mainPage < 0 || fullState->mainPage > 1 || paramId <= DX7_NONE || paramId >= DX7_PARAM_COUNT) {
        return;
    }
    const struct EditorPageButton* page = currentPage();
    if (page == nullptr) {
        return;
    }

    // Need to know where on the page the parameter is, to update it.
    int slotPosition = -1;
    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        const EditorSlot slot = page->states[getButtonState(page)]->slots[e];
        if (slot.id == paramId) {
            slotPosition = e;
            break;
        }
    }

    if (paramId == DX7_FEEDBACK && slotPosition >= 0) {
        uint8_t source = 0;
        uint8_t dest = 0;
        if (EditorDx7Algo::getFeedback(this->timbre()->getPatchMemory()[134], source, dest)) {
            tft_->highlightFeedback(slotPosition, source, dest);
            feedbackChangedCounter_ = 4;
            feedbackSource_ = source;
            feedbackDest_ = dest;
        }
    }

    if (paramId >= DX7_EG_RATE_1 && paramId <= DX7_EG_LEVEL_4) {
        refreshOscillatorOperatorEnvelope();
    }
    if (paramId == DX7_ALGORITHM) {
        tft_->drawAlgoTable(EditorDx7Algo::table((int)newValue));
    }

    if (slotPosition >= 0) {
        valueChangedCounter_[slotPosition] = 4;
        tft_->setCharBackgroundColor(getTheme().backgroundColor);
        tft_->setCharColor(COLOR_YELLOW);
        displayParamValue(slotPosition, COLOR_YELLOW);
    }
}

void EditorDx7::encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {
    // DX7 has no multiple-operator edition yet.
}

void EditorDx7::buttonPressed(int button) {
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
        if (selectorKind(button) == EditorSelectorKind::Operator) {
            selectOperator(button, 5);
        } else if (editPage_ == button) {
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

void EditorDx7::buttonLongPressed(int instrument, int button) {
    int mainPage = synthState_->fullState_.mainPage;

    if (mainPage != 1) {
        return;
    }

    uint8_t* program = timbre()->getPatchMemory();
    int block = (5 - operatorNumber_) * 21;

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
    int op = operatorNumber_;
    Dx7EnvelopeDisplay env;
    getDx7EnvelopeDisplay(timbre()->getPatchMemory(), op, &env);

    tft_->oscilloBgSetEnvelope(env.time[0], env.time[1], env.time[2], env.time[3], env.level[0], env.level[1], env.level[2], env.level[3],
                               CURVE_TYPE_LIN, CURVE_TYPE_LIN, CURVE_TYPE_LIN, CURVE_TYPE_LIN);
    tft_->oscilloBgActionEnvelope();
}

void EditorDx7::slowTick() {
    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        if (valueChangedCounter_[e] > 0) {
            valueChangedCounter_[e]--;
            if (valueChangedCounter_[e] == 0 && synthState_->fullState_.mainPage >= 0) {
                tft_->setCharBackgroundColor(getTheme().backgroundColor);
                displayParamValue(e, COLOR_WHITE);
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
