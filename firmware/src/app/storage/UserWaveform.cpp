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

/*

 #include "LiquidCrystal.h"
extern LiquidCrystal      lcd;
*/
// User waveforms
extern float userWaveform[6][1024];

extern char lineBuffer[512];
#define LINE_BUFFER_SIZE 512

extern char* oscShapeNames[];
extern struct WaveTable waveTables[];

#include "UserWaveform.h"

UserWaveform::UserWaveform() {
    for (int k = 0; k < 6; k++) {
        userWaveFormNames_[k][4] = 0;
    }
}

UserWaveform::~UserWaveform() {
}

const char* UserWaveform::getFolderName() {
    return USERWAVEFORM_DIR;
}

void UserWaveform::loadUserWaveforms() {
    char fileName[30];

    for (int f = 0; f < 6; f++) {
        // Check if bin exists

        fsu_->copy_string(fileName, USERWAVEFORM_FILENAME_BIN);
        fileName[20] = (char)('1' + f);

        int sizeBin = checkSize(fileName);

        if (sizeBin != -1) {
            loadUserWaveformFromBin(f, fileName);
        } else {
            fsu_->copy_string(fileName, USERWAVEFORM_FILENAME_TXT);
            fileName[20] = (char)('1' + f);

            int sizeTxt = checkSize(fileName);
            if (sizeTxt == -1) {
                // Does not exist. Neither Bin nor txt => Silence
                for (int s = 0; s < 1024; s++) {
                    userWaveform[f][s] = 0.0f;
                }
            } else {
                numberOfSample_ = -1;
                userWaveFormNames_[f][0] = 0;
                loadUserWaveformFromTxt(f, fileName, sizeTxt);

                if (numberOfSample_ > 0) {
                    if (numberOfSample_ > 512 && numberOfSample_ < 1024) {
                        interpolate(userWaveform[f], numberOfSample_, 1024);
                    } else if (numberOfSample_ > 256 && numberOfSample_ < 512) {
                        interpolate(userWaveform[f], numberOfSample_, 512);
                    } else if (numberOfSample_ > 128 && numberOfSample_ < 256) {
                        interpolate(userWaveform[f], numberOfSample_, 256);
                    } else if (numberOfSample_ > 64 && numberOfSample_ < 128) {
                        interpolate(userWaveform[f], numberOfSample_, 128);
                    } else if (numberOfSample_ > 32 && numberOfSample_ < 64) {
                        interpolate(userWaveform[f], numberOfSample_, 64);
                    }

                    normalize(userWaveform[f], numberOfSample_);

                    fsu_->copy_string(fileName, USERWAVEFORM_FILENAME_BIN);
                    fileName[20] = (char)('1' + f);
                    saveUserWaveformToBin(f, fileName);
                    // Reload from Bin
                    loadUserWaveformFromBin(f, fileName);
                }
            }
        }
    }
}

void UserWaveform::loadUserWaveformFromTxt(int f, const char* fileName, int size) {
    int readIndex = 0;
    floatRead_ = 0;

    while (floatRead_ != numberOfSample_) {
        int toRead = (size - readIndex > LINE_BUFFER_SIZE) ? LINE_BUFFER_SIZE : size - readIndex;
        load(fileName, readIndex, (void*)&lineBuffer, toRead);

        int used = fillUserWaveFormFromTxt(f, lineBuffer, toRead, (readIndex + toRead) >= size);
        if (used < 0) {
            return;
        }
        readIndex += used;
    }
}

