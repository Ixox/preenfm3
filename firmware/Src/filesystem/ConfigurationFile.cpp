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


#include "ConfigurationFile.h"
#include "Menu.h"

extern char lineBuffer[512];

ConfigurationFile::ConfigurationFile() {
	numberOfFilesMax_ = 0;
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
    	loop = fsu_->getLine(readProperties, line);
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
    	wptr += fsu_->copy_string((char*)storageBuffer + wptr, midiConfig[k].title);
    	storageBuffer[wptr++] = '\n';
    	if (midiConfig[k].maxValue < 10 && midiConfig[k].valueName != 0) {
	    	wptr += fsu_->copy_string((char*)storageBuffer + wptr, "#   0=");
			for (int o=0; o<midiConfig[k].maxValue; o++) {
		    	wptr += fsu_->copy_string((char*)storageBuffer + wptr, midiConfig[k].valueName[o]);
				if ( o != midiConfig[k].maxValue - 1) {
			    	wptr += fsu_->copy_string((char*)storageBuffer + wptr, ", ");
				} else {
			    	storageBuffer[wptr++] = '\n';
				}
			}
    	}
    	wptr += fsu_->copy_string((char*)storageBuffer + wptr, midiConfig[k].nameInFile);
    	storageBuffer[wptr++] = '=';
    	wptr += fsu_->printInt((char*)storageBuffer + wptr, (int)midiConfigBytes[k]);
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

	int equalPos = fsu_->getPositionOfEqual(line);
	if (equalPos == -1) {
		return;
	}
	fsu_->getKey(line, key);

	for (int k=0; k < MIDICONFIG_SIZE; k++) {
		if (fsu_->str_cmp(key, midiConfig[k].nameInFile) == 0) {
			fsu_->getValue(line + equalPos+1, value);
			midiConfigBytes[k] = fsu_->toInt(value);
			return;
		}
	}
}


