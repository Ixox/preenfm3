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

#include "ScreenCoordinator.h"
#include "ScreenEditor.h"
#include "ScreenMenu.h"
#include "ScreenMixer.h"
#include "ScreenSequencer.h"
#include "ScreenTypes.h"
#include "SynthState.h"
#include "Timbre.h"

extern struct WaveTable waveTables[NUMBER_OF_WAVETABLES];

int getLength(const char* str) {
    int length = 0;
    for (const char* c = str; *c != '\0'; c++) {
        length++;
    }
    return length;
}

ScreenCoordinator::ScreenCoordinator() {
}

ScreenCoordinator::~ScreenCoordinator() {
}

void ScreenCoordinator::init(FirmwareTftDisplay* tft) {

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

void ScreenCoordinator::setDisplays(ScreenMixer* mixer, ScreenEditor* editor, ScreenMenu* menu, ScreenSequencer* sequencer) {
    this->mixer_ = mixer;
    this->editor_ = editor;
    this->menu_ = menu;
    this->sequencer_ = sequencer;
    this->sequencer_->setRefreshStatusPointer(&refreshStatus_, &endRefreshStatus_);
    this->mixer_->setRefreshStatusPointer(&refreshStatus_, &endRefreshStatus_);
}

void ScreenCoordinator::afterNewMixerLoad() {
    editor_->resetAllPresetModified();
    this->refreshOscilloBG();
}

void ScreenCoordinator::refreshAllScreenByStep() {
    switch (this->synthState_->fullState_.synthMode) {
    case SYNTH_MODE_MIXER:
        mixer_->refreshMixerByStep(currentTimbre_, refreshStatus_, endRefreshStatus_);
        break;
    case SYNTH_MODE_EDITOR:
        editor_->refreshEditorByStep(refreshStatus_, endRefreshStatus_);
        break;
    case SYNTH_MODE_MENU:
        menu_->refreshMenuByStep(currentTimbre_, refreshStatus_);
        break;
    case SYNTH_MODE_SEQUENCER:
        sequencer_->refreshSequencerByStep(currentTimbre_, refreshStatus_, endRefreshStatus_);
        break;
    }

    if (refreshStatus_ > 0) {
        refreshStatus_--;
    }
    if (refreshStatus_ == 0) {
        tft_->restartRefreshTft();
    }
}

void ScreenCoordinator::newPresetName(int timbre) {
    if (this->synthState_->getSynthMode() == SYNTH_MODE_MIXER) {
        mixer_->refreshInstrument(timbre);
    } else if (this->synthState_->getSynthMode() == SYNTH_MODE_SEQUENCER) {
        sequencer_->refreshInstrument(timbre);
    } else if (currentTimbre_ == timbre && this->synthState_->getSynthMode() == SYNTH_MODE_EDITOR) {
        editor_->displayPreset();
    }
}

void ScreenCoordinator::newTimbre(int timbre) {
    currentTimbre_ = timbre;
    editor_->newTimbre(timbre);

    if (this->synthState_->fullState_.synthMode == SYNTH_MODE_EDITOR) {
        tft_->clearActions();
        tft_->pauseRefresh();
        tft_->clear();
        this->refreshOscilloBG();
        editor_->drawAlgorithm();
        editor_->displayPreset();
        refreshStatus_ = 20;
        endRefreshStatus_ = 0;
    } else if (this->synthState_->fullState_.synthMode == SYNTH_MODE_MIXER) {
        refreshStatus_ = 19;
        endRefreshStatus_ = 14;
    } else if (this->synthState_->fullState_.synthMode == SYNTH_MODE_SEQUENCER) {
        sequencer_->newTimbre(timbre, refreshStatus_, endRefreshStatus_);
    }
}

void ScreenCoordinator::newMixerEdit(int oldButton, int newButton) {
    tft_->pauseRefresh();

    if (((oldButton == BUTTONID_MIXER_GLOBAL || newButton == BUTTONID_MIXER_GLOBAL) && (oldButton != newButton)) ||
        ((oldButton == BUTTONID_MIXER_SCALA || newButton == BUTTONID_MIXER_SCALA))) {
        tft_->clearMixerLabels();
        refreshStatus_ = 19;
    } else {
        refreshStatus_ = 12;
    }

    tft_->clearMixerValues();
}

void ScreenCoordinator::newEngineParamValue(int timbre, int paramId, int context, float oldValue, float newValue) {
    checkPresetModified(timbre);
    if (this->synthState_->getSynthMode() == SYNTH_MODE_EDITOR) {
        editor_->newEngineParamValue(refreshStatus_, timbre, paramId, context, oldValue, newValue);
    }
}

void ScreenCoordinator::newEngineParamValueFromExternal(int timbre, int paramId, int context, float oldValue, float newValue) {
    checkPresetModified(timbre);

    if (this->synthState_->getSynthMode() == SYNTH_MODE_EDITOR && this->synthState_->fullState_.mainPage != -1 &&
        timbre == currentTimbre_) {
        editor_->newEngineParamValue(refreshStatus_, timbre, paramId, context, oldValue, newValue);
    }
}

void ScreenCoordinator::newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
    // Filter MIXER mode here and filter page in displayMixer
    if (this->synthState_->getSynthMode() == SYNTH_MODE_MIXER) {
        mixer_->newMixerValueFromExternal(mixerValueType, timbre, oldValue, newValue);
    }
}

/*
 * Menu Listener
 */

void ScreenCoordinator::newSynthMode(FullState* fullState) {

    endRefreshStatus_ = 0;

    switch (fullState->synthMode) {
    case SYNTH_MODE_MIXER:
        refreshStatus_ = 21;
        break;
    case SYNTH_MODE_EDITOR:
        refreshStatus_ = 21;
        break;
    case SYNTH_MODE_MENU:
        menu_->setPreviousSynthMode(fullState->synthModeBeforeMenu);
        refreshStatus_ = 21;
        break;
    case SYNTH_MODE_SEQUENCER:
        refreshStatus_ = 21;
        sequencer_->cleanCurrentState();
        break;
    }
}

