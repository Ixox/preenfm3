/*
 * ScalaFile.h
 *
 *  Created on: 24 juil. 2015
 *      Author: xavier
 */

#ifndef SCALAFILE_H_
#define SCALAFILE_H_

#include "PreenFMFileType.h"
#define MAX_NUMBER_OF_INTERVALS 36

class MixerState;

class ScalaFile: public PreenFMFileType {
public:
    ScalaFile();
    virtual ~ScalaFile();
    float* loadScalaScale(MixerState* mixerState, int instrumentNumber);
    float* applyScalaScale(MixerState* mixerState, int instrumentNumber);
    void updateScale(MixerState* mixerState, int instrumentNumber);

protected:
    const char* getFolderName();
    bool isCorrectFile(char *name, int size);

private:
    float getScalaIntervale(const char* line);
    struct PFM3File *scalaScaleFile;
    float interval[NUMBER_OF_TIMBRES][MAX_NUMBER_OF_INTERVALS];
    uint8_t numberOfDegrees[NUMBER_OF_TIMBRES];

};

#endif /* SCALAFILE_H_ */
