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

#include "FMDisplay3.h"
#include "FMDisplay.h"
#include "FMDisplayMixer.h"
#include "FMDisplayMenu.h"
#include "FMDisplayEditor.h"
#include "FMDisplaySequencer.h"
#include "ili9341.h"

extern struct WaveTable waveTables[NUMBER_OF_WAVETABLES];
extern uint8_t midiControllerMode;

int getLength(const char *str) {
    int length = 0;
    for (const char *c = str; *c != '\0'; c++) {
        length++;
    }
    return length;
}

FMDisplay3::FMDisplay3() {
}

FMDisplay3::~FMDisplay3() {
}

void FMDisplay3::init(FirmwareTftDisplay *tft) {

    for (int w = 0; w < NUMBER_OF_WAVETABLES; w++) {
        tft->initWaveFormExt(w, waveTables[w].table, waveTables[w].max);
    }

    refreshStatus_ = 0;
    endRefreshStatus_ = 0;
    this->tft_ = tft;

    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        noteOnCounter_[k] = 0;
    }
    currentTimbre_ = 0;

    tft->clear();
}

void FMDisplay3::setDisplays(FMDisplayMixer *displayMixer, FMDisplayEditor *displayEditor, FMDisplayMenu *displayMenu, FMDisplaySequencer *displaySequencer) {
    this->displayMixer_ = displayMixer;
    this->displayEditor_ = displayEditor;
    this->displayMenu_ = displayMenu;
    this->displaySequencer_ = displaySequencer;
    this->displaySequencer_->setRefreshStatusPointer(&refreshStatus_, &endRefreshStatus_);
    this->displayMixer_->setRefreshStatusPointer(&refreshStatus_, &endRefreshStatus_);
}

bool FMDisplay3::shouldThisValueShowUpPfm3(int row, int encoder, int encoder6) {
    int algo = this->synthState_->params->engine1.algo;
    switch (row) {
        case ROW_MODULATION1:
        case ROW_MODULATION2:
        case ROW_MODULATION3:
            if (unlikely(encoder6 >= algoInformation[algo].im)) {
                return false;
            }
        case ROW_OSC_MIX1:
        case ROW_OSC_MIX2:
        case ROW_OSC_MIX3:
            if (unlikely(encoder6 >= algoInformation[algo].mix)) {
                return false;
            }
            break;
        case ROW_EFFECT: {
            if (unlikely(encoder == 0)) {
                return true;
            }
            int effect = this->synthState_->params->effect.type;
            if (filterRowDisplay[effect].paramName[encoder - 1] == NULL) {
                return false;
            }
        }
            break;
        case ROW_ARPEGGIATOR1:
            if (encoder >= 1 && this->synthState_->params->engineArp1.clock == 0.0f) {
                return false;
            }
            break;
        default:
            break;
    }

    return true;
}

void FMDisplay3::afterNewMixerLoad() {
    displayEditor_->resetAllPresetModified();
}

void FMDisplay3::refreshAllScreenByStep() {
    switch (this->synthState_->fullState.synthMode) {
        case SYNTH_MODE_MIXER:
            displayMixer_->refreshMixerByStep(currentTimbre_, refreshStatus_, endRefreshStatus_);
            break;
        case SYNTH_MODE_EDIT_PFM3:
            displayEditor_->refreshEditorByStep(refreshStatus_, endRefreshStatus_);
            break;
        case SYNTH_MODE_MENU:
            displayMenu_->refreshMenuByStep(currentTimbre_, refreshStatus_);
            break;
        case SYNTH_MODE_SEQUENCER:
            displaySequencer_->refreshSequencerByStep(currentTimbre_, refreshStatus_, endRefreshStatus_);
            break;
    }

    if (refreshStatus_ > 0) {
        refreshStatus_--;
    }
    if (refreshStatus_ == 0) {
        tft_->restartRefreshTft();
    }
}

void FMDisplay3::newPresetName(int timbre) {
	if (this->synthState_->getSynthMode() == SYNTH_MODE_MIXER) {
		displayMixer_->refreshInstrument(timbre);
	} else if (this->synthState_->getSynthMode() == SYNTH_MODE_SEQUENCER) {
		displaySequencer_->refreshInstrument(timbre);
	} else if (currentTimbre_ == timbre && this->synthState_->getSynthMode() == SYNTH_MODE_EDIT_PFM3) {
    	displayEditor_->displayPreset();
    }
}

void FMDisplay3::newTimbre(int timbre) {
    currentTimbre_ = timbre;
    displayEditor_->newTimbre(timbre);

    if (this->synthState_->fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        tft_->clearActions();
        tft_->pauseRefresh();
        tft_->clear();
        tft_->drawAlgo(this->synthState_->params->engine1.algo);
        displayEditor_->displayPreset();
        refreshStatus_ = 20;
        endRefreshStatus_ = 0;

        this->refreshOscilloBG();
    } else if (this->synthState_->fullState.synthMode == SYNTH_MODE_MIXER) {
        refreshStatus_ = 19;
        endRefreshStatus_ = 14;
    } else if (this->synthState_->fullState.synthMode == SYNTH_MODE_SEQUENCER) {
        displaySequencer_->newTimbre(timbre, refreshStatus_, endRefreshStatus_);
    }
}

