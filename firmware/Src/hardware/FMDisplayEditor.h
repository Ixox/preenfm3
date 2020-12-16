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

#ifndef FMDISPLAY_EDITOR
#define FMDISPLAY_EDITOR

#include "TftDisplay.h"
#include "SynthParamListener.h"
#include "FMDisplay.h"

class SynthState;
class TftDisplay;

struct Pfm3OneButtonState {
    const char *stateLabel;
    const RowEncoder rowEncoder[6];
};

struct Pfm3OneButton {
    const char *buttonLabel;
    const uint8_t buttonId;
    const int numberOfStates;
    const Pfm3OneButtonState *states[];

};

struct Pfm3EditMenu {
    const char *mainMenuName;
    const int numberOfButtons;
    const Pfm3OneButton *pages[];
};

struct PfmMainMenu {
    const Pfm3EditMenu *editMenu[6];
};

class FMDisplayEditor {
public:
    FMDisplayEditor();

    void init(SynthState *synthState, TftDisplay *tft) {
        synthState_ = synthState;
        tft_ = tft;
    }

    void refreshEditorByStep(int &refreshStatus, int &endRefreshStatus);
    void displayParamValue(int encoder, TFT_COLOR color);
    void newParamValue(int &refreshStatus, int timbre, int currentRow, int encoder, ParameterDisplay *param, float oldValue, float newValue);
    void tempoClick();
    bool isPresetModifed(int timbre) {
        return presetModifed_[timbre];
    }
    void setPresetModified(int timbre, bool state);
    void resetAllPresetModified();
    void updateEncoderValueWithoutCursor(int row, int encoder, ParameterDisplay *param,  float oldValue, float newFloatValue);

    void updateStepSequencer(int currentRow, int encoder, int oldValue, int newValue);
    void updateArpPattern(int currentRow, int encoder, int oldValue, int newValue);
    void displayPreset();

    void encoderTurnedPfm3(int encoder, int ticks);
    void encoderTurnedPfm2(int row, int encoder4, int ticks, bool specialOpCase = true);
    void encoderTurnedWhileButtonPressed(int encoder6, int ticks, int button);
    void buttonPressed(int button);
    void newTimbre(int timbre);

    void refreshOscillatorOperatorShape();
    void refreshOscillatorOperatorEnvelope();
    void refreshLfoOscillator();
    void refreshLfoEnv();

private:
    void checkOperatorNumber();
    bool isInOperatorPage();
    bool isInMatrixPage();
    void resetHideParams();
    uint8_t getX(int encoder);
    TftDisplay *tft_;
    SynthState *synthState_;

    bool presetModifed_[NUMBER_OF_TIMBRES];
    bool hideParam_[NUMBER_OF_ENCODERS];
    // Value Changed info
    uint8_t valueChangedCounter_[NUMBER_OF_ENCODERS];
    uint8_t imChangedCounter_[NUMBER_OF_ENCODERS];
    int currentTimbre_;
    bool multipleEdition_;

    // Remember curent editPage in mainPage
    uint8_t editPageSelected_[6];
    // Remember previous lfo freq value
    float lfoFreqPreviousValue[5];
};

#endif
