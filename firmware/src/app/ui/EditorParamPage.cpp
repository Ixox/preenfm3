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
    arpPatternBuffer_ = 0;
    arpPatternLengthBuffer_ = 16;
    arpPatternBufferValid_ = false;
    for (int k = 0; k < 16; k++) {
        arpStepModesBuffer_[k] = ARP_STEP_MODE_NORM;
    }
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

bool EditorParamPage::isInArpPatternPage() const {
    const struct EditorPageButton* page = currentPage();
    return page != nullptr && page->buttonId == BUTTONID_EDIT_ARPEGIATOR && synthState_->fullState_.buttonState[page->buttonId] == 1;
}

bool EditorParamPage::copyArpUserPattern() {
    Pfm3Preset* preset = pfm3Preset();
    const int pattern = (int)preset->engineArp2.pattern;
    if (pattern < 0 || pattern >= ARPEGGIATOR_PATTERN_COUNT) {
        parent_->displayPopup(COLOR_RED, "Cannot", 6);
        return false;
    }
    const bool factory = pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT;
    uint16_t mask = 0;
    uint16_t accent = 0;
    int length = 16;
    int modes[16];
    getArpStepDisplay(pattern, factory, mask, accent, length, modes);

    arpPatternBuffer_ = ARP_PATTERN_ACCENT(mask, accent);
    for (int k = 0; k < 16; k++) {
        arpStepModesBuffer_[k] = (uint8_t)modes[k];
    }
    arpPatternLengthBuffer_ = (uint8_t)length;
    arpPatternBufferValid_ = true;
    if (factory) {
        parent_->displayPopup(COLOR_GREEN, "Copy Fct", 8);
    } else {
        parent_->displayPopup(COLOR_GREEN, "Copy Usr", 8);
    }
    return true;
}

bool EditorParamPage::pasteArpUserPattern() {
    Pfm3Preset* preset = pfm3Preset();
    const int pattern = (int)preset->engineArp2.pattern;
    if (pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT || !arpPatternBufferValid_) {
        parent_->displayPopup(COLOR_RED, "Cannot", 6);
        return false;
    }
    const int userIndex = pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT;
    if (userIndex < 0 || userIndex >= ARPEGGIATOR_USER_PATTERN_COUNT) {
        parent_->displayPopup(COLOR_RED, "Cannot", 6);
        return false;
    }
    Timbre* timbre = synthState_->getTimbre(currentTimbre());
    timbre->setArpUserPattern(userIndex, arpPatternBuffer_);

    ArpAdvancedParams adv = timbre->sharedParams().arpAdvanced;
    for (int k = 0; k < 16; k++) {
        arpAdvancedSetStepMode(adv, userIndex, k, arpStepModesBuffer_[k]);
    }
    arpAdvancedSetUserLength(adv, userIndex, arpPatternLengthBuffer_);
    timbre->setArpAdvanced(adv);

    for (int e = 2; e < NUMBER_OF_ENCODERS; e++) {
        displayParamValue(e, COLOR_WHITE);
    }
    queueArpStepsBg();
    parent_->displayPopup(COLOR_GREEN, "Paste Usr", 9);
    return true;
}

void EditorParamPage::buttonLongPressed(int instrument, int button) {
    (void)instrument;
    if (!isInArpPatternPage()) {
        return;
    }
    if (button == BUTTON_PREVIOUS_INSTRUMENT) {
        copyArpUserPattern();
    } else if (button == BUTTON_NEXT_INSTRUMENT) {
        pasteArpUserPattern();
    }
}

static void printPaddedInt(FirmwareTftDisplay* tft, int value, int width) {
    char buf[8];
    if (width > 7) {
        width = 7;
    } else if (width < 1) {
        width = 1;
    }
    for (int i = 0; i < width; i++) {
        buf[i] = ' ';
    }
    buf[width] = 0;
    int pos = width - 1;
    if (value == 0) {
        buf[pos--] = '0';
    }
    while (value > 0 && pos >= 0) {
        buf[pos--] = (char)('0' + (value % 10));
        value /= 10;
    }
    tft->print(buf);
}