void FMDisplay3::newMixerEdit(int oldButton, int newButton) {
    tft_->pauseRefresh();

    if (((oldButton == BUTTONID_MIXER_GLOBAL || newButton == BUTTONID_MIXER_GLOBAL) && (oldButton != newButton))
        || ((oldButton == BUTTONID_MIXER_SCALA || newButton == BUTTONID_MIXER_SCALA))) {
        tft_->clearMixerLabels();
        refreshStatus_ = 19;
    } else {
        refreshStatus_ = 12;
    }

    tft_->clearMixerValues();
}

void FMDisplay3::newParamValue(int timbre, int currentRow, int encoder, ParameterDisplay *param, float oldValue, float newValue) {
    checkPresetModified(timbre);

    if (this->synthState_->getSynthMode() == SYNTH_MODE_EDIT_PFM3) {
        displayEditor_->newParamValue(refreshStatus_, timbre, currentRow, encoder, param, oldValue, newValue);
    }
}

void FMDisplay3::newParamValueFromExternal(int timbre, int currentRow, int encoder, ParameterDisplay *param, float oldValue, float newValue) {
    checkPresetModified(timbre);

    // If Operator we must send
    if (this->synthState_->getSynthMode() == SYNTH_MODE_EDIT_PFM3 && this->synthState_->fullState.mainPage != -1 && timbre == currentTimbre_) {

        displayEditor_->newParamValue(refreshStatus_, timbre, currentRow, encoder, param, oldValue, newValue);
    }
}

void FMDisplay3::newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
    // Filter MIXER mode here and filter page in displayMixer
    if (this->synthState_->getSynthMode() == SYNTH_MODE_MIXER) {
        displayMixer_->newMixerValueFromExternal(mixerValueType, timbre, oldValue, newValue);
    }
}

/*
 * Menu Listener
 */

void FMDisplay3::newSynthMode(FullState *fullState) {

    switch (fullState->synthMode) {
        case SYNTH_MODE_MIXER:
            refreshStatus_ = 21;
            break;
        case SYNTH_MODE_EDIT_PFM3:
            refreshStatus_ = 21;
            break;
        case SYNTH_MODE_MENU:
            displayMenu_->setPreviousSynthMode(fullState->synthModeBeforeMenu);
            refreshStatus_ = 21;
            break;
        case SYNTH_MODE_SEQUENCER:
            refreshStatus_ = 21;
            displaySequencer_->cleanCurrentState();
            break;
    }
}

int FMDisplay3::rowInc(MenuState menuState) {
    switch (menuState) {
        case MENU_MIXER_SAVE_ENTER_NAME:
        case MENU_SEQUENCER_SAVE_ENTER_NAME:
        case MENU_PRESET_SAVE_ENTER_NAME:
        case MAIN_MENU:
        case MENU_SD_RENAME_MIXER_ENTER_NAME:
        case MENU_SD_RENAME_PRESET_ENTER_NAME:
        case MENU_SD_RENAME_SEQUENCE_ENTER_NAME:
            return 0;
        default:
            break;
    }
    return 1;
}

void FMDisplay3::newMenuState(FullState *fullState) {
    displayMenu_->newMenuState(fullState);
    refreshStatus_ = 6;
}

void FMDisplay3::newMenuSelect(FullState *fullState) {
    displayMenu_->newMenuSelect(fullState);
}

void FMDisplay3::menuBack(const MenuItem *oldMenuItem, FullState *fullState) {
    displayMenu_->menuBack(oldMenuItem, fullState);
    refreshStatus_ = 6;
}

// VisualInfo

void FMDisplay3::midiClock(bool show) {
    if (midiControllerMode != 0) {
        return;
    }

    int x = 240 - 4 * TFT_SMALL_CHAR_WIDTH;
    int y = 1;
    tft_->setCursorInPixel(x, y);
    switch (this->synthState_->fullState.synthMode) {
        case SYNTH_MODE_MIXER:
            tft_->setCharBackgroundColor(COLOR_DARK_GREEN);
            break;
        case SYNTH_MODE_MENU:
            tft_->setCharBackgroundColor(COLOR_DARK_RED);
            break;
        case SYNTH_MODE_EDIT_PFM3:
            tft_->setCharBackgroundColor(COLOR_DARK_BLUE);
            break;
        case SYNTH_MODE_SEQUENCER:
            tft_->setCharBackgroundColor(COLOR_DARK_YELLOW);
            break;
    }
    tft_->setCharColor(COLOR_WHITE);
    if (show) {
        // The led in preenfm3 shows up when the sequencer is activated
        // HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_SET);
        tft_->printSmallChar('*');
    } else {
        // HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_RESET);
        tft_->printSmallChar(' ');
    }
    tft_->setCharBackgroundColor(COLOR_BLACK);
}