int UserWaveform::fillUserWaveFormFromTxt(int f, char* buffer, int filled, bool last) {
    int index = 0;
    bool bStop = false;
    while (!bStop) {
        int floatSize = 0;

        // Init name
        if (userWaveFormNames_[f][0] == 0) {

            while (fsu_->isSeparator(buffer[index])) {
                index++;
            }
            int b = 0;
            while (b < 4 && !fsu_->isSeparator(buffer[index])) {
                userWaveFormNames_[f][b++] = buffer[index++];
            }
            // complete with space
            while (b < 4) {
                userWaveFormNames_[f][b++] = ' ';
            }
        }
        // init number of sample
        if (numberOfSample_ <= 0) {
            while (fsu_->isSeparator(buffer[index])) {
                index++;
            }
            numberOfSample_ = (int)(fsu_->stof(&buffer[index], floatSize) + .5f);

            if (numberOfSample_ < 32 || numberOfSample_ > 1024) {
                return numberOfSampleError(f);
            }
            waveTables[f + 8].max = (numberOfSample_ - 1);
            index += floatSize;
        }

        userWaveform[f][floatRead_++] = fsu_->stof(&buffer[index], floatSize);
        index += floatSize;

        // Stop if index > (filled - 30) Or if last && floatRead == 1024
        bStop = (!last && index > (filled - 40)) || floatRead_ == numberOfSample_;
    }
    return index;
}

int UserWaveform::numberOfSampleError(int f) {
    numberOfSample_ = 0;
    userWaveFormNames_[f][0] = '#';
    oscShapeNames[8 + f] = userWaveFormNames_[f];
    return -1;
}

void UserWaveform::loadUserWaveformFromBin(int f, const char* fileName) {
    load(fileName, 0, userWaveFormNames_[f], 4);
    oscShapeNames[8 + f] = userWaveFormNames_[f];

    load(fileName, 4, &numberOfSample_, 4);
    waveTables[f + 8].max = (numberOfSample_ - 1);
    waveTables[f + 8].precomputedValue = (waveTables[f + 8].max + 1) * waveTables[f + 8].useFreq * PREENFM_FREQUENCY_INVERSED;

    int sampleSize = numberOfSample_ * 4;
    if (sampleSize <= 512) {
        load(fileName, 8, userWaveform[f], sampleSize);
    } else {
        int loadIndex = 0;
        char* readBuffer = (char*)userWaveform[f];
        while (loadIndex < sampleSize) {
            load(fileName, 8 + loadIndex, readBuffer + loadIndex, 512);
            loadIndex += 512;
        }
    }
}

void UserWaveform::saveUserWaveformToBin(int f, const char* fileName) {
    save(fileName, 0, userWaveFormNames_[f], 4);
    save(fileName, 4, &numberOfSample_, 4);
    int sampleSize = numberOfSample_ * 4;
    if (sampleSize <= 512) {
        save(fileName, 8, userWaveform[f], sampleSize);
    } else {
        int saveIndex = 0;
        char* readBuffer = (char*)userWaveform[f];
        while (saveIndex < sampleSize) {
            save(fileName, 8 + saveIndex, readBuffer + saveIndex, 512);
            saveIndex += 512;
        }
    }
}

void UserWaveform::normalize(float* buffer, int numberOfSamples) {
    float min = 0;
    float max = 0;
    float average = 0;
    for (int i = 0; i < numberOfSamples; i++) {
        average += buffer[i];
        if (buffer[i] < min) {
            min = buffer[i];
        }
        if (buffer[i] > max) {
            max = buffer[i];
        }
    }
    average /= numberOfSamples;
    min -= average;
    max -= average;
    float m1 = min != 0 ? -1 / min : 1;
    float m2 = max != 0 ? 1 / max : 1;
    float m = m1 > m2 ? m2 : m1;

    for (int i = 0; i < numberOfSamples; i++) {
        buffer[i] -= average;
        buffer[i] *= m;
    }
}

void UserWaveform::interpolate(float* buffer, int sourceNumberOfSamples, int targetNumberOfSamples) {
    for (int i = targetNumberOfSamples - 1; i >= 0; i--) {
        float pos = (float)i * (float)sourceNumberOfSamples / (float)targetNumberOfSamples;
        int iPos = pos;
        float decimal = pos - iPos;
        buffer[i] = buffer[iPos] * (1 - decimal) + buffer[iPos + 1] * decimal;
    }
    numberOfSample_ = targetNumberOfSamples;
}
