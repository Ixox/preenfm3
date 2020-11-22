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



#include <MixerBank.h>
#include <ScalaFile.h>
#include "Sequencer.h"
#include "MixerState.h"
#include "TftDisplay.h"

extern TftDisplay tft;

extern char patch_zeros[ALIGNED_PATCH_ZERO];

__attribute__((section(".ram_d2b"))) struct PFM3File preenFMMixerAlloc[NUMBEROFPREENFMMIXERS];
__attribute__((section(".ram_d2b"))) static FIL mixerFile;


MixerBank::MixerBank() {
    this->numberOfFilesMax_ = NUMBEROFPREENFMMIXERS;
    this->myFiles_ = preenFMMixerAlloc;
}

MixerBank::~MixerBank() {
    // TODO Auto-generated destructor stub
}

void MixerBank::init(struct OneSynthParams*timbre1, struct OneSynthParams*timbre2, struct OneSynthParams*timbre3,
        struct OneSynthParams*timbre4, struct OneSynthParams*timbre5, struct OneSynthParams*timbre6) {
    this->timbre[0] = timbre1;
    this->timbre[1] = timbre2;
    this->timbre[2] = timbre3;
    this->timbre[3] = timbre4;
    this->timbre[4] = timbre5;
    this->timbre[5] = timbre6;


    for (uint32_t k = 0; k < ALIGNED_PATCH_ZERO; k++) {
        patch_zeros[k] = 0;
    }
}

void MixerBank::setMixerState(struct MixerState* mixerState) {
    this->mixerState = mixerState;
}

void MixerBank::setSequencer(Sequencer* sequencer) {
    this->sequencer = sequencer;
}

void MixerBank::setScalaFile(ScalaFile* scalaFile) {
    this->scalaFile = scalaFile;
}

const char* MixerBank::getFolderName() {
    return PREENFM_DIR;
}

bool MixerBank::isCorrectFile(char *name, int size) {
    // exclude default.mix
    if (size < 100000) {
        return false;
    }

    int pointPos = -1;
    for (int k = 1; k < 9 && pointPos == -1; k++) {
        if (name[k] == '.') {
            pointPos = k;
        }
    }
    if (pointPos == -1)
        return false;
    if (name[pointPos + 1] != 'm' && name[pointPos + 1] != 'M')
        return false;
    if (name[pointPos + 2] != 'i' && name[pointPos + 2] != 'I')
        return false;
    if (name[pointPos + 3] != 'x' && name[pointPos + 3] != 'X')
        return false;

    return true;
}

void MixerBank::removeDefaultMixer() {
    remove(DEFAULT_MIXER);
}


/*
 * Default mixer also save the current sequence
 */
bool MixerBank::saveDefaultMixer() {
    bool savedOK = true;

    FRESULT fatFSResult = f_open(&mixerFile, getFileName(DEFAULT_MIXER), FA_OPEN_ALWAYS | FA_WRITE);
    if (fatFSResult == FR_OK) {
        f_lseek(&mixerFile, 0);
        savedOK = saveMixerData(&mixerFile, 0, this->mixerState);

        f_close(&mixerFile);
    } else {
        savedOK = false;
    }

    return savedOK;
}


bool MixerBank::saveMixerData(FIL* file, uint8_t mixerNumber, MixerState* mixerStateToSave) {
    uint32_t mixerStateSize;
    UINT byteWritten;

    for (int i = 0; i < PROPERTY_FILE_SIZE; i++) {
        storageBuffer[i] = 0;
    }
    mixerStateToSave->getFullState(storageBuffer, &mixerStateSize);

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        FlashSynthParams *toSave = (FlashSynthParams *)(storageBuffer + ALIGNED_PATCH_SIZE * t + ALIGNED_MIXER_SIZE);
        convertParamsToFlash(this->timbre[t], toSave, true);
    }
    f_lseek(file, mixerNumber * FULL_MIXER_SIZE);
    f_write(file, (void*) storageBuffer, ALIGNED_MIXER_SIZE + ALIGNED_PATCH_SIZE * NUMBER_OF_TIMBRES, &byteWritten) ;

    return true;
}