void FMDisplay3::noteOn(int timbre, bool show) {
    if (midiControllerMode != 0) {
        return;
    }
    int x = 240 - (3 - (timbre % 3)) * TFT_SMALL_CHAR_WIDTH;
    int y = 1 + (timbre / 3) * TFT_SMALL_CHAR_HEIGHT;
    tft_->setCursorInPixel(x, y);
    switch (this->synthState_->fullState.synthMode) {
        case SYNTH_MODE_MIXER:
            tft_->setCharBackgroundColor(COLOR_DARK_GREEN);
            break;
        case SYNTH_MODE_MENU:
            tft_->setCharBackgroundColor(COLOR_DARK_RED);
            break;
        case SYNTH_MODE_EDIT_PFM3:
            tft_->setCharBackgroundColor(COLOR_DARK_BLUE);
            break;
        case SYNTH_MODE_SEQUENCER:
            tft_->setCharBackgroundColor(COLOR_DARK_YELLOW);
            break;
    }
    tft_->setCharColor(COLOR_WHITE);

    if (show) {
        if (noteOnCounter_[timbre] == 0) {
            tft_->printSmallChar((char) ('0' + timbre + 1));
        }
        noteOnCounter_[timbre] = 2;
    } else {
        tft_->printSmallChar(' ');
    }
    if (likely(this->synthState_->fullState.synthMode == SYNTH_MODE_SEQUENCER)) {
        // special case for SEQUENCER
        if (show) {
            if (noteOnCounter_[timbre] == 0) {
                displaySequencer_->noteOn(timbre, show);
            }
            noteOnCounter_[timbre] = 2;
        } else {
            displaySequencer_->noteOn(timbre, show);
        }
    }
}

void FMDisplay3::tempoClick() {
    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        if (noteOnCounter_[timbre] > 0) {
            noteOnCounter_[timbre]--;
            if (noteOnCounter_[timbre] == 0) {
                noteOn(timbre, false);
            }
        }
    }

    if (this->synthState_->fullState.synthMode == SYNTH_MODE_MIXER) {
        displayMixer_->tempoClick();
    } else if (this->synthState_->fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        displayEditor_->tempoClick();
    } else if (this->synthState_->fullState.synthMode == SYNTH_MODE_SEQUENCER) {
        displaySequencer_->tempoClick();
    }
}

void FMDisplay3::afterNewParamsLoad(int timbre) {
    displayEditor_->setPresetModified(timbre, false);

    if (currentTimbre_ == timbre && this->synthState_->fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        refreshStatus_ = 20;
    } else if (this->synthState_->fullState.synthMode == SYNTH_MODE_MIXER) {
        tft_->clearMixerLabels();
        refreshStatus_ = 19;
        endRefreshStatus_ = 14;
    }
}

void FMDisplay3::newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    displayMixer_->newMixerValue(valueType, timbre, oldValue, newValue, refreshStatus_);
}

void FMDisplay3::newPfm3Page(FullState *fullState) {
    if (unlikely(fullState->synthMode == SYNTH_MODE_REINIT_TFT)) {
        tft_->reset();
    } else {
        refreshStatus_ = 20;
        this->refreshOscilloBG();
    }
}

void FMDisplay3::refreshOscilloBG() {
    FullState *fullState = &this->synthState_->fullState;
    if (fullState->mainPage == 1) {
        if (fullState->editPage == 0) {
            displayEditor_->refreshOscillatorOperatorShape();
        } else {
            displayEditor_->refreshOscillatorOperatorEnvelope();
        }
    } else if (fullState->mainPage == 3) {
        if (fullState->editPage == 0) {
            displayEditor_->refreshLfoOscillator();
        } else if (fullState->editPage == 1) {
            displayEditor_->refreshLfoEnv();
        } else {
            tft_->oscilloBgActionClear();
        }
    } else {
        tft_->oscilloBgActionClear();
    }
}

// Overide SynthParamListener
void FMDisplay3::playNote(int timbre, char note, char velocity) {
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_YELLOW);
    tft_->setCursorInPixel(227, 22);
    tft_->print((char) 143);
}

void FMDisplay3::stopNote(int timbre, char note) {
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCursorInPixel(227, 22);
    tft_->print(' ');
}

void FMDisplay3::checkPresetModified(int timbre) {
    if (!displayEditor_->isPresetModifed(timbre)) {
        displayEditor_->setPresetModified(timbre, true);
        if (this->synthState_->fullState.synthMode == SYNTH_MODE_EDIT_PFM3 && timbre == currentTimbre_) {
            int length = getLength(this->synthState_->params->presetName);
            tft_->setCursor(3 + length, 0);
            tft_->setCharBackgroundColor(COLOR_DARK_BLUE);
            tft_->print('*');
        }
    }
}