static const char* arpStepModeName(int mode) {
    static const char* names[ARP_STEP_MODE_COUNT] = {"Norm", "Tie ", "Oct+", "Rt*2", "Chrd", "Slap", "Dyad", "Oct-"};
    if (mode < 0 || mode >= ARP_STEP_MODE_COUNT) {
        mode = 0;
    }
    return names[mode];
}

static TFT_COLOR arpStepColor(int mode, bool accentOn) {
    switch (mode) {
    case ARP_STEP_MODE_TIE:
        return COLOR_CYAN;
    case ARP_STEP_MODE_OCT_UP:
        return COLOR_BLUE;
    case ARP_STEP_MODE_RATCHET2:
        return COLOR_GREEN;
    case ARP_STEP_MODE_CHORD:
        return COLOR_YELLOW;
    case ARP_STEP_MODE_SLAP:
        return COLOR_RED;
    case ARP_STEP_MODE_DYAD:
        return COLOR_ORANGE;
    case ARP_STEP_MODE_OCT_DOWN:
        return COLOR_LIGHT_GREEN;
    default:
        return accentOn ? COLOR_RED : COLOR_WHITE;
    }
}

void EditorParamPage::getArpStepDisplay(int index, bool factory, uint16_t& mask, uint16_t& accent, int& length, int modes[16]) const {
    Pfm3Preset* preset = pfm3Preset();
    for (int k = 0; k < 16; k++) {
        modes[k] = ARP_STEP_MODE_NORM;
    }
    if (factory) {
        if (index < 0 || index >= ARPEGGIATOR_PRESET_PATTERN_COUNT) {
            index = 0;
        }
        uint16_t tie = 0, oct = 0, rt = 0, octDown = 0, dyad = 0, chord = 0, slap = 0;
        const int seed = arpAdvancedGetEuclideanSeed(preset->arpAdvanced);
        arpComputePresetMasks(ARP_PATTERN_GETMASK(lut_res_arpeggiator_patterns[index]),
                              ARP_PATTERN_GETACCENT(lut_res_arpeggiator_patterns[index]), lut_res_arpeggiator_ties[index], seed, &accent,
                              &tie, &oct, &rt, &octDown, &dyad, &chord, &slap);
        mask = ARP_PATTERN_GETMASK(lut_res_arpeggiator_patterns[index]);
        length = arpPresetLength(seed);
        for (int k = 0; k < 16; k++) {
            const uint16_t bit = (uint16_t)(1u << k);
            if (tie & bit) {
                modes[k] = ARP_STEP_MODE_TIE;
            } else if (oct & bit) {
                modes[k] = ARP_STEP_MODE_OCT_UP;
            } else if (rt & bit) {
                modes[k] = ARP_STEP_MODE_RATCHET2;
            } else if (octDown & bit) {
                modes[k] = ARP_STEP_MODE_OCT_DOWN;
            } else if (dyad & bit) {
                modes[k] = ARP_STEP_MODE_DYAD;
            } else if (chord & bit) {
                modes[k] = ARP_STEP_MODE_CHORD;
            } else if (slap & bit) {
                modes[k] = ARP_STEP_MODE_SLAP;
            }
        }
        return;
    }

    const int user = index - ARPEGGIATOR_PRESET_PATTERN_COUNT;
    const arp_pattern_t word = preset->engineArpUserPatterns.patterns[user];
    mask = ARP_PATTERN_GETMASK(word);
    accent = ARP_PATTERN_GETACCENT(word);
    length = arpAdvancedGetUserLength(preset->arpAdvanced, user);
    for (int k = 0; k < 16; k++) {
        modes[k] = arpAdvancedGetStepMode(preset->arpAdvanced, user, k);
    }
}

