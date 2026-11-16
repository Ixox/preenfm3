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

#include "ScreenMsfaEditor.h"

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

enum MsfaParamId {
    MSFA_NONE = 0,
    MSFA_ALGORITHM,
    MSFA_FEEDBACK,
    MSFA_OSC_KEY_SYNC,
    MSFA_LFO_SPEED,
    MSFA_LFO_DELAY,
    MSFA_LFO_PITCH_MOD_DEPTH,
    MSFA_LFO_AMP_MOD_DEPTH,
    MSFA_LFO_KEY_SYNC,
    MSFA_LFO_WAVEFORM,
    MSFA_LFO_PITCH_MOD_SENSITIVITY,
    MSFA_EG_RATE_1,
    MSFA_EG_RATE_2,
    MSFA_EG_RATE_3,
    MSFA_EG_RATE_4,
    MSFA_EG_LEVEL_1,
    MSFA_EG_LEVEL_2,
    MSFA_EG_LEVEL_3,
    MSFA_EG_LEVEL_4,
    MSFA_LEVEL_SCALING_BREAK_POINT,
    MSFA_LEVEL_SCALING_LEFT_DEPTH,
    MSFA_LEVEL_SCALING_RIGHT_DEPTH,
    MSFA_LEVEL_SCALING_LEFT_CURVE,
    MSFA_LEVEL_SCALING_RIGHT_CURVE,
    MSFA_RATE_SCALING,
    MSFA_AMPLITUDE_MOD_SENSITIVITY,
    MSFA_KEY_VELOCITY_SENSITIVITY,
    MSFA_OPERATOR_OUTPUT_LEVEL,
    MSFA_OSCILLATOR_MODE,
    MSFA_COARSE_FREQUENCY,
    MSFA_FINE_FREQUENCY,
    MSFA_DETUNE,
    MSFA_OPERATOR_ENABLED,
    MSFA_PITCH_EG_RATE_1,
    MSFA_PITCH_EG_RATE_2,
    MSFA_PITCH_EG_RATE_3,
    MSFA_PITCH_EG_RATE_4,
    MSFA_PITCH_EG_LEVEL_1,
    MSFA_PITCH_EG_LEVEL_2,
    MSFA_PITCH_EG_LEVEL_3,
    MSFA_PITCH_EG_LEVEL_4,
    MSFA_PARAM_COUNT
};

enum MsfaParamKind {
    MSFA_KIND_GLOBAL,
    MSFA_KIND_OPERATOR,
    MSFA_KIND_PITCH_RATE,
    MSFA_KIND_PITCH_LEVEL,
    MSFA_KIND_OP_ENABLED
};

enum MsfaDisplayType {
    MSFA_DISPLAY_INT,
    MSFA_DISPLAY_STRINGS,
    MSFA_DISPLAY_ALGO,
    MSFA_DISPLAY_DETUNE
};

struct MsfaParamDesc {
    const char *name;
    uint8_t kind;
    uint8_t offset;
    uint8_t minValue;
    uint8_t maxValue;
    uint8_t displayType;
    const char **valueNames;
};

static const char *msfaOffOn[] = { "Off", "On " };
static const char *msfaOscMode[] = { "Ratio", "Fixed" };
static const char *msfaLfoWave[] = { "Tri", "Saw-", "Saw+", "Sqr", "Sin", "S&H" };
static const char *msfaCurve[] = { "-Lin", "-Exp", "+Exp", "+Lin" };

