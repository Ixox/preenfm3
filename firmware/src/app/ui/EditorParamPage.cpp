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
#include "engine/pfm3/Pfm3ParameterRows.h"
#include "ScreenEditor.h"
#include "ScreenTypes.h"
#include "SynthState.h"
#include "Timbre.h"
#include "preenfm3.h"
#include <FirmwareTftDisplay.h>

struct Pfm3Preset* EditorParamPage::pfm3Preset() const {
    return synthState_->getTimbre(currentTimbre())->getPfm3Preset();
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

void EditorParamPage::encoderTurnedForStepSequencer(int row, int encoder4, int encoder6, int ticks) {
    int whichStepSeq = row - ROW_LFOSEQ1;
    Pfm3Preset* preset = pfm3Preset();
    Timbre* timbre = synthState_->getTimbre(currentTimbre());
    StepSequencerSteps* seqSteps = &((StepSequencerSteps*)(&preset->lfoSteps1))[whichStepSeq];

    if (encoder6 == 3) {
        int oldPos = timbre->getStepSelection(whichStepSeq);
        int newValue = oldPos + (ticks > 0 ? 1 : -1);

        if (newValue > 15) {
            newValue = 0;
        } else if (newValue < 0) {
            newValue = 15;
        }

        timbre->setStepSelection(whichStepSeq, newValue);
        synthState_->propagateNewPfm3ParamValue(currentTimbre(), row, encoder4, (ParameterDisplay*)NULL, oldPos, newValue);
    } else if (encoder6 == 4) {
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
            synthState_->propagateNewPfm3ParamValue(currentTimbre(), row, encoder4, (ParameterDisplay*)NULL, oldValue, (int)(*step));
        }
    }
}

void EditorParamPage::encoderTurnedForArpPattern(int row, int encoder, int ticks) {
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
            synthState_->propagateNewPfm3ParamValue(currentTimbre(), row, encoder, (ParameterDisplay*)NULL, oldPos, patternSelect_);
        }
    } else if (encoder == 1) {
        // Change value(s)
        if (preset->engineArp2.pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
            return;
        }
        arp_pattern_t pattern =
            preset->engineArpUserPatterns.patterns[(int)preset->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT];
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
            synthState_->propagateNewPfm3ParamValue(currentTimbre(), row, encoder, (ParameterDisplay*)NULL, oldMask, newMask);
        }
    }
}

void EditorParamPage::drawEditorButton(int button, const struct EditorEditMenu* editMenu, const struct EditorOneButton* page) {
    if (button < editMenu->numberOfButtons) {
        const struct EditorOneButton* buttonPage = editMenu->pages[button];
        tft_->drawButton(buttonPage->buttonLabel, 270, 29, button, buttonPage->numberOfStates,
                         synthState_->fullState_.editPage == button ? synthState_->fullState_.buttonState[page->buttonId] + 1 : 0);
    } else {
        tft_->drawButton("", 270, 29, button, 0, 0);
    }
}

void EditorParamPage::drawParameterName(int button, const struct RowEncoder& rowEncoder, const struct EditorEditMenu* editMenu,
                                        const struct EditorOneButton* page) {
    (void)editMenu;
    (void)page;
    uint8_t x = getEncoderX(button);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_MEDIUM_GRAY);
    tft_->setCursor(x, button > 2 ? LINE_PARAM_NAME2 : LINE_PARAM_NAME1);

    if (rowEncoder.row != ROW_NONE) {
        const struct ParameterRowDisplay* paramRow = allParameterRows.row[rowEncoder.row];
        switch (rowEncoder.row) {
        case ROW_EFFECT1:
            if (rowEncoder.encoder > 0) {
                int effect = pfm3Preset()->effect1.type;
                if (fx1RowDisplay[effect].paramName[rowEncoder.encoder - 1] != NULL) {
                    tft_->print(fx1RowDisplay[effect].paramName[rowEncoder.encoder - 1]);
                } else {
                    hideParam_[button] = true;
                    tft_->setCharColor(COLOR_DARK_GRAY);
                    tft_->print("None");
                }
            } else {
                tft_->print(paramRow->paramName[rowEncoder.encoder]);
            }
            break;
        case ROW_EFFECT2:
            if (rowEncoder.encoder > 0) {
                int effect = pfm3Preset()->effect2.type;
                if (fx2RowDisplay[effect].paramName[rowEncoder.encoder - 1] != NULL) {
                    tft_->print(fx2RowDisplay[effect].paramName[rowEncoder.encoder - 1]);
                } else {
                    hideParam_[button] = true;
                    tft_->setCharColor(COLOR_DARK_GRAY);
                    tft_->print("None");
                }
            } else {
                tft_->print(paramRow->paramName[rowEncoder.encoder]);
            }
            break;
        default:
            tft_->print(paramRow->paramName[rowEncoder.encoder]);
        }
    }
}

