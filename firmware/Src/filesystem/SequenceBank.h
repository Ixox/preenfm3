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
