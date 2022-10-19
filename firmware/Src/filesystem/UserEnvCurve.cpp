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

#define LINE_BUFFER_SIZE 64
extern char lineBuffer[64];

// User curves
extern float userEnvCurves[4][64];
extern char *envCurveNames[];

#include "UserEnvCurve.h"

UserEnvCurve::UserEnvCurve() {
    for (int k=0; k<4; k++) {
        userEnvCurveNames[k][4] = 0;
    }
}

UserEnvCurve::~UserEnvCurve() {
}

const char* UserEnvCurve::getFolderName() {
    return USERCURVE_DIR;
}

void UserEnvCurve::loadUserEnvCurves() {
    char fileName[30];


    for (int f = 0; f < 4; f++) {
        // Check if bin exists

        fsu_->copy_string(fileName, USERCURVE_FILENAME_BIN);
        fileName[20] = (char)('1' + f);

        int sizeBin = checkSize(fileName);

        if (sizeBin != -1) {
            loadUserEnvCurveFromBin(f, fileName);
        } else {
            fsu_->copy_string(fileName, USERCURVE_FILENAME_TXT);
            fileName[20] = (char)('1' + f);

            int sizeTxt = checkSize(fileName);
            if (sizeTxt == -1) {
                // Does not exist. Neither Bin nor txt => Linear env
                for (float s = 0; s < 64; s++) {
                    userEnvCurves[f][(int) s] = s / 64;
                }
            } else {
                numberOfSample = -1;
                userEnvCurveNames[f][0] = 0;
                loadUserEnvCurveFromTxt(f, fileName, sizeTxt);

                if (numberOfSample > 0) {
                    if (numberOfSample > 3 && numberOfSample < 64) {
                        interpolate(userEnvCurves[f], numberOfSample, 64);
                    }

                    normalize(userEnvCurves[f], numberOfSample);

                    fsu_->copy_string(fileName, USERCURVE_FILENAME_BIN);
                    fileName[20] = (char)('1' + f);
                    saveUserEnvCurveToBin(f, fileName);
                    // Reload from Bin
                    loadUserEnvCurveFromBin(f, fileName);
                }
            }
        }
    }
}

void UserEnvCurve::loadUserEnvCurveFromBin(int f, const char* fileName) {
    load(fileName, 0, userEnvCurveNames[f], 4);
    envCurveNames[3 + f] = userEnvCurveNames[f];

    load(fileName, 4, &numberOfSample, 2);

    int sampleSize = numberOfSample * 4;
    load(fileName, 6, userEnvCurves[f], sampleSize);
}

void UserEnvCurve::saveUserEnvCurveToBin(int f, const char* fileName) {
    save(fileName, 0, userEnvCurveNames[f], 4);
    save(fileName, 4, &numberOfSample, 2);
    int sampleSize = numberOfSample * 4;
    save(fileName, 6, userEnvCurves[f], sampleSize);
}

void UserEnvCurve::loadUserEnvCurveFromTxt(int f, const char* fileName, int size) {
    int readIndex = 0;
    floatRead = 0;

    while (floatRead != numberOfSample) {
        int toRead = (size - readIndex > LINE_BUFFER_SIZE) ? LINE_BUFFER_SIZE : size - readIndex;
        load(fileName, readIndex,  (void*)&lineBuffer, toRead);

        int used = fillUserEnvCurveFromTxt(f, lineBuffer, toRead, (readIndex+toRead) >= size);
        if (used < 0) {
            return;
        }
        readIndex += used;
    }
}

int UserEnvCurve::fillUserEnvCurveFromTxt(int f, char* buffer, int filled, bool last) {
    int index = 0;
    bool bStop = false;
    while (!bStop) {
        int floatSize = 0;

        // Init name
        if (userEnvCurveNames[f][0] == 0) {

            while (fsu_->isSeparator(buffer[index])) {
                index++;
            }
            int b = 0;
            while (b<4 && !fsu_->isSeparator(buffer[index])) {
                userEnvCurveNames[f][b++] = buffer[index++];
            }
            // complete with space
            while (b<4) {
                userEnvCurveNames[f][b++] = ' ';
            }
        }
        // init number of sample
        if (numberOfSample <= 0) {
            while (fsu_->isSeparator(buffer[index])) {
                index++;
            }
            numberOfSample = (int)(fsu_->stof(&buffer[index], floatSize) + .5f);

            if (numberOfSample != 64) {
                return numberOfSampleError(f);
            }

            index += floatSize;
        }

        userEnvCurves[f][floatRead++] = fsu_->stof(&buffer[index], floatSize);
        index += floatSize;

        // Stop if index > (filled - 30) Or if last && floatRead == 1024
        bStop = (!last && index > (filled - 40)) || floatRead == numberOfSample;
    }
    return index;
}

void UserEnvCurve::normalize(float* buffer, int numberOfSamples) {
    float min = 0;
    float max = 0;
    float average = 0;
    for (int i=0; i < numberOfSamples; i++) {
        average += buffer[i];
        if (buffer[i] < min) {
            min = buffer[i];
        }
        if (buffer[i] > max) {
            max = buffer[i];
        }
    }
	
    float m = (max-min) == 0 ? 1 / (max-min) : 1;

    for (int i=0; i < numberOfSamples; i++) {
        buffer[i] -= min;
        buffer[i] *= m;
		if(buffer[i]>1) {
			buffer[i] = 1;
		}
		if(buffer[i]<0) {
			buffer[i] = 0;
		}
    }
}

int UserEnvCurve::numberOfSampleError(int f) {
    numberOfSample = 0;
    userEnvCurveNames[f][0] = '#';
    return -1;
}

void UserEnvCurve::interpolate(float* buffer, int sourceNumberOfSamples, int targetNumberOfSamples) {
    for (int i = targetNumberOfSamples-1; i>=0; i--) {
        float pos = (float)i * (float)sourceNumberOfSamples / (float)targetNumberOfSamples;
        int iPos = pos;
        float decimal = pos - iPos;
        buffer[i] = buffer[iPos] * (1 - decimal) + buffer[iPos + 1] * decimal;
    }
    numberOfSample = targetNumberOfSamples;
}
