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


#ifndef SCALAFILE_H_
#define SCALAFILE_H_

#include "PreenFMFileType.h"
#define MAX_NUMBER_OF_INTERVALS 36

class MixerState;

class ScalaFile: public PreenFMFileType {
public:
    ScalaFile();
    virtual ~ScalaFile();
    float* loadScalaScale(MixerState* mixerState, int instrumentNumber);
    float* applyScalaScale(MixerState* mixerState, int instrumentNumber);
    void updateScale(MixerState* mixerState, int instrumentNumber);

protected:
    const char* getFolderName();
    bool isCorrectFile(char *name, int size);

private:
    float getScalaIntervale(const char* line);
    struct PFM3File *scalaScaleFile;
    float interval[NUMBER_OF_TIMBRES][MAX_NUMBER_OF_INTERVALS];
    uint8_t numberOfDegrees[NUMBER_OF_TIMBRES];

};

#endif /* SCALAFILE_H_ */
