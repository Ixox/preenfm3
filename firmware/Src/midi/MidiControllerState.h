/*
 * MidiControllerState.h
 *
 *  Created on: 31 oct. 2021
 *      Author: Xavier
 */

#ifndef MIDI_MIDICONTROLLERSTATE_H_
#define MIDI_MIDICONTROLLERSTATE_H_

#include "Common.h"


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
    uint16_t controller;
    uint16_t value;
    uint16_t valueLow;
    uint16_t valueHigh;
};

class FMDisplayMidiController;
class MidiControllerFile;

class MidiControllerState  {
    friend FMDisplayMidiController;
    friend MidiControllerFile;
public:
    MidiControllerState();
    virtual ~MidiControllerState();

    void encoderDelta(uint8_t globalMidiChannel, uint32_t encoder, int delta);
    void buttonDown(uint8_t globalMidiChannel, uint32_t button);
    bool buttonUp(uint8_t globalMidiChannel, uint32_t button);

private:
    void sendMidiDin5Out();
    void strcpy(char* src, char *dest, int len);

    MidiEncoder encoder_[6];
    MidiButton button_[6];
};

#endif /* MIDI_MIDICONTROLLERSTATE_H_ */
