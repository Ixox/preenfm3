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

#include <math.h>
#include "ScalaFile.h"
#include "MixerState.h"

// store scalaScaleFrequencies in ITCMRAM
__attribute__((section(".instruction_ram")))  float scalaFrequency[6][128];
__attribute__((section(".ram_d2b"))) struct PFM3File scalaScaleFileAlloc[NUMBEROFSCALASCALEFILES];


extern char lineBuffer[512];
extern float diatonicScaleFrequency[];


ScalaFile::ScalaFile() {
	this->numberOfFilesMax_ = NUMBEROFSCALASCALEFILES;
	this->scalaScaleFile = scalaScaleFileAlloc;
	this->myFiles_ = scalaScaleFile;
}

ScalaFile::~ScalaFile() {
	// TODO Auto-generated destructor stub
}

const char* ScalaFile::getFolderName() {
	return SCALA_DIR;
}



float* ScalaFile::loadScalaScale(MixerState* mixerState, int instrumentNumber) {
    if (mixerState->instrumentState_[instrumentNumber].scalaEnable == 0) {
        return diatonicScaleFrequency;
    }
    // propertyFiles is a 4096 char array
    char* scalaBuffer = storageBuffer;
    char *line = lineBuffer;

    const PFM3File* scaleFile = getFile(mixerState->instrumentState_[instrumentNumber].scaleScaleNumber);

    if (mixerState->instrumentState_[instrumentNumber].scalaScaleFileName[0] != 0) {
        if (fsu_->str_cmp(mixerState->instrumentState_[instrumentNumber].scalaScaleFileName, scaleFile->name) != 0) {
            // Name don't feet, new scales have been updated
            // Let's try to find the correct name
            int newNumber = getFileIndex(mixerState->instrumentState_[instrumentNumber].scalaScaleFileName);
            if (newNumber != -1) {
                scaleFile = getFile(newNumber);
                mixerState->instrumentState_[instrumentNumber].scaleScaleNumber = newNumber;
            } else {
                return (float*)0;
            }
        }
    } else {
        // v0.99 and before bug : If scalaScaleFileName is null we keep the number and copy the new name
        for (int c = 0; c < 13; c++) {
            mixerState->instrumentState_[instrumentNumber].scalaScaleFileName[c] = scaleFile->name[c];
        }
    }


    int size;
    if ((size = load(getFullName(scaleFile->name), 0,  (void*)scalaBuffer, 0)) == 0) {
        return (float*)0;
    }

    int loop = 0;
    char *readProperties = scalaBuffer;
    int state = 0;

    numberOfDegrees[instrumentNumber] = 0;
    for (int i=0; i< 24; i++) {
    	interval[instrumentNumber][i] = 0.0f;
    }
    while (loop !=-1 && (readProperties - scalaBuffer) < size) {
    	loop = fsu_->getLine(readProperties, line);
    	if (line[0] != '!') {
            switch (state) {
                case 0:
                    // line contains short description
                    break;
                case 1:
                    // line contains number of degrees
                	numberOfDegrees[instrumentNumber] = fsu_->toInt(line);
                	if (numberOfDegrees[instrumentNumber] > MAX_NUMBER_OF_INTERVALS) {
                		numberOfDegrees[instrumentNumber] = MAX_NUMBER_OF_INTERVALS;
                	}
                    break;
                default:
                    // contains the frequencey (state-2)
                	if ((state -2) < MAX_NUMBER_OF_INTERVALS) {
                		interval[instrumentNumber][state -2] = getScalaIntervale(line);
                	}
                    break;
            }
            state ++;
    	}
    	readProperties += loop;
    }
    if (numberOfDegrees[instrumentNumber] == 0) {
        return diatonicScaleFrequency;
    }
    return applyScalaScale(mixerState, instrumentNumber);
}

float* ScalaFile::applyScalaScale(MixerState* mixerState, int instrumentNumber) {
    if (mixerState->instrumentState_[instrumentNumber].scalaEnable == 0) {
        return diatonicScaleFrequency;
    }

    float *instrumentScalaFrequency = scalaFrequency[instrumentNumber];

	float octaveRatio = interval[instrumentNumber][numberOfDegrees[instrumentNumber]-1];
	int octaveDegree = numberOfDegrees[instrumentNumber];
	if (mixerState->instrumentState_[instrumentNumber].scalaMapping == SCALA_MAPPING_KEYB) {
		octaveDegree = ((numberOfDegrees[instrumentNumber] + 11) / 12) * 12;
	}

	// Fill middle C with 262.626 Hz.
	// Global tuning is applied when note are pressed
	float defaultFreq = 261.626f;
	instrumentScalaFrequency[60] = defaultFreq;

	// Fill all C
	int firstC = 0;
	int lastNote = 127;
	for (int n = 60 - octaveDegree; n >=0; n = n - octaveDegree) {
	    instrumentScalaFrequency[n] = instrumentScalaFrequency[n + octaveDegree]  / octaveRatio;
		firstC = n;
	}
	for (int n = 60 + octaveDegree; n <=127; n = n + octaveDegree) {
	    instrumentScalaFrequency[n] = instrumentScalaFrequency[n - octaveDegree]  * octaveRatio;
	}

	// init unusable last 8 notes
	for (int n = 0; n < firstC; n++) {
	    instrumentScalaFrequency[n] = defaultFreq;
	}

	// Fill all notes
	for (int octave = firstC ; octave <= 127; octave += octaveDegree) {
		for (int n = 1; n < numberOfDegrees[instrumentNumber]; n++) {
			if (octave + n <= 127) {
			    instrumentScalaFrequency[octave + n] = instrumentScalaFrequency[octave] * interval[instrumentNumber][n-1];
				lastNote = octave + n;
			}
		}
		// Remaining note must be silent
		for (int nn = numberOfDegrees[instrumentNumber] ; nn < octaveDegree; nn++) {
			if (octave + nn <= 127) {
			    instrumentScalaFrequency[octave + nn] = 0.0f;
				lastNote = octave + nn;
			}
		}
	}
	for (int n = lastNote; n < 128; n++) {
	    instrumentScalaFrequency[n] = defaultFreq;
	}
    return instrumentScalaFrequency;
}



float ScalaFile::getScalaIntervale(const char* line) {
    int slashPos = fsu_->getPositionOfSlash(line);
    if (slashPos != -1) {
        float num = fsu_->toFloat(line);
        float den = fsu_->toFloat(line + slashPos + 1);
        return num/den;
    } else {
        return pow(2.0f, fsu_->toFloat(line) / 1200.0f);
    }
}


bool ScalaFile::isCorrectFile(char *name, int size)  {
	// scalaScale Dump sysex size is 4104
	if (size >= 2048) {
		return false;
	}

	int pointPos = -1;
    for (int k=1; k<9 && pointPos == -1; k++) {
        if (name[k] == '.') {
            pointPos = k;
        }
    }
    if (pointPos == -1) return false;
    if (name[pointPos+1] != 's' && name[pointPos+1] != 'S') return false;
    if (name[pointPos+2] != 'c' && name[pointPos+2] != 'C') return false;
    if (name[pointPos+3] != 'l' && name[pointPos+3] != 'L') return false;

    return true;
}


void ScalaFile::updateScale(MixerState* mixerState, int instrumentNumber) {

}

