/*
 * Copyright 2020 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier <dot> hosxe (at) g m a i l <dot> com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <FirmwareTftDisplay.h>
#include "ScreenEditor.h"
#include "ScreenMsfaEditor.h"
#include "ScreenPfm3Editor.h"
#include "ScreenTypes.h"
#include "MsfaEngine.h"
#include "SynthState.h"
#include "Timbre.h"
#include "preenfm3.h"

static ScreenMsfaEditor msfaEditorInstance;
static ScreenPfm3Editor pfm3EditorInstance;

ScreenEditor::ScreenEditor() {
    msfaEditor_ = &msfaEditorInstance;
    pfm3Editor_ = &pfm3EditorInstance;
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        presetModifed_[k] = false;
    }
}

void ScreenEditor::init(SynthState *synthState, FirmwareTftDisplay *tft) {
    synthState_ = synthState;
    tft_ = tft;
    msfaEditor_->init(this, synthState, tft);
    pfm3Editor_->init(this, synthState, tft);
}

bool ScreenEditor::isMsfaTimbre() const {
    return synthState_->mixerState.instrumentState_[currentTimbre_].engine == SYNTH_ENGINE_MSFA;
}

const struct EditorEditMenu* ScreenEditor::currentEditMenu() const {
    int page = synthState_->fullState.mainPage;
    if (isMsfaTimbre()) {
        if (page >= 0 && page <= 1) {
            return msfaEditor_->getEditMenu(page);
        }
        if (page == 2) {
            return pfm3Editor_->getEditMenu(4);
        }
    }
    return pfm3Editor_->getEditMenu(page);
}

const char* ScreenEditor::mainMenuName(int button) const {
    if (isMsfaTimbre()) {
        if (button >= 0 && button <= 1) {
            return msfaEditor_->getEditMenu(button)->mainMenuName;
        }
        if (button == 2) {
            return pfm3Editor_->getEditMenu(4)->mainMenuName;
        }
        return "";
    }
    return pfm3Editor_->getEditMenu(button)->mainMenuName;
}

void ScreenEditor::markPresetModified(int timbre) {
    presetModifed_[timbre] = true;
    if (timbre == currentTimbre_) {
        displayPreset();
    }
}

void ScreenEditor::newTimbre(int timbre) {
    currentTimbre_ = timbre;
    if (isMsfaTimbre()) {
        msfaEditor_->checkOperatorNumber();
    } else {
        pfm3Editor_->checkOperatorNumber();
    }
}

void ScreenEditor::newParamValue(int &refreshStatus, int timbre, int currentRow, int encoder,
    ParameterDisplay *param, float oldValue, float newValue) {
    if (isMsfaTimbre()) {
        msfaEditor_->newParamValue(refreshStatus, timbre, currentRow, encoder, param, oldValue, newValue);
    } else {
        pfm3Editor_->newParamValue(refreshStatus, timbre, currentRow, encoder, param, oldValue, newValue);
    }
}

void ScreenEditor::displayParamValue(int encoder, TFT_COLOR color) {
    if (isMsfaTimbre() && synthState_->fullState.mainPage >= 0 && synthState_->fullState.mainPage <= 1) {
        msfaEditor_->displayParamValue(encoder, color);
    } else {
        pfm3Editor_->displayParamValue(encoder, color);
    }
}

void ScreenEditor::refreshEditorByStep(int &refreshStatus, int &endRefreshStatus) {
    if (isMsfaTimbre() && synthState_->fullState.mainPage >= 0 && synthState_->fullState.mainPage <= 1) {
        msfaEditor_->refreshEditorByStep(refreshStatus, endRefreshStatus);
    } else {
        pfm3Editor_->refreshEditorByStep(refreshStatus, endRefreshStatus);
    }
}

void ScreenEditor::tempoClick() {
    if (isMsfaTimbre()) {
        msfaEditor_->tempoClick();
    } else {
        pfm3Editor_->tempoClick();
    }
    displayLevelMeter();
}

void ScreenEditor::displayLevelMeter() {
    static float lastVolume, lastGainReduction;

    if (synthState_->mixerState.levelMeterWhere_ == 2) {
        float volume = getCompInstrumentVolume(currentTimbre_);
        float gr = getCompInstrumentGainReduction(currentTimbre_);
        // gr tend to slower reach 0
        // Let'as accelerate when we don't want to draw the metter anymore
        if (gr > -.1f) {
            gr = 0;
        }
        if (volume != lastVolume || gr != lastGainReduction) {
            lastVolume = volume;
            lastGainReduction = gr;

            bool isCompressed = synthState_->mixerState.instrumentState_[currentTimbre_].compressorType > 0;
            tft_->drawLevelMetter(5, 150, 230, 3, 5, volume, isCompressed, gr);
        }
    }
}

void ScreenEditor::displayPreset() {

    tft_->fillArea(0, 0, 240, 21, COLOR_DARK_BLUE);

    tft_->setCharBackgroundColor(COLOR_DARK_BLUE);
    tft_->setCharColor(COLOR_YELLOW);
    tft_->setCursorInPixel(2, 2);
    if (synthState_->getIsPlayingNote()) {
        tft_->print('~');
    } else {
        tft_->print('I');
    }
    tft_->print((char) ('0' + currentTimbre_ + 1));
    tft_->print(' ');
    tft_->setCharColor(COLOR_WHITE);
    tft_->print(synthState_->params->presetName);
    tft_->setCharColor(COLOR_YELLOW);
    if (presetModifed_[currentTimbre_]) {
        tft_->print('*');
    }
    tft_->setCharBackgroundColor(COLOR_BLACK);
}

void ScreenEditor::resetAllPresetModified() {
    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        presetModifed_[k] = false;
    }
}

void ScreenEditor::setPresetModified(int timbre, bool state) {
    presetModifed_[timbre] = state;
}

void ScreenEditor::updateArpPattern(int currentRow, int encoder, int oldValue, int newValue) {

    const int index = (int) synthState_->params->engineArp2.pattern - ARPEGGIATOR_PRESET_PATTERN_COUNT;
    const arp_pattern_t user_pattern = synthState_->params->engineArpUserPatterns.patterns[index];

    if (synthState_->params->engineArp2.pattern < ARPEGGIATOR_PRESET_PATTERN_COUNT) {
        return;
    }

    if (encoder == 1) {
        // new value to display
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(synthState_->patternSelect, 6);
        tft_->print((char) (127 + ((newValue >> (synthState_->patternSelect)) & 0x1) * 15));
    } else if (encoder == 0) {
        if ((oldValue % 4) == 0) {
            tft_->setCharColor(COLOR_WHITE);
        } else {
            tft_->setCharColor(COLOR_GRAY);
        }
        tft_->setCursor(oldValue, 6);
        tft_->print((char) (127 + ((user_pattern >> (oldValue)) & 0x1) * 15));
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(newValue, 6);
        tft_->print((char) (127 + ((user_pattern >> (newValue)) & 0x1) * 15));
    }
}

void ScreenEditor::updateStepSequencer(int currentRow, int encoder, int oldValue, int newValue) {
    int whichStepSeq = currentRow - ROW_LFOSEQ1;

    StepSequencerSteps *seqSteps = &((StepSequencerSteps*) (&synthState_->params->lfoSteps1))[whichStepSeq];

    tft_->setCharBackgroundColor(COLOR_BLACK);

    if (encoder == 3) {
        // new value to display
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(synthState_->stepSelect[whichStepSeq], 6);
        tft_->print((char) (127 + newValue));
    } else if (encoder == 2) {
        if ((oldValue % 4) == 0) {
            tft_->setCharColor(COLOR_WHITE);
        } else {
            tft_->setCharColor(COLOR_GRAY);
        }
        tft_->setCursor(oldValue, 6);
        tft_->print((char) (127 + seqSteps->steps[oldValue]));
        tft_->setCharColor(COLOR_YELLOW);
        tft_->setCursor(newValue, 6);
        tft_->print((char) (127 + seqSteps->steps[newValue]));
    }
}

void ScreenEditor::encoderTurned(int encoder6, int ticks) {
    struct FullState *fullState = &(synthState_->fullState);

    if (unlikely(fullState->mainPage == -1)) {
        return;
    }

    if (isMsfaTimbre() && fullState->mainPage <= 1) {
        msfaEditor_->encoderTurned(encoder6, ticks);
    } else {
        pfm3Editor_->encoderTurned(encoder6, ticks);
    }
}

void ScreenEditor::encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button) {

    if (isMsfaTimbre() && synthState_->fullState.mainPage >= 0 && synthState_->fullState.mainPage <= 1) {
        msfaEditor_->encoderTurnedWhileButtonPressed(encoder6, ticks, button);
    } else {
        pfm3Editor_->encoderTurnedWhileButtonPressed(encoder6, ticks, button);
    }
}

void ScreenEditor::buttonPressed(int button) {
    if (isMsfaTimbre() && synthState_->fullState.mainPage >= 0 && synthState_->fullState.mainPage <= 1) {
        msfaEditor_->buttonPressed(button);
    } else if (synthState_->fullState.mainPage == -1) {
        buttonPressedMenuPage(button);
    } else {
        pfm3Editor_->buttonPressed(button);
    }

    synthState_->propagateNewPfm3Page();
}

void ScreenEditor::buttonPressedMenuPage(int button) {
    if (button >= BUTTON_PFM3_1 && button <= BUTTON_PFM3_6
            && !(isMsfaTimbre() && button >= BUTTON_PFM3_4)) {
        synthState_->fullState.mainPage = button;
        int editPage = editPageSelected_[synthState_->fullState.mainPage];
        if (editPage >= currentEditMenu()->numberOfButtons) {
            editPage = 0;
        }
        synthState_->fullState.editPage = editPage;
     }
}

void ScreenEditor::displayPopup(TFT_COLOR color, char* text, uint8_t length) {
    tft_->fillArea(60, 66, 120, 36, color);
    tft_->fillArea(62, 68, 116, 32, COLOR_BLACK);
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(color);
    tft_->setCursorInPixel(119 - ((length * 11) / 2), 75);
    tft_->print(text);
    tft_->waitCycle(500);
    synthState_->propagateNewPfm3Page();
}

void ScreenEditor::buttonLongPressed(int instrument, int button) {
    if (isMsfaTimbre() && synthState_->fullState.mainPage >= 0 && synthState_->fullState.mainPage <= 1) {
        msfaEditor_->buttonLongPressed(instrument, button);
        return;
    }

    pfm3Editor_->buttonLongPressed(instrument, button);
}

void ScreenEditor::refreshOscillatorOperatorShape() {
    if (isMsfaTimbre()) {
        msfaEditor_->refreshOscillatorOperatorShape();
    } else {
        pfm3Editor_->refreshOscillatorOperatorShape();
    }
}

void ScreenEditor::refreshOscillatorOperatorEnvelope() {
    int op = synthState_->fullState.operatorNumber;
    if (isMsfaTimbre()) {
        MsfaEnvelopeDisplay env;
        msfaGetEnvelopeDisplay(synthState_->getTimbre(currentTimbre_)->getMsfaProgram(), op, &env);

        tft_->oscilloBgSetEnvelope(env.time[0], env.time[1], env.time[2], env.time[3],
                env.level[0], env.level[1], env.level[2], env.level[3],
                CURVE_TYPE_LIN, CURVE_TYPE_LIN, CURVE_TYPE_LIN, CURVE_TYPE_LIN);
        tft_->oscilloBgActionEnvelope();
        return;
    }
    EnvelopeTimeMemory *envTime = &synthState_->params->env1Time;
    EnvelopeLevelMemory *envLevel = &synthState_->params->env1Level;
    EnvelopeCurveParams *envCurve = &synthState_->params->env1Curve;
    tft_->oscilloBgSetEnvelope(envTime[op].attackTime, envTime[op].decayTime, envTime[op].sustainTime, envTime[op].releaseTime,
            envLevel[op].attackLevel, envLevel[op].decayLevel, envLevel[op].sustainLevel, envLevel[op].releaseLevel,
            envCurve[op].attackCurve, envCurve[op].decayCurve, envCurve[op].sustainCurve, envCurve[op].releaseCurve);
    tft_->oscilloBgActionEnvelope();
}

void ScreenEditor::refreshLfoOscillator() {
    if (isMsfaTimbre()) {
        msfaEditor_->refreshLfoOscillator();
    } else {
        pfm3Editor_->refreshLfoOscillator();
    }
}

void ScreenEditor::refreshLfoEnv() {
    if (isMsfaTimbre()) {
        msfaEditor_->refreshLfoEnv();
    } else {
        pfm3Editor_->refreshLfoEnv();
    }
}
