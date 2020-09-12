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



#ifndef FIRMWAREFILE_H_
#define FIRMWAREFILE_H_

#include "stm32h7xx_hal.h"

typedef enum {
    FILE_OK = 0,
    FILE_READ_ONLY,
    FILE_EMPTY
} FileType;

struct FirmwarePFM3File {
    char name[13];
    FileType fileType;
    uint32_t size;
};


class FirmwareFile  {
public:
	FirmwareFile();
	virtual ~FirmwareFile();
    const struct FirmwarePFM3File* getFile(uint32_t fileNumber);
    uint32_t getNumberOfFiles() { return numberOfFiles; };
    const char* getFullName(const char* fileName);

protected:
    int initFiles();
	const char* getFolderName();
	bool isCorrectFile(char *name, int size);
	int strlen(const char *str);
	int strcmp(const char *s1, const char *s2);
	void swapFiles(struct FirmwarePFM3File* bankFiles, int i, int j);
	void sortFiles(struct FirmwarePFM3File* bankFiles, int numberOfFiles);

private:
    uint32_t numberOfFiles;
	uint32_t numberOfFilesMax;
    struct FirmwarePFM3File *myFiles;
    char fullName[64];
    bool isInitialized;
};

#endif /* FIRMWAREFILE_H_ */
