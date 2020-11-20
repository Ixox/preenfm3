/*
 * Copyright 2020 Xavier Hosxe
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

#include <MixerState.h>

MixerState::MixerState() {
    // TODO Auto-generated constructor stub

}

MixerState::~MixerState() {
    // TODO Auto-generated destructor stub
}

void MixerState::getFullState(char* buffer, uint32_t *size) {
    uint32_t index = 0;

    buffer[index++] = MIXER_BANK_CURRENT_VERSION;
    for (int i = 0; i < 12; i++) {
        buffer[index++] = mixName[i];
    }
    buffer[index++] = currentChannel;
    buffer[index++] = globalChannel;
    buffer[index++] = midiThru;

    uint8_t *tuningUint8 = (uint8_t*)&tuning;
    buffer[index++] = tuningUint8[0];
    buffer[index++] = tuningUint8[1];
    buffer[index++] = tuningUint8[2];
    buffer[index++] = tuningUint8[3];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        buffer[index++] = instrumentState[t].out;
        buffer[index++] = instrumentState[t].midiChannel;
        buffer[index++] = instrumentState[t].firstNote;
        buffer[index++] = instrumentState[t].lastNote;
        buffer[index++] = instrumentState[t].shiftNote;
        buffer[index++] = instrumentState[t].numberOfVoices;
        buffer[index++] = instrumentState[t].scalaEnable;
        buffer[index++] = instrumentState[t].scalaMapping;
        buffer[index++] = instrumentState[t].scaleScaleNumber >> 8;
        buffer[index++] = (instrumentState[t].scaleScaleNumber & 0xff);
        for (int s = 0; s < 12; s++) {
            buffer[index++] = instrumentState[t].scalaScaleFileName[s];
        }
        uint8_t *volumeUint8 = (uint8_t*)&instrumentState[t].volume;
        buffer[index++] = volumeUint8[0];
        buffer[index++] = volumeUint8[1];
        buffer[index++] = volumeUint8[2];
        buffer[index++] = volumeUint8[3];
        // Pan
        buffer[index++] = instrumentState[t].pan;
        // Compressor
        buffer[index++] = instrumentState[t].compressorType;
    }

    *size = index;
}

void MixerState::getFullDefaultState(char * buffer, uint32_t *size, uint8_t mixNumber) {
    // == Default mixer for new bank
    char defaultMixName[13] = "Mix \0\0\0\0\0\0\0\0";
    if (mixNumber > 99) {
        mixNumber = 99;
    }
    uint8_t dizaine = mixNumber / 10;
    defaultMixName[4] = (char)('0' + dizaine);
    defaultMixName[5] = (char)('0' + (mixNumber - dizaine * 10));
    uint32_t index = 0;
    buffer[index++] = MIXER_BANK_CURRENT_VERSION;
    for (int i = 0; i < 12; i++) {
        buffer[index++] = defaultMixName[i];
    }
    buffer[index++] = currentChannel;
    buffer[index++] = globalChannel;
    buffer[index++] = midiThru;

    float defaultTuning = 440.0f;
    uint8_t *tuningUint8 = (uint8_t*)&defaultTuning;
    buffer[index++] = tuningUint8[0];
    buffer[index++] = tuningUint8[1];
    buffer[index++] = tuningUint8[2];
    buffer[index++] = tuningUint8[3];

    uint8_t outs[] = { 1, 1, 4, 4, 6, 8 };
    int numberOfVoices[]= { 3, 3, 3, 2, 1, 1 };
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        buffer[index++] = outs[t];
        buffer[index++] = 1 + t;
        buffer[index++] = 0;
        buffer[index++] = 127;
        buffer[index++] = 0;
        buffer[index++] = numberOfVoices[t];
        buffer[index++] = 0;
        buffer[index++] = 0;
        buffer[index++] = 0;
        buffer[index++] = 0;
        for (int s = 0; s < 12; s++) {
            buffer[index++] = 0;
        }
        float defaultVolume = 1.0f;
        uint8_t *volumeUint8 = (uint8_t*)&defaultVolume;
        buffer[index++] = volumeUint8[0];
        buffer[index++] = volumeUint8[1];
        buffer[index++] = volumeUint8[2];
        buffer[index++] = volumeUint8[3];
        // Pan
        buffer[index++] = 0;
        // Compressor
        buffer[index++] = 0;
    }

    *size = index;
}

void MixerState::restoreFullState(char* buffer) {
    uint8_t version = buffer[0];
    setDefaultValues();
    switch (version) {
        case MIXER_BANK_VERSION1:
            restoreFullStateVersion1(buffer);
            break;
        case MIXER_BANK_VERSION2:
            restoreFullStateVersion2(buffer);
            break;
        case MIXER_BANK_VERSION3:
            restoreFullStateVersion3(buffer);
            break;
    }
}


void MixerState::setDefaultValues() {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState[t].pan = 0;
        instrumentState[t].compressorType = 0;
    }
}

void MixerState::restoreFullStateVersion1(char* buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName[i] = buffer[index++] ;
    }
    currentChannel = buffer[index++];
    globalChannel = buffer[index++];
    midiThru = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*)&tuning;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState[t].out = buffer[index++];
        instrumentState[t].midiChannel = buffer[index++];
        instrumentState[t].firstNote = buffer[index++];
        instrumentState[t].lastNote = buffer[index++];
        instrumentState[t].shiftNote = buffer[index++];
        instrumentState[t].numberOfVoices = buffer[index++];
        instrumentState[t].scalaEnable = buffer[index++];
        instrumentState[t].scalaMapping = buffer[index++];
        instrumentState[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState[t].scalaScaleFileName[s] = buffer[index++] ;
        }
        uint8_t *volumeUint8 = (uint8_t*)&instrumentState[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2]= buffer[index++];
        volumeUint8[3] = buffer[index++];
    }
}

/*
 * Version 2 has pan
 */