void EditorParamPage::displayCustomValue(int encoder, const EditorSlot& slot, TFT_COLOR color) {
    (void)color;
    Pfm3Preset* preset = pfm3Preset();
    const int index = (int)preset->engineArp2.pattern;
    const bool factory = index < ARPEGGIATOR_PRESET_PATTERN_COUNT;
    const int sel = (int)patternSelect_;

    if (slot.context == (uint8_t)EditorCustomContext::ArpAdvanced) {
        const ArpAdvancedParams& adv = preset->arpAdvanced;
        switch (slot.id) {
        case 0:
            printPaddedInt(tft_, arpAdvancedGetSwing(adv), 3);
            break;
        case 1:
            printPaddedInt(tft_, arpAdvancedGetGroove(adv), 2);
            break;
        case 2:
            printPaddedInt(tft_, arpAdvancedGetHumanize(adv), 3);
            break;
        default:
            break;
        }
        return;
    }

    uint16_t mask = 0;
    uint16_t accent = 0;
    int length = 16;
    int modes[16];
    getArpStepDisplay(index, factory, mask, accent, length, modes);

    if (slot.id == 0) {
        // Cursor value: selected step, 1..16. The steps themselves are drawn
        // in the oscilloscope background by queueArpStepsBg().
        printPaddedInt(tft_, sel + 1, 3);
        return;
    }

    if (slot.id == 1) {
        static const char* states[3] = {"Off ", "On  ", "Acc "};
        const bool on = ((mask >> sel) & 0x1u) != 0;
        const bool acc = ((accent >> sel) & 0x1u) != 0;
        if (factory) {
            tft_->setCharColor(COLOR_DARK_GRAY);
        }
        tft_->print(states[on ? (acc ? 2 : 1) : 0]);
        return;
    }

    if (slot.id == 2) {
        if (factory) {
            tft_->setCharColor(COLOR_DARK_GRAY);
        }
        tft_->print(arpStepModeName(modes[sel]));
        return;
    }

    if (factory) {
        printPaddedInt(tft_, arpAdvancedGetEuclideanSeed(preset->arpAdvanced), 3);
    } else {
        printPaddedInt(tft_, length, 3);
    }
}

