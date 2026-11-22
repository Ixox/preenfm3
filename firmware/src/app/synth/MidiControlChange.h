/*
 * Copyright 2026 Xavier Hosxe
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef MIDI_CONTROL_CHANGE_H_
#define MIDI_CONTROL_CHANGE_H_

/*
 * CCs handled by MidiDecoder or shared by all engines: notes, mixer, global
 * FX, Arp, sequencer and the FX1/FX2 pages.
 */
enum SharedControlChange {
    CC_BANK_SELECT = 0,
    CC_MODWHEEL = 1,
    CC_BREATH = 2,
    CC_GLIDE = 5,
    CC_MIXER_VOLUME = 7,
    CC_MIXER_PAN = 10,
    CC_MIXER_SEND = 11,
    CC_MIX5 = 31,
    CC_BANK_SELECT_LSB = 32,
    CC_OSC1_WARP = 33,
    CC_OSC2_WARP = 34,
    CC_OSC3_WARP = 35,
    CC_PAN5 = 36,
    CC_MIX6 = 37,
    CC_PAN6 = 39,
    CC_TIMBRE_DRYWET = 40,
    CC_TIMBRE_DYNAMICS = 41,
    CC_ENGINE_DECIMATION = 42,
    CC_PLAY_MODE = 43,
    CC_MFX_RETURN = 44,
    CC_MFX_PRESET = 45,
    CC_HOLD_PEDAL = 64,
    CC_FILTER2_TYPE = 66,
    CC_FILTER2_PARAM1 = 67,
    CC_FILTER2_PARAM2 = 68,
    CC_FILTER2_MIX = 69,
    CC_FILTER_TYPE = 70,
    CC_FILTER_PARAM1 = 71,
    CC_FILTER_PARAM2 = 72,
    CC_FILTER_GAIN = 73,
    CC_MPE_SLIDE_CC74 = 74,
    CC_ARP_CLOCK = 100,
    CC_ARP_DIRECTION = 101,
    CC_ARP_OCTAVE = 102,
    CC_ARP_PATTERN = 103,
    CC_ARP_DIVISION = 104,
    CC_ARP_DURATION = 105,
    CC_SEQ_START_ALL = 106,
    CC_SEQ_START_INST = 107,
    CC_SEQ_RECORD_INST = 108,
    CC_SEQ_SET_SEQUENCE = 109,
    CC_SEQ_TRANSPOSE = 110,
    CC_OSC4_WARP = 111,
    CC_OSC5_WARP = 112,
    CC_OSC6_WARP = 113,
    CC_CURRENT_INSTRUMENT = 119,
    CC_ALL_SOUND_OFF = 120,
    CC_ALL_NOTES_OFF = 123,
    CC_OMNI_OFF = 124,
    CC_OMNI_ON = 125,
    CC_RESET = 127
};

#endif
