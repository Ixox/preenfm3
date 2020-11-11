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

#ifndef SYNTH_MIXERSTATE_H_
#define SYNTH_MIXERSTATE_H_

#include "Common.h"

enum MIXER_BANK_VERSION {
    MIXER_BANK_VERSION1 = 1,
    // Pan
    MIXER_BANK_VERSION2
};

#define MIXER_BANK_CURRENT_VERSION MIXER_BANK_VERSION2



struct MixerInstrumentState {
    uint8_t out;
    uint8_t midiChannel;
    uint8_t firstNote, lastNote;
    int8_t shiftNote;
    uint8_t numberOfVoices;
    // Scala tuning is based on MixerState.tuning
    uint8_t scalaEnable;
    uint8_t scalaMapping;
    uint16_t scaleScaleNumber;
    char scalaScaleFileName[13];
    float volume;
    float *scaleFrequencies;
    int8_t pan;
};



class MixerState {
public:
    MixerState();
    virtual ~MixerState();

    void getFullState(char* buffer, uint32_t *size);
    void getFullDefaultState(char* buffer, uint32_t *size, uint8_t mixNumber);
    void restoreFullState(char* buffer);
    static char* getMixNameFromFile(char* buffer);

    char mixName[13];
    uint8_t currentChannel;
    uint8_t globalChannel;
    uint8_t midiThru;
    float tuning;
    struct MixerInstrumentState instrumentState[NUMBER_OF_TIMBRES];

private :
    void restoreFullStateVersion1(char* buffer);
    void restoreFullStateVersion2(char* buffer);
};

#endif /* SYNTH_MIXERSTATE_H_ */