void MixerState::restoreFullStateVersion2(char* buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName[i] = buffer[index++] ;
    }
    currentChannel = buffer[index++];
    globalChannel = buffer[index++];
    midiThru = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*)&tuning;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState[t].out = buffer[index++];
        instrumentState[t].midiChannel = buffer[index++];
        instrumentState[t].firstNote = buffer[index++];
        instrumentState[t].lastNote = buffer[index++];
        instrumentState[t].shiftNote = buffer[index++];
        instrumentState[t].numberOfVoices = buffer[index++];
        instrumentState[t].scalaEnable = buffer[index++];
        instrumentState[t].scalaMapping = buffer[index++];
        instrumentState[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState[t].scalaScaleFileName[s] = buffer[index++] ;
        }
        uint8_t *volumeUint8 = (uint8_t*)&instrumentState[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2]= buffer[index++];
        volumeUint8[3] = buffer[index++];
        instrumentState[t].pan = buffer[index++];
    }}

/*
 * Version 2 has compressor
 */
void MixerState::restoreFullStateVersion3(char* buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName[i] = buffer[index++] ;
    }
    currentChannel = buffer[index++];
    globalChannel = buffer[index++];
    midiThru = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*)&tuning;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState[t].out = buffer[index++];
        instrumentState[t].midiChannel = buffer[index++];
        instrumentState[t].firstNote = buffer[index++];
        instrumentState[t].lastNote = buffer[index++];
        instrumentState[t].shiftNote = buffer[index++];
        instrumentState[t].numberOfVoices = buffer[index++];
        instrumentState[t].scalaEnable = buffer[index++];
        instrumentState[t].scalaMapping = buffer[index++];
        instrumentState[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState[t].scalaScaleFileName[s] = buffer[index++] ;
        }
        uint8_t *volumeUint8 = (uint8_t*)&instrumentState[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2]= buffer[index++];
        volumeUint8[3] = buffer[index++];
        instrumentState[t].pan = buffer[index++];
        instrumentState[t].compressorType = buffer[index++];
    }}


char* MixerState::getMixNameFromFile(char* buffer) {
    uint8_t version = buffer[0];
    switch (version) {
        case MIXER_BANK_VERSION1:
            return buffer + 1;
        case MIXER_BANK_VERSION2:
            return buffer + 1;
        case MIXER_BANK_VERSION3:
            return buffer + 1;
    }
}

