/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MIDI_DATA_H_
#define MIDI_DATA_H_

#include <stdint.h>

enum MidiDataType {
    MIDI_DATA_CC,
    MIDI_DATA_USER_CC,
    MIDI_DATA_NRPN,
    MIDI_DATA_AFTERTOUCH,
    MIDI_DATA_PITCHBEND,
    MIDI_DATA_MPE_AFTERTOUCH,
    MIDI_DATA_MPE_PITCHBEND,
    MIDI_DATA_MPE_SLIDE,
};

struct MidiData {
    MidiDataType type;
    uint8_t channel;
    uint8_t cc;
    uint8_t ccValue;
    uint16_t nrpnParam;
    // NRPN 14-bit value, or raw aftertouch/pitch bend/MPE value.
    uint16_t nrpnValue;
};

#endif
