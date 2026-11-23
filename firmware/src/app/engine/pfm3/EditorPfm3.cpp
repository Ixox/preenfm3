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

#include "EditorPfm3.h"
#include "EditorPfm3Algo.h"
#include "Pfm3Logo.h"
#include "Pfm3ParamUiData.h"
#include "Pfm3Preset.h"
#include "ScreenEditor.h"
#include "ScreenTypes.h"
#include "SynthState.h"
#include <FirmwareTftDisplay.h>

EditorPfm3::EditorPfm3() {
    for (int i = 0; i < NUMBER_OF_ENCODERS; i++) {
        imChangedCounter_[i] = 0;
        mixChangedCounter_[i] = 0;
        mixChangeOperatorNumber_[i] = 0;
    }
    multipleEdition_ = false;
    bufferPageNumber_ = -1;
    bufferEditNumber_ = 0;
    for (int i = 0; i < 6; i++) {
        bufferParam_[i] = 0.0f;
    }
    
    theme_.buttonSelectorFilled = false;
}

const char* syncNames[] = {"Int", "Ext"};

// =================================================
// constant for pfm3 editor
// =================================================

/*
 * Generic slot description for the Engine/Mono pages, used by the generic
 * branches in printParameterName/displayParamValue/encoderTurned.
 */
// clang-format off
static const EditorPageState engineGenericState =
    {"Engine",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Engine1, 0, 0)), // Algo
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Engine1, 0, 1)), // Velo
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Engine1, 0, 2)), // Mode
         BUTTON_EMPTY(),
     }};

static const EditorPageState monoGenericState =
    {"Mono",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Engine2, 0, 0)), // Glide
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Engine1, 0, 3)), // Speed
         BUTTON_EMPTY(),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Engine2, 0, 1)), // Spread
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Engine2, 0, 2)), // Detune
         BUTTON_EMPTY(),
     }};

static const EditorPageState imIndexGenericState =
    {"Modulation Index",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 1, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 2, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 3, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 4, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 5, 0)),
     }};

static const EditorPageState imVelocityGenericState =
    {"Velocity",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 1, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 2, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 3, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 4, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Mod, 5, 1)),
     }};

static const EditorPageState mixer123GenericState =
    {"Op Mixer",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 1, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 2, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 1, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 2, 1)),
     }};

static const EditorPageState mixer456GenericState =
    {"Op Mixer",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 3, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 4, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 5, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 3, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 4, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::OscMix, 5, 1)),
     }};

/*
 * Dynamic pages: static slot layouts patched in place with the selected
 * operator / matrix row / LFO / envelope / sequencer / midi curve.
 */
static EditorPageState oscPage =
    {"Oscillator",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Osc, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Osc, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Osc, 0, 2)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Osc, 0, 3)),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
     }};

static EditorPageState envTimePage =
    {"Env Time",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvTime, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvTime, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvTime, 0, 2)),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvTime, 0, 3)),
     }};

static EditorPageState envCurvePage =
    {"Env Curve",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvCurve, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvCurve, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvCurve, 0, 2)),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvCurve, 0, 3)),
     }};

static EditorPageState envLevelPage =
    {"Env Level",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvLevel, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvLevel, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvLevel, 0, 2)),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::EnvLevel, 0, 3)),
     }};

static EditorPageState matrixPage =
    {"",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Matrix, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Matrix, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Matrix, 0, 2)),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Matrix, 0, 3)),
     }};

static EditorPageState lfoPage =
    {"",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoOsc, 0, 0)),
         {
             EditorSlotKind::Action,
             (uint16_t)EditorActionId::LfoSync,
             0,
         },
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoOsc, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoOsc, 0, 3)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoOsc, 0, 2)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoPhase, 0, 0)),
     }};

static EditorPageState lfoEnvPage =
    {"",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoEnv1, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoEnv1, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoEnv1, 0, 2)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoEnv1, 0, 3)),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
     }};

static EditorPageState stepSeqPage =
    {"",
     {
         {
             EditorSlotKind::Action,
             (uint16_t)EditorActionId::StepSync,
             0,
         },
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoSeq, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::LfoSeq, 0, 1)),
         {
             EditorSlotKind::Custom,
             0,
             0,
         },
         {
             EditorSlotKind::Custom,
             1,
             0,
         },
         BUTTON_EMPTY(),
     }};

static EditorPageState midiPage =
    {"",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::MidiCurve, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::MidiCurve, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::MidiCurve, 0, 2)),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
     }};

static EditorPageState emptyPage =
    {"",
     {
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
         BUTTON_EMPTY(),
     }};

static const EditorPageState perfPage =
    {"Performance",
     {
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Performance, 0, 0)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Performance, 0, 1)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Performance, 0, 2)),
         BUTTON_SIMPLE_PARAM((uint16_t)pfm3Param(Pfm3Block::Performance, 0, 3)),
         BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect1, 0, 1), 1),
         BUTTON_SIMPLE_PARAM((uint16_t)commonParam(CommonBlock::Effect1, 0, 2), 1),
     }};

const struct EditorPageButton pfm3ButtonEngine =
    {"Engine",
     BUTTONID_EDIT_ENGINE,
     2,
     {
         &engineGenericState,
         &monoGenericState,
     }};

