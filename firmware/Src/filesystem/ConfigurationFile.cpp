/*
 * ConfigurationFile.cpp
 *
 *  Created on: 24 juil. 2015
 *      Author: xavier
 */

#include "ConfigurationFile.h"
#include "Menu.h"

#define SCALA_ENABLED   "scala_enabled"
#define SCALA_FILENAME  "scala_filename"
#define SCALA_FREQUENCY "scala_frequency"
#define SCALA_KEYBOARD "scala_keyboard"

extern char lineBuffer[512];

ConfigurationFile::ConfigurationFile() {
	numberOfFilesMax = 0;
}

ConfigurationFile::~ConfigurationFile() {
}

const char* ConfigurationFile::getFolderName() {
	return PREENFM_DIR;
}


void ConfigurationFile::loadConfig(uint8_t* midiConfigBytes) {
	char *line = lineBuffer;
    char* reachableProperties = storageBuffer;
    int size = checkSize(PROPERTIES);
    if (size >= PROPERTY_FILE_SIZE || size == -1) {
    	// ERROR
    	return;
    }
    reachableProperties[size] = 0;

    int result = load(PROPERTIES, 0,  reachableProperties, size);
    int loop = 0;
    char *readProperties = reachableProperties;
    while (loop !=-1 && (readProperties - reachableProperties) < size) {
    	loop = fsu->getLine(readProperties, line);
    	if (line[0] != '#') {
    		fillMidiConfig(midiConfigBytes, line);
    	}
    	readProperties += loop;
    }
}


void ConfigurationFile::saveConfig(uint8_t* midiConfigBytes) {
    int wptr = 0;
    for (int k=0; k<MIDICONFIG_SIZE; k++) {
    	storageBuffer[wptr++] = '#';
    	storageBuffer[wptr++] = ' ';
    	wptr += fsu->copy_string((char*)storageBuffer + wptr, midiConfig[k].title);
    	storageBuffer[wptr++] = '\n';
    	if (midiConfig[k].maxValue < 10 && midiConfig[k].valueName != 0) {
	    	wptr += fsu->copy_string((char*)storageBuffer + wptr, "#   0=");
			for (int o=0; o<midiConfig[k].maxValue; o++) {
		    	wptr += fsu->copy_string((char*)storageBuffer + wptr, midiConfig[k].valueName[o]);
				if ( o != midiConfig[k].maxValue - 1) {
			    	wptr += fsu->copy_string((char*)storageBuffer + wptr, ", ");
				} else {
			    	storageBuffer[wptr++] = '\n';
				}
			}
    	}
    	wptr += fsu->copy_string((char*)storageBuffer + wptr, midiConfig[k].nameInFile);
    	storageBuffer[wptr++] = '=';
    	wptr += fsu->printInt((char*)storageBuffer + wptr, (int)midiConfigBytes[k]);
    	storageBuffer[wptr++] = '\n';
    	storageBuffer[wptr++] = '\n';
    }
    // delete it so that we're sure the new one has the right size...
    remove(PROPERTIES);
    save(PROPERTIES, 0,  storageBuffer, wptr);
}



void ConfigurationFile::fillMidiConfig(uint8_t* midiConfigBytes, char* line) {
	char key[21];
	char value[21];

	int equalPos = fsu->getPositionOfEqual(line);
	if (equalPos == -1) {
		return;
	}
	fsu->getKey(line, key);

	for (int k=0; k < MIDICONFIG_SIZE; k++) {
		if (fsu->str_cmp(key, midiConfig[k].nameInFile) == 0) {
			fsu->getValue(line + equalPos+1, value);
			midiConfigBytes[k] = fsu->toInt(value);
			return;
		}
	}
}