void EditorParamPage::displayParamValue(int encoder, TFT_COLOR color) {
    const struct EditorEditMenu* editMenu = currentEditMenu();
    if (editMenu == nullptr) {
        return;
    }
    const struct EditorOneButton* page = currentPage();
    uint8_t buttonState = synthState_->fullState_.buttonState[page->buttonId];
    const struct RowEncoder rowEncoder = page->states[buttonState]->rowEncoder[encoder];

    uint8_t x = getEncoderX(encoder);

    tft_->setCursor(x, encoder > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);

    if (rowEncoder.encoder < ENCODER_INVISIBLE && !hideParam_[encoder]) {
        int row = rowEncoder.row;
        // Operator page
        if (synthState_->fullState_.mainPage == 1) {
            row += synthState_->fullState_.operatorNumber;
        }

        struct ParameterDisplay* param = &allParameterRows.row[row]->params[rowEncoder.encoder];
        float newValue = ((float*)pfm3Preset())[row * NUMBER_OF_ENCODERS_PFM2 + rowEncoder.encoder];

        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(color);
        updateEncoderValueWithoutCursor(row, rowEncoder.encoder, param, newValue, newValue);
    } else {
        if (hideParam_[encoder]) {
            tft_->print("     ");
        }
    }
}

void EditorParamPage::newPfm3ParamValue(int& refreshStatus, int timbre, int currentRow, int encoder, ParameterDisplay* param,
                                        float oldValue, float newValue) {
    (void)timbre;

    const struct EditorEditMenu* editMenu = currentEditMenu();
    if (editMenu == nullptr) {
        return;
    }
    const struct EditorOneButton* page = currentPage();
    uint8_t buttonState = synthState_->fullState_.buttonState[page->buttonId];

    int encoder6 = -1;
    for (int enc = 0; enc < NUMBER_OF_ENCODERS; enc++) {
        const struct RowEncoder rowEncoder = page->states[buttonState]->rowEncoder[enc];
        if (rowEncoder.row == currentRow && encoder == rowEncoder.encoder) {
            encoder6 = enc;
            break;
        }
    }

    // Can be called when new param value is received on Midi
    // So encoder6 can often be -1
    if (encoder6 == -1) {
        return;
    }

    // Shared Arp/Fx page special cases.
    switch (currentRow) {
    case ROW_ARPEGGIATOR3:
        parent_->updateArpPattern(currentRow, encoder, oldValue, newValue);
        return;
    case ROW_ARPEGGIATOR2:
        if (encoder == ENCODER_ARPEGGIATOR_PATTERN) {
            refreshStatus = 3;
        }
        break;
    case ROW_EFFECT1:
    case ROW_EFFECT2:
        if (encoder == ENCODER_EFFECT_TYPE) {
            resetHideParams();
            refreshStatus = 11;
        }
        break;
    }

    uint8_t x = getEncoderX(encoder6);

    tft_->setCursor(x, encoder6 > 2 ? LINE_PARAM_VALUE2 : LINE_PARAM_VALUE1);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_YELLOW);
    valueChangedCounter_[encoder6] = 4;
    updateEncoderValueWithoutCursor(currentRow, encoder, param, oldValue, newValue);
}