const struct EditorPageButton pfm3ButtonIM =
    {"MI",
     BUTTONID_EDIT_IM,
     2,
     {
         &imIndexGenericState,
         &imVelocityGenericState,
     }};

const struct EditorPageButton pfm3ButtonMixer =
    {"Mix",
     BUTTONID_EDIT_MIX,
     2,
     {
         &mixer123GenericState,
         &mixer456GenericState,
     }};

const struct EditorPageButton pfm3ButtonOPEnv2 =
    {"Level",
     BUTTONID_ONLY_ONE_STATE,
     1,
     {
         &envLevelPage,
     }};

const struct EditorPageButton pfm3ButtonOPEnv1 =
    {"Env",
     BUTTONID_ENV_1,
     2,
     {
         &envTimePage,
         &envCurvePage,
     }};

const struct EditorPageButton pfm3ButtonOPShape =
    {"Osc",
     BUTTONID_ONLY_ONE_STATE,
     1,
     {
         &oscPage,
     }};

const struct EditorPageButton pfm3ButtonOP1_2 =
    {"1-2",
     BUTTONID_OPERATOR_1_2,
     2,
     {
         &emptyPage,
         &emptyPage,
     }};

const struct EditorPageButton pfm3ButtonOP3_4 =
    {"3-4",
     BUTTONID_OPERATOR_3_4,
     2,
     {
         &emptyPage,
         &emptyPage,
     }};

const struct EditorPageButton pfm3ButtonOP5_6 =
    {"5-6",
     BUTTONID_OPERATOR_5_6,
     2,
     {
         &emptyPage,
         &emptyPage,
     }};

const struct EditorPageButton pfm3ButtonMatrix1_2 =
    {"1-2",
     BUTTONID_MATRIX_1_2,
     2,
     {
         &matrixPage,
         &matrixPage,
     }};

const struct EditorPageButton pfm3ButtonMatrix3_4 =
    {"3-4",
     BUTTONID_MATRIX_3_4,
     2,
     {
         &matrixPage,
         &matrixPage,
     }};

const struct EditorPageButton pfm3ButtonMatrix5_6 =
    {"5-6",
     BUTTONID_MATRIX_5_6,
     2,
     {
         &matrixPage,
         &matrixPage,
     }};

const struct EditorPageButton pfm3ButtonMatrix7_8 =
    {"7-8",
     BUTTONID_MATRIX_7_8,
     2,
     {
         &matrixPage,
         &matrixPage,
     }};

const struct EditorPageButton pfm3ButtonMatrix9_10 =
    {"9-10",
     BUTTONID_MATRIX_9_10,
     2,
     {
         &matrixPage,
         &matrixPage,
     }};

const struct EditorPageButton pfm3ButtonMatrix11_12 =
    {"11-12",
     BUTTONID_MATRIX_11_12,
     2,
     {
         &matrixPage,
         &matrixPage,
     }};

const struct EditorPageButton pfm3ButtonLfos =
    {"Lfos",
     BUTTONID_EDIT_LFOS,
     3,
     {
         &lfoPage,
         &lfoPage,
         &lfoPage,
     }};

const struct EditorPageButton pfm3ButtonEnvs =
    {"Envs",
     BUTTONID_EDIT_ENVS,
     2,
     {
         &lfoEnvPage,
         &lfoEnvPage,
     }};

const struct EditorPageButton pfm3ButtonSteps =
    {"Steps",
     BUTTONID_EDIT_STEPS,
     2,
     {
         &stepSeqPage,
         &stepSeqPage,
     }};

const struct EditorPageButton pfm3ButtonMidiNotes =
    {"Midi",
     BUTTONID_EDIT_NOTES,
     2,
     {
         &midiPage,
         &midiPage,
     }};

const struct EditorMenu operatorEditMenu =
    {"Oper",
     6,
     {
         &pfm3ButtonOPShape,
         &pfm3ButtonOPEnv1,
         &pfm3ButtonOPEnv2,
         &pfm3ButtonOP1_2,
         &pfm3ButtonOP3_4,
         &pfm3ButtonOP5_6,
     }};

const struct EditorMenu modulatorEditorMenu =
    {"Modul",
     4,
     {
         &pfm3ButtonLfos,
         &pfm3ButtonEnvs,
         &pfm3ButtonSteps,
         &pfm3ButtonMidiNotes,
     }};

const struct EditorMenu pfm3EngineMenu =
    {"FM",
     3,
     {
         &pfm3ButtonEngine,
         &pfm3ButtonIM,
         &pfm3ButtonMixer,
     }};

const struct EditorMenu pfm3MatrixMenu =
    {"Matrix",
     6,
     {
         &pfm3ButtonMatrix1_2,
         &pfm3ButtonMatrix3_4,
         &pfm3ButtonMatrix5_6,
         &pfm3ButtonMatrix7_8,
         &pfm3ButtonMatrix9_10,
         &pfm3ButtonMatrix11_12,
     }};

const struct EditorPageButton perfEditorPage =
    {"Perf",
     BUTTONID_PERFORMANCE,
     1,
     {
         &perfPage,
     }};

static const struct EditorMenu pfm3PerfMenu =
    {"Perf",
     1,
     {
         &perfEditorPage,
     }};

