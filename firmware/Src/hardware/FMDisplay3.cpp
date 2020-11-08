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

extern struct WaveTable waveTables[NUMBER_OF_WAVETABLES];

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

void FMDisplay3::init(TftDisplay* tft) {

    for (int w = 0; w < NUMBER_OF_WAVETABLES; w ++) {
        tft->initWaveFormExt(w, waveTables[w].table, waveTables[w].max);
    }

    refreshStatus = 0;
    endRefreshStatus = 0;
    this->tft = tft;

    for (int k = 0; k < NUMBER_OF_TIMBRES; k++) {
        noteOnCounter[k] = 0;
    }
    currentTimbre = 0;

    tft->clear();
}

void FMDisplay3::setDisplays(FMDisplayMixer* displayMixer, FMDisplayEditor* displayEditor, FMDisplayMenu* displayMenu, FMDisplaySequencer* displaySequencer) {
    this->displayMixer = displayMixer;
    this->displayEditor = displayEditor;
    this->displayMenu = displayMenu;
    this->displaySequencer = displaySequencer;
    this->displaySequencer->setRefreshStatusPointer(&refreshStatus, &endRefreshStatus);
}


bool FMDisplay3::shouldThisValueShowUpPfm3(int row, int encoder, int encoder6) {
    int algo = this->synthState->params->engine1.algo;
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
        int effect = this->synthState->params->effect.type;
        if (filterRowDisplay[effect].paramName[encoder - 1] == NULL) {
            return false;
        }
    }
        break;
    case ROW_ARPEGGIATOR1:
        if (encoder >= 1 && this->synthState->params->engineArp1.clock == 0.0f) {
            return false;
        }
        break;
    default:
        break;
    }

    return true;
}

void FMDisplay3::afterNewMixerLoad() {
    displayEditor->resetAllPresetModified();
}



void FMDisplay3::refreshAllScreenByStep() {
    switch (this->synthState->fullState.synthMode) {
    case SYNTH_MODE_MIXER:
        displayMixer->refreshMixerByStep(currentTimbre, refreshStatus, endRefreshStatus);
        break;
    case SYNTH_MODE_EDIT_PFM3:
        displayEditor->refreshEditorByStep(refreshStatus, endRefreshStatus);
        break;
    case SYNTH_MODE_MENU:
        displayMenu->refreshMenuByStep(currentTimbre, refreshStatus, menuRow);
        break;
    case SYNTH_MODE_SEQUENCER:
        displaySequencer->refreshSequencerByStep(currentTimbre, refreshStatus, endRefreshStatus);
        break;
    }


    if (refreshStatus > 0) {
        refreshStatus--;
    }
    if (refreshStatus == 0) {
        tft->restartRefreshTft();
    }
}

void FMDisplay3::newPresetName(int timbre) {
    if (currentTimbre == timbre && this->synthState->getSynthMode() == SYNTH_MODE_EDIT_PFM3) {
        int l = getLength(this->synthState->params->presetName);

        tft->setCursor(3, 0);
        tft->print(this->synthState->params->presetName);
        if (displayEditor->isPresetModifed(timbre)) {
            tft->print('*');
            l++;
        }
        for (int k = l; k < 13; k++) {
            tft->print(' ');
        }
    }
}
;


void FMDisplay3::newTimbre(int timbre) {
    currentTimbre = timbre;
    displayEditor->newTimbre(timbre);

    if (this->synthState->fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        tft->clearActions();
        tft->pauseRefresh();
        tft->clear();
        tft->drawAlgo(this->synthState->params->engine1.algo);
        displayEditor->displayPreset();
        refreshStatus = 20;
        endRefreshStatus = 0;

        this->refreshOscilloBG();
    } else if (this->synthState->fullState.synthMode == SYNTH_MODE_MIXER) {
        refreshStatus = 19;
        endRefreshStatus = 14;
    } else if (this->synthState->fullState.synthMode == SYNTH_MODE_SEQUENCER) {
        displaySequencer->newTimbre(timbre, refreshStatus, endRefreshStatus);
    }
}




