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

#ifndef MIDI_MIDICONTROLLERSTATE_H_
#define MIDI_MIDICONTROLLERSTATE_H_

#include "Common.h"

#define MIDI_NUMBER_OF_PAGES 5

enum {
    MIDI_CONTROLLER_VERSION_1 = 1
};

#define MIDI_CONTROLLER_CURRENT_VERSION MIDI_CONTROLLER_VERSION_1

enum MidiEncoderType {
    MIDI_ENCODER_TYPE_CC,
    MIDI_ENCODER_TYPE_NRPN
};

enum MidiButtonType {
    MIDI_BUTTON_TYPE_PUSH,
    MIDI_BUTTON_TYPE_TOGGLE
};


struct MidiEncoder {
    char name[6];
    MidiEncoderType encoderType;
    uint8_t midiChannel;
    uint16_t controller;
    uint16_t value;
    uint16_t maxValue;
    uint16_t minValue;
};

struct MidiButton {
    char name[6];
    MidiButtonType buttonType;
    uint8_t midiChannel;
    uint8_t value; // 0 : of, 1 : on
    uint16_t controller;
    uint16_t valueOff;
    uint16_t valueOn;
    // Utility
    uint16_t getValue() { return value == 0 ? valueOff : valueOn; }
};

struct MidiPage {
    MidiEncoder encoder_[6];
    MidiButton button_[6];
};

class FMDisplayMidiController;
class MidiControllerFile;

class MidiControllerState  {
public:
    MidiControllerState();
    virtual ~MidiControllerState();

    void encoderDelta(uint8_t pageNumber, uint8_t globalMidiChannel, uint32_t encoderNumber, int delta);
    void buttonDown(uint8_t pageNumber, uint8_t globalMidiChannel, uint32_t buttonNumber);
    bool buttonUp(uint8_t pageNumber, uint8_t globalMidiChannel, uint32_t buttonNumber);

    MidiPage* getMidiPage(int pageNumber) { return &midiPage_[pageNumber]; }
    MidiEncoder* getEncoder(int pageNumber, int encoderNumber) { return &midiPage_[pageNumber].encoder_[encoderNumber]; }
    MidiButton* getButton(int pageNumber, int buttonNumber) { return &midiPage_[pageNumber].button_[buttonNumber]; }

private:
    void sendMidiDin5Out();
    void strcpy(char* dest, const char *src, int len);
    MidiPage midiPage_[MIDI_NUMBER_OF_PAGES];
};

#endif /* MIDI_MIDICONTROLLERSTATE_H_ */
