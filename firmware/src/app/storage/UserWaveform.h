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


#ifndef USERWAVEFORMS_H_
#define USERWAVEFORMS_H_



#include "PreenFMFileType.h"

class UserWaveform: public PreenFMFileType {
public:
    UserWaveform();
    virtual ~UserWaveform();
    void loadUserWaveforms();

protected:
    const char* getFolderName();
    bool isCorrectFile(char *name, int size) { return true; }

private:
    void loadUserWaveformFromTxt(int f, const char* fileName, int size);
    int fillUserWaveFormFromTxt(int f, char* buffer, int filled, bool last);
    void loadUserWaveformFromBin(int f, const char* fileName);
    void saveUserWaveformToBin(int f, const char* fileName);
    void interpolate(float* source, int sourceNumberOfSamples, int targetNumberOfSamples);
    void normalize(float* buffer, int numberOfSamples);
    int numberOfSampleError(int f);

    int numberOfSample;
    char userWaveFormNames[6][5];
    int floatRead;
};

#endif /* USERWAVEFORMS_H_ */