void EditorParamPage::encoderTurnedForCustom(int encoder, const EditorSlot& slot, int ticks) {
    Pfm3Preset* preset = pfm3Preset();
    Timbre* timbre = synthState_->getTimbre(currentTimbre());
    const int index = (int)preset->engineArp2.pattern;
    const bool factory = index < ARPEGGIATOR_PRESET_PATTERN_COUNT;
    const int dir = (ticks > 0) ? 1 : -1;

    if (slot.context == (uint8_t)EditorCustomContext::ArpAdvanced) {
        ArpAdvancedParams adv = timbre->sharedParams().arpAdvanced;
        switch (slot.id) {
        case 0:
            arpAdvancedSetSwing(adv, arpAdvancedGetSwing(adv) + dir);
            break;
        case 1:
            arpAdvancedSetGroove(adv, arpAdvancedGetGroove(adv) + dir);
            break;
        case 2:
            arpAdvancedSetHumanize(adv, arpAdvancedGetHumanize(adv) + dir);
            break;
        default:
            return;
        }
        timbre->setArpAdvanced(adv);
        displayParamValue(encoder, COLOR_YELLOW);
        return;
    }

    const int oldStep = (int)patternSelect_;
    ArpBgUpdate update;

    switch (slot.id) {
    case 0: {
        int pos = (int)patternSelect_ + dir;
        if (pos < 0) {
            pos = 15;
        } else if (pos > 15) {
            pos = 0;
        }
        patternSelect_ = (char)pos;
        update.cursorMask = (uint16_t)((1u << oldStep) | (1u << (int)patternSelect_));
        displayParamValue(2, COLOR_WHITE);
        displayParamValue(3, COLOR_WHITE);
        displayParamValue(4, COLOR_WHITE);
        break;
    }
    case 1: {
        if (factory) {
            return;
        }
        const int user = index - ARPEGGIATOR_PRESET_PATTERN_COUNT;
        arp_pattern_t word = preset->engineArpUserPatterns.patterns[user];
        uint16_t mask = ARP_PATTERN_GETMASK(word);
        uint16_t accent = ARP_PATTERN_GETACCENT(word);
        const int sel = (int)patternSelect_;
        const bool on = ((mask >> sel) & 0x1u) != 0;
        const bool acc = ((accent >> sel) & 0x1u) != 0;
        int state = on ? (acc ? 2 : 1) : 0;
        state += dir;
        if (state < 0) {
            state = 0;
        } else if (state > 2) {
            state = 2;
        }
        if (state == 0) {
            mask &= (uint16_t)~(1u << sel);
            accent &= (uint16_t)~(1u << sel);
        } else if (state == 1) {
            mask |= (uint16_t)(1u << sel);
            accent &= (uint16_t)~(1u << sel);
        } else {
            mask |= (uint16_t)(1u << sel);
            accent |= (uint16_t)(1u << sel);
        }
        ARP_PATTERN_SETMASK(word, mask);
        ARP_PATTERN_SETACCENT(word, accent);
        timbre->setArpUserPattern(user, word);
        update.stepMask = (uint16_t)(1u << (int)patternSelect_);
        displayParamValue(2, COLOR_WHITE);
        displayParamValue(3, COLOR_YELLOW);
        break;
    }
    case 2: {
        if (factory) {
            return;
        }
        const int user = index - ARPEGGIATOR_PRESET_PATTERN_COUNT;
        ArpAdvancedParams adv = timbre->sharedParams().arpAdvanced;
        int mode = arpAdvancedGetStepMode(adv, user, (int)patternSelect_) + dir;
        if (mode < 0) {
            mode = 0;
        } else if (mode >= ARP_STEP_MODE_COUNT) {
            mode = ARP_STEP_MODE_COUNT - 1;
        }
        arpAdvancedSetStepMode(adv, user, (int)patternSelect_, mode);
        timbre->setArpAdvanced(adv);
        update.modeMask = (uint16_t)(1u << (int)patternSelect_);
        displayParamValue(2, COLOR_WHITE);
        displayParamValue(3, COLOR_WHITE);
        displayParamValue(4, COLOR_YELLOW);
        break;
    }
    case 3: {
        ArpAdvancedParams adv = timbre->sharedParams().arpAdvanced;
        if (factory) {
            int seed = arpAdvancedGetEuclideanSeed(adv) + dir;
            if (seed < 0) {
                seed = 0;
            } else if (seed > 15) {
                seed = 15;
            }
            arpAdvancedSetEuclideanSeed(adv, seed);
            timbre->setArpAdvanced(adv);
            update.full = true;
            displayParamValue(2, COLOR_WHITE);
            displayParamValue(3, COLOR_WHITE);
            displayParamValue(4, COLOR_WHITE);
        } else {
            const int user = index - ARPEGGIATOR_PRESET_PATTERN_COUNT;
            const int oldLength = arpAdvancedGetUserLength(adv, user);
            int length = oldLength + dir;
            if (length < 1) {
                length = 1;
            } else if (length > 16) {
                length = 16;
            }
            arpAdvancedSetUserLength(adv, user, length);
            timbre->setArpAdvanced(adv);
            update.lengthMask = (uint16_t)((1u << (oldLength - 1)) | (1u << (length - 1)));
            displayParamValue(2, COLOR_WHITE);
        }
        displayParamValue(5, COLOR_YELLOW);
        break;
    }
    default:
        update.full = true;
        break;
    }
    queueArpStepsBg(update);
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
    if (slot.kind == EditorSlotKind::Custom) {
        if (slot.context == (uint8_t)EditorCustomContext::ArpAdvanced) {
            static const char* names[3] = {"Swing", "Grve", "Humn"};
            tft_->print(names[slot.id < 3 ? slot.id : 0]);
        } else {
            const bool factory = (int)pfm3Preset()->engineArp2.pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT;
            if (factory && (slot.id == 1 || slot.id == 2)) {
                tft_->setCharColor(COLOR_DARK_GRAY);
            }
            switch (slot.id) {
            case 0:
                tft_->print("Cursr");
                break;
            case 1:
                tft_->print("Step");
                break;
            case 2:
                tft_->print("Mode");
                break;
            case 3:
                tft_->print(factory ? "Eucl" : "Len");
                break;
            default:
                break;
            }
        }
        return;
    }
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
        tft_->setCharBackgroundColor(COLOR_BLACK);
        tft_->setCharColor(color);
        displayCustomValue(encoder, slot, color);
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
        const EditorSlot gridSlot = slotForEncoder(2);
        if (gridSlot.kind == EditorSlotKind::Custom && gridSlot.context == (uint8_t)EditorCustomContext::ArpPattern) {
            for (int e = 2; e < NUMBER_OF_ENCODERS; e++) {
                displayParamValue(e, COLOR_WHITE);
            }
            queueArpStepsBg();
        }
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
        // Pattern changed: names can change colour (Step/Mode are only
        // editable on user patterns), so redraw names as well as values.
        refreshStatus = 12;
        const EditorSlot gridSlot = slotForEncoder(2);
        if (gridSlot.kind == EditorSlotKind::Custom && gridSlot.context == (uint8_t)EditorCustomContext::ArpPattern) {
            for (int e = 2; e < NUMBER_OF_ENCODERS; e++) {
                displayParamValue(e, COLOR_WHITE);
            }
            queueArpStepsBg();
        }
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
        encoderTurnedForCustom(encoder, slot, ticks);
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
        refreshOscilloBG();
        break;
    }
    }
}

