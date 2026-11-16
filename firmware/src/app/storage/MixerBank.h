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

#ifndef MIXERBANK_H
#define MIXERBANK_H

#include "PreenFMFileType.h"

class ScalaFile;
class Sequencer;
class MixerState;
class Timbre;

class MixerBank : public PreenFMFileType {
public:
    MixerBank();
    virtual ~MixerBank();
    void init(Timbre* timbre1, Timbre* timbre2, Timbre* timbre3, Timbre* timbre4, Timbre* timbre5, Timbre* timbre6);
    void setMixerState(MixerState* mixerState);
    void setSequencer(Sequencer* sequencer);
    void setScalaFile(ScalaFile* scalaFile);

    bool saveDefaultMixer();
    bool loadDefaultMixer();
    void removeDefaultMixer();
    void createMixerBank(const char* name);

    bool loadMixer(const struct PFM3File* mixer, int mixerNumber);
    const char* loadMixerName(const struct PFM3File* mixer, int mixerNumber);
    bool saveMixer(const struct PFM3File* mixer, int mixerNumber, char* mixerName);

protected:
    const char* getFolderName();
    bool isCorrectFile(char* name, int size);
    Timbre* timbre_[NUMBER_OF_TIMBRES];
    bool saveMixerData(FIL* file, uint8_t mixerNumber, MixerState* mixerStateToSave);
    bool loadMixerData(FIL* file, uint8_t mixerNumber);

private:
    char presetName_[13];
    MixerState* mixerState_;
    ScalaFile* scalaFile_;
    Sequencer* sequencer_;
};

#endif /* MIXERBANK_H */
