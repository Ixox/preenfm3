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

}

MixerState::~MixerState() {
}

void MixerState::getFullState(char *buffer, uint32_t *size) {
    uint32_t index = 0;

    buffer[index++] = MIXER_BANK_CURRENT_VERSION;
    for (int i = 0; i < 12; i++) {
        buffer[index++] = mixName_[i];
    }
    buffer[index++] = MPE_inst1_;
    buffer[index++] = currentChannel_;
    buffer[index++] = globalChannel_;
    buffer[index++] = midiThru_;

    uint8_t *tuningUint8 = (uint8_t*) &tuning_;
    buffer[index++] = tuningUint8[0];
    buffer[index++] = tuningUint8[1];
    buffer[index++] = tuningUint8[2];
    buffer[index++] = tuningUint8[3];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        buffer[index++] = instrumentState_[t].out;
        buffer[index++] = instrumentState_[t].midiChannel;
        buffer[index++] = instrumentState_[t].firstNote;
        buffer[index++] = instrumentState_[t].lastNote;
        buffer[index++] = instrumentState_[t].shiftNote;
        buffer[index++] = instrumentState_[t].numberOfVoices;
        buffer[index++] = instrumentState_[t].scalaEnable;
        buffer[index++] = instrumentState_[t].scalaMapping;
        buffer[index++] = instrumentState_[t].scaleScaleNumber >> 8;
        buffer[index++] = (instrumentState_[t].scaleScaleNumber & 0xff);
        for (int s = 0; s < 12; s++) {
            buffer[index++] = instrumentState_[t].scalaScaleFileName[s];
        }
        uint8_t *volumeUint8 = (uint8_t*) &instrumentState_[t].volume;
        buffer[index++] = volumeUint8[0];
        buffer[index++] = volumeUint8[1];
        buffer[index++] = volumeUint8[2];
        buffer[index++] = volumeUint8[3];
        // Pan
        buffer[index++] = instrumentState_[t].pan;
        // Compressor
        buffer[index++] = instrumentState_[t].compressorType;
        // FX send
        buffer[index++] = (char)(instrumentState_[t].send * 100.0f);
    }

    // levelMetter: default mixer only
    buffer[index++] = levelMeterWhere_;

    // User CC
    for (int u = 0; u < 4; u++) {
        buffer[index++] =  userCC_[u];
    }

    // reverb preset, output, volume
    buffer[index++] = reverbPreset_;
    buffer[index++] = reverbOutput_;
    buffer[index++] = (char)(100.0f * reverbLevel_) ;

    *size = index;
}

void MixerState::getFullDefaultState(char *buffer, uint32_t *size, uint8_t mixNumber) {
    // == Default mixer for new bank
    char defaultMixName[13] = "Mix \0\0\0\0\0\0\0\0";
    if (mixNumber > 99) {
        mixNumber = 99;
    }
    uint8_t dizaine = mixNumber / 10;
    defaultMixName[4] = (char) ('0' + dizaine);
    defaultMixName[5] = (char) ('0' + (mixNumber - dizaine * 10));
    uint32_t index = 0;
    buffer[index++] = MIXER_BANK_CURRENT_VERSION;
    for (int i = 0; i < 12; i++) {
        buffer[index++] = defaultMixName[i];
    }
    buffer[index++] = 0;
    buffer[index++] = 0;
    buffer[index++] = 0;
    buffer[index++] = 0;

    float defaultTuning = 440.0f;
    uint8_t *tuningUint8 = (uint8_t*) &defaultTuning;
    buffer[index++] = tuningUint8[0];
    buffer[index++] = tuningUint8[1];
    buffer[index++] = tuningUint8[2];
    buffer[index++] = tuningUint8[3];

    uint8_t outs[] = { 1, 1, 4, 4, 6, 8 };
    int numberOfVoices[] = { 3, 3, 3, 2, 1, 1 };
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
        uint8_t *volumeUint8 = (uint8_t*) &defaultVolume;
        buffer[index++] = volumeUint8[0];
        buffer[index++] = volumeUint8[1];
        buffer[index++] = volumeUint8[2];
        buffer[index++] = volumeUint8[3];
        // Pan
        buffer[index++] = 0;
        // Compressor (instrument 0 has a medium default comp)
        buffer[index++] = (t == 0 ? 2 : 0);
        // FX send
        buffer[index++] = 0;
    }
    // levelMetter: default mixer only
    buffer[index++] = 1;

    // User CC
    for (int u = 0; u < 4; u++) {
        buffer[index++] = 34 + u;
    }

    // reverb preset, output, volume
    buffer[index++] = 7;
    buffer[index++] = 0;
    buffer[index++] = 100;
    *size = index;
}

