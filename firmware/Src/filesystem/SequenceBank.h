/*
 * SequenceBank.h
 *
 *  Created on: Sep 5, 2020
 *      Author: xavier
 */

#ifndef FILESYSTEM_SEQUENCEBANK_H_
#define FILESYSTEM_SEQUENCEBANK_H_

#include "PreenFMFileType.h"

enum SEQUENCE_BANK_VERSION {
    SEQUENCE_BANK_VERSION1 = 1
};

#define SEQUENCE_BANK_CURRENT_VERSION SEQUENCE_BANK_VERSION1

class Sequencer;

class SequenceBank : public PreenFMFileType {
public:
    SequenceBank();
    void setSequencer(Sequencer* sequencer);
    void loadSequence(const struct PFM3File* sequenceFile, int sequenceNumber);
    const char* loadSequenceName(const struct PFM3File* sequenceFile, int sequenceNumber);
    void createSequenceFile(const char* name);
    void saveSequence(const struct PFM3File* mixer, int sequenceNumber, char* sequenceName);

private:
    void loadSequenceVersion1(FIL* sequenceFile, int patchNumber);
    const char* getFolderName();
    bool isCorrectFile(char *name, int size) ;
    char sequenceName[13];
    Sequencer* sequencer;
};



#endif /* FILESYSTEM_SEQUENCEBANK_H_ */
