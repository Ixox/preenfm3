/*
 * ConfigurationFile.h
 *
 *  Created on: 24 juil. 2015
 *      Author: xavier
 */

#ifndef CONFIGURATIONFILE_H_
#define CONFIGURATIONFILE_H_

#include "PreenFMFileType.h"
#include "ScalaFile.h"

class ConfigurationFile: public PreenFMFileType {
public:
	ConfigurationFile();
	virtual ~ConfigurationFile();


	void loadConfig(uint8_t* midiConfigBytes);
	void saveConfig(uint8_t* midiConfigBytes);

protected:
	ScalaFile* scalaFile;
	const char* getFolderName();
	bool isCorrectFile(char *name, int size) { return true; }

	void fillMidiConfig(uint8_t* midiConfigBytes, char* line);
};

#endif /* CONFIGURATIONFILE_H_ */