static const struct EditorMainMenu mainMenu =
    {&pfm3EngineMenu,
     &operatorEditMenu,
     &pfm3MatrixMenu,
     &modulatorEditorMenu,
     nullptr,
     &pfm3PerfMenu};

// clang-format on

uint8_t EditorPfm3::currentElement() const {
    const struct FullState* fs = &synthState_->fullState_;
    int mainPage = fs->mainPage;
    if (mainPage == 1) {
        uint8_t op = (uint8_t)operatorNumber_;
        return op > 5 ? 0 : op;
    }
    if (mainPage == 2) {
        int editPage = editPage_;
        if (editPage < 0 || editPage > 5) {
            editPage = 0;
        }
        uint8_t state = fs->buttonState[BUTTONID_MATRIX_1_2 + editPage];
        uint8_t row = (uint8_t)(editPage * 2 + (state & 0x1));
        return row > 11 ? 11 : row;
    }
    if (mainPage == 3) {
        switch (editPage_) {
        case 0: {
            uint8_t lfo = fs->buttonState[BUTTONID_EDIT_LFOS];
            return lfo > 2 ? 0 : lfo;
        }
        case 1:
            return fs->buttonState[BUTTONID_EDIT_ENVS] == 0 ? 0 : 1;
        case 2: {
            uint8_t step = fs->buttonState[BUTTONID_EDIT_STEPS];
            return step > 1 ? 0 : step;
        }
        case 3:
            return fs->buttonState[BUTTONID_EDIT_NOTES] == 0 ? 0 : 1;
        default:
            return 0;
        }
    }
    return 0;
}

void EditorPfm3::updatePage() const {
    int mainPage = synthState_->fullState_.mainPage;
    if (mainPage != 1 && mainPage != 2 && mainPage != 3) {
        return;
    }
    const struct EditorPageButton* button = currentPage();
    if (button == nullptr) {
        return;
    }
    uint8_t state = getButtonState(button->buttonId);
    if (state >= (uint8_t)button->stateCount) {
        state = 0;
    }
    EditorPageState* page = const_cast<EditorPageState*>(button->states[state]);
    uint8_t element = currentElement();

    for (int i = 0; i < NUMBER_OF_ENCODERS; i++) {
        EditorSlot& slot = page->slots[i];
        if (slot.kind == EditorSlotKind::Param && slot.context == 0) {
            Pfm3Param id = (Pfm3Param)slot.id;
            uint8_t block = pfm3ParamBlockOf(id);
            if (block == (uint8_t)Pfm3Block::LfoPhase) {
                slot.id = (uint16_t)pfm3ParamWithField(id, element);
            } else if (block == (uint8_t)Pfm3Block::LfoEnv1 || block == (uint8_t)Pfm3Block::LfoEnv2) {
                slot.id = (uint16_t)pfm3ParamWithBlock(id, element == 0 ? Pfm3Block::LfoEnv1 : Pfm3Block::LfoEnv2);
            } else {
                slot.id = (uint16_t)pfm3ParamWithElement(id, element);
            }
        } else if (slot.kind == EditorSlotKind::Action || slot.kind == EditorSlotKind::Custom) {
            slot.context = element;
        }
    }
}

void EditorPfm3::init(ScreenEditor* parent, SynthState* synthState, FirmwareTftDisplay* tft) {
    parent_ = parent;
    synthState_ = synthState;
    tft_ = tft;
    parent->insertEditor(this);

    EditorPfm3Algo::init();
}

SynthEngineType EditorPfm3::getEngineType() const {
    return SYNTH_ENGINE_PFM3;
}

const char* EditorPfm3::getEngineName() const {
    return "preenfm3";
}

void EditorPfm3::displayEditorTitle() {
    // 100x100 asset centred in the free title band (y=21..159).
    tft_->drawLogoAt(&pfm3Logo, 70, 40);
}

const struct EditorMenu* EditorPfm3::getEngineMenu(int page) const {
    if (page >= 0 && page < 6) {
        return mainMenu.editMenu[page];
    }
    return nullptr;
}

int EditorPfm3::getCommonPage(int page) const {
    if (page == 4) {
        return COMMON_PAGE_ARP_FX;
    }
    return COMMON_PAGE_NONE;
}

