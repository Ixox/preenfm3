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



/*
 * SEQUENCE_BANK_VERSION1 : uin32_t on 4 bytes
 * Per Bank :
 * - State : 1024 bytes (contains its own version)
 * - Sequencer : 28720 bytes
     - SeqMidiAction actions[2048]; (16384 bytes)
        uint16_t when;
        uint16_t nextIndex;
        uint8_t actionType;
        uint8_t param1;
        uint8_t param2;
        uint8_t unused;
     - StepSeqValue stepNotes[6][256 + 1]; (12336 bytes)
        uint8_t values[8];
 */


#include <SequenceBank.h>
#include "Sequencer.h"


__attribute__((section(".ram_d2b"))) struct PFM3File preenFMSequenceAlloc[NUMBEROFPREENFMSEQUENCES];

extern SeqMidiAction actions[SEQ_ACTION_SIZE];
extern StepSeqValue stepNotes[NUMBER_OF_TIMBRES][256 + 1];

SequenceBank::SequenceBank() {
    this->numberOfFilesMax = NUMBEROFPREENFMSEQUENCES;
    this->myFiles = preenFMSequenceAlloc;
}

const char* SequenceBank::getFolderName() {
    return PREENFM_DIR;
}


bool SequenceBank::isCorrectFile(char *name, int size) {
    int pointPos = -1;
    for (int k = 1; k < 9 && pointPos == -1; k++) {
        if (name[k] == '.') {
            pointPos = k;
        }
    }
    if (pointPos == -1)
        return false;
    if (name[pointPos + 1] != 's' && name[pointPos + 1] != 'S')
        return false;
    if (name[pointPos + 2] != 'e' && name[pointPos + 2] != 'E')
        return false;
    if (name[pointPos + 3] != 'q' && name[pointPos + 3] != 'Q')
        return false;

    return true;
}

void SequenceBank::setSequencer(Sequencer* sequencer) {
    this->sequencer = sequencer;
}

void SequenceBank::loadSequence(const struct PFM3File* bank, int patchNumber) {
    if (!isInitialized) {
        initFiles();
    }

    const char* fullSeqBankName = getFullName(bank->name);
    uint32_t bankBersion;
    UINT byteRead;
    FIL fileToLoad;

    if (f_open(&fileToLoad, fullSeqBankName, FA_READ) == FR_OK) {
        f_read(&fileToLoad, (void *)&bankBersion, 4, &byteRead);

        switch (bankBersion) {
            case SEQUENCE_BANK_VERSION1:
                loadSequenceVersion1(&fileToLoad, patchNumber);
                break;
        }
        f_close(&fileToLoad);
    }
}

void SequenceBank::loadSequenceVersion1(FIL* sequenceFile, int patchNumber) {
    UINT byteRead;
    f_lseek(sequenceFile, 4 + ((1024 + 28720) * patchNumber));

    for (int i = 0; i < 1024; i++) {
        storageBuffer[i] = 0;
    }

    // We load 1024 bytes for sequencer fullstate
    f_read(sequenceFile, storageBuffer, 1024, &byteRead);
    sequencer->setFullState((uint8_t*)storageBuffer);

    f_read(sequenceFile, actions, 16384, &byteRead);
    f_read(sequenceFile, stepNotes, 12336, &byteRead);
}


const char* SequenceBank::loadSequenceName(const struct PFM3File* bank, int patchNumber) {
    if (!isInitialized) {
        initFiles();
    }

    const char* fullSeqBankName = getFullName(bank->name);
    uint32_t bankBersion;
    UINT byteRead;
    FIL fileToLoad;

    if (f_open(&fileToLoad, fullSeqBankName, FA_READ) == FR_OK) {
        f_read(&fileToLoad, (void *)&bankBersion, 4, &byteRead);

        switch (bankBersion) {
            case SEQUENCE_BANK_VERSION1:
                f_lseek(&fileToLoad, 4 + (1024 + 28720) * patchNumber);
                f_read(&fileToLoad, storageBuffer, 20, &byteRead);
                const char* sequenceNameInBuffer = sequencer->getSequenceNameInBuffer(storageBuffer);
                for (int s = 0; s < 12; s++) {
                    sequenceName[s] = sequenceNameInBuffer[s];
                }
                sequenceName[12] = 0;
                f_close(&fileToLoad);
                return sequenceName;
                break;
        }
    }

    return "##";
}


void SequenceBank::createSequenceFile(const char* name) {
    if (!isInitialized) {
        initFiles();
    }

    FIL sequenceFile;
    const struct PFM3File * newBank = addEmptyFile(name);
    const char* fullBankName = getFullName(name);
    UINT byteWritten;
    uint32_t seqStatesize;

    if (newBank == 0) {
        return;
    }

    FRESULT fatFSResult = f_open(&sequenceFile, fullBankName, FA_OPEN_ALWAYS | FA_WRITE);
    if (fatFSResult != FR_OK) {
        return;
    }

    uint32_t bankVersion = (uint32_t)SEQUENCE_BANK_CURRENT_VERSION;
    f_write(&sequenceFile, (void *)&bankVersion, 4, &byteWritten);

    for (int i = 0; i < PROPERTY_FILE_SIZE; i++) {
        storageBuffer[i] = 0;
    }

    sequencer->getFullDefaultState((uint8_t*)storageBuffer, &seqStatesize);
    char* sequenceNameInBuffer = sequencer->getSequenceNameInBuffer(storageBuffer);
    fsu->copy(sequenceNameInBuffer, "Sequence \0\0\0\0", 12);
    for (int s = 0; s < NUMBER_OF_SEQUENCES_PER_BANK; s++) {
        // Name
        fsu->addNumber(sequenceNameInBuffer, 9, s + 1);

        // We save 1024 bytes for sequencer fullstate
        f_write(&sequenceFile, storageBuffer, 1024, &byteWritten);

        int numberOfZeros = sizeof(actions) + sizeof(stepNotes);
        while (numberOfZeros > 0) {
            UINT toWrite = numberOfZeros > 4096 ? 4096 : numberOfZeros;
            f_write(&sequenceFile, storageBuffer + 1024, toWrite, &byteWritten);
            numberOfZeros -= byteWritten;
        }
    }
    f_close(&sequenceFile);
}

void SequenceBank::saveSequence(const struct PFM3File* sequencePFMFile, int sequenceNumber, char* sequenceName) {
    if (!isInitialized) {
        initFiles();
    }

    const char* fullSeqBankName = getFullName(sequencePFMFile->name);
    UINT byteWritten;
    FIL sequenceFile;
    uint32_t seqStatesize;

    for (int i = 0; i < 1024; i++) {
        storageBuffer[i] = 0;
    }

    sequencer->setSequenceName(sequenceName);

    if (f_open(&sequenceFile, fullSeqBankName, FA_WRITE) == FR_OK) {
        f_lseek(&sequenceFile, 4 + ((1024 + 28720) * sequenceNumber));

        // We copy the state to property files
        sequencer->getFullState((uint8_t*)storageBuffer, &seqStatesize);


        // and save 1024 chars
        f_write(&sequenceFile, storageBuffer, 1024, &byteWritten);

        f_write(&sequenceFile, actions, 16384, &byteWritten);
        f_write(&sequenceFile, stepNotes, 12336, &byteWritten);

        f_close(&sequenceFile);
    }

}