void EditorParamPage::tempoClick() {
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

void EditorParamPage::encoderTurned(int encoder6, int ticks) {
    struct FullState* fullState = &(synthState_->fullState_);

    if (unlikely(fullState->mainPage == -1)) {
        return;
    }

    const struct EditorEditMenu* editMenu = currentEditMenu();
    if (editMenu == nullptr) {
        return;
    }
    const struct EditorOneButton* page = currentPage();
    const struct RowEncoder rowEncoder = page->states[fullState->buttonState[page->buttonId]]->rowEncoder[encoder6];

    int encoder4 = rowEncoder.encoder;
    int row = rowEncoder.row;

    // Step sequencer special case
    if (unlikely(row == ROW_LFOSEQ1 || row == ROW_LFOSEQ2)) {
        if (encoder4 >= 2) {
            encoderTurnedForStepSequencer(row, encoder4, encoder6, ticks);
            return;
        }
    };

    encoderTurnedPfm2(row, encoder4, ticks);
}

void EditorParamPage::encoderTurnedPfm2(int row, int encoder4, int ticks, bool specialOpCase) {

    // SYNC_LFO & SYNC_STEPS are 2 special case
    if (unlikely(row == ROW_NONE && encoder4 < ENCODER_MODULATOR_SYNC_LFO)) {
        return;
    }
    if (unlikely(row == ROW_ARPEGGIATOR3)) {
        encoderTurnedForArpPattern(row, encoder4, ticks);
        return;
    }

    //
    int num;
    struct ParameterDisplay* param;
    if (unlikely(synthState_->fullState_.mainPage == 1) && specialOpCase) {
        // operator is a bit different with PFM3
        num = encoder4 + (row + synthState_->fullState_.operatorNumber) * NUMBER_OF_ENCODERS_PFM2;
        param = &(allParameterRows.row[row + synthState_->fullState_.operatorNumber]->params[encoder4]);
        row += synthState_->fullState_.operatorNumber;
    } else if (unlikely(encoder4 == ENCODER_MODULATOR_SYNC_LFO)) {
        // Simulate we're Osc freq
        int lfoButtonState = synthState_->fullState_.buttonState[BUTTONID_EDIT_LFOS];

        row = ROW_LFOOSC1 + lfoButtonState;
        encoder4 = ENCODER_LFO_FREQ;
        num = encoder4 + row * NUMBER_OF_ENCODERS_PFM2;
        param = &(allParameterRows.row[row]->params[encoder4]);

        // Block if necessary
        if ((ticks > 0 && ((float*)pfm3Preset())[num] > 99.95f) ||
            (ticks < 0 && ((float*)pfm3Preset())[num] < 99.95f)) {
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
        lfoFreqPreviousValue_[lfoButtonState] = ((float*)pfm3Preset())[num];

        if (ticks > 0) {
            ticks = 20;
            ((float*)pfm3Preset())[num] = freqToSet - .1f * ticks;
        } else {
            ticks = -1000;
            ((float*)pfm3Preset())[num] = freqToSet - .1f * ticks;
        }
    } else if (unlikely(encoder4 == ENCODER_MODULATOR_SYNC_STEPS)) {
        // Simulate we're Osc freq
        int stepButtonState = synthState_->fullState_.buttonState[BUTTONID_EDIT_STEPS];

        row = ROW_LFOSEQ1 + stepButtonState;
        encoder4 = ENCODER_STEPSEQ_BPM;
        num = encoder4 + row * NUMBER_OF_ENCODERS_PFM2;
        param = &(allParameterRows.row[row]->params[encoder4]);

        // Block if necessary
        if ((ticks > 0 && ((float*)pfm3Preset())[num] > 240.5f) ||
            (ticks < 0 && ((float*)pfm3Preset())[num] < 240.5f)) {
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
        lfoFreqPreviousValue_[3 + stepButtonState] = ((float*)pfm3Preset())[num];

        if (ticks > 0) {
            ticks = 50;
            ((float*)pfm3Preset())[num] = bpmToSet - ticks;
        } else {
            ticks = -240;
            ((float*)pfm3Preset())[num] = bpmToSet - ticks;
        }
    } else {
        num = encoder4 + row * NUMBER_OF_ENCODERS_PFM2;
        param = &(allParameterRows.row[row]->params[encoder4]);
    }
    float newValue;
    float oldValue;

    if (param->displayType == DISPLAY_TYPE_STRINGS) {
        // Do not use encoder acceleration
        ticks = ticks > 0 ? 1 : -1;
    }

    if (param->valueNameOrder == NULL) {
        // Not string parameters

        // floating point test to be sure numberOfValues is different from 1.
        // for voices when number of voices forced to 0
        if (param->numberOfValues < 1.5) {
            return;
        }

        float& value = ((float*)pfm3Preset())[num];
        oldValue = value;

        float inc = param->incValue;

        // Slow down LFO frequency
        if (unlikely(param->displayType == DISPLAY_TYPE_FLOAT_LFO_FREQUENCY)) {
            if (oldValue < 1.0f) {
                inc = inc * .1f;
            }
        }

        int tickIndex = (value - param->minValue) / inc + .0005f + ticks;
        newValue = param->minValue + tickIndex * inc;

        if (newValue > param->maxValue) {
            newValue = param->maxValue;
        }
        if (newValue < param->minValue) {
            newValue = param->minValue;
        }
        value = newValue;
    } else {
        float* value = &((float*)pfm3Preset())[num];
        newValue = oldValue = (*value);

        // Must use newValue (int) so that the minValue comparaison works
        // Is there any other order than the default one
        int pos = param->valueNameOrderReversed[(int)(*value)];

        if (ticks > 0 && pos < param->maxValue) {
            newValue = param->valueNameOrder[pos + 1];
        }
        if (ticks < 0 && pos > param->minValue) {
            newValue = param->valueNameOrder[pos - 1];
        }

        (*value) = (float)newValue;
    }
    if (newValue != oldValue) {
        synthState_->propagateNewPfm3ParamValue(synthState_->getCurrentTimbre(), row, encoder4, param, oldValue, newValue);
    }
}

void EditorParamPage::buttonPressed(int button) {
    switch (button) {
    case BUTTON_PFM3_EDIT:
    case BUTTON_PFM3_MENU:
        synthState_->fullState_.mainPage = -1;
        break;
    case BUTTON_PFM3_1:
    case BUTTON_PFM3_2:
    case BUTTON_PFM3_3:
    case BUTTON_PFM3_4:
    case BUTTON_PFM3_5:
    case BUTTON_PFM3_6: {
        const struct EditorEditMenu* editMenu = currentEditMenu();
        if (editMenu == nullptr) {
            break;
        }
        const struct EditorOneButton* page = currentPage();
        if (button >= editMenu->numberOfButtons) {
            break;
        }
        if (synthState_->fullState_.editPage == button) {
            if (page->numberOfStates > 0) {
                synthState_->fullState_.buttonState[page->buttonId] =
                    (synthState_->fullState_.buttonState[page->buttonId] + 1) % page->numberOfStates;
            }
        } else {
            // If new page has no state.. then we don't activate it
            if (editMenu->pages[button]->numberOfStates > 0) {
                // save what editPage is selected in current mainPage
                parent_->setEditPageSelected(synthState_->fullState_.mainPage, button);

                synthState_->fullState_.editPage = button;
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

void EditorParamPage::updateEncoderValueWithoutCursor(int row, int encoder, ParameterDisplay* param, float oldValue, float newFloatValue) {

    int newValue = (int)newFloatValue;

    switch (param->displayType) {
    case DISPLAY_TYPE_STRINGS:

        // Special case for play mode (mono, poly, unis)
        if (likely(param->valueName != polyMonoNames)) {
            tft_->print(param->valueName[newValue]);
        } else {
            // Display color error information
            if (synthState_->mixerState_.MPE_inst1_ > 0 && newValue != 1) {
                tft_->setCharColor(COLOR_RED);
            }
            tft_->print(param->valueName[newValue]);
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
        int oRow = row - ROW_OSC_FIRST;
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
            int whichStepSeq = row - ROW_LFOSEQ1;
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
