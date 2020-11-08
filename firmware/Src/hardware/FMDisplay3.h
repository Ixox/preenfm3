/*
 * Copyright 2019 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
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

#ifndef FMDISPLAY3_H_
#define FMDISPLAY3_H_

#include "SynthStateAware.h"
#include "TftDisplay.h"
#include "Menu.h"
#include "VisualInfo.h"
#include "Storage.h"


class FMDisplayEditor;
class FMDisplayMixer;
class FMDisplayMenu;
class FMDisplaySequencer;


class FMDisplay3: public SynthParamListener, public SynthMenuListener, public SynthStateAware, public VisualInfo {
public:
    FMDisplay3();
    ~FMDisplay3();
    void init(TftDisplay* tft);
    void setDisplays(FMDisplayMixer* displayMixer, FMDisplayEditor* dDisplayEditor, FMDisplayMenu* displayMenu, FMDisplaySequencer* displaySequencer);
    void customCharsInit();

    bool shouldThisValueShowUpPfm3(int row, int encoder, int encoder6);
    void updateEncoderValue(int refreshStatus);
    void updateCCValue(int refreshStatus);
    void newTimbre(int timbre);

    // VisualInfo
    void midiClock(bool show);
    void noteOn(int timbre, bool show);
    void tempoClick();

    int getRowNumberToDiplay(int row) {
        if (row <= ROW_ENGINE_LAST) {
            return row - ROW_ENGINE_FIRST + 1;
        } else if (row <= ROW_OSC_LAST) {
            return row - ROW_OSC_FIRST + 1;
        } else if (row <= ROW_ENV_LAST) {
            return ((row - ROW_ENV_FIRST) >> 1) + 1;
        } else if (row <= ROW_MATRIX_LAST) {
            return row - ROW_MATRIX_FIRST + 1;
        } else if (row <= ROW_LFOOSC3) {
            return row - ROW_LFO_FIRST + 1;
        } else if (row <= ROW_LFOENV2) {
            return row - ROW_LFOENV1 + 1;
        } else if (row <= ROW_LFOSEQ2) {
            return row - ROW_LFOSEQ1 + 1;
        }
        return 0;
    }

    bool needRefresh() {
        return refreshStatus != 0;
    }

    void refreshAllScreenByStep();
    void displayInstrumentNumber();

    void checkPresetModified(int timbre);

    int rowInc(MenuState menuState);
    void newSynthMode(FullState* fullState);
    void newMenuState(FullState* fullState);
    void newMenuSelect(FullState* fullState);
    void newPfm3Page(FullState* fullState);
    void menuBack(const MenuItem* oldMenuItem, FullState* fullState);
    void refreshOscilloBG();


    void newParamValueFromExternal(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue);
    void newMixerValueFromExternal(int timbre, int mixerValueType, float oldValue, float newValue);
    void newParamValue(int timbre, int currentRow, int encoder, ParameterDisplay* param, float oldValue, float newValue);
    void newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue);
    void newMixerEdit(int oldButton, int newButton);

    void newPresetName(int timbre);

    void beforeNewParamsLoad(int timbre) {
    }

    void afterNewParamsLoad(int timbre);

    void afterNewMixerLoad();

    // Overide SynthParamListener
    void playNote(int timbre, char note, char velocity);
    void stopNote(int timbre, char note);

    void setRefreshStatus(int refreshStatus) {
        this->refreshStatus = refreshStatus;
    }

private:
    TftDisplay* tft;
    int refreshStatus;
    int endRefreshStatus;

    int menuRow;
    // Local value preset modified to know whether it's currently showing up
    int currentTimbre;
    // Midi info
    int noteOnCounter[NUMBER_OF_TIMBRES];



    // One Display class per main page
    FMDisplayMixer* displayMixer;
    FMDisplayMenu* displayMenu;
    FMDisplayEditor* displayEditor;
    FMDisplaySequencer* displaySequencer;
};

#endif /* FMDISPLAY3_H_ */