void MixerState::restoreFullState(char *buffer) {
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
        case MIXER_BANK_VERSION4:
            restoreFullStateVersion4(buffer);
            break;
        case MIXER_BANK_VERSION5:
            restoreFullStateVersion5(buffer);
            break;
        case MIXER_BANK_VERSION6:
            restoreFullStateVersion6(buffer);
            break;
    }
}

void MixerState::setDefaultValues() {
    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState_[t].pan = 0;
        instrumentState_[t].send = 0;
        instrumentState_[t].compressorType = 0;
    }
    // Let's set instrument 1 to Medium comp by default
    instrumentState_[0].compressorType = 2;
    levelMeterWhere_ = 1;
    // User CC
    for (int u = 0; u < 4; u++) {
        userCC_[u] = 34 + u;
    }
    MPE_inst1_ = 0;

    reverbPreset_ = 7;
    reverbLevel_ = 1.0f;
    // Default output = 1&2
    reverbOutput_ = 0;

}

void MixerState::restoreFullStateVersion1(char *buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName_[i] = buffer[index++];
    }
    currentChannel_ = buffer[index++];
    globalChannel_ = buffer[index++];
    midiThru_ = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*) &tuning_;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState_[t].out = buffer[index++];
        instrumentState_[t].midiChannel = buffer[index++];
        instrumentState_[t].firstNote = buffer[index++];
        instrumentState_[t].lastNote = buffer[index++];
        instrumentState_[t].shiftNote = buffer[index++];
        instrumentState_[t].numberOfVoices = buffer[index++];
        instrumentState_[t].scalaEnable = buffer[index++];
        instrumentState_[t].scalaMapping = buffer[index++];
        instrumentState_[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState_[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState_[t].scalaScaleFileName[s] = buffer[index++];
        }
        uint8_t *volumeUint8 = (uint8_t*) &instrumentState_[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2] = buffer[index++];
        volumeUint8[3] = buffer[index++];
    }

}

/*
 * Version 2 has pan
 */
void MixerState::restoreFullStateVersion2(char *buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName_[i] = buffer[index++];
    }
    currentChannel_ = buffer[index++];
    globalChannel_ = buffer[index++];
    midiThru_ = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*) &tuning_;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState_[t].out = buffer[index++];
        instrumentState_[t].midiChannel = buffer[index++];
        instrumentState_[t].firstNote = buffer[index++];
        instrumentState_[t].lastNote = buffer[index++];
        instrumentState_[t].shiftNote = buffer[index++];
        instrumentState_[t].numberOfVoices = buffer[index++];
        instrumentState_[t].scalaEnable = buffer[index++];
        instrumentState_[t].scalaMapping = buffer[index++];
        instrumentState_[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState_[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState_[t].scalaScaleFileName[s] = buffer[index++];
        }
        uint8_t *volumeUint8 = (uint8_t*) &instrumentState_[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2] = buffer[index++];
        volumeUint8[3] = buffer[index++];
        instrumentState_[t].pan = buffer[index++];
    }
}

/*
 * Version 3 has compressor
 */
void MixerState::restoreFullStateVersion3(char *buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName_[i] = buffer[index++];
    }
    currentChannel_ = buffer[index++];
    globalChannel_ = buffer[index++];
    midiThru_ = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*) &tuning_;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState_[t].out = buffer[index++];
        instrumentState_[t].midiChannel = buffer[index++];
        instrumentState_[t].firstNote = buffer[index++];
        instrumentState_[t].lastNote = buffer[index++];
        instrumentState_[t].shiftNote = buffer[index++];
        instrumentState_[t].numberOfVoices = buffer[index++];
        instrumentState_[t].scalaEnable = buffer[index++];
        instrumentState_[t].scalaMapping = buffer[index++];
        instrumentState_[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState_[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState_[t].scalaScaleFileName[s] = buffer[index++];
        }
        uint8_t *volumeUint8 = (uint8_t*) &instrumentState_[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2] = buffer[index++];
        volumeUint8[3] = buffer[index++];
        instrumentState_[t].pan = buffer[index++];
        instrumentState_[t].compressorType = buffer[index++];
    }

    levelMeterWhere_ = buffer[index++];
}

/*
 * Version 4 has userCC
 */
