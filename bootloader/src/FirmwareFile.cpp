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


#include "FirmwareFile.h"
#include "fatfs.h"

__attribute__((section(".ram_d2"))) uint8_t firmwareRead[2048];
__attribute__((section(".ram_d2"))) struct FirmwarePFM3File firmwareAlloc[16];



FirmwareFile::FirmwareFile() {
    isInitialized = false;
    numberOfFilesMax = 16;
    myFiles = firmwareAlloc;
}

FirmwareFile::~FirmwareFile() {
}

uint32_t FirmwareFile::getNumberOfFiles() {
    if (!isInitialized) {
        initFiles();
    }
    return numberOfFiles;
}



const struct FirmwarePFM3File* FirmwareFile::getFile(uint32_t fileNumber) {
    if (!isInitialized) {
        initFiles();
    }
    if (fileNumber < 0 || fileNumber >= numberOfFilesMax) {
        return &myFiles[0];
    }
    return &myFiles[fileNumber];
}

// Init bank
int FirmwareFile::initFiles() {

    numberOfFiles = 0;
    FRESULT res;
    DIR dir;
    static FILINFO fno;

    for (uint32_t k = 0; k < numberOfFilesMax; k++) {
        myFiles[k].fileType = FILE_EMPTY;
    }

    res = f_opendir(&dir, getFolderName());
    if (res == FR_OK) {

        while (numberOfFiles < numberOfFilesMax) {
            bool beforePoint = true;
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) {
                break;
            }
            if (!(fno.fattrib & AM_DIR)) {
                if (isCorrectFile((char*) fno.fname, fno.fsize)) {
                    for (int k = 0; k < 13; k++) {
                        if (fno.fname[k] == ' ') {
                            myFiles[numberOfFiles].name[k] = '_';
                        } else if (fno.fname[k] == 0 && beforePoint) {
                            myFiles[numberOfFiles].name[k] = '~';
                        } else {
                            if (fno.fname[k] == '.') {
                                beforePoint = false;
                            }
                            myFiles[numberOfFiles].name[k] = fno.fname[k];
                        }
                    }
                    myFiles[numberOfFiles].size = fno.fsize;
                    if (fno.fname[0] == '_') {
                        myFiles[numberOfFiles].fileType = FILE_READ_ONLY;
                    } else {
                        myFiles[numberOfFiles].fileType = FILE_OK;
                    }
                    numberOfFiles++;
                }
            }
        }
        f_closedir(&dir);
    }
    sortFiles(myFiles, numberOfFiles);
    isInitialized = true;
    return numberOfFiles;
}

const char* FirmwareFile::getFolderName() {
	return "0:/";
}

bool FirmwareFile::isCorrectFile(char *name, int size)  {
    int pointPos = -1;
    for (int k = 10; k > 2 && pointPos == -1; k--) {
        if (name[k] == '.') {
            pointPos = k;
        }
    }
    if (pointPos == -1) return false;
    if (name[pointPos+1] != 'b' && name[pointPos+1] != 'B') return false;
    if (name[pointPos+2] != 'i' && name[pointPos+2] != 'I') return false;
    if (name[pointPos+3] != 'n' && name[pointPos+3] != 'N') return false;

    return true;
}

const char* FirmwareFile::getFullName(const char* fileName) {
    const char* pathName = getFolderName();
    int pos = 0;
    int cpt = 0;
    for (int k =0; k < strlen(pathName) && cpt++<24; k++) {
        this->fullName[pos++] = pathName[k];
    }
    this->fullName[pos++] = '/';
    cpt = 0;
    for (int k = 0; k <strlen(fileName) && cpt++<14 ; k++) {
        this->fullName[pos++] = fileName[k];
    }
    this->fullName[pos] = 0;
    return this->fullName;
}

int FirmwareFile::strlen(const char *str) {
    int k;
    for (k=0; k<1000 && str[k] != 0; k++);
    return k;
}

int FirmwareFile::strcmp(const char *s1, const char *s2) {
    while (*s1==*s2)
    {
        if(*s1=='\0')
            return(0);
        s1++;
        s2++;
    }
    char rs1 = *s1;
    char rs2 = *s2;
    if (rs1 == '_') {
        rs1 = 0;
    }
    if (rs2 == '_') {
        rs2 = 0;
    }
    return(rs1-rs2);
}





void FirmwareFile::swapFiles(struct FirmwarePFM3File* bankFiles, int i, int j) {
    if (i == j) {
        return;
    }
    struct FirmwarePFM3File tmp = bankFiles[i];
    bankFiles[i] = bankFiles[j];
    bankFiles[j] = tmp;
}

void FirmwareFile::sortFiles(struct FirmwarePFM3File* bankFiles, int numberOfFiles) {
    for (int i = 0; i < numberOfFiles - 1; i++) {
        int minBank = i;
        for (int j = i + 1; j < numberOfFiles; j++) {
            if (strcmp(bankFiles[minBank].name, bankFiles[j].name) > 0) {
                minBank = j;
            }
        }
        swapFiles(bankFiles, i, minBank);
    }
}