void EditorPfm3::newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) {
    (void)refreshStatus;
    (void)timbre;
    (void)oldValue;

    if (context == 0 && pfm3ParamBlockOf((Pfm3Param)paramId) == (uint8_t)Pfm3Block::StepSeq) {
        displayParamValue(3, COLOR_WHITE);
        return;
    }
    if (context == 1 && commonBlockOf((CommonParamId)paramId) == (uint8_t)CommonBlock::ArpPattern) {
        displayParamValue(3, COLOR_WHITE);
        return;
    }
    int encoder = -1;
    for (int enc = 0; enc < NUMBER_OF_ENCODERS; enc++) {
        const EditorSlot slot = slotForEncoder(enc);
        if (slot.kind == EditorSlotKind::Param && slot.id == (uint16_t)paramId && slot.context == (uint8_t)context) {
            encoder = enc;
            break;
        }
    }
    // Can be called when new param value is received on Midi
    // So encoder can often be -1
    if (encoder != -1) {
        uint16_t flatIndex = (context == 0) ? pfm3FlatIndex((Pfm3Param)paramId) : commonFlatIndex((CommonParamId)paramId);
        int encoderInRow = flatIndex % PFM3_PARAMS_PER_ROW;
        uint8_t x = getEncoderX(encoder);

        tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);

        EditorParamInfo info = paramInfoFor(encoder);

        // Special cases
        if (context == 0) {
            Pfm3Param id = (Pfm3Param)paramId;
            uint8_t block = pfm3ParamBlockOf(id);
            uint8_t field = pfm3ParamFieldOf(id);
            uint8_t element = pfm3ParamElementOf(id);

            if (block == (uint8_t)Pfm3Block::Engine1 && field == 0) {
                checkOperatorNumber();
                tft_->drawAlgoTable(EditorPfm3Algo::table(newValue));
            } else if (block == (uint8_t)Pfm3Block::EnvTime || block == (uint8_t)Pfm3Block::EnvLevel ||
                       block == (uint8_t)Pfm3Block::EnvCurve) {
                refreshOscillatorOperatorEnvelope();
            } else if (block == (uint8_t)Pfm3Block::Mod) {
                if (hideParam_[encoder]) {
                    return;
                }
                struct ModulationIndex im = EditorPfm3Algo::modulationIndex[(int)pfm3Preset()->engine1.algo][encoder];
                if (im.source != im.destination) {
                    tft_->highlightIM(encoder, im.source, im.destination);
                } else {
                    tft_->highlightFeedback(encoder, im.source, im.destination);
                }
                imChangedCounter_[encoder] = 4;
            } else if (block == (uint8_t)Pfm3Block::OscMix) {
                if (hideParam_[encoder]) {
                    return;
                }
                int currentAlgo = (int)pfm3Preset()->engine1.algo;
                int operatorNumber = EditorPfm3Algo::table(currentAlgo)[element * 2 + 1] - 1;
                tft_->highlightOperator(operatorNumber);
                mixChangedCounter_[encoder] = 4;
                mixChangeOperatorNumber_[encoder] = operatorNumber;
            } else if (block == (uint8_t)Pfm3Block::Osc) {
                switch (field) {
                case 0: // shape
                    refreshOscillatorOperatorShape();
                    break;
                case 1: // frequency type, refresh frequency and fine tune
                    refreshStatus = 10;
                    break;
                case 2: { // frequency
                    struct OscillatorParams* opParams = &pfm3Preset()->osc1;
                    if (opParams[operatorNumber_].frequencyType == OSC_FT_FIXE) {
                        refreshStatus = 9;
                    }
                    break;
                }
                case 3: { // fine tune
                    struct OscillatorParams* opParams = &pfm3Preset()->osc1;
                    if (opParams[operatorNumber_].frequencyType == OSC_FT_FIXE) {
                        refreshStatus = 10;
                    }
                    break;
                }
                }
            } else if (block == (uint8_t)Pfm3Block::LfoOsc || block == (uint8_t)Pfm3Block::LfoPhase) {
                refreshLfoOscillator();
            } else if (block == (uint8_t)Pfm3Block::LfoEnv1 || block == (uint8_t)Pfm3Block::LfoEnv2) {
                refreshLfoEnv();
            }
        } else {
            CommonBlock block = (CommonBlock)commonBlockOf((CommonParamId)paramId);
            uint8_t field = commonFieldOf((CommonParamId)paramId);
            if (block == CommonBlock::Arp2 && field == 0) {
                // Refresh pattern
                refreshStatus = 3;
            } else if ((block == CommonBlock::Effect1 || block == CommonBlock::Effect2) && field == 0) {
                resetHideParams();
                refreshStatus = 11;
            }
        }

        tft_->setCharBackgroundColor(COLOR_BLACK);
        if (!multipleEdition_) {
            tft_->setCharColor(COLOR_YELLOW);
        } else {
            tft_->setCharColor(COLOR_GREEN);
        }
        valueChangedCounter_[encoder] = 4;
        updateEncoderValueWithoutCursor(encoderInRow, (context == 0) ? (int)pfm3ParamElementOf((Pfm3Param)paramId) : 0, info, oldValue,
                                        newValue);
    }
}

void EditorPfm3::displayParamValue(int encoder, TFT_COLOR color) {
    EditorSlot slot = slotForEncoder(encoder);
    uint8_t x = getEncoderX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    if (slot.kind == EditorSlotKind::Action) {
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(color);
        if (slot.id == (uint16_t)EditorActionId::LfoSync) {
            int lfoState = synthState_->fullState_.buttonState[BUTTONID_EDIT_LFOS];
            struct LfoParams* lfoParams = &pfm3Preset()->lfoOsc1;
            tft_->print(syncNames[lfoParams[lfoState].freq < 100.0f ? 0 : 1]);
        } else {
            int stepState = synthState_->fullState_.buttonState[BUTTONID_EDIT_STEPS];
            struct StepSequencerParams* stepParams = &pfm3Preset()->lfoSeq1;
            tft_->print(syncNames[stepParams[stepState].bpm < 240.5f ? 0 : 1]);
        }
        return;
    }
    if (slot.kind == EditorSlotKind::Custom) {
        EditorParamInfo info = {};
        info.displayType = (slot.id == 0) ? DISPLAY_TYPE_STEP_SEQ1 : DISPLAY_TYPE_STEP_SEQ2;
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(color);
        updateEncoderValueWithoutCursor(2 + (int)slot.id, (int)slot.context, info, 0.0f, 0.0f);
        return;
    }
    if (slot.kind != EditorSlotKind::Param || hideParam_[encoder]) {
        if (hideParam_[encoder]) {
            tft_->print("     ");
        }
        return;
    }
    uint16_t flatIndex;
    if (slot.context == 0) {
        flatIndex = pfm3FlatIndex((Pfm3Param)slot.id);
    } else {
        flatIndex = commonFlatIndex((CommonParamId)slot.id);
    }
    if (flatIndex == 0xFFFF) {
        return;
    }
    float value = ((float*)pfm3Preset())[flatIndex];
    EditorParamInfo info = paramInfoFor(encoder);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);
    updateEncoderValueWithoutCursor((int)(flatIndex % PFM3_PARAMS_PER_ROW),
                                    (slot.context == 0) ? (int)pfm3ParamElementOf((Pfm3Param)slot.id) : 0, info, value, value);
    return;
}