static const struct MsfaParamDesc msfaParams[MSFA_PARAM_COUNT] = {
    // MSFA_NONE - Empty slot
    { "",        MSFA_KIND_GLOBAL,      0,   0,  0,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_ALGORITHM - Algorithm (DX7 algorithm number, displayed 1-32)
    { "Algo",    MSFA_KIND_GLOBAL,      134, 0,  31, MSFA_DISPLAY_ALGO,    nullptr },
    // MSFA_FEEDBACK - Feedback amount
    { "Fdbk",    MSFA_KIND_GLOBAL,      135, 0,  7,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_OSC_KEY_SYNC - Oscillator key sync
    { "OSync",   MSFA_KIND_GLOBAL,      136, 0,  1,  MSFA_DISPLAY_STRINGS, msfaOffOn },
    // MSFA_LFO_SPEED - LFO speed
    { "Speed", MSFA_KIND_GLOBAL,      137, 0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LFO_DELAY - LFO delay
    { "Delay", MSFA_KIND_GLOBAL,      138, 0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LFO_PITCH_MOD_DEPTH - LFO pitch modulation depth
    { "Pitch",   MSFA_KIND_GLOBAL,      139, 0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LFO_AMP_MOD_DEPTH - LFO amplitude modulation depth
    { "Amp",   MSFA_KIND_GLOBAL,      140, 0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LFO_KEY_SYNC - LFO key sync
    { "K Sync",  MSFA_KIND_GLOBAL,      141, 0,  1,  MSFA_DISPLAY_STRINGS, msfaOffOn },
    // MSFA_LFO_WAVEFORM - LFO waveform
    { "Wave",    MSFA_KIND_GLOBAL,      142, 0,  5,  MSFA_DISPLAY_STRINGS, msfaLfoWave },
    // MSFA_LFO_PITCH_MOD_SENSITIVITY - LFO pitch modulation sensitivity
    { "PitchM",   MSFA_KIND_GLOBAL,      143, 0,  7,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_RATE_1 - Envelope generator rate 1
    { "Rate 1",      MSFA_KIND_OPERATOR,    0,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_RATE_2 - Envelope generator rate 2
    { "Rate 2",      MSFA_KIND_OPERATOR,    1,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_RATE_3 - Envelope generator rate 3
    { "Rate 3",      MSFA_KIND_OPERATOR,    2,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_RATE_4 - Envelope generator rate 4
    { "Rate 4",      MSFA_KIND_OPERATOR,    3,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_LEVEL_1 - Envelope generator level 1
    { "Lvl 1",      MSFA_KIND_OPERATOR,    4,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_LEVEL_2 - Envelope generator level 2
    { "Lvl 2",      MSFA_KIND_OPERATOR,    5,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_LEVEL_3 - Envelope generator level 3
    { "Lvl 3",      MSFA_KIND_OPERATOR,    6,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_EG_LEVEL_4 - Envelope generator level 4
    { "Lvl 4",      MSFA_KIND_OPERATOR,    7,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LEVEL_SCALING_BREAK_POINT - Level scaling break point
    { "Break",   MSFA_KIND_OPERATOR,    8,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LEVEL_SCALING_LEFT_DEPTH - Level scaling left depth
    { "Lvl Dp", MSFA_KIND_OPERATOR,    9,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LEVEL_SCALING_RIGHT_DEPTH - Level scaling right depth
    { "Rt Dep", MSFA_KIND_OPERATOR,    10,  0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_LEVEL_SCALING_LEFT_CURVE - Level scaling left curve
    { "Lv Curv", MSFA_KIND_OPERATOR,    11,  0,  3,  MSFA_DISPLAY_STRINGS, msfaCurve },
    // MSFA_LEVEL_SCALING_RIGHT_CURVE - Level scaling right curve
    { "Rt Curv", MSFA_KIND_OPERATOR,    12,  0,  3,  MSFA_DISPLAY_STRINGS, msfaCurve },
    // MSFA_RATE_SCALING - Rate scaling
    { "Rt Sc", MSFA_KIND_OPERATOR,    13,  0,  7,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_AMPLITUDE_MOD_SENSITIVITY - Amplitude modulation sensitivity
    { "A Mod",     MSFA_KIND_OPERATOR,    14,  0,  3,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_KEY_VELOCITY_SENSITIVITY - Key velocity sensitivity
    { "K Velo",     MSFA_KIND_OPERATOR,    15,  0,  7,  MSFA_DISPLAY_INT,     nullptr },
    // MSFA_OPERATOR_OUTPUT_LEVEL - Operator output level
    { "Out Lvl", MSFA_KIND_OPERATOR,    16,  0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_OSCILLATOR_MODE - Oscillator mode (ratio or fixed)
    { "Mode",    MSFA_KIND_OPERATOR,    17,  0,  1,  MSFA_DISPLAY_STRINGS, msfaOscMode },
    // MSFA_COARSE_FREQUENCY - Coarse frequency
    { "Coarse",  MSFA_KIND_OPERATOR,    18,  0,  31, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_FINE_FREQUENCY - Fine frequency
    { "Fine",    MSFA_KIND_OPERATOR,    19,  0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_DETUNE - Detune
    { "Detune",  MSFA_KIND_OPERATOR,    20,  0,  14, MSFA_DISPLAY_DETUNE,  nullptr },
    // MSFA_OPERATOR_ENABLED - Operator enabled (on/off)
    { "Op On",   MSFA_KIND_OP_ENABLED,  0,   0,  1,  MSFA_DISPLAY_STRINGS, msfaOffOn },
    // MSFA_PITCH_EG_RATE_1 - Pitch envelope generator rate 1
    { "Rate 1",    MSFA_KIND_PITCH_RATE,  0,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_RATE_2 - Pitch envelope generator rate 2
    { "Rate 2",    MSFA_KIND_PITCH_RATE,  1,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_RATE_3 - Pitch envelope generator rate 3
    { "Rate 3",    MSFA_KIND_PITCH_RATE,  2,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_RATE_4 - Pitch envelope generator rate 4
    { "Rate 4",    MSFA_KIND_PITCH_RATE,  3,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_LEVEL_1 - Pitch envelope generator level 1
    { "Lvl 1",    MSFA_KIND_PITCH_LEVEL, 0,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_LEVEL_2 - Pitch envelope generator level 2
    { "Lvl 2",    MSFA_KIND_PITCH_LEVEL, 1,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_LEVEL_3 - Pitch envelope generator level 3
    { "Lvl 3",    MSFA_KIND_PITCH_LEVEL, 2,   0,  99, MSFA_DISPLAY_INT,     nullptr },
    // MSFA_PITCH_EG_LEVEL_4 - Pitch envelope generator level 4
    { "Lvl 4",    MSFA_KIND_PITCH_LEVEL, 3,   0,  99, MSFA_DISPLAY_INT,     nullptr }
};

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
    BUTTONID_MSFA_MAIN,
    1,
    { &msfaEngineState }
};

static const struct EditorOneButton msfaButtonLfo = {
    "LFO",
    BUTTONID_MSFA_LFO,
    2,
    { &msfaLfoModState, &msfaLfoState }
};

static const struct EditorOneButton msfaButtonPitchEnv = {
    "Pitch",
    BUTTONID_MSFA_PITCH_ENV,
    2,
    { &msfaPitchRateState, &msfaPitchLevelState }
};

static const struct EditorOneButton msfaButtonLevel = {
    "Level",
    BUTTONID_MSFA_AMP,
    2,
    { &msfaAmpState, &msfaScalState }
};

static const struct EditorOneButton msfaButtonOsc = {
    "Osc",
    BUTTONID_MSFA_FREQ,
    1,
    { &msfaFreqState }
};

static const struct EditorOneButton msfaButtonEnv = {
    "Env",
    BUTTONID_MSFA_EG_RATE,
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

static const struct EditorEditMenu *msfaEditMenus[2] = {
    &msfaGlobalMenu,
    &msfaOperMenu
};

ScreenMsfaEditor::ScreenMsfaEditor() {
    parent_ = nullptr;
    synthState_ = nullptr;
    tft_ = nullptr;
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

void ScreenMsfaEditor::init(ScreenEditor *parent, SynthState *synthState, FirmwareTftDisplay *tft) {
    parent_ = parent;
    synthState_ = synthState;
    tft_ = tft;
}

const struct EditorEditMenu* ScreenMsfaEditor::getEditMenu(int page) const {
    if (page >= 0 && page < 2) {
        return msfaEditMenus[page];
    }
    return nullptr;
}

uint8_t ScreenMsfaEditor::getX(int encoder) {
    uint8_t x = (encoder) * 8;
    if (x >= 24) {
        x -= 24;
    }
    return x;
}

int ScreenMsfaEditor::currentTimbre() const {
    return synthState_->getCurrentTimbre();
}

Timbre* ScreenMsfaEditor::timbre() {
    return synthState_->getTimbre(currentTimbre());
}

uint8_t ScreenMsfaEditor::paramValue(uint8_t paramId) {
    const struct MsfaParamDesc *d = &msfaParams[paramId];
    uint8_t *program = timbre()->getMsfaProgramMutable();

    switch (d->kind) {
    case MSFA_KIND_GLOBAL:
        return program[d->offset];
    case MSFA_KIND_OPERATOR:
        return program[(5 - synthState_->fullState.operatorNumber) * 21 + d->offset];
    case MSFA_KIND_PITCH_RATE:
        return program[126 + d->offset];
    case MSFA_KIND_PITCH_LEVEL:
        return program[130 + d->offset];
    case MSFA_KIND_OP_ENABLED:
        return (program[155] >> (5 - synthState_->fullState.operatorNumber)) & 0x1;
    }
    return 0;
}

void ScreenMsfaEditor::setParamValue(uint8_t paramId, uint8_t value) {
    const struct MsfaParamDesc *d = &msfaParams[paramId];
    uint8_t *program = timbre()->getMsfaProgramMutable();

    switch (d->kind) {
    case MSFA_KIND_GLOBAL:
        program[d->offset] = value;
        break;
    case MSFA_KIND_OPERATOR:
        program[(5 - synthState_->fullState.operatorNumber) * 21 + d->offset] = value;
        break;
    case MSFA_KIND_PITCH_RATE:
        program[126 + d->offset] = value;
        break;
    case MSFA_KIND_PITCH_LEVEL:
        program[130 + d->offset] = value;
        break;
    case MSFA_KIND_OP_ENABLED: {
        uint8_t bit = 5 - synthState_->fullState.operatorNumber;
        if (value) {
            program[155] |= (1U << bit);
        } else {
            program[155] &= ~(1U << bit);
        }
        break;
    }
    }
}

void ScreenMsfaEditor::displayValue(uint8_t paramId, uint8_t value) {
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

void ScreenMsfaEditor::displayParamValue(int encoder, TFT_COLOR color) {
    struct FullState *fullState = &synthState_->fullState;
    const struct EditorEditMenu *editMenu = getEditMenu(fullState->mainPage);
    const struct EditorOneButton *page = editMenu->pages[fullState->editPage];
    const struct RowEncoder rowEncoder =
        page->states[fullState->buttonState[page->buttonId]]->rowEncoder[encoder];

    uint8_t x = getX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);

    if (rowEncoder.row != MSFA_NONE) {
        displayValue(rowEncoder.row, paramValue(rowEncoder.row));
    } else {
        tft_->print("     ");
    }
}

void ScreenMsfaEditor::refreshEditorByStep(int &refreshStatus, int &endRefreshStatus) {
    struct FullState *fullState = &synthState_->fullState;
    const struct EditorEditMenu *editMenu = getEditMenu(fullState->mainPage);
    const struct EditorOneButton *page = editMenu->pages[fullState->editPage];

    switch (refreshStatus) {
    case 21:
        tft_->pauseRefresh();
        tft_->clear();
        parent_->displayPreset();
        tft_->oscilloForceNextDisplay();
        tft_->drawDx7Algo(timbre()->getMsfaProgram()[134]);
        break;
    case 20:
        tft_->pauseRefresh();
        tft_->fillArea(0, 40, 240, 120, COLOR_BLACK);
        tft_->fillArea(0, 270, 240, 30, COLOR_BLACK);
        tft_->setCursor(0, 2);
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(COLOR_CYAN);
        if (fullState->mainPage == 1) {
            tft_->print("Op ");
            tft_->print(fullState->operatorNumber + 1);
            tft_->print(" ");
        }
        tft_->print(page->states[fullState->buttonState[page->buttonId]]->stateLabel);
        tft_->drawDx7Algo(timbre()->getMsfaProgram()[134]);
        break;
    case 18:
    case 17:
    case 16:
    case 15:
    case 14:
    case 13: {
        int button = 18 - refreshStatus;

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
        break;
    }
    case 12:
    case 11:
    case 10:
    case 9:
    case 8:
    case 7: {
        int button = 12 - refreshStatus;
        const struct RowEncoder rowEncoder =
            page->states[fullState->buttonState[page->buttonId]]->rowEncoder[button];
        uint8_t x = getX(button);

        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(COLOR_MEDIUM_GRAY);
        tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);

        if (rowEncoder.row != MSFA_NONE) {
            tft_->print(msfaParams[rowEncoder.row].name);
        } else {
            tft_->print("     ");
        }
        break;
    }
    case 6:
    case 5:
    case 4:
    case 3:
    case 2:
    case 1:
        displayParamValue(6 - refreshStatus, COLOR_WHITE);
        break;
    }

    if (refreshStatus == endRefreshStatus) {
        endRefreshStatus = 0;
        refreshStatus = 0;
    }
}

void ScreenMsfaEditor::encoderTurned(int encoder6, int ticks) {
    struct FullState *fullState = &synthState_->fullState;

    if (fullState->mainPage < 0) {
        return;
    }

    const struct EditorEditMenu *editMenu = getEditMenu(fullState->mainPage);
    const struct EditorOneButton *page = editMenu->pages[fullState->editPage];
    const struct RowEncoder rowEncoder =
        page->states[fullState->buttonState[page->buttonId]]->rowEncoder[encoder6];

    if (rowEncoder.row == MSFA_NONE) {
        return;
    }

    const struct MsfaParamDesc *d = &msfaParams[rowEncoder.row];
    uint8_t oldValue = paramValue(rowEncoder.row);
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

    setParamValue(rowEncoder.row, (uint8_t) newValue);
    applyProgramChange();

    int refreshStatus = 0;
    newParamValue(refreshStatus, currentTimbre(), rowEncoder.row, encoder6, nullptr, oldValue, newValue);

    if (rowEncoder.row == MSFA_ALGORITHM) {
        tft_->drawDx7Algo(newValue);
    }
    parent_->setPresetModified(currentTimbre(), true);

    valueChangedCounter_[encoder6] = 4;
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_YELLOW);
    displayParamValue(encoder6, COLOR_YELLOW);
}

void ScreenMsfaEditor::newParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
    ParameterDisplay *param, float oldValue, float newValue) {
    // MSFA parameter changes come from encoderTurned with param == nullptr.
    // PFM3 rows (Arp/Fx/Perf) also reach this function through ScreenEditor.
    if (param != nullptr || currentRow <= MSFA_NONE || currentRow >= MSFA_PARAM_COUNT) {
        return;
    }

    if (currentRow == MSFA_FEEDBACK) {
        uint8_t source = 0;
        uint8_t dest = 0;
        if (tft_->getDx7Feedback(this->timbre()->getMsfaProgram()[134], source, dest)) {
            tft_->highlightFeedback(encoder, source, dest);
            feedbackChangedCounter_ = 4;
            feedbackSource_ = source;
            feedbackDest_ = dest;
        }
    } else if (msfaParams[currentRow].kind == MSFA_KIND_OPERATOR
            || msfaParams[currentRow].kind == MSFA_KIND_OP_ENABLED) {
        uint8_t op = synthState_->fullState.operatorNumber;
        tft_->highlightOperator(op);
        operatorChangedCounter_[encoder] = 4;
        highlightedOperator_[encoder] = op;
    }

    if (currentRow >= MSFA_EG_RATE_1 && currentRow <= MSFA_EG_LEVEL_4) {
        parent_->refreshOscillatorOperatorEnvelope();
    }
}

void ScreenMsfaEditor::encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button) {
    // MSFA has no multiple-operator edition yet.
}

void ScreenMsfaEditor::buttonPressed(int button) {
    struct FullState *fullState = &synthState_->fullState;
    int mainPage = fullState->mainPage;
    const struct EditorEditMenu *editMenu = getEditMenu(mainPage);
    const struct EditorOneButton *page = editMenu->pages[fullState->editPage];

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

void ScreenMsfaEditor::buttonLongPressed(int instrument, int button) {
    int mainPage = synthState_->fullState.mainPage;

    if (mainPage != 1) {
        return;
    }

    uint8_t *program = timbre()->getMsfaProgramMutable();
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

void ScreenMsfaEditor::refreshOscillatorOperatorShape() {
    tft_->oscilloBgActionClear();
}

void ScreenMsfaEditor::refreshLfoOscillator() {
    tft_->oscilloBgActionClear();
}

void ScreenMsfaEditor::refreshLfoEnv() {
    tft_->oscilloBgActionClear();
}

void ScreenMsfaEditor::tempoClick() {
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

void ScreenMsfaEditor::applyProgramChange() {
    timbre()->msfaAfterNewParamsLoad();
}
