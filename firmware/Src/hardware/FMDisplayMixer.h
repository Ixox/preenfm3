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

#ifndef FMDISPLAY_MIXER
#define FMDISPLAY_MIXER

#include "Common.h"
#include "FMDisplay.h"

class SynthState;
class TftDisplay;


// Mixer struct

struct Pfm3MixerButtonStateParam {
    float minValue;
    float maxValue;
    float numberOfValues;
    ParameterDisplayType displayType;
    const char **valueName;
};

struct Pfm3MixerButtonState {
    const char *stateLabel;
    enum MixerValueType mixerValueType;
    struct Pfm3MixerButtonStateParam buttonState;
};

struct Pfm3MixerButton {
    const char *buttonLabel;
    int numberOfStates;
    const Pfm3MixerButtonState *state[];
};

struct PfmMixerMenu {
    const Pfm3MixerButton *mixerButton[6];
};

class FMDisplayMixer {
public:
    FMDisplayMixer();

    void init(SynthState *synthState, TftDisplay *tft) {
        synthState_ = synthState;
        tft_ = tft;
    }

    void displayMixerValue(int timbre);
    void refreshMixerByStep(int currentTimbre, int &refreshStatus, int &endRefreshStatus);
    void tempoClick();
    void newMixerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue);
    void newMixerValueFromExternal(uint8_t valueType, uint8_t timbre, float oldValue, float newValue);
    void encoderTurned(int encoder, int ticks);
    void buttonPressed(int button);
    void* getValuePointer(int valueType, int encoder);

private:
    void refreshMixerRowGlobalOptions(int row);
    void displayMixerValueInteger(int timbre, int x, int value);
    TftDisplay *tft_;
    SynthState *synthState_;
    uint8_t valueChangedCounter_[NUMBER_OF_ENCODERS_PFM3];
};

#endif