void EditorPfm3::encoderTurned(int encoder, int ticks) {
    EditorSlot slot = slotForEncoder(encoder);
    if (slot.kind == EditorSlotKind::Action) {
        encoderTurnedCustomAction((EditorActionId)slot.id, ticks);
        return;
    }
    if (slot.kind == EditorSlotKind::Custom) {
        encoderTurnedForStepSequencer((int)slot.context, encoder, ticks);
        return;
    }
    if (slot.kind == EditorSlotKind::Param) {
        uint16_t flatIndex;
        if (slot.context == 0) {
            flatIndex = pfm3FlatIndex((Pfm3Param)slot.id);
        } else {
            flatIndex = commonFlatIndex((CommonParamId)slot.id);
        }
        if (flatIndex != 0xFFFF) {
            encoderTurnedFlat((int)flatIndex, ticks);
        }
    }
}

void EditorPfm3::drawAlgorithm() {
    tft_->drawAlgoTable(EditorPfm3Algo::table(pfm3Preset()->engine1.algo));
}

void EditorPfm3::printPageHeader() {
    int mainPage = synthState_->fullState_.mainPage;
    uint8_t element = currentElement();
    switch (mainPage) {
    case 1:
        tft_->print("Op ");
        tft_->print(element + 1);
        tft_->print(" ");
        break;
    case 2:
        tft_->print("Matrix ");
        tft_->print(element + 1);
        break;
    case 3:
        switch (editPage_) {
        case 0:
            tft_->print("LFO ");
            break;
        case 1:
            tft_->print("Free Env ");
            break;
        case 2:
            tft_->print("Step Sequencer ");
            break;
        case 3:
            tft_->print("Midi note ");
            break;
        default:
            return;
        }
        tft_->print(element + 1);
        break;
    default:
        break;
    }
}

EditorSelectorKind EditorPfm3::selectorKind(int button) const {
    if (isInOperatorPage(synthState_->fullState_.mainPage) && button >= 3 && button <= 5) {
        return EditorSelectorKind::Operator;
    }
    if (isInMatrixPage(synthState_->fullState_.mainPage)) {
        return EditorSelectorKind::MatrixRow;
    }
    return EditorSelectorKind::None;
}

void EditorPfm3::drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) {
    switch (selectorKind(button)) {
    case EditorSelectorKind::Operator:
        drawOperatorSelector(button, algoInformation[(int)pfm3Preset()->engine1.algo].osc - 1);
        return;
    case EditorSelectorKind::MatrixRow:
        drawMatrixSelector(button, page->buttonId, 12);
        return;
    default:
        drawEditorButtonDefault(button, editMenu, page);
        return;
    }
}

void EditorPfm3::printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                                    const struct EditorPageButton* page) {
    (void)editMenu;
    (void)page;
    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_MEDIUM_GRAY);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);

    if (slot.kind == EditorSlotKind::Action || slot.kind == EditorSlotKind::Custom) {
        tft_->print(slot.kind == EditorSlotKind::Action ? "Sync" : "Steps");
        return;
    }
    if (slot.kind != EditorSlotKind::Param) {
        return;
    }
    if (slot.context != 0) {
        EditorParamPage::printParameterName(button, slot, editMenu, page);
        return;
    }
    Pfm3Param id = (Pfm3Param)slot.id;
    int element = pfm3ParamElementOf(id);
    int currentAlgo = (int)pfm3Preset()->engine1.algo;
    EditorParamInfo info = paramInfoFor(button);

    if (pfm3ParamBlockOf(id) == (uint8_t)Pfm3Block::Mod) {
        struct ModulationIndex im = EditorPfm3Algo::modulationIndex[currentAlgo][element];
        if (im.source == 0) {
            hideParam_[button] = true;
        } else if (element < 5) {
            tft_->print((int)im.source);
            tft_->print('-');
            tft_->print((int)im.destination);
        } else {
            tft_->print("Fdbk");
        }
        return;
    }

    if (pfm3ParamBlockOf(id) == (uint8_t)Pfm3Block::OscMix) {
        if (element < algoInformation[currentAlgo].mix) {
            tft_->print(info.name);
            tft_->print(element + 1);
            tft_->printSmallChar('(');
            tft_->printSmallChar(EditorPfm3Algo::table(currentAlgo)[element * 2 + 1]);
            tft_->printSmallChar(')');
        } else {
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->print(info.name);
            tft_->print(element + 1);
            hideParam_[button] = true;
        }
        return;
    }

    if (id == pfm3Param(Pfm3Block::Engine2, 0, 0) && pfm3Preset()->engine1.playMode == PLAY_MODE_POLY) {
        hideParam_[button] = true;
        tft_->setCharColor(COLOR_DARK_GRAY);
    } else if ((id == pfm3Param(Pfm3Block::Engine2, 0, 1) || id == pfm3Param(Pfm3Block::Engine2, 0, 2)) &&
               pfm3Preset()->engine1.playMode != PLAY_MODE_UNISON) {
        hideParam_[button] = true;
        tft_->setCharColor(COLOR_DARK_GRAY);
    }
    tft_->print(info.name);
    return;
}