int ScreenCoordinator::rowInc(MenuState menuState) {
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

void ScreenCoordinator::newMenuState(FullState* fullState) {
    menu_->newMenuState(fullState);
    refreshStatus_ = 6;
}

void ScreenCoordinator::newMenuSelect(FullState* fullState) {
    menu_->newMenuSelect(fullState);
}

void ScreenCoordinator::menuBack(const MenuItem* oldMenuItem, FullState* fullState) {
    menu_->menuBack(oldMenuItem, fullState);
    refreshStatus_ = 6;
}

// VisualInfo

void ScreenCoordinator::midiClock(bool show) {
    int x = 240 - 4 * TFT_SMALL_CHAR_WIDTH;
    int y = 1;
    tft_->setCursorInPixel(x, y);
    switch (this->synthState_->fullState_.synthMode) {
    case SYNTH_MODE_MIXER:
        tft_->setCharBackgroundColor(COLOR_DARK_GREEN);
        break;
    case SYNTH_MODE_MENU:
        tft_->setCharBackgroundColor(COLOR_DARK_RED);
        break;
    case SYNTH_MODE_EDITOR:
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

void ScreenCoordinator::noteOn(int timbre, bool show) {
    int x = 240 - (3 - (timbre % 3)) * TFT_SMALL_CHAR_WIDTH;
    int y = 1 + (timbre / 3) * TFT_SMALL_CHAR_HEIGHT;
    tft_->setCursorInPixel(x, y);
    switch (this->synthState_->fullState_.synthMode) {
    case SYNTH_MODE_MIXER:
        tft_->setCharBackgroundColor(COLOR_DARK_GREEN);
        break;
    case SYNTH_MODE_MENU:
        tft_->setCharBackgroundColor(COLOR_DARK_RED);
        break;
    case SYNTH_MODE_EDITOR:
        tft_->setCharBackgroundColor(COLOR_DARK_BLUE);
        break;
    case SYNTH_MODE_SEQUENCER:
        tft_->setCharBackgroundColor(COLOR_DARK_YELLOW);
        break;
    }
    tft_->setCharColor(COLOR_WHITE);

    if (show) {
        if (noteOnCounter_[timbre] == 0) {
            tft_->printSmallChar((char)('0' + timbre + 1));
        }
        noteOnCounter_[timbre] = 2;
    } else {
        tft_->printSmallChar(' ');
    }
    if (likely(this->synthState_->fullState_.synthMode == SYNTH_MODE_SEQUENCER)) {
        // special case for SEQUENCER
        if (show) {
            if (noteOnCounter_[timbre] == 0) {
                sequencer_->noteOn(timbre, show);
            }
            noteOnCounter_[timbre] = 2;
        } else {
            sequencer_->noteOn(timbre, show);
        }
    }
}

void ScreenCoordinator::slowTick() {
    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        if (noteOnCounter_[timbre] > 0) {
            noteOnCounter_[timbre]--;
            if (noteOnCounter_[timbre] == 0) {
                noteOn(timbre, false);
            }
        }
    }

    if (this->synthState_->fullState_.synthMode == SYNTH_MODE_MIXER) {
        mixer_->slowTick();
    } else if (this->synthState_->fullState_.synthMode == SYNTH_MODE_EDITOR) {
        editor_->slowTick();
    } else if (this->synthState_->fullState_.synthMode == SYNTH_MODE_SEQUENCER) {
        sequencer_->slowTick();
    }
}

void ScreenCoordinator::afterNewParamsLoad(int timbre) {
    editor_->setPresetModified(timbre, false);

    if (currentTimbre_ == timbre && this->synthState_->fullState_.synthMode == SYNTH_MODE_EDITOR) {
        refreshStatus_ = 20;
    } else if (this->synthState_->fullState_.synthMode == SYNTH_MODE_MIXER) {
        tft_->clearMixerLabels();
        refreshStatus_ = 19;
        endRefreshStatus_ = 14;
    }
}

void ScreenCoordinator::newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    mixer_->newMixerValue(valueType, timbre, oldValue, newValue, refreshStatus_);
}

void ScreenCoordinator::newScreenPage(FullState* fullState) {
    if (unlikely(fullState->synthMode == SYNTH_MODE_REINIT_TFT)) {
        tft_->reset();
    } else {
        refreshStatus_ = 20;
        this->refreshOscilloBG();
    }
}

void ScreenCoordinator::refreshOscilloBG() {
    editor_->refreshOscilloBG();
}

// Overide SynthParamListener
void ScreenCoordinator::playNote(int timbre, char note, char velocity) {
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCharColor(COLOR_YELLOW);
    tft_->setCursorInPixel(227, 22);
    tft_->print((char)143);
}

void ScreenCoordinator::stopNote(int timbre, char note) {
    tft_->setCharBackgroundColor(COLOR_BLACK);
    tft_->setCursorInPixel(227, 22);
    tft_->print(' ');
}

void ScreenCoordinator::checkPresetModified(int timbre) {
    if (!editor_->isPresetModifed(timbre)) {
        editor_->setPresetModified(timbre, true);
        if (this->synthState_->fullState_.synthMode == SYNTH_MODE_EDITOR && timbre == currentTimbre_) {
            int length = getLength(synthState_->getTimbre(currentTimbre_)->getPresetName());
            tft_->setCursor(3 + length, 0);
            tft_->setCharBackgroundColor(COLOR_DARK_BLUE);
            tft_->print('*');
        }
    }
}
