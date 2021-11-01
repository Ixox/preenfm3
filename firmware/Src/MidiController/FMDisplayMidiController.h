/*
 * Copyright 2021 Xavier Hosxe
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

#ifndef FMDISPLAY_MIDI_CONTROLLER
#define FMDISPLAY_MIDI_CONTROLLER

#include "Common.h"
#include "EncodersListener.h"

class MidiControllerState;
class TftDisplay;
class MidiControllerFile;

enum {
    DISPLAY_MIDI_CONTROLLER_MAIN = 0,
    DISPLAY_MIDI_CONTROLLER_EDIT_ENCODER,
    DISPLAY_MIDI_CONTROLLER_EDIT_BUTTON,
    DISPLAY_MIDI_CONTROLLER_SAVE
};


enum EncoderParamType {
    ENCODER_PARAM_NAME,
    ENCODER_PARAM_MIDI_CHANNEL,
    ENCODER_PARAM_CONTROLLER,
    ENCODER_PARAM_MIN,
    ENCODER_PARAM_MAX
};

enum ButtonParamType {
    BUTTON_PARAM_NAME,
    BUTTON_PARAM_MIDI_CHANNEL,
    BUTTON_PARAM_CONTROLLER,
    BUTTON_PARAM_BUTTON_TYPE,
    BUTTON_PARAM_HIGH
};

class FMDisplayMidiController : public EncodersListener {
public:
    FMDisplayMidiController();

    void init(TftDisplay *tft, MidiControllerState *midiControllerState, MidiControllerFile *midiControllerFile);
    void refreshAllScreenByStep();
    void refreshAllScreenByStepEditEncoder();
    void refreshAllScreenByStepEditButton();
    void refreshAllScreenByStepSave();
    void tempoClick();

    void newMidiControllerValue(uint8_t valueType, uint8_t timbre, float oldValue, float newValue, int &refreshStatus);
    void newMidiControllerValueFromExternal(uint8_t valueType, uint8_t timbre, float oldValue, float newValue);

    bool needRefresh() {
        return refreshStatus_ != 0;
    }

    void setRefreshStatus(int newRefreshStatus);
    void setResetRefreshStatus() { refreshStatus_= 21; }
    void displayEncoderValue(int encoder);
    void displayButtonValue(int button);

    void encoderTurned(int encoder, int ticks);
    void encoderTurnedWhileButtonPressed(int encoder, int ticks, int button);
    void buttonPressed(int button);
    void buttonLongPressed(int button);
    void twoButtonsPressed(int button1, int button2);
    void buttonUp(int button);
    void buttonDown(int button);

    void displayEncoderParam(int encoder, EncoderParamType paramType);
    void displayButtonParam(int button, ButtonParamType paramType);
    void displayMidiChannel();

private:
    TftDisplay *tft_;
    MidiControllerState *midiControllerState_;
    MidiControllerFile *midiControllerFile_;

    int refreshStatus_;
    uint8_t displayMode_;
    uint8_t editControl_;
    uint8_t editLetterPosition_;
    bool menuPressed_;
    uint8_t midiChannel_;

private:
};

#endif