void EditorPfm3::preparePage() {
    updatePage();
    if (synthState_->fullState_.mainPage == 1) {
        tft_->clearAlgoFG();
        tft_->highlightOperator(operatorNumber_);
    }
    EditorParamPage::preparePage();
}

void EditorPfm3::slowTick() {
    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        if (imChangedCounter_[e] > 0) {
            imChangedCounter_[e]--;
            if (imChangedCounter_[e] == 0) {
                struct ModulationIndex im = EditorPfm3Algo::modulationIndex[(int)pfm3Preset()->engine1.algo][e];
                if (im.source != im.destination) {
                    tft_->eraseHighlightIM(e, im.source, im.destination);
                } else {
                    tft_->eraseHighlightFeedback(e, im.source, im.destination);
                }
            }
        }
        if (mixChangedCounter_[e] > 0) {
            mixChangedCounter_[e]--;
            if (mixChangedCounter_[e] == 0) {
                tft_->eraseHighlightOperator(mixChangeOperatorNumber_[e]);
            }
        }
    }
    EditorParamPage::slowTick();
}

void EditorPfm3::encoderTurnedWhileButtonPressed(int encoder, int ticks, int button) {

    // SYNTH_MODE_EDIT
    switch (button) {
    case BUTTON_PREVIOUS_INSTRUMENT: {
        EditorSlot slot = slotForEncoder(encoder);
        if (slot.kind != EditorSlotKind::Param) {
            break;
        }
        uint16_t flatIndex = (slot.context == 0) ? pfm3FlatIndex((Pfm3Param)slot.id) : commonFlatIndex((CommonParamId)slot.id);
        if (flatIndex == 0xFFFF) {
            break;
        }

        const struct Pfm3Preset* defaultParams = &defaultPreset;
        float* value = &((float*)pfm3Preset())[flatIndex];
        float oldValue = *value;
        float newValue = ((float*)defaultParams)[flatIndex];
        (*value) = newValue;

        Pfm3Param pid = pfm3FlatToParam(flatIndex);
        if ((uint16_t)pid != PFM3_PARAM_NONE) {
            synthState_->propagateNewEngineParamValue(synthState_->getCurrentTimbre(), (int)pid, 0, oldValue, newValue);
        } else {
            CommonParamId cid = commonFlatToParam(flatIndex);
            if (cid != COMMON_PARAM_NONE) {
                synthState_->propagateNewEngineParamValue(synthState_->getCurrentTimbre(), (int)cid, 1, oldValue, newValue);
            }
        }
        break;
    }
    case BUTTON_SWITCH_MENU: {

        // All update must be in green
        multipleEdition_ = true;

        EditorSlot slot = slotForEncoder(encoder);
        if (slot.kind == EditorSlotKind::Param && slot.context == 0) {
            Pfm3Param id = (Pfm3Param)slot.id;
            uint8_t block = pfm3ParamBlockOf(id);
            uint8_t field = pfm3ParamFieldOf(id);
            int currentAlgo = (int)pfm3Preset()->engine1.algo;

            // Modify param of all operator of same types.
            if (block == (uint8_t)Pfm3Block::EnvTime || block == (uint8_t)Pfm3Block::EnvLevel) {
                int currentOpType = algoOpInformation[currentAlgo][pfm3ParamElementOf(id)];
                for (int op = 0; op < NUMBER_OF_OPERATORS; op++) {
                    if (currentOpType == algoOpInformation[currentAlgo][op]) {
                        // Same operator type : carrier or modulation
                        uint16_t flatIndex = pfm3FlatIndex(pfm3Param((Pfm3Block)block, op, field));
                        encoderTurnedFlat(flatIndex, ticks);
                    }
                }
            } else if (block == (uint8_t)Pfm3Block::Mod) {
                int imMax = algoInformation[currentAlgo].im;
                for (int m = 0; m < imMax; m++) {
                    uint16_t flatIndex = pfm3FlatIndex(pfm3Param(Pfm3Block::Mod, m, field));
                    encoderTurnedFlat(flatIndex, ticks);
                }
            } else if (block == (uint8_t)Pfm3Block::OscMix) {
                for (int m = 0; m < NUMBER_OF_OPERATORS; m++) {
                    if (algoOpInformation[currentAlgo][m] == 1) {
                        uint16_t flatIndex = pfm3FlatIndex(pfm3Param(Pfm3Block::OscMix, m, field));
                        encoderTurnedFlat(flatIndex, ticks);
                    }
                }
            }
        }

        // End of multiple edition
        multipleEdition_ = false;

        break;
    }
    }
}

