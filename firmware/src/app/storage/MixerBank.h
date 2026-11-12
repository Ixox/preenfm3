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

class MixerBank : public PreenFMFileType {
public:
	MixerBank();
	virtual ~MixerBank();
    void init(struct OneSynthParams*timbre1, struct OneSynthParams*timbre2, struct OneSynthParams*timbre3,
            struct OneSynthParams*timbre4, struct OneSynthParams*timbre5, struct OneSynthParams*timbre6);
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
	bool isCorrectFile(char *name, int size);
    struct OneSynthParams* timbre[NUMBER_OF_TIMBRES];
    bool saveMixerData(FIL* file, uint8_t mixerNumber, MixerState* mixerStateToSave);
    bool loadMixerData(FIL* file, uint8_t mixerNumber);

private:
    char presetName[13];
    MixerState* mixerState;
    ScalaFile* scalaFile;
    Sequencer* sequencer;
};

#endif /* MIXERBANK_H */