void MixerState::restoreFullStateVersion4(char *buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName_[i] = buffer[index++];
    }
    currentChannel_ = buffer[index++];
    globalChannel_ = buffer[index++];
    midiThru_ = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*) &tuning_;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState_[t].out = buffer[index++];
        instrumentState_[t].midiChannel = buffer[index++];
        instrumentState_[t].firstNote = buffer[index++];
        instrumentState_[t].lastNote = buffer[index++];
        instrumentState_[t].shiftNote = buffer[index++];
        instrumentState_[t].numberOfVoices = buffer[index++];
        instrumentState_[t].scalaEnable = buffer[index++];
        instrumentState_[t].scalaMapping = buffer[index++];
        instrumentState_[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState_[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState_[t].scalaScaleFileName[s] = buffer[index++];
        }
        uint8_t *volumeUint8 = (uint8_t*) &instrumentState_[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2] = buffer[index++];
        volumeUint8[3] = buffer[index++];
        instrumentState_[t].pan = buffer[index++];
        instrumentState_[t].compressorType = buffer[index++];
    }

    levelMeterWhere_ = buffer[index++];

    // User CC
    for (int u = 0; u < 4; u++) {
        userCC_[u] = buffer[index++];
    }
}

/*
 * Version 4 has MPE
 */
void MixerState::restoreFullStateVersion5(char *buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName_[i] = buffer[index++];
    }
    MPE_inst1_ = buffer[index++];
    currentChannel_ = buffer[index++];
    globalChannel_ = buffer[index++];
    midiThru_ = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*) &tuning_;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState_[t].out = buffer[index++];
        instrumentState_[t].midiChannel = buffer[index++];
        instrumentState_[t].firstNote = buffer[index++];
        instrumentState_[t].lastNote = buffer[index++];
        instrumentState_[t].shiftNote = buffer[index++];
        instrumentState_[t].numberOfVoices = buffer[index++];
        instrumentState_[t].scalaEnable = buffer[index++];
        instrumentState_[t].scalaMapping = buffer[index++];
        instrumentState_[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState_[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState_[t].scalaScaleFileName[s] = buffer[index++];
        }
        uint8_t *volumeUint8 = (uint8_t*) &instrumentState_[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2] = buffer[index++];
        volumeUint8[3] = buffer[index++];
        instrumentState_[t].pan = buffer[index++];
        instrumentState_[t].compressorType = buffer[index++];
    }

    levelMeterWhere_ = buffer[index++];

    // User CC
    for (int u = 0; u < 4; u++) {
        userCC_[u] = buffer[index++];
    }
}

/*
 * With FX send + reverb global params
 */
void MixerState::restoreFullStateVersion6(char *buffer) {
    int index = 0;
    index++; // version

    for (int i = 0; i < 12; i++) {
        mixName_[i] = buffer[index++];
    }
    MPE_inst1_ = buffer[index++];
    currentChannel_ = buffer[index++];
    globalChannel_ = buffer[index++];
    midiThru_ = buffer[index++];

    uint8_t *tuningUint8 = (uint8_t*) &tuning_;
    tuningUint8[0] = buffer[index++];
    tuningUint8[1] = buffer[index++];
    tuningUint8[2] = buffer[index++];
    tuningUint8[3] = buffer[index++];

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        instrumentState_[t].out = buffer[index++];
        instrumentState_[t].midiChannel = buffer[index++];
        instrumentState_[t].firstNote = buffer[index++];
        instrumentState_[t].lastNote = buffer[index++];
        instrumentState_[t].shiftNote = buffer[index++];
        instrumentState_[t].numberOfVoices = buffer[index++];
        instrumentState_[t].scalaEnable = buffer[index++];
        instrumentState_[t].scalaMapping = buffer[index++];
        instrumentState_[t].scaleScaleNumber = buffer[index++] << 8;
        instrumentState_[t].scaleScaleNumber += buffer[index++];
        for (int s = 0; s < 12; s++) {
            instrumentState_[t].scalaScaleFileName[s] = buffer[index++];
        }
        uint8_t *volumeUint8 = (uint8_t*) &instrumentState_[t].volume;
        volumeUint8[0] = buffer[index++];
        volumeUint8[1] = buffer[index++];
        volumeUint8[2] = buffer[index++];
        volumeUint8[3] = buffer[index++];
        instrumentState_[t].pan = buffer[index++];
        instrumentState_[t].compressorType = buffer[index++];
        instrumentState_[t].send = 0.01f * buffer[index++];
    }

    levelMeterWhere_ = buffer[index++];

    // User CC
    for (int u = 0; u < 4; u++) {
        userCC_[u] = buffer[index++];
    }

    reverbPreset_ = buffer[index++];
    reverbOutput_ = buffer[index++];
    reverbLevel_ = .01f * buffer[index++] ;

}


char* MixerState::getMixNameFromFile(char *buffer) {
    uint8_t version = buffer[0];
    switch (version) {
        // All version have the name at the same place
        default:
            return buffer + 1;
    }
}