void EditorPfm3::buttonPressed(int button) {
    if (selectorKind(button) == EditorSelectorKind::Operator) {
        const struct EditorMenu* editMenu = currentEditMenu();
        if (editMenu == nullptr || button >= editMenu->buttonCount) {
            return;
        }
        selectOperator(button, algoInformation[(int)pfm3Preset()->engine1.algo].osc - 1);
        return;
    }

    EditorParamPage::buttonPressed(button);
}

void EditorPfm3::buttonLongPressed(int instrument, int button) {
    if (isInArpPatternPage()) {
        EditorParamPage::buttonLongPressed(instrument, button);
        return;
    }
    if (button == BUTTON_PREVIOUS_INSTRUMENT) {
        // Can be copied ?
        bool canBeCopied = isInMatrixPage(synthState_->fullState_.mainPage) || isInOperatorPage(synthState_->fullState_.mainPage) ||
                           (isInModulatorPage(synthState_->fullState_.mainPage) && editPage_ == 0);

        if (canBeCopied) {
            bufferPageNumber_ = synthState_->fullState_.mainPage;
            bufferEditNumber_ = editPage_;

            if (isInOperatorPage(synthState_->fullState_.mainPage) && editPage_ != 0) {
                // Env : we only store the op number.
                // We'll copy time, level and shape
                bufferParam_[0] = operatorNumber_;
                parent_->displayPopup(COLOR_GREEN, "Copy Env", 8);
            } else {
                for (int e = 0; e < 6; e++) {
                    const EditorSlot slot = slotForEncoder(e);
                    if (slot.kind != EditorSlotKind::Param) {
                        continue;
                    }
                    uint16_t flatIndex = (slot.context == 0) ? pfm3FlatIndex((Pfm3Param)slot.id) : commonFlatIndex((CommonParamId)slot.id);
                    if (flatIndex != 0xFFFF) {
                        bufferParam_[e] = ((float*)pfm3Preset())[flatIndex];
                    }
                }
                if (isInMatrixPage(synthState_->fullState_.mainPage)) {
                    parent_->displayPopup(COLOR_GREEN, "Copy Mtx", 8);
                } else if (isInOperatorPage(synthState_->fullState_.mainPage)) {
                    parent_->displayPopup(COLOR_GREEN, "Copy Osc", 8);
                } else {
                    // LFO
                    parent_->displayPopup(COLOR_GREEN, "Copy LFO", 8);
                }
            }
        }
    } else if (button == BUTTON_NEXT_INSTRUMENT) {
        // Can be pasted ?
        bool canBePasted = isInMatrixPage(synthState_->fullState_.mainPage) && isInMatrixPage(bufferPageNumber_);
        // Operator Osc
        canBePasted |= isInOperatorPage(synthState_->fullState_.mainPage) && isInOperatorPage(bufferPageNumber_) &&
                       bufferEditNumber_ == 0 && editPage_ == 0;
        // Operator Env
        canBePasted |= isInOperatorPage(synthState_->fullState_.mainPage) && isInOperatorPage(bufferPageNumber_) &&
                       bufferEditNumber_ != 0 && editPage_ != 0;
        // LFO
        canBePasted |= isInModulatorPage(synthState_->fullState_.mainPage) && isInModulatorPage(bufferPageNumber_) && editPage_ == 0;

        if (canBePasted) {
            if (isInOperatorPage(synthState_->fullState_.mainPage) && editPage_ != 0) {
                if (bufferParam_[0] != operatorNumber_) {
                    uint8_t opNum = bufferParam_[0];
                    float* timeSource = (float*)(((EnvelopeTimeMemory*)&pfm3Preset()->env1Time) + opNum);
                    float* timeDest = (float*)(((EnvelopeTimeMemory*)&pfm3Preset()->env1Time) + operatorNumber_);
                    for (int i = 0; i < 4; i++) {
                        timeDest[i] = timeSource[i];
                    }
                    float* levelSource = (float*)(((EnvelopeLevelMemory*)&pfm3Preset()->env1Level) + opNum);
                    float* levelDest = (float*)(((EnvelopeLevelMemory*)&pfm3Preset()->env1Level) + operatorNumber_);
                    for (int i = 0; i < 4; i++) {
                        levelDest[i] = levelSource[i];
                    }
                    float* curveSource = (float*)(((EnvelopeCurveParams*)&pfm3Preset()->env1Curve) + opNum);
                    float* curveDest = (float*)(((EnvelopeCurveParams*)&pfm3Preset()->env1Curve) + operatorNumber_);
                    for (int i = 0; i < 4; i++) {
                        curveDest[i] = curveSource[i];
                    }
                }
                parent_->displayPopup(COLOR_GREEN, "Paste Env", 9);
            } else {
                for (int e = 0; e < 6; e++) {
                    const EditorSlot slot = slotForEncoder(e);
                    if (slot.kind != EditorSlotKind::Param) {
                        continue;
                    }
                    uint16_t flatIndex = (slot.context == 0) ? pfm3FlatIndex((Pfm3Param)slot.id) : commonFlatIndex((CommonParamId)slot.id);
                    if (flatIndex != 0xFFFF) {
                        ((float*)pfm3Preset())[flatIndex] = bufferParam_[e];
                    }
                }
                if (isInMatrixPage(synthState_->fullState_.mainPage)) {
                    parent_->displayPopup(COLOR_GREEN, "Paste Mtx", 9);
                } else if (isInOperatorPage(synthState_->fullState_.mainPage)) {
                    parent_->displayPopup(COLOR_GREEN, "Paste Osc", 9);
                } else {
                    // LFO
                    parent_->displayPopup(COLOR_GREEN, "Paste LFO", 9);
                }
            }
        } else {
            parent_->displayPopup(COLOR_RED, "Cannot", 6);
        }
    }
}

