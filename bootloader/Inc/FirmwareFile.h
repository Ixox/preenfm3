/*
 * FirmwareFile.h
 *
 *  Created on: 24 juil. 2015
 *      Author: xavier
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