void FMDisplay3::newMixerEdit(int oldButton, int newButton) {
    tft->pauseRefresh();

    if (((oldButton == BUTTONID_MIXER_GLOBAL || newButton == BUTTONID_MIXER_GLOBAL) && (oldButton != newButton))
        || ((oldButton == BUTTONID_MIXER_SCALA || newButton == BUTTONID_MIXER_SCALA))) {
        tft->clearMixerLabels();
        refreshStatus = 19;
    } else {
        refreshStatus = 12;
    }

    tft->clearMixerValues();
}



void FMDisplay3::newParamValue(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue) {
    checkPresetModified(timbre);

    if (this->synthState->getSynthMode() == SYNTH_MODE_EDIT_PFM3) {
        displayEditor->newParamValue(refreshStatus, timbre, currentRow, encoder, param, oldValue, newValue);
    }
}

void FMDisplay3::newParamValueFromExternal(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue,
        float newValue) {
    checkPresetModified(timbre);

    // If Operator we must send
    if (this->synthState->getSynthMode() == SYNTH_MODE_EDIT_PFM3 && this->synthState->fullState.mainPage != -1 && timbre == currentTimbre ) {

        displayEditor->newParamValue(refreshStatus, timbre, currentRow, encoder, param, oldValue, newValue);
    }
}

void FMDisplay3::newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue) {
    // Filter MIXER mode here and filter page in displayMixer
    if (this->synthState->getSynthMode() == SYNTH_MODE_MIXER) {
        displayMixer->newMixerValueFromExternal(mixerValueType, timbre, oldValue, newValue);
    }
}

/*
 * Menu Listener
 */

