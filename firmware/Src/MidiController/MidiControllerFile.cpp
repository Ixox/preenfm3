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
    uint16_t* p = (uint16_t*)reachableProperties;
    int version = (int)*(p++);

    switch (version) {
    case MIDI_CONTROLLER_VERSION_1: {
        for (int pageNumber = 0; pageNumber < MIDI_NUMBER_OF_PAGES; pageNumber++) {
            for (int e = 0; e < 6; e++) {
                MidiEncoder *encoder = midiControllerState->getEncoder(pageNumber, e);
                char *nameP = (char*)p;
                for (int c = 0; c < 6 ; c++) {
                    encoder->name[c] = *(nameP++);
                }
                // Let skip 8 bytes
                p += 4;
                encoder->encoderType = (MidiEncoderType)*(p++);
                encoder->midiChannel = *(p++);
                encoder->controller = *(p++);
                encoder->value = *(p++);
                encoder->maxValue = *(p++);
                encoder->minValue = *(p++);
            }
            for (int b = 0; b < 6; b++) {
                MidiButton *button = midiControllerState->getButton(pageNumber, b);
                char *nameP = (char*)p;
                for (int c = 0; c < 6 ; c++) {
                    button->name[c] = *(nameP++);
                }
                // Let skip 8 bytes
                p += 4;
                button->buttonType = (MidiButtonType)*(p++);
                button->midiChannel = *(p++);
                button->controller = *(p++);
                button->value = *(p++);
                button->valueOff = *(p++);
                button->valueOn = *(p++);
            }
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

    uint16_t* p = (uint16_t*)reachableProperties;
    *(p++) = MIDI_CONTROLLER_CURRENT_VERSION;

    for (int pageNumber = 0; pageNumber < MIDI_NUMBER_OF_PAGES; pageNumber++) {
        for (int e = 0; e < 6; e++) {
            MidiEncoder *encoder = midiControllerState->getEncoder(pageNumber, e);
            char *nameP = (char*)p;
            for (int c = 0; c < 6 ; c++) {
                *(nameP++) = encoder->name[c];
            }
            // Let skip 8 bytes
            p += 4;
            *(p++) = encoder->encoderType;
            *(p++) = encoder->midiChannel;
            *(p++) = encoder->controller;
            *(p++) = encoder->value;
            *(p++) = encoder->maxValue;
            *(p++) = encoder->minValue;
        }
        for (int b = 0; b < 6; b++) {
            MidiButton *button = midiControllerState->getButton(pageNumber, b);
            char *nameP = (char*)p;
            for (int c = 0; c < 6 ; c++) {
                *(nameP++) = button->name[c];
            }
            // Let skip 8 bytes
            p += 4;
            *(p++) = button->buttonType;
            *(p++) = button->midiChannel;
            *(p++) = button->controller;
            *(p++) = button->value;
            *(p++) = button->valueOff;
            *(p++) = button->valueOn;
        }
    }

    int size = ((uint32_t)p) -  ((uint32_t)reachableProperties);
    remove(MIDI_CONTROLLER_STATE);
    save(MIDI_CONTROLLER_STATE, 0,  reachableProperties, size);
}