bool EditorPfm3::isInOperatorPage(uint8_t pageNumber) const {
    return pageNumber == 1;
}

bool EditorPfm3::isInMatrixPage(uint8_t pageNumber) const {
    // Main page 2 is the shared Arp/Fx page, not the PFM3 matrix.
    return getEngineType() == parent_->currentEngineType() && pageNumber == 2;
}

bool EditorPfm3::isInModulatorPage(uint8_t pageNumber) {
    return pageNumber == 3;
}

void EditorPfm3::refreshOscilloBG() {
    if (synthState_->fullState_.mainPage == 1) {
        if (editPage_ == 0) {
            refreshOscillatorOperatorShape();
        } else {
            refreshOscillatorOperatorEnvelope();
        }
    } else if (synthState_->fullState_.mainPage == 3) {
        if (editPage_ == 0) {
            refreshLfoOscillator();
        } else if (editPage_ == 1) {
            refreshLfoEnv();
        } else {
            tft_->oscilloBgActionClear();
        }
    } else {
        tft_->oscilloBgActionClear();
    }
}

void EditorPfm3::refreshOscillatorOperatorShape() {
    int op = operatorNumber_;
    OscillatorParams* oscillatorParams = &pfm3Preset()->osc1;

    tft_->oscilloBgActionOperatorShape((int)oscillatorParams[op].shape);
}

void EditorPfm3::refreshLfoOscillator() {
    const struct EditorPageButton* page = currentPage();
    if (page == nullptr) {
        return;
    }

    if (page != &pfm3ButtonLfos) {
        return;
    }
    uint8_t lfoNumber = synthState_->fullState_.buttonState[page->buttonId];

    LfoParams* lfoParam = &pfm3Preset()->lfoOsc1;
    float* lfoPhase = (float*)&pfm3Preset()->lfoPhases;

    tft_->oscilloBgSetLfo(lfoParam[lfoNumber].shape, lfoParam[lfoNumber].freq, lfoParam[lfoNumber].keybRamp, lfoParam[lfoNumber].bias,
                          lfoPhase[lfoNumber]);
    tft_->oscilloBgActionLfo();
}

void EditorPfm3::refreshLfoEnv() {
    const struct EditorPageButton* page = currentPage();
    if (page == nullptr) {
        return;
    }

    if (page != &pfm3ButtonEnvs) {
        return;
    }
    uint8_t envNumber = synthState_->fullState_.buttonState[page->buttonId];

    if (envNumber == 0) {
        EnvelopeLfoParams* envParam = &pfm3Preset()->lfoEnv1;
        tft_->oscilloBgSetLfoEnvelope(envParam->attack, envParam->decay, 1.0f, envParam->release, 1.0f, envParam->sustain,
                                      envParam->sustain, 0.0f);
        tft_->oscilloBgActionEnvelope();
    } else if (envNumber == 1) {
        Envelope2LfoParams* envParam = &pfm3Preset()->lfoEnv2;
        tft_->oscilloBgSetLfoEnvelope(envParam->silence, envParam->attack, envParam->decay, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        tft_->oscilloBgActionEnvelope();
    }
}

void EditorPfm3::refreshOscillatorOperatorEnvelope() {
    int op = operatorNumber_;
    EnvelopeTimeMemory* envTime = &pfm3Preset()->env1Time;
    EnvelopeLevelMemory* envLevel = &pfm3Preset()->env1Level;
    EnvelopeCurveParams* envCurve = &pfm3Preset()->env1Curve;
    tft_->oscilloBgSetEnvelope(envTime[op].attackTime, envTime[op].decayTime, envTime[op].sustainTime, envTime[op].releaseTime,
                               envLevel[op].attackLevel, envLevel[op].decayLevel, envLevel[op].sustainLevel, envLevel[op].releaseLevel,
                               envCurve[op].attackCurve, envCurve[op].decayCurve, envCurve[op].sustainCurve, envCurve[op].releaseCurve);
    tft_->oscilloBgActionEnvelope();
}

void EditorPfm3::newTimbre() {
    if (getEngineType() == parent_->currentEngineType()) {
        checkOperatorNumber();
    }
}

void EditorPfm3::checkOperatorNumber() {
    int numberOfOsc = algoInformation[(int)pfm3Preset()->engine1.algo].osc;

    if (unlikely(numberOfOsc == 3)) {
        // Special case for 3 operators
        // We don't want button 4 to be selected
        synthState_->fullState_.buttonState[BUTTONID_OPERATOR_3_4] = 0;
    }

    // If lower number of Osc, adjust current selected one so that it shows up on screen
    // when going to operator page
    if (operatorNumber_ > (numberOfOsc - 1)) {
        operatorNumber_ = (numberOfOsc - 1);
    }
    updatePage();
}