void FMDisplay3::newSynthMode(FullState* fullState) {

    switch (fullState->synthMode) {
    case SYNTH_MODE_MIXER:
        refreshStatus = 21;
        break;
    case SYNTH_MODE_EDIT_PFM3:
        refreshStatus = 21;
        break;
    case SYNTH_MODE_MENU:
        displayMenu->setPreviousSynthMode(fullState->synthModeBeforeMenu);
        refreshStatus = 21;
        break;
    case SYNTH_MODE_SEQUENCER:
        refreshStatus = 21;
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

void FMDisplay3::newMenuState(FullState* fullState) {
    displayMenu->newMenuState(fullState);
    refreshStatus = 6;
}



void FMDisplay3::newMenuSelect(FullState* fullState) {
    displayMenu->newMenuSelect(fullState);
}

void FMDisplay3::menuBack(const MenuItem* oldMenuItem, FullState* fullState) {
    displayMenu->menuBack(oldMenuItem, fullState);
    refreshStatus = 6;
}


// VisualInfo

void FMDisplay3::midiClock(bool show) {
    int x = 240 - 4 * TFT_SMALL_CHAR_WIDTH;
    int y = 1;
    tft->setCursorInPixel(x, y);
    switch (this->synthState->fullState.synthMode) {
    case SYNTH_MODE_MIXER:
        tft->setCharBackgroundColor(COLOR_DARK_GREEN);
        break;
    case SYNTH_MODE_MENU:
        tft->setCharBackgroundColor(COLOR_DARK_RED);
        break;
    case SYNTH_MODE_EDIT_PFM3:
        tft->setCharBackgroundColor(COLOR_DARK_BLUE);
        break;
    case SYNTH_MODE_SEQUENCER:
        tft->setCharBackgroundColor(COLOR_DARK_YELLOW);
        break;
    }
    tft->setCharColor(COLOR_WHITE);
    if (show) {
        // The led in preenfm3 shows up when the sequencer is activated
        // HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_SET);
        tft->printSmallChar('*');
    } else {
        // HAL_GPIO_WritePin(LED_CONTROL_GPIO_Port, LED_CONTROL_Pin, GPIO_PIN_RESET);
        tft->printSmallChar(' ');
    }
    tft->setCharBackgroundColor(COLOR_BLACK);
}

void FMDisplay3::noteOn(int timbre, bool show) {
    if (likely(this->synthState->fullState.synthMode != SYNTH_MODE_SEQUENCER)) {
        int x = 240 - (3 - (timbre % 3)) * TFT_SMALL_CHAR_WIDTH;
        int y = 1 + (timbre / 3) * TFT_SMALL_CHAR_HEIGHT;
        tft->setCursorInPixel(x, y);
        switch (this->synthState->fullState.synthMode) {
        case SYNTH_MODE_MIXER:
            tft->setCharBackgroundColor(COLOR_DARK_GREEN);
            break;
        case SYNTH_MODE_MENU:
            tft->setCharBackgroundColor(COLOR_DARK_RED);
            break;
        case SYNTH_MODE_EDIT_PFM3:
            tft->setCharBackgroundColor(COLOR_DARK_BLUE);
            break;
        }
        tft->setCharColor(COLOR_WHITE);

        if (show) {
            if (noteOnCounter[timbre] == 0) {
                tft->printSmallChar((char) ('0' + timbre + 1));
            }
            noteOnCounter[timbre] = 2;
        } else {
            tft->printSmallChar(' ');
        }
    } else {
        // special case for SEQUENCER
        if (show) {
            if (noteOnCounter[timbre] == 0) {
                displaySequencer->noteOn(timbre, show);
            }
            noteOnCounter[timbre] = 2;
        } else {
            displaySequencer->noteOn(timbre, show);
        }
    }
}

void FMDisplay3::tempoClick() {
    for (int timbre = 0; timbre < NUMBER_OF_TIMBRES; timbre++) {
        if (noteOnCounter[timbre] > 0) {
            noteOnCounter[timbre]--;
            if (noteOnCounter[timbre] == 0) {
                noteOn(timbre, false);
            }
        }
    }

    if (this->synthState->fullState.synthMode == SYNTH_MODE_MIXER) {
        displayMixer->tempoClick();
    } else if (this->synthState->fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        displayEditor->tempoClick();
    }
}

void FMDisplay3::afterNewParamsLoad(int timbre) {
    displayEditor->setPresetModified(timbre, false);

    if (currentTimbre == timbre && this->synthState->fullState.synthMode == SYNTH_MODE_EDIT_PFM3) {
        tft->clearActions();
        tft->clear();
        displayEditor->displayPreset();
        refreshStatus = 12;
    }
}

void FMDisplay3::newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue) {
    displayMixer->newMixerValue(valueType, timbre, oldValue, newValue);
}


void FMDisplay3::newPfm3Page(FullState* fullState) {
	if (unlikely(fullState->synthMode == SYNTH_MODE_COLORS)) {
		tft->clear();
		float h = 320.0f / (NUMBER_OF_TFT_COLORS);
		for (int c = 0; c < NUMBER_OF_TFT_COLORS; c++) {
			int y = h * c;
			tft->fillArea(0, y, 240, h, c + 1);
		}
	} else {
		refreshStatus = 20;
		this->refreshOscilloBG();
	}
}


void FMDisplay3::refreshOscilloBG() {
    FullState* fullState = &this->synthState->fullState;
    if (fullState->mainPage == 1) {
        if (fullState->editPage == 0) {
            displayEditor->refreshOscillatorOperatorShape();
        } else {
            displayEditor->refreshOscillatorOperatorEnvelope();
        }
    } else if (fullState->mainPage == 3) {
        if (fullState->editPage == 0) {
            displayEditor->refreshLfoOscillator();
        } else if (fullState->editPage == 1) {
            displayEditor->refreshLfoEnv();
        } else {
            tft->oscilloBgActionClear();
        }
    } else {
        tft->oscilloBgActionClear();
    }
}

// Overide SynthParamListener
void FMDisplay3::playNote(int timbre, char note, char velocity) {
    tft->setCharBackgroundColor(COLOR_BLACK);
    tft->setCharColor(COLOR_YELLOW);
    tft->setCursorInPixel(227, 22);
    tft->print((char)143);
}

void FMDisplay3::stopNote(int timbre, char note) {
    tft->setCharBackgroundColor(COLOR_BLACK);
    tft->setCursorInPixel(227, 22);
    tft->print(' ');
}

void FMDisplay3::checkPresetModified(int timbre) {
    if (!displayEditor->isPresetModifed(timbre)) {
        displayEditor->setPresetModified(timbre, true);
        if (this->synthState->fullState.synthMode == SYNTH_MODE_EDIT_PFM3 && timbre == currentTimbre) {
            int length = getLength(this->synthState->params->presetName);
            tft->setCursor(3 + length, 0);
            tft->setCharBackgroundColor(COLOR_DARK_BLUE);
            tft->print('*');
        }
    }
}