bool MixerBank::loadMixerData(FIL* file, uint8_t mixerNumber) {
    UINT byteRead;
    UINT result;

    for (uint32_t i = 0; i < PROPERTY_FILE_SIZE; i++) {
        storageBuffer[i] = 0;
    }

    f_lseek(file, mixerNumber * FULL_MIXER_SIZE);
    f_read(file, storageBuffer, MIXER_SIZE, &byteRead);
    mixerState->restoreFullState(storageBuffer);

    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        f_lseek(file, mixerNumber * FULL_MIXER_SIZE + ALIGNED_MIXER_SIZE + t * ALIGNED_PATCH_SIZE);
        result = f_read(file, storageBuffer, PFM_PATCH_SIZE, &byteRead);
        if (result == FR_OK && byteRead == PFM_PATCH_SIZE) {
            convertFlashToParams((struct FlashSynthParams *) storageBuffer, this->timbre[t], true);

            // Init scala scale if enabled
            if (mixerState->instrumentState_[t].scalaEnable == 1) {
                scalaFile->loadScalaScale(mixerState, t);
            }
        } else {
            this->timbre[t]->presetName[0] = '#';
            this->timbre[t]->presetName[1] = '#';
            this->timbre[t]->presetName[2] = 0;
        }
    }
    return true;
}

bool MixerBank::loadDefaultMixer() {
    FRESULT result = f_open(&mixerFile, getFileName(DEFAULT_MIXER), FA_READ);
    if (result == FR_OK) {
		loadMixerData(&mixerFile, 0);
        f_close(&mixerFile);
    } else {
        return false;
    }
    return true;
}

void MixerBank::createMixerBank(const char* name) {
    if (!isInitialized_) {
        initFiles();
    }
    uint32_t defaultMixerSize;
    const struct PFM3File * newBank = addEmptyFile(name);
    const char* fullBankName = getFullName(name);

    if (newBank == 0) {
        return;
    }

    FRESULT fatFSResult = f_open(&mixerFile, fullBankName, FA_OPEN_ALWAYS | FA_WRITE);
    if (fatFSResult != FR_OK) {
        return;
    }

    for (uint32_t i = 0; i < PROPERTY_FILE_SIZE; i++) {
        storageBuffer[i] = 0;
    }


    for (int t = 0; t < NUMBER_OF_TIMBRES; t++) {
        FlashSynthParams *toSave = (FlashSynthParams *)(storageBuffer + ALIGNED_PATCH_SIZE * t + ALIGNED_MIXER_SIZE);
        convertParamsToFlash(&preenMainPreset, toSave, true);
    }

    UINT byteWritten;
    for (int mixerNumber = 0; mixerNumber < NUMBER_OF_MIXERS_PER_BANK; mixerNumber++) {

        tft.setCharColor(COLOR_GRAY);
        tft.setCursor(15, 12);
        tft.print(mixerNumber + 1);
        tft.print("/");
        tft.print(NUMBER_OF_MIXERS_PER_BANK);


        mixerState->getFullDefaultState(storageBuffer, &defaultMixerSize, mixerNumber + 1);
        f_write(&mixerFile, (void*) storageBuffer, ALIGNED_MIXER_SIZE + ALIGNED_PATCH_SIZE * NUMBER_OF_TIMBRES, &byteWritten);
    }
    f_close(&mixerFile);
}

bool MixerBank::loadMixer(const struct PFM3File* mixer, int mixerNumber) {
    const char* fullBankName = getFullName(mixer->name);

    FRESULT result = f_open(&mixerFile, fullBankName, FA_READ);
    if (result == FR_OK) {
        // Point to asked mixer
        loadMixerData(&mixerFile, mixerNumber);
        f_close(&mixerFile);
    } else {
        return false;
    }
    return true;
}

const char* MixerBank::loadMixerName(const struct PFM3File* mixer, int mixerNumber) {
    const char* fullBankName = getFullName(mixer->name);
    load(fullBankName, FULL_MIXER_SIZE * mixerNumber, (void*) storageBuffer, 16);
    char* presetNameBuffer =  MixerState::getMixNameFromFile(storageBuffer);
    for (int p = 0; p < 12; p++) {
        presetName[p] = presetNameBuffer[p];
    }
    presetName[12] = 0;
    return presetName;
}

bool MixerBank::saveMixer(const struct PFM3File* mixer, int mixerNumber, char* mixerName) {
    const char* fullBankName = getFullName(mixer->name);

    fsu_->copy(this->mixerState->mixName_, mixerName, 12);

    FRESULT result = f_open(&mixerFile, fullBankName, FA_WRITE);
    if (result == FR_OK) {
        // Point to asked mixer
        f_lseek(&mixerFile, FULL_MIXER_SIZE * mixerNumber );
        saveMixerData(&mixerFile, mixerNumber, this->mixerState);
        f_close(&mixerFile);
    } else {
        return false;
    }
    return true;
}