void EditorParamPage::preparePage() {
    resetHideParams();
}

void EditorParamPage::queueArpStepsBg() {
    ArpBgUpdate update;
    update.full = true;
    queueArpStepsBg(update);
}

void EditorParamPage::queueArpStepsBg(const ArpBgUpdate& update) {
    if (!isInArpPatternPage()) {
        return;
    }
    Pfm3Preset* preset = pfm3Preset();
    const int index = (int)preset->engineArp2.pattern;
    const bool factory = index < ARPEGGIATOR_PRESET_PATTERN_COUNT;
    uint16_t mask = 0;
    uint16_t accent = 0;
    int length = 16;
    int modes[16];
    getArpStepDisplay(index, factory, mask, accent, length, modes);

    uint8_t levels[16];
    uint8_t colors[16];
    uint8_t modeColors[16];
    uint16_t modeMask = 0;
    for (int k = 0; k < 16; k++) {
        const bool on = ((mask >> k) & 0x1u) != 0;
        const bool acc = ((accent >> k) & 0x1u) != 0;
        const bool isMode = modes[k] != ARP_STEP_MODE_NORM;
        levels[k] = on ? 15 : 0;
        colors[k] = (uint8_t)(acc ? COLOR_RED : COLOR_WHITE);
        modeColors[k] = (uint8_t)(isMode ? arpStepColor(modes[k], acc) : COLOR_WHITE);
        if (isMode) {
            modeMask |= (uint16_t)(1u << k);
        }
    }
    tft_->oscilloBgSetArpSteps(levels, colors, modeColors, modeMask, (int)patternSelect_, length);
    tft_->oscilloBgArp(update);
}

void EditorParamPage::refreshOscilloBG() {
    if (isInArpPatternPage()) {
        queueArpStepsBg();
    } else {
        tft_->oscilloBgActionClear();
    }
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
