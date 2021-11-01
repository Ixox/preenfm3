/*
 * Copyright 2021 Xavier Hosxe
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


#include "MidiControllerFile.h"

MidiControllerFile::MidiControllerFile() {
}

MidiControllerFile::~MidiControllerFile() {
}

void MidiControllerFile::loadConfig(MidiControllerState* midiControllerState) {
    char* reachableProperties = storageBuffer;

    int size = checkSize(MIDI_CONTROLLER_STATE);
    if (size >= PROPERTY_FILE_SIZE || size == -1) {
        // ERROR
        return;
    }
    reachableProperties[size] = 0;

    load(MIDI_CONTROLLER_STATE, 0,  reachableProperties, size);
    // First int is the version
    uint32_t* p = (uint32_t*)reachableProperties;
    int version = (int)*(p++);

    switch (version) {
    case MIDI_CONTROLLER_VERSION_1: {
        for (int e = 0; e < 6; e++) {
            char *nameP = (char*)p;
            for (int c = 0; c < 6 ; c++) {
                midiControllerState->encoder_[e].name[c] = *(nameP++);
            }
            // Let skip 8
            p++;
            p++;
            midiControllerState->encoder_[e].encoderType = (MidiEncoderType)*(p++);
            midiControllerState->encoder_[e].midiChannel = *(p++);
            midiControllerState->encoder_[e].controller = *(p++);
            midiControllerState->encoder_[e].value = *(p++);
            midiControllerState->encoder_[e].maxValue = *(p++);
            midiControllerState->encoder_[e].minValue = *(p++);
        }
        for (int b = 0; b < 6; b++) {
            char *nameP = (char*)p;
            for (int c = 0; c < 6 ; c++) {
                midiControllerState->button_[b].name[c] = *(nameP++);
            }
            // Let skip 8
            p++;
            p++;
            midiControllerState->button_[b].buttonType = (MidiButtonType)*(p++);
            midiControllerState->button_[b].midiChannel = *(p++);
            midiControllerState->button_[b].controller = *(p++);
            midiControllerState->button_[b].value = *(p++);
            midiControllerState->button_[b].valueLow = *(p++);
            midiControllerState->button_[b].valueHigh = *(p++);
        }
        break;
    }
    }
}


void MidiControllerFile::saveConfig(MidiControllerState* midiControllerState) {
    char* reachableProperties = storageBuffer;

    for (int i = 0; i < PROPERTY_FILE_SIZE; i++) {
        reachableProperties[i] = 0;
    }

    uint32_t* p = (uint32_t*)reachableProperties;
    *(p++) = MIDI_CONTROLLER_CURRENT_VERSION;


    for (int e = 0; e < 6; e++) {
        char *nameP = (char*)p;
        for (int c = 0; c < 6 ; c++) {
            *(nameP++) = midiControllerState->encoder_[e].name[c];
        }
        // Let skip 8
        p++;
        p++;
        *(p++) = midiControllerState->encoder_[e].encoderType;
        *(p++) = midiControllerState->encoder_[e].midiChannel;
        *(p++) = midiControllerState->encoder_[e].controller;
        *(p++) = midiControllerState->encoder_[e].value;
        *(p++) = midiControllerState->encoder_[e].maxValue;
        *(p++) = midiControllerState->encoder_[e].minValue;
    }
    for (int b = 0; b < 6; b++) {
        char *nameP = (char*)p;
        for (int c = 0; c < 6 ; c++) {
            *(nameP++) = midiControllerState->button_[b].name[c];
        }
        // Let skip 8
        p++;
        p++;
        *(p++) = midiControllerState->button_[b].buttonType;
        *(p++) = midiControllerState->button_[b].midiChannel;
        *(p++) = midiControllerState->button_[b].controller;
        *(p++) = midiControllerState->button_[b].value;
        *(p++) = midiControllerState->button_[b].valueLow;
        *(p++) = midiControllerState->button_[b].valueHigh;
    }

    int size = ((uint32_t)p) -  ((uint32_t)reachableProperties);
    remove(MIDI_CONTROLLER_STATE);
    save(MIDI_CONTROLLER_STATE, 0,  reachableProperties, size);
}



