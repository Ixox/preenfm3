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

#include "EditorParamPage.h"
#include "ScreenEditor.h"
#include "ScreenTypes.h"
#include "SynthEngineFactory.h"
#include "SynthState.h"
#include "Timbre.h"
#include "engine/pfm3/Preenfm3Engine.h"
#include "preenfm3.h"
#include <FirmwareTftDisplay.h>

const char* lfoOscMidiClock[] = {"C/16", "Ck/8", "Ck/4", "Ck/2", "Ck  ", "Ck*2", "Ck*3", "Ck*4", "Ck*8"};

const char* lfoSeqMidiClock[] = {"Ck/4", "Ck/2", "Ck  ", "Ck*2", "Ck*4"};

struct FilterRowDisplay fx1RowDisplay[FILTER_LAST] = {
    {NULL, NULL, "Gain"},     {"Pan ", NULL, "Gain"},   {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"LoFr", "Boos", "Gain"},
    {"Freq", "Q   ", "Gain"}, {"Samp", "Bits", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Q   ", "Gain"},
    {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"},
    {"Freq", "Amp ", "Gain"}, {"Freq", "Amp ", "Gain"}, {"Freq", "Amp ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"},
    {"Freq", "Mix ", "Gain"}, {"Freq", "Mod ", "Gain"}, {"Pos ", "Sprd", "Gain"}, {"Thrs", "Tone", "Gain"}, {"Driv", "Tone", "Gain"},
    {"Driv", "Tone", "Gain"}, {"Driv", "Tone", "Gain"}, {"Thrs", "Tone", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"},
    {"Freq", "Fold", "Gain"}, {"Freq", "Fold", "Gain"}, {"Pos ", "Freq", "Gain"}, {"Pos ", "Freq", "Gain"}, {"Freq", "Sprd", "Gain"},
    {"Freq", "Sprd", "Gain"}, {"Freq", "Sprd", "Gain"}, {"Freq", "Sprd", "Gain"}, {"Vowl", "Tone", "Gain"}, {"Vowl", "Tone", "Gain"},
    {"Vowl", "Tone", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"},
    {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Freq", "Res ", "Gain"}, {"Smp1", "Smp2", "Gain"}

};
struct FilterRowDisplay fx2RowDisplay[FILTER2_LAST] = {
    {NULL, NULL, "Gain"},     {"Rate", "Feed", "Mix "}, {"Rate", "M/S ", "Mix "}, {"Rate", "Vibr", "Mix "}, {"Detu", "Freq", "Mix "},
    {"Ptch", "Feed", "Mix "}, {"Ptc1", "Ptc2", "Mix "}, {"Freq", "Feed", "Mix "}, {"Time", "Feed", "Mix "}, {"Time", "Feed", "Mix "},
    {"Time", "Feed", "Mix "}, {"Size", "Sprd", "Mix "}, {"Tune", "Sprd", "Mix "}, {"Freq", "Reso", "Mix "}};

struct Pfm3Preset* EditorParamPage::pfm3Preset() const {
    return SynthEngineFactory::getPfm3(currentTimbre())->getPfm3Preset();
}

EditorParamPage::EditorParamPage() {
    for (int i = 0; i < NUMBER_OF_ENCODERS; i++) {
        hideParam_[i] = false;
        valueChangedCounter_[i] = 0;
    }
    for (int l = 0; l < 5; l++) {
        lfoFreqPreviousValue_[l] = -1.0f;
    }
    patternSelect_ = 0;
}

int EditorParamPage::getStepSelection(int which) const {
    return synthState_->getTimbre(currentTimbre())->getStepSelection(which);
}

void EditorParamPage::encoderTurnedForStepSequencer(int whichStepSeq, int encoder, int ticks) {
    Pfm3Preset* preset = pfm3Preset();
    Timbre* timbre = synthState_->getTimbre(currentTimbre());
    StepSequencerSteps* seqSteps = &((StepSequencerSteps*)(&preset->lfoSteps1))[whichStepSeq];

    if (encoder == 3) {
        int oldPos = timbre->getStepSelection(whichStepSeq);
        int newValue = oldPos + (ticks > 0 ? 1 : -1);

        if (newValue > 15) {
            newValue = 0;
        } else if (newValue < 0) {
            newValue = 15;
        }

        timbre->setStepSelection(whichStepSeq, newValue);
        synthState_->propagateNewEngineParamValue(currentTimbre(), (int)pfm3Param(Pfm3Block::StepSeq, whichStepSeq, 0), 0, oldPos,
                                                  newValue);
    } else if (encoder == 4) {
        char* step = &seqSteps->steps[(int)timbre->getStepSelection(whichStepSeq)];
        int oldValue = (int)(*step);
        int newValue = (*step) + ticks;

        if (newValue > 15) {
            newValue = 15;
        } else if (newValue < 0) {
            newValue = 0;
        }
        if ((*step) != newValue) {
            (*step) = newValue;
            synthState_->propagateNewEngineParamValue(
                currentTimbre(), (int)pfm3Param(Pfm3Block::StepSeq, whichStepSeq, timbre->getStepSelection(whichStepSeq)), 0, oldValue,
                (int)(*step));
        }
    }
}

void EditorParamPage::encoderTurnedForArpPattern(int encoder, int ticks) {
    Pfm3Preset* preset = pfm3Preset();
    if (encoder == 0) {
        // Encoder 0: move cursor
        int oldPos = patternSelect_;
        int newPos = oldPos;
        newPos += (ticks > 0 ? 1 : -1);

        if (newPos > 15) {
            newPos = 0;
        } else if (newPos < 0) {
            newPos = 15;
        }
        if (newPos != oldPos) {
            patternSelect_ = newPos;
            synthState_->propagateNewEngineParamValue(currentTimbre(), (int)commonParam(CommonBlock::ArpPattern, 0, 0), 1, oldPos,
                                                      patternSelect_);
        }
    } else if (encoder == 1) {
        // Change value(s)
        if (preset->engineArp2.pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
            return;
        }
        arp_pattern_t pattern = preset->engineArpUserPatterns.patterns[(int)preset->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT];
        const uint16_t oldMask = ARP_PATTERN_GETMASK(pattern);
        uint16_t newMask = oldMask;

        uint16_t bitsToModify = 0;
        switch (encoder) {
        case 1:
            bitsToModify = 0x1 << patternSelect_;
            break; // modify single note
        case 2:
            bitsToModify = 0x1111 << (patternSelect_ & 3);
            break; // modify all
        case 3:
            bitsToModify = 0xf << ((patternSelect_ >> 2) << 2);
            break; // modify entire bar
        }
        if (ticks > 0) {
            newMask |= bitsToModify;
        } else {
            newMask &= ~bitsToModify;
        }

        if (oldMask != newMask) {
            ARP_PATTERN_SETMASK(pattern, newMask);
            preset->engineArpUserPatterns.patterns[(int)preset->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT] = pattern;
            synthState_->propagateNewEngineParamValue(currentTimbre(), (int)commonParam(CommonBlock::ArpPattern, 0, 0), 1, oldMask,
                                                      newMask);
        }
    }
}

void EditorParamPage::drawEditorButton(int button, const struct EditorMenu* editMenu, const struct EditorPageButton* page) {
    drawEditorButtonDefault(button, editMenu, page);
}

void EditorParamPage::printParameterName(int button, const EditorSlot& slot, const struct EditorMenu* editMenu,
                                         const struct EditorPageButton* page) {
    (void)editMenu;
    (void)page;
    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_MEDIUM_GRAY);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);
    if (slot.kind != EditorSlotKind::Param) {
        return;
    }
    CommonParamId cid = (CommonParamId)slot.id;
    CommonBlock block = (CommonBlock)commonBlockOf(cid);
    if (block == CommonBlock::Effect1 || block == CommonBlock::Effect2) {
        int effect = (block == CommonBlock::Effect1) ? (int)pfm3Preset()->effect1.type : (int)pfm3Preset()->effect2.type;
        int field = commonFieldOf(cid);
        if (field == 0) {
            tft_->print(paramInfoFor(button).name);
            return;
        }
        const char* name =
            (block == CommonBlock::Effect1) ? fx1RowDisplay[effect].paramName[field - 1] : fx2RowDisplay[effect].paramName[field - 1];
        if (name != nullptr) {
            tft_->print(name);
        } else {
            hideParam_[button] = true;
            tft_->setCharColor(COLOR_DARK_GRAY);
            tft_->print("None");
        }
        return;
    }
    tft_->print(paramInfoFor(button).name);
}

void EditorParamPage::displayParamValue(int encoder, TFT_COLOR color) {
    EditorSlot slot = slotForEncoder(encoder);
    uint8_t x = getEncoderX(encoder);
    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    if (slot.kind == EditorSlotKind::Custom) {
        EditorParamInfo info = {};
        info.displayType = DISPLAY_TYPE_ARP_PATTERN;
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(color);
        updateEncoderValueWithoutCursor((int)slot.id, 0, info, 0.0f, 0.0f);
        return;
    }
    if (slot.kind != EditorSlotKind::Param || hideParam_[encoder]) {
        if (hideParam_[encoder]) {
            tft_->print("     ");
        }
        return;
    }
    uint16_t flatIndex = commonFlatIndex((CommonParamId)slot.id);
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

void EditorParamPage::newEngineParamValue(int& refreshStatus, int timbre, int paramId, int context, float oldValue, float newValue) {
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
    if (encoder == -1) {
        return;
    }

    // Shared Arp/Fx page special cases.
    CommonBlock block = (CommonBlock)commonBlockOf((CommonParamId)paramId);
    uint8_t field = commonFieldOf((CommonParamId)paramId);
    if (block == CommonBlock::Arp2 && field == 0) {
        refreshStatus = 3;
    } else if ((block == CommonBlock::Effect1 || block == CommonBlock::Effect2) && field == 0) {
        resetHideParams();
        refreshStatus = 11;
    }

    uint16_t flatIndex = (context == 0) ? pfm3FlatIndex((Pfm3Param)paramId) : commonFlatIndex((CommonParamId)paramId);
    if (flatIndex == 0xFFFF) {
        return;
    }
    int encoderInRow = flatIndex % PFM3_PARAMS_PER_ROW;

    uint8_t x = getEncoderX(encoder);

    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_YELLOW);
    valueChangedCounter_[encoder] = 4;
    EditorParamInfo info = paramInfoFor(encoder);
    updateEncoderValueWithoutCursor(encoderInRow, (context == 0) ? (int)pfm3ParamElementOf((Pfm3Param)paramId) : 0, info, oldValue,
                                    newValue);
}

void EditorParamPage::slowTick() {
    for (int e = 0; e < NUMBER_OF_ENCODERS; e++) {
        if (valueChangedCounter_[e] > 0) {
            valueChangedCounter_[e]--;
            if (valueChangedCounter_[e] == 0) {
                if (synthState_->fullState_.mainPage != -1) {
                    tft_->setCharBackgroundColor(COLOR_BLACK);
                    displayParamValue(e, COLOR_WHITE);
                }
            }
        }
    }
}

void EditorParamPage::encoderTurned(int encoder, int ticks) {
    EditorSlot slot = slotForEncoder(encoder);
    if (slot.kind == EditorSlotKind::Custom) {
        encoderTurnedForArpPattern((int)slot.id, ticks);
        return;
    }
    if (slot.kind == EditorSlotKind::Param) {
        uint16_t flatIndex = commonFlatIndex((CommonParamId)slot.id);
        if (flatIndex != 0xFFFF) {
            encoderTurnedFlat((int)flatIndex, ticks);
        }
    }
}

void EditorParamPage::encoderTurnedFlat(int flatIndex, int ticks) {
    applyTicks(flatIndex, ticks);
}

void EditorParamPage::encoderTurnedCustomAction(EditorActionId action, int ticks) {
    int flatIndex;
    if (action == EditorActionId::LfoSync) {
        // Simulate we're Osc freq
        int lfoButtonState = synthState_->fullState_.buttonState[BUTTONID_EDIT_LFOS];

        flatIndex = (int)pfm3FlatIndex(pfm3Param(Pfm3Block::LfoOsc, lfoButtonState, 1));

        // Block if necessary
        if ((ticks > 0 && ((float*)pfm3Preset())[flatIndex] > 99.95f) || (ticks < 0 && ((float*)pfm3Preset())[flatIndex] < 99.95f)) {
            return;
        }

        float freqToSet;
        if (lfoFreqPreviousValue_[lfoButtonState] == -1.0f) {
            freqToSet = ticks > 0 ? 100.4f : 2.0f;
        } else {
            freqToSet = lfoFreqPreviousValue_[lfoButtonState];
            // User arrive in Ext/Int area with Freq encoder ?
            if (ticks < 0 && freqToSet > 99.95f) {
                freqToSet = 99.9f;
            } else if (ticks > 0 && freqToSet < 99.95f) {
                freqToSet = 100.0f;
            }
        }
        lfoFreqPreviousValue_[lfoButtonState] = ((float*)pfm3Preset())[flatIndex];

        if (ticks > 0) {
            ticks = 20;
            ((float*)pfm3Preset())[flatIndex] = freqToSet - .1f * ticks;
        } else {
            ticks = -1000;
            ((float*)pfm3Preset())[flatIndex] = freqToSet - .1f * ticks;
        }
    } else if (action == EditorActionId::StepSync) {
        // Simulate we're Osc freq
        int stepButtonState = synthState_->fullState_.buttonState[BUTTONID_EDIT_STEPS];

        flatIndex = (int)pfm3FlatIndex(pfm3Param(Pfm3Block::LfoSeq, stepButtonState, 0));

        // Block if necessary
        if ((ticks > 0 && ((float*)pfm3Preset())[flatIndex] > 240.5f) || (ticks < 0 && ((float*)pfm3Preset())[flatIndex] < 240.5f)) {
            return;
        }

        float bpmToSet;
        if (lfoFreqPreviousValue_[3 + stepButtonState] == -1.0f) {
            bpmToSet = ticks > 0 ? 243.0f : 120.0f;
        } else {
            bpmToSet = lfoFreqPreviousValue_[3 + stepButtonState];
            // User arrive in Ext/Int area with BPM encoder ?
            if (ticks < 0 && bpmToSet > 240.5f) {
                bpmToSet = 240.0f;
            } else if (ticks > 0 && bpmToSet < 240.5f) {
                bpmToSet = 241.0f;
            }
        }
        lfoFreqPreviousValue_[3 + stepButtonState] = ((float*)pfm3Preset())[flatIndex];

        if (ticks > 0) {
            ticks = 50;
            ((float*)pfm3Preset())[flatIndex] = bpmToSet - ticks;
        } else {
            ticks = -240;
            ((float*)pfm3Preset())[flatIndex] = bpmToSet - ticks;
        }
    } else {
        return;
    }
    applyTicks(flatIndex, ticks);
}

void EditorParamPage::applyTicks(int flatIndex, int ticks) {
    Pfm3Param pid = pfm3FlatToParam(flatIndex);
    CommonParamId cid = COMMON_PARAM_NONE;
    const ParamFullDescription* full;
    if ((uint16_t)pid != PFM3_PARAM_NONE) {
        full = &pfm3FullDesc(pid);
    } else {
        cid = commonFlatToParam(flatIndex);
        if (cid == COMMON_PARAM_NONE) {
            return;
        }
        full = &commonFullDesc(cid);
    }

    const ParamDesc& desc = full->desc;
    const ParamUiDesc& ui = full->ui;
    float maxValue = desc.max;
    if ((uint16_t)pid != PFM3_PARAM_NONE && pfm3ParamBlockOf(pid) == (uint8_t)Pfm3Block::LfoOsc && pfm3ParamFieldOf(pid) == 1) {
        maxValue = LFO_FREQ_MAX + 0.9f;
    }
    float inc = desc.step;

    float newValue;
    float oldValue;

    if (ui.displayType == DISPLAY_TYPE_STRINGS) {
        // Do not use encoder acceleration
        ticks = ticks > 0 ? 1 : -1;
    }

    if (ui.valueOrder == NULL) {
        // Not string parameters

        // floating point test to be sure numberOfValues is different from 1.
        // for voices when number of voices forced to 0
        if (ui.values < 1.5f) {
            return;
        }

        float& value = ((float*)pfm3Preset())[flatIndex];
        oldValue = value;

        // Slow down LFO frequency
        if (unlikely(ui.displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY)) {
            if (oldValue < 1.0f) {
                inc = inc * .1f;
            }
        }

        int tickIndex = (value - desc.min) / inc + .0005f + ticks;
        newValue = desc.min + tickIndex * inc;

        if (newValue > maxValue) {
            newValue = maxValue;
        }
        if (newValue < desc.min) {
            newValue = desc.min;
        }
        value = newValue;
    } else {
        float* value = &((float*)pfm3Preset())[flatIndex];
        newValue = oldValue = (*value);

        // Must use newValue (int) so that the minValue comparaison works
        // Is there any other order than the default one
        int pos = ui.valueOrderReversed[(int)(*value)];

        if (ticks > 0 && pos < maxValue) {
            newValue = ui.valueOrder[pos + 1];
        }
        if (ticks < 0 && pos > desc.min) {
            newValue = ui.valueOrder[pos - 1];
        }

        (*value) = (float)newValue;
    }
    if (newValue != oldValue) {
        if ((uint16_t)pid != PFM3_PARAM_NONE) {
            synthState_->propagateNewEngineParamValue(synthState_->getCurrentTimbre(), (int)pid, 0, oldValue, newValue);
        } else {
            synthState_->propagateNewEngineParamValue(synthState_->getCurrentTimbre(), (int)cid, 1, oldValue, newValue);
        }
    }
}

void EditorParamPage::buttonPressed(int button) {
    switch (button) {
    case BUTTON_SWITCH_EDIT:
    case BUTTON_SWITCH_MENU:
        synthState_->fullState_.mainPage = -1;
        break;
    case BUTTON_SWITCH_1:
    case BUTTON_SWITCH_2:
    case BUTTON_SWITCH_3:
    case BUTTON_SWITCH_4:
    case BUTTON_SWITCH_5:
    case BUTTON_SWITCH_6: {
        const struct EditorMenu* editMenu = currentEditMenu();
        if (editMenu == nullptr) {
            break;
        }
        const struct EditorPageButton* page = currentPage();
        if (button >= editMenu->buttonCount) {
            break;
        }
        if (editPage_ == button) {
            if (page->stateCount > 0) {
                synthState_->fullState_.buttonState[page->buttonId] =
                    (synthState_->fullState_.buttonState[page->buttonId] + 1) % page->stateCount;
                updatePage();
            }
        } else {
            // If new page has no state.. then we don't activate it
            if (editMenu->buttons[button]->stateCount > 0) {
                // save what editPage is selected in current mainPage
                setEditPageSelected(synthState_->fullState_.mainPage, button);

                setEditPage(button);
            }
        }
        break;
    }
    }
}

void EditorParamPage::preparePage() {
    resetHideParams();
}

void EditorParamPage::refreshOscilloBG() {
    tft_->oscilloBgActionClear();
}

void EditorParamPage::resetHideParams() {
    for (int h = 0; h < NUMBER_OF_ENCODERS; h++) {
        hideParam_[h] = false;
    }
}

void EditorParamPage::updateEncoderValueWithoutCursor(int encoder, int element, const EditorParamInfo& info, float oldValue,
                                                      float newFloatValue) {

    int newValue = (int)newFloatValue;

    switch (info.displayType) {
    case DISPLAY_TYPE_STRINGS:

        // Special case for play mode (mono, poly, unis)
        if (likely(info.valueNames != polyMonoNames)) {
            tft_->print(info.valueNames[newValue]);
        } else {
            // Display color error information
            if (synthState_->mixerState_.MPE_inst1_ > 0 && newValue != 1) {
                tft_->setCharColor(COLOR_RED);
            }
            tft_->print(info.valueNames[newValue]);
            int numberOfVoices = synthState_->mixerState_.instrumentState_[synthState_->getCurrentTimbre()].numberOfVoices;
            if (numberOfVoices == 0 || (newValue == 1 && numberOfVoices == 1) || (newValue == 0 && numberOfVoices > 1) ||
                (newValue == 2 && numberOfVoices == 1)) {
                tft_->setCharColor(COLOR_RED);
            } else {
                tft_->setCharColor(COLOR_GRAY);
            }
            int displayedVoices = numberOfVoices;
            if (newValue == PLAY_MODE_UNISON && displayedVoices > MAX_UNISON_VOICES) {
                displayedVoices = MAX_UNISON_VOICES;
            }
            tft_->print("(");
            tft_->print(displayedVoices);
            tft_->print(')');
            // Clear the last character left by a longer previous value.
            tft_->print(' ');
        }
        break;
    case DISPLAY_TYPE_FLOAT_LFO_FREQUENCY:
        if (newFloatValue * 10.0f > LFO_FREQ_MAX_TIMES_10) {

            int stringIndex = newFloatValue * 10.0f + .005f;
            tft_->print(' ');
            tft_->print(lfoOscMidiClock[stringIndex - LFO_FREQ_MAX_TIMES_10 - 1]);

            if (oldValue * 10.0f <= ((float)LFO_FREQ_MAX_TIMES_10) + .5f) {
                // Update Sync : Second button in the page
                displayParamValue(1, COLOR_WHITE);
            }
        } else {
            tft_->printFloatWithSpace(newFloatValue);
            if (oldValue * 10.0f > LFO_FREQ_MAX_TIMES_10) {
                // Update Sync : Second button in the page
                displayParamValue(1, COLOR_WHITE);
            }
        }
        break;
    case DISPLAY_TYPE_STEP_SEQ_BPM:
        if (newValue > 240.5f) {
            tft_->print(lfoSeqMidiClock[newValue - 241]);
            if (oldValue < 240.5f) {
                // Update Sync : Second button in the page
                displayParamValue(0, COLOR_WHITE);
            }
        } else {
            tft_->printValueWithSpace(newValue);
            if (oldValue > 240.5f) {
                // Update Sync : Second button in the page
                displayParamValue(0, COLOR_WHITE);
            }
        }
        break;
    case DISPLAY_TYPE_LFO_KSYN:
        if (unlikely(newFloatValue < 0.0f)) {
            tft_->print(" Off ");
            break;
        }
        tft_->printFloatWithSpace(newFloatValue);
        break;
    case DISPLAY_TYPE_FLOAT:
        tft_->printFloatWithSpace(newFloatValue);
        break;
        // else what follows
    case DISPLAY_TYPE_INT:
        tft_->printValueWithSpace(newValue);
        break;
    case DISPLAY_TYPE_FLOAT_OSC_FREQUENCY: {
        // Hack... to deal with the special case of the fixed frequency.....
        int oRow = element;
        OscillatorParams* oParam = (OscillatorParams*)&pfm3Preset()->osc1;
        OscFrequencyType ft = (OscFrequencyType)oParam[oRow].frequencyType;

        if (ft == OSC_FT_FIXE) {
            newValue = oParam[oRow].frequencyMul * 1000.0 + oParam[oRow].detune * 100;
            if (newValue < 0) {
                newValue = 0;
            }
            tft_->print(newValue);
            if (newValue < 10000) {
                tft_->print(' ');
            }
            if (newValue < 1000) {
                tft_->print(' ');
            }
            if (newValue < 100) {
                tft_->print(' ');
            }
            if (newValue < 10) {
                tft_->print(' ');
            }
        } else {
            tft_->printFloatWithSpace(newFloatValue);
        }
        break;
    }
    case DISPLAY_TYPE_NONE:
        tft_->print("    ");
        break;
    case DISPLAY_TYPE_STEP_SEQ2:
    case DISPLAY_TYPE_STEP_SEQ1: {
        // Display the steps only once
        if (encoder == 2) {
            int whichStepSeq = element;
            StepSequencerSteps* seqSteps = &((StepSequencerSteps*)(&pfm3Preset()->lfoSteps1))[whichStepSeq];

            tft_->setCharBackgroundColor(COLOR_BLACK);
            for (int k = 0; k < 16; k++) {
                if (getStepSelection(whichStepSeq) == k) {
                    tft_->setCharColor(COLOR_YELLOW);
                } else if ((k % 4) == 0) {
                    tft_->setCharColor(COLOR_WHITE);
                } else {
                    tft_->setCharColor(COLOR_GRAY);
                }
                tft_->print((char)(127 + seqSteps->steps[k]));
            }
        }
    } break;
    case DISPLAY_TYPE_ARP_PATTERN: {
        // Display the steps only once
        if (encoder == 0) {

            const int index = (int)pfm3Preset()->engineArp2.pattern;
            arp_pattern_t user_pattern;
            bool editable = false;

            if (pfm3Preset()->engineArp2.pattern >= ARPEGGIATOR_PRESET_PATTERN_COUNT) {
                user_pattern = pfm3Preset()->engineArpUserPatterns.patterns[index - ARPEGGIATOR_PRESET_PATTERN_COUNT];
                editable = true;
            } else {
                user_pattern = lut_res_arpeggiator_patterns[index];
            }

            tft_->setCharBackgroundColor(COLOR_BLACK);
            for (int k = 0; k < 16; k++) {
                bool stepOn = ((user_pattern >> (k)) & 0x1) > 0;
                if (patternSelect_ == k && editable) {
                    tft_->setCharColor(COLOR_YELLOW);
                } else if ((k % 4) == 0) {
                    tft_->setCharColor(COLOR_WHITE);
                } else {
                    tft_->setCharColor(COLOR_GRAY);
                }
                tft_->print((char)(127 + (stepOn ? 15 : 0)));
            }
        }
    } break;
    }

    tft_->setCharColor(COLOR_WHITE);
}

EditorParamInfo EditorParamPage::paramInfoFor(int encoder) const {
    EditorSlot slot = slotForEncoder(encoder);
    EditorParamInfo info = {};
    if (slot.kind != EditorSlotKind::Param) {
        return info;
    }

    const ParamFullDescription* full;
    bool isEngine = (slot.context == 0);
    if (isEngine) {
        full = &pfm3FullDesc((Pfm3Param)slot.id);
    } else {
        full = &commonFullDesc((CommonParamId)slot.id);
    }

    const ParamDesc& desc = full->desc;
    const ParamUiDesc& ui = full->ui;
    info.name = ui.name;
    info.displayType = ui.displayType;
    info.valueNames = ui.valueNames;
    info.valueOrder = ui.valueOrder;
    info.valueOrderReversed = ui.valueOrderReversed;
    info.min = desc.min;
    info.max = desc.max;
    if (isEngine) {
        Pfm3Param id = (Pfm3Param)slot.id;
        if (pfm3ParamBlockOf(id) == (uint8_t)Pfm3Block::LfoOsc && pfm3ParamFieldOf(id) == 1) {
            info.max = LFO_FREQ_MAX + 0.9f;
        }
    }
    info.nameCount = (uint8_t)ui.values;
    info.incValue = desc.step;
    info.step = desc.step;
    return info;
}
